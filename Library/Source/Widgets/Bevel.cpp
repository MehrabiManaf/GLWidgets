// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Bevel.hpp>

#include <GLWidgets/Paint/Painter.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace glw {

Color lit(Color c, float amount) {
    const float to = amount >= 0 ? 1.0f : 0.0f;
    const float t = std::min(1.0f, std::abs(amount));
    return {c.r + (to - c.r) * t, c.g + (to - c.g) * t, c.b + (to - c.b) * t, c.a};
}

void paintRaised(paint::Painter& p, const Rect& rect, float radius, Color background, Color outline, const Bevel& bevel,
                 bool pressed) {
    if (rect.empty())
        return;
    if (bevel.relief > 0 && background.a > 0) {
        // a relief, lit: its shadow under it, a little down; pressed, it goes down (lower, no shadow)
        if (!pressed && bevel.shadow.a > 0 && background.a >= 1)
            p.drawShadow({rect.x, rect.y + bevel.relief * 0.5f, rect.width, rect.height}, radius, 1.5f + bevel.relief, bevel.shadow);
        p.fillRelief(rect, radius, pressed ? bevel.relief * 0.3f : bevel.relief, bevel.roundness, background);
        if (outline.a > 0)
            p.strokeRoundedRect(rect, radius, 1, outline);
        return;
    }
    const float g = bevel.gradient;
    const Rect inner = rect.inflated(-1);
    if (!pressed) {
        // the shadow, a little down: only the part under its bottom edge shows (a translucent control would show all of it)
        if (bevel.shadow.a > 0 && background.a >= 1)
            p.drawShadow({rect.x, rect.y + 1, rect.width, rect.height}, radius, 1.5f, bevel.shadow);
        p.fillRoundedRect(rect, radius, lit(background, g), lit(background, -g / 2));
        if (bevel.highlight.a > 0 && background.a > 0)
            p.strokeRoundedRect(inner, std::max(0.0f, radius - 1), 1, bevel.highlight, withAlpha(bevel.highlight, 0));
    } else {
        p.fillRoundedRect(rect, radius, lit(background, -g / 2), background);
        if (bevel.shadow.a > 0 && background.a > 0)
            p.strokeRoundedRect(inner, std::max(0.0f, radius - 1), 1, withAlpha(bevel.shadow, bevel.shadow.a * 0.6f),
                                withAlpha(bevel.shadow, 0));
    }
    if (outline.a > 0)
        p.strokeRoundedRect(rect, radius, 1, outline);
}

void paintSunken(paint::Painter& p, const Rect& rect, float radius, Color background, Color outline, const Bevel& bevel) {
    if (rect.empty())
        return;
    if (bevel.relief > 0) {
        // a relief sunk into what it is on, lit: its rim shadowing its floor away from the light
        p.fillRelief(rect, radius, -bevel.relief, bevel.roundness, background);
        if (outline.a > 0)
            p.strokeRoundedRect(rect, radius, 1, outline);
        return;
    }
    // the light line: what of a box a dip lower shows under it
    if (bevel.highlight.a > 0)
        p.fillRoundedRect({rect.x, rect.y + 1, rect.width, rect.height}, radius, bevel.highlight);
    p.fillRoundedRect(rect, radius, background);
    if (bevel.shadow.a > 0)
        p.strokeRoundedRect(rect.inflated(-1), std::max(0.0f, radius - 1), 1, bevel.shadow, withAlpha(bevel.shadow, 0));
    if (outline.a > 0)
        p.strokeRoundedRect(rect, radius, 1, outline);
}

void paintFocusGlow(paint::Painter& p, const Rect& rect, float radius, Color glow) {
    if (glow.a > 0)
        p.drawShadow(rect.inflated(1), radius + 1, 2.5f, glow);
}

Ripple::Ripple(std::function<void()> onStep) : progress_(std::move(onStep)) {
    progress_.set(1);
}

void Ripple::start(Scheduler* scheduler, Point at, Duration duration) {
    at_ = at;
    progress_.set(0);
    progress_.animateTo(scheduler, 1, duration, Easing::Linear);
}

void Ripple::paint(paint::Painter& p, const Rect& rect, float radius, Color color) const {
    if (!progress_.running() || color.a <= 0 || rect.empty())
        return;
    // out to the end furthest from where it was pressed, fast at first; full until halfway, then fading. As high as the
    // control and as round: inside it whatever its shape
    const float t = progress_.value();
    const float grown = 1 - (1 - t) * (1 - t) * (1 - t);
    const float x = std::clamp(at_.x, rect.x, rect.right());
    const float reach = std::max(x - rect.x, rect.right() - x) * grown + rect.height / 2;
    const float left = std::max(rect.x, x - reach), right = std::min(rect.right(), x + reach);
    const float alpha = color.a * std::min(1.0f, 2 * (1 - t));
    p.fillRoundedRect({left, rect.y, right - left, rect.height}, radius, withAlpha(color, alpha));
}

void paintScrollThumb(paint::Painter& p, const Rect& thumb, bool vertical, bool outerAtEnd, float wide, Color color, float radius) {
    constexpr float margin = 2;
    const float across = vertical ? thumb.width : thumb.height;
    const float full = std::max(1.0f, across - 2 * margin);
    const float thin = std::max(2.0f, full * 0.45f);
    const float thickness = thin + (full - thin) * std::clamp(wide, 0.0f, 1.0f);
    Rect r;
    if (vertical) {
        const float x = outerAtEnd ? thumb.right() - margin - thickness : thumb.x + margin;
        r = {x, thumb.y + margin, thickness, std::max(0.0f, thumb.height - 2 * margin)};
    } else {
        const float y = outerAtEnd ? thumb.bottom() - margin - thickness : thumb.y + margin;
        r = {thumb.x + margin, y, std::max(0.0f, thumb.width - 2 * margin), thickness};
    }
    p.fillRoundedRect(r, std::min(radius, std::min(r.width, r.height) / 2), color);
}

} // namespace glw
