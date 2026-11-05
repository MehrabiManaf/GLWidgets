// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ScrollArea.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <utility>

namespace glw {

namespace {

constexpr float wheelStep = 48;     // dips a step of the wheel scrolls: three lines or so

} // namespace

ScrollArea::ScrollArea() : ScrollArea(Props{}) {}

ScrollArea::ScrollArea(Props props) : horizontal_(props.horizontal), vertical_(props.vertical), style_(std::move(props.style)) {
    setClipsChildren(true);
}

void ScrollArea::setContent(std::unique_ptr<Widget> content) {
    if (content_)
        removeChild(*content_);
    content_ = content ? &addChild(std::move(content)) : nullptr;
    smoothX_.stop();
    smoothY_.stop();
    offset_ = {};
}

void ScrollArea::setStyle(std::optional<ScrollBarStyle> style) {
    style_ = std::move(style);
    invalidateLayout();
}

const ScrollBarStyle& ScrollArea::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.scrollBar;
    static const ScrollBarStyle unstyled = Theme::dark().scrollBar;
    return unstyled;
}

// ---- layout ----

Size ScrollArea::measureContent(Size available) {
    if (!content_)
        return {};
    const Insets m = content_->margin();
    const Size space{horizontal_ != ScrollBarPolicy::Never ? unbounded : std::max(0.0f, available.width - m.horizontal()),
                     vertical_ != ScrollBarPolicy::Never ? unbounded : std::max(0.0f, available.height - m.vertical())};
    const Size s = content_->measure(space);
    // as much as there is room for: the rest is scrolled to
    return {std::min(s.width + m.horizontal(), available.width), std::min(s.height + m.vertical(), available.height)};
}

void ScrollArea::arrangeContent(const Rect& area) {
    viewport_ = area;
    if (!content_)
        return;
    const float bar = style().width;
    const bool rtl = rightToLeft();
    const Insets m = content_->margin();
    Size wanted;
    // a bar takes room, which may make the other one needed: decided again until it holds
    showVertical_ = vertical_ == ScrollBarPolicy::Always;
    showHorizontal_ = horizontal_ == ScrollBarPolicy::Always;
    for (int pass = 0; pass < 3; ++pass) {
        viewport_ = area;
        if (showVertical_) {
            viewport_.width = std::max(0.0f, viewport_.width - bar);
            if (rtl)
                viewport_.x += bar;
        }
        if (showHorizontal_)
            viewport_.height = std::max(0.0f, viewport_.height - bar);
        const Size s = content_->measure({horizontal_ != ScrollBarPolicy::Never ? unbounded : std::max(0.0f, viewport_.width - m.horizontal()),
                                          vertical_ != ScrollBarPolicy::Never ? unbounded : std::max(0.0f, viewport_.height - m.vertical())});
        wanted = {s.width + m.horizontal(), s.height + m.vertical()};
        if (vertical_ == ScrollBarPolicy::Auto && !showVertical_ && wanted.height > viewport_.height + 0.5f)
            showVertical_ = true;
        else if (horizontal_ == ScrollBarPolicy::Auto && !showHorizontal_ && wanted.width > viewport_.width + 0.5f)
            showHorizontal_ = true;
        else
            break;
    }
    contentSize_ = {horizontal_ != ScrollBarPolicy::Never ? std::max(wanted.width, viewport_.width) : viewport_.width,
                    vertical_ != ScrollBarPolicy::Never ? std::max(wanted.height, viewport_.height) : viewport_.height};
    const Point limit = maximumOffset();
    offset_ = {std::clamp(offset_.x, 0.0f, limit.x), std::clamp(offset_.y, 0.0f, limit.y)};
    placeContent();
}

void ScrollArea::placeContent() {
    // from the start side: the content's start is the viewport's, less what is scrolled
    const bool rtl = rightToLeft();
    const float x = rtl ? viewport_.right() - contentSize_.width + offset_.x : viewport_.x - offset_.x;
    arrangeInCell(*content_, {x, viewport_.y - offset_.y, contentSize_.width, contentSize_.height}, rtl);
}

Point ScrollArea::maximumOffset() const {
    return {std::max(0.0f, contentSize_.width - viewport_.width), std::max(0.0f, contentSize_.height - viewport_.height)};
}

void ScrollArea::setOffset(Point offset) {
    smoothX_.stop();
    smoothY_.stop();
    show(offset);
}

