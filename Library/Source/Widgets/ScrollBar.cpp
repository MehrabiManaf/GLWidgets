// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ScrollBar.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <utility>

namespace glw {

ScrollBar::ScrollBar(Orientation orientation, std::function<void(double, bool)> scroll, std::function<void()> repaint)
    : orientation_(orientation), scroll_(std::move(scroll)), repaint_(std::move(repaint)), wide_(repaint_) {}

void ScrollBar::place(const Rect& track, double content, double view, double offset, double going, bool rightToLeft, float minimumThumb) {
    track_ = track;
    content_ = content;
    view_ = view;
    offset_ = offset;
    going_ = going;
    rightToLeft_ = rightToLeft;
    minimumThumb_ = minimumThumb;
    shown_ = content > view + 0.5 && !track.empty();
    if (!shown_) {
        grab_.reset();
        hovered_ = Part::None;
    }
}

float ScrollBar::thumbLength() const {
    const float length = orientation_ == Orientation::Vertical ? track_.height : track_.width;
    const double share = content_ > 0 ? view_ / content_ : 1.0;
    return std::min(length, std::max(minimumThumb_, static_cast<float>(length * share)));
}

float ScrollBar::thumbAt() const {
    const float length = orientation_ == Orientation::Vertical ? track_.height : track_.width;
    const double room = content_ - view_;
    return room > 0 ? static_cast<float>((length - thumbLength()) * std::clamp(offset_ / room, 0.0, 1.0)) : 0.0f;
}

Rect ScrollBar::thumb() const {
    if (!shown_)
        return {};
    const float length = thumbLength(), at = thumbAt();
    if (orientation_ == Orientation::Vertical)
        return {track_.x, track_.y + at, track_.width, length};
    return {rightToLeft_ ? track_.right() - at - length : track_.x + at, track_.y, length, track_.height};
}

float ScrollBar::along(Point p) const {
    if (orientation_ == Orientation::Vertical)
        return p.y - track_.y;
    return rightToLeft_ ? track_.right() - p.x : p.x - track_.x;
}

void ScrollBar::setHovered(Part part) {
    if (part != hovered_) {
        hovered_ = part;
        if (repaint_)
            repaint_();
    }
}

bool ScrollBar::mouse(const MouseEvent& event) {
    if (!shown_)
        return false;
    switch (event.type) {
    case MouseEventType::Press: {
        if (event.button != platform::MouseButton::Left || !track_.contains(event.position))
            return false;
        const float at = along(event.position), start = thumbAt();
        if (at >= start && at <= start + thumbLength()) {
            grab_ = at - start;
        } else {
            // a page toward where the track was pressed, from where the scroll was going
            const double to = going_ + (at < start ? -1.0 : 1.0) * page * view_;
            scroll_(std::clamp(to, 0.0, std::max(0.0, content_ - view_)), true);
            going_ = to;
        }
        if (repaint_)
            repaint_();
        return true;
    }
    case MouseEventType::Move: {
        if (grab_) {
            // the thumb follows the mouse; the offset, the thumb
            const float room = (orientation_ == Orientation::Vertical ? track_.height : track_.width) - thumbLength();
            const float at = std::clamp(along(event.position) - *grab_, 0.0f, std::max(room, 0.0f));
            scroll_(room > 0 ? at / room * std::max(0.0, content_ - view_) : 0.0, false);
            return true;
        }
        const Rect t = thumb();
        setHovered(!track_.contains(event.position) ? Part::None : t.contains(event.position) ? Part::Thumb : Part::Track);
        return false;
    }
    case MouseEventType::Release:
        if (!grab_)
            return false;
        grab_.reset();
        setHovered(track_.contains(event.position) ? Part::Track : Part::None);
        if (repaint_)
            repaint_();
        return true;
    case MouseEventType::Wheel:
        return false;
    }
    return false;
}

void ScrollBar::leave() {
    if (!grab_)
        setHovered(Part::None);
}

void ScrollBar::paint(PaintContext& context, const ScrollBarStyle& style, bool track) {
    if (!shown_)
        return;
    paint::Painter& p = context.painter;
    // thin until the mouse is over it or its thumb is dragged
    const bool over = hovered_ != Part::None || grab_;
    wide_.animateTo(context.ui.scheduler, over ? 1.0f : 0.0f, context.ui.theme.motion.fast);
    if (style.trackBevel.relief > 0) {
        // a groove the thumb runs in (an overlaid bar's: showing as it widens)
        const float shown = track ? 1.0f : wide_.value();
        if (shown > 0) {
            const Rect groove = track_.inflated(-1);
            paintSunken(p, groove, std::min(style.cornerRadius, std::min(groove.width, groove.height) / 2),
                        withAlpha(style.track, style.track.a * shown), {0, 0, 0, 0}, style.trackBevel);
        }
    } else if (track) {
        p.fillRect(track_, style.track);
    }
    const VisualState state{.hovered = hovered_ == Part::Thumb || grab_.has_value(), .pressed = grab_.has_value()};
    const bool vertical = orientation_ == Orientation::Vertical;
    paintScrollThumb(p, thumb(), vertical, !vertical || !rightToLeft_, wide_.value(), style.thumb.resolve(state), style.cornerRadius,
                     style.thumbBevel);
}

WheelScroll wheelScroll(SmoothScroll& smooth, Scheduler* scheduler, double shown, double step, bool precise, double limit) {
    const double before = smooth.target(shown);
    WheelScroll w;
    if (precise) {
        w.target = std::clamp(before + step, 0.0, std::max(0.0, limit));
    } else {
        w.target = smooth.aim(scheduler, shown, step, 0, std::max(0.0, limit));
        w.aimed = true;
    }
    w.moved = w.target != before;
    return w;
}

} // namespace glw
