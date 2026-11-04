// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Canvas.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float wheelZoom = 1.15f;      // a step of the wheel zooms by this much
constexpr float panSlop = 3;            // dips the right button moves before it pans (and asks for no menu)
constexpr float leastGridGap = 6;       // dips between grid lines, at least: closer ones are left out

} // namespace

Canvas::Canvas() {
    setClipsChildren(true);
    setFocusable(true);
}

Canvas::~Canvas() = default;

const CanvasStyle& Canvas::style() const {
    if (const UiContext* ui = context())
        return ui->theme.canvas;
    static const CanvasStyle unstyled = Theme::dark().canvas;
    return unstyled;
}

// ---- items ----

std::unique_ptr<Widget> Canvas::removeItem(Widget& item) {
    positions_.erase(&item);
    return removeChild(item);
}

void Canvas::setItemPosition(Widget& item, Point at) {
    positions_[&item] = at;
    // laid out already: only moved (nothing else is laid out again)
    const Rect bounds = item.bounds();
    if (bounds.width > 0 || bounds.height > 0)
        item.arrange({at.x, at.y, bounds.width, bounds.height});
    else
        invalidateLayout();
    invalidatePaint();
}

Point Canvas::itemPosition(const Widget& item) const {
    const auto it = positions_.find(&item);
    return it != positions_.end() ? it->second : Point{};
}

Rect Canvas::itemRect(const Widget& item) const {
    const Point at = itemPosition(item);
    return {at.x, at.y, item.size().width, item.size().height};
}

Rect Canvas::itemsBounds() const {
    Rect all;
    for (const auto& child : children()) {
        if (child->visible())
            all = all.united(itemRect(*child));
    }
    return all;
}

std::vector<Widget*> Canvas::itemsIn(const Rect& area) const {
    std::vector<Widget*> found;
    for (const auto& child : children()) {
        if (child->visible() && itemRect(*child).intersects(area))
            found.push_back(child.get());
    }
    return found;
}

void Canvas::raiseItem(Widget& item) {
    setChildIndex(item, children().size());
}

void Canvas::lowerItem(Widget& item) {
    setChildIndex(item, 0);
}

// ---- the view ----

void Canvas::setZoomRange(float minimum, float maximum) {
    minimumZoom_ = std::max(0.001f, minimum);
    maximumZoom_ = std::max(minimumZoom_, maximum);
    setZoom(zoom_);
}

void Canvas::zoomSmoothly(float zoom, std::optional<Point> anchor) {
    zoom = std::clamp(zoom, minimumZoom_, maximumZoom_);
    if (zoom == (viewMotion_.running() ? move_.toZoom : zoom_))
        return;
    const Point at = anchor.value_or(Point{size().width / 2, size().height / 2});
    const UiContext* ui = context();
    moveView(zoom, toCanvas(at), at, ui ? Duration(ui->theme.motion.scroll) : Duration::zero());
}

void Canvas::moveView(float zoom, Point canvasPoint, Point at, Duration duration) {
    move_ = {zoom_, zoom, toCanvas(at), canvasPoint, at};
    viewMotion_.set(0);
    viewMotion_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    viewStepped();
}

void Canvas::viewStepped() {
    const float t = viewMotion_.value();
    const float zoom = std::exp(std::log(move_.fromZoom) + (std::log(move_.toZoom) - std::log(move_.fromZoom)) * t);
    const Point point = move_.fromPoint + (move_.toPoint - move_.fromPoint) * t;
    zoom_ = zoom;
    origin_ = move_.at - point * zoom;
    viewChanged();
}

void Canvas::setZoom(float zoom, std::optional<Point> anchor) {
    viewMotion_.set(1);         // what was on its way stops
    zoom = std::clamp(zoom, minimumZoom_, maximumZoom_);
    if (zoom == zoom_)
        return;
    // the point of the canvas under the anchor stays under it
    const Point at = anchor.value_or(Point{size().width / 2, size().height / 2});
    const Point under = toCanvas(at);
    zoom_ = zoom;
    origin_ = at - under * zoom_;
    viewChanged();
}

void Canvas::setOrigin(Point origin) {
    viewMotion_.set(1);
    if (origin.x == origin_.x && origin.y == origin_.y)
        return;
    origin_ = origin;
    viewChanged();
}

void Canvas::centerOn(Point point) {
    setOrigin(Point{size().width / 2, size().height / 2} - point * zoom_);
}

void Canvas::frame(const Rect& area, float margin, bool animated) {
    if (area.empty())
        return;
    if (size().width <= 0 || size().height <= 0) {
        pendingFrame_ = area.inflated(margin);
        return;
    }
    const float width = std::max(1.0f, size().width - 2 * margin), height = std::max(1.0f, size().height - 2 * margin);
    const float zoom = std::clamp(std::min({width / area.width, height / area.height, 1.0f}), minimumZoom_, maximumZoom_);
    const Point middle{size().width / 2, size().height / 2};
    const UiContext* ui = context();
    if (animated && ui) {
        // the middle of it going to the middle of the area
        moveView(zoom, area.center(), middle, ui->theme.motion.slow);
        return;
    }
    viewMotion_.set(1);
    zoom_ = zoom;
    origin_ = middle - area.center() * zoom_;
    viewChanged();
}