void ScrollArea::show(Point offset) {
    const Point limit = maximumOffset();
    offset = {std::clamp(offset.x, 0.0f, limit.x), std::clamp(offset.y, 0.0f, limit.y)};
    if (offset == offset_)
        return;
    offset_ = offset;
    if (content_)
        placeContent();     // the content moves; its size, and everything else, stays
    invalidatePaint();
}

Point ScrollArea::going() const {
    return {static_cast<float>(smoothX_.target(offset_.x)), static_cast<float>(smoothY_.target(offset_.y))};
}

void ScrollArea::scrollTo(Point offset) {
    const Point limit = maximumOffset();
    offset = {std::clamp(offset.x, 0.0f, limit.x), std::clamp(offset.y, 0.0f, limit.y)};
    const Point to = going();
    const UiContext* ui = context();
    const Duration duration = ui ? Duration(ui->theme.motion.scroll) : Duration::zero();
    if (offset.x != to.x)
        smoothX_.scrollTo(scheduler(), offset_.x, offset.x, duration);
    if (offset.y != to.y)
        smoothY_.scrollTo(scheduler(), offset_.y, offset.y, duration);
}

void ScrollArea::scrollBy(Point delta) {
    scrollTo(going() + delta);
}

void ScrollArea::revealArea(const Rect& area) {
    const bool rtl = rightToLeft();
    // from where it shows now (where `area` is), eased there
    Point offset = offset_;
    // above the viewport, or longer than it: its top shows; below it: its bottom
    if (area.top() < viewport_.top() || area.height > viewport_.height)
        offset.y += area.top() - viewport_.top();
    else if (area.bottom() > viewport_.bottom())
        offset.y += area.bottom() - viewport_.bottom();
    // across: moving the content right is less offset in left to right, more in right to left
    const float sign = rtl ? -1.0f : 1.0f;
    if (area.right() > viewport_.right())
        offset.x += sign * (area.right() - viewport_.right());
    else if (area.left() < viewport_.left())
        offset.x -= sign * (viewport_.left() - area.left());
    if (offset != offset_)          // it shows: a scroll on its way is left alone
        scrollTo(offset);
}

// ---- the bars ----

std::optional<ScrollArea::Bar> ScrollArea::verticalBar() const {
    if (!showVertical_)
        return std::nullopt;
    const ScrollBarStyle& s = style();
    const Rect area = contentRect();
    Bar bar;
    bar.track = {rightToLeft() ? area.x : area.right() - s.width, viewport_.y, s.width, viewport_.height};
    const float length = std::min(bar.track.height, std::max(s.minimumThumb, bar.track.height * viewport_.height / contentSize_.height));
    const float limit = maximumOffset().y;
    const float at = limit > 0 ? (bar.track.height - length) * offset_.y / limit : 0;
    bar.thumb = {bar.track.x, bar.track.y + at, bar.track.width, length};
    return bar;
}

std::optional<ScrollArea::Bar> ScrollArea::horizontalBar() const {
    if (!showHorizontal_)
        return std::nullopt;
    const ScrollBarStyle& s = style();
    Bar bar;
    bar.track = {viewport_.x, viewport_.bottom(), viewport_.width, s.width};
    const float length = std::min(bar.track.width, std::max(s.minimumThumb, bar.track.width * viewport_.width / contentSize_.width));
    const float limit = maximumOffset().x;
    const float at = limit > 0 ? (bar.track.width - length) * offset_.x / limit : 0;
    // from the start side
    const float x = rightToLeft() ? bar.track.right() - length - at : bar.track.x + at;
    bar.thumb = {x, bar.track.y, length, bar.track.height};
    return bar;
}

ScrollArea::Part ScrollArea::partAt(Point p) const {
    if (const auto bar = verticalBar(); bar && bar->track.contains(p))
        return bar->thumb.contains(p) ? Part::VerticalThumb : Part::VerticalTrack;
    if (const auto bar = horizontalBar(); bar && bar->track.contains(p))
        return bar->thumb.contains(p) ? Part::HorizontalThumb : Part::HorizontalTrack;
    return Part::None;
}

