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

void ScrollArea::placeBars() {
    const ScrollBarStyle& s = style();
    const Rect area = contentRect();
    const bool rtl = rightToLeft();
    const Point going = this->going();
    if (showVertical_)
        verticalBar_.place({rtl ? area.x : area.right() - s.width, viewport_.y, s.width, viewport_.height}, contentSize_.height,
                            viewport_.height, offset_.y, going.y, rtl, s.minimumThumb);
    else
        verticalBar_.hide();
    if (showHorizontal_)
        horizontalBar_.place({viewport_.x, viewport_.bottom(), viewport_.width, s.width}, contentSize_.width, viewport_.width, offset_.x,
                              going.x, rtl, s.minimumThumb);
    else
        horizontalBar_.hide();
}

void ScrollArea::paintOver(PaintContext& context) {
    placeBars();
    const ScrollBarStyle& s = style();
    verticalBar_.paint(context, s);
    horizontalBar_.paint(context, s);
}

// ---- events ----

void ScrollArea::onMouse(MouseEvent& event) {
    placeBars();
    if (event.type == MouseEventType::Wheel) {
        // with Shift, or a wheel that turns across: across, toward the end side when it turns to the right in left to right
        const bool shift = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
        const float across = shift ? -event.wheelDelta.y : event.wheelDelta.x * (rightToLeft() ? -1.0f : 1.0f);
        const float down = shift ? 0.0f : -event.wheelDelta.y;
        const UiContext* ui = context();
        const Duration duration = ui ? Duration(ui->theme.motion.scroll) : Duration::zero();
        const Point limit = maximumOffset();
        bool moved = false;
        if (across != 0) {
            const WheelScroll w = wheelScroll(smoothX_, scheduler(), offset_.x, across * wheelStep, event.wheelPrecise, limit.x);
            if (w.moved)
                smoothX_.scrollTo(scheduler(), offset_.x, w.target, duration, w.aimed);
            moved = moved || w.moved;
        }
        if (down != 0) {
            const WheelScroll w = wheelScroll(smoothY_, scheduler(), offset_.y, down * wheelStep, event.wheelPrecise, limit.y);
            if (w.moved)
                smoothY_.scrollTo(scheduler(), offset_.y, w.target, duration, w.aimed);
            moved = moved || w.moved;
        }
        // what does not move this one may move a scroll area around it
        event.handled = moved;
        return;
    }
    // its bars first: pressed, dragged, let go
    if (verticalBar_.mouse(event) || horizontalBar_.mouse(event))
        event.handled = true;
}

void ScrollArea::onKey(KeyEvent& event) {
    // what the focused widget in it did not take
    if (!event.pressed)
        return;
    const auto page = static_cast<float>(ScrollBar::page);
    if (event.key == platform::Key::PageDown)
        scrollBy({0, viewport_.height * page});
    else if (event.key == platform::Key::PageUp)
        scrollBy({0, -viewport_.height * page});
    else
        return;
    event.handled = true;
}

void ScrollArea::onHoverChanged(bool hovered) {
    if (!hovered) {
        verticalBar_.leave();
        horizontalBar_.leave();
    }
}

} // namespace glw