void Canvas::frameItems(float margin, bool animated) {
    if (size().width <= 0 || size().height <= 0) {
        pendingItemsFrame_ = margin;
        return;
    }
    frame(itemsBounds(), margin, animated);
}

Rect Canvas::visibleArea() const {
    return viewTransform().unmap(Rect{0, 0, size().width, size().height});
}

void Canvas::setGridVisible(bool visible) {
    grid_ = visible;
    invalidatePaint();
}

void Canvas::viewChanged() {
    invalidatePaint();          // nothing is laid out again: its items are where they were on the canvas
    if (onViewChanged_)
        onViewChanged_();
}

// ---- layout ----

Size Canvas::measureContent(Size available) {
    return {std::isfinite(available.width) ? available.width : 400.0f, std::isfinite(available.height) ? available.height : 300.0f};
}

void Canvas::arrangeContent(const Rect&) {
    // each item where it was put, as big as it wants
    for (const auto& child : children()) {
        if (!child->visible())
            continue;
        const Size wanted = child->measure({unbounded, unbounded});
        const Point at = itemPosition(*child);
        child->arrange({at.x, at.y, wanted.width, wanted.height});
    }
    if (size().width > 0 && size().height > 0) {
        if (pendingFrame_)
            frame(*std::exchange(pendingFrame_, std::nullopt), 0, false);     // what it shows first: at once
        if (pendingItemsFrame_)
            frame(itemsBounds(), *std::exchange(pendingItemsFrame_, std::nullopt), false);
    }
}

// ---- painting ----

void Canvas::paint(PaintContext& context) {
    const CanvasStyle& s = style();
    paint::Painter& p = context.painter;
    const Rect all{0, 0, size().width, size().height};
    p.pushClip(all);
    p.fillRect(all, s.background);
    if (grid_ && s.gridSpacing > 0) {
        // lines every spacing of the canvas, every major one only when they would be too close
        float spacing = s.gridSpacing;
        const int every = std::max(1, s.majorEvery);
        bool minor = true;
        if (spacing * zoom_ < leastGridGap) {
            spacing *= static_cast<float>(every);
            minor = false;
        }
        if (spacing * zoom_ >= leastGridGap) {
            const Rect area = visibleArea();
            const auto lines = [&](float from, float to, auto&& draw) {
                for (long long k = static_cast<long long>(std::ceil(from / spacing)); k * spacing <= to; ++k)
                    draw(static_cast<float>(k) * spacing, !minor || k % every == 0);
            };
            lines(area.left(), area.right(), [&](float x, bool major) {
                const float at = std::round(fromCanvas({x, 0}).x);
                p.fillRect({at, 0, 1, all.height}, major ? s.majorGrid : s.grid);
            });
            lines(area.top(), area.bottom(), [&](float y, bool major) {
                const float at = std::round(fromCanvas({0, y}).y);
                p.fillRect({0, at, all.width, 1}, major ? s.majorGrid : s.grid);
            });
        }
    }
    p.pushTransform(viewTransform());
    paintUnderItems(context);
    p.popTransform();
    p.popClip();
}

void Canvas::paintOver(PaintContext& context) {
    paint::Painter& p = context.painter;
    p.pushClip({0, 0, size().width, size().height});
    p.pushTransform(viewTransform());
    paintOverItems(context);
    p.popTransform();
    p.popClip();
}

// ---- the mouse ----

std::optional<platform::Cursor> Canvas::cursorAt(Point point) const {
    if (panning_ != Pan::None)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

void Canvas::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Wheel:
        if (event.wheelDelta.y != 0) {
            zoomSmoothly((viewMotion_.running() ? move_.toZoom : zoom_) * std::pow(wheelZoom, event.wheelDelta.y), event.position);
            event.handled = true;
        }
        break;
    case MouseEventType::Press:
        if (panning_ != Pan::None)
            return;
        if (event.button == platform::MouseButton::Middle || event.button == platform::MouseButton::Right) {
            panning_ = event.button == platform::MouseButton::Middle ? Pan::Middle : Pan::Right;
            panFrom_ = event.position;
            panMoved_ = false;
            event.handled = true;
        }
        break;
    case MouseEventType::Move:
        if (panning_ != Pan::None) {
            const Point moved = event.position - panFrom_;
            if (!panMoved_ && std::abs(moved.x) < panSlop && std::abs(moved.y) < panSlop) {
                event.handled = true;
                return;
            }
            panMoved_ = true;
            panFrom_ = event.position;
            setOrigin(origin_ + moved);
            event.handled = true;
        }
        break;
    case MouseEventType::Release: {
        const bool middle = event.button == platform::MouseButton::Middle && panning_ == Pan::Middle;
        const bool right = event.button == platform::MouseButton::Right && panning_ == Pan::Right;
        if (!middle && !right)
            return;
        event.handled = true;
        panning_ = Pan::None;
        if (right && !panMoved_ && onContextRequested_)
            onContextRequested_(toCanvas(event.position));
        break;
    }
    }
}

} // namespace glw