void ScrollArea::paintOver(PaintContext& context) {
    const ScrollBarStyle& s = style();
    paint::Painter& p = context.painter;
    const Duration fast = context.ui.theme.motion.fast;
    // thin until the mouse is over the bar or the thumb is dragged
    const auto draw = [&](const Bar& bar, Part thumb, Part track, Animation& wide) {
        p.fillRect(bar.track, s.track);
        const bool over = hoveredPart_ == thumb || hoveredPart_ == track || dragging_ == thumb;
        wide.animateTo(scheduler(), over ? 1.0f : 0.0f, fast);
        const VisualState state{.hovered = hoveredPart_ == thumb || dragging_ == thumb, .pressed = dragging_ == thumb};
        const bool vertical = thumb == Part::VerticalThumb;
        paintScrollThumb(p, bar.thumb, vertical, !vertical || !rightToLeft(), wide.value(), s.thumb.resolve(state), s.cornerRadius);
    };
    if (const auto bar = verticalBar())
        draw(*bar, Part::VerticalThumb, Part::VerticalTrack, wideVertical_);
    if (const auto bar = horizontalBar())
        draw(*bar, Part::HorizontalThumb, Part::HorizontalTrack, wideHorizontal_);
}

// ---- events ----

void ScrollArea::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Wheel: {
        // with Shift, or a wheel that turns across: across, toward the end side when it turns to the right in left to right
        const bool across = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
        Point delta{event.wheelDelta.x * wheelStep * (rightToLeft() ? -1.0f : 1.0f), -event.wheelDelta.y * wheelStep};
        if (across)
            delta = {-event.wheelDelta.y * wheelStep, 0};
        const Point before = going();
        // turned again on the way: further (by fractions of a step, a touchpad's or a fine wheel's: as it turns)
        if (delta.x != 0 && !event.wheelPrecise)
            delta.x = static_cast<float>(smoothX_.boosted(scheduler(), delta.x));
        if (delta.y != 0 && !event.wheelPrecise)
            delta.y = static_cast<float>(smoothY_.boosted(scheduler(), delta.y));
        scrollBy(delta);
        // what does not move this one may move a scroll area around it
        event.handled = going() != before;
        break;
    }
    case MouseEventType::Press: {
        if (event.button != platform::MouseButton::Left)
            return;
        const Part part = partAt(event.position);
        if (part == Part::None)
            return;
        event.handled = true;
        if (part == Part::VerticalThumb) {
            dragging_ = part;
            grab_ = event.position.y - verticalBar()->thumb.y;
        } else if (part == Part::HorizontalThumb) {
            dragging_ = part;
            grab_ = event.position.x - horizontalBar()->thumb.x;
        } else if (part == Part::VerticalTrack) {
            // a page toward where the track was pressed
            const float page = viewport_.height * 0.9f;
            scrollBy({0, event.position.y < verticalBar()->thumb.y ? -page : page});
        } else {
            const auto bar = horizontalBar();
            const float page = viewport_.width * 0.9f;
            const bool leftOfThumb = event.position.x < bar->thumb.x;
            scrollBy({(leftOfThumb != rightToLeft()) ? -page : page, 0});
        }
        invalidatePaint();
        break;
    }
    case MouseEventType::Move: {
        if (dragging_ == Part::VerticalThumb) {
            const Bar bar = *verticalBar();
            const float room = bar.track.height - bar.thumb.height;
            const float at = std::clamp(event.position.y - grab_ - bar.track.y, 0.0f, room);
            setOffset({offset_.x, room > 0 ? at / room * maximumOffset().y : 0});
            event.handled = true;
        } else if (dragging_ == Part::HorizontalThumb) {
            const Bar bar = *horizontalBar();
            const float room = bar.track.width - bar.thumb.width;
            const float at = std::clamp(event.position.x - grab_ - bar.track.x, 0.0f, room);
            const float fromStart = rightToLeft() ? room - at : at;
            setOffset({room > 0 ? fromStart / room * maximumOffset().x : 0, offset_.y});
            event.handled = true;
        }
        if (const Part part = partAt(event.position); part != hoveredPart_) {
            hoveredPart_ = part;
            invalidatePaint();
        }
        break;
    }
    case MouseEventType::Release:
        if (dragging_ != Part::None) {
            dragging_ = Part::None;
            event.handled = true;
            invalidatePaint();
        }
        break;
    }
}

void ScrollArea::onKey(KeyEvent& event) {
    // what the focused widget in it did not take
    if (!event.pressed)
        return;
    if (event.key == platform::Key::PageDown)
        scrollBy({0, viewport_.height * 0.9f});
    else if (event.key == platform::Key::PageUp)
        scrollBy({0, -viewport_.height * 0.9f});
    else
        return;
    event.handled = true;
}

void ScrollArea::onHoverChanged(bool hovered) {
    if (!hovered && hoveredPart_ != Part::None) {
        hoveredPart_ = Part::None;
        invalidatePaint();
    }
}

} // namespace glw
