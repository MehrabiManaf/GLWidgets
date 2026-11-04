// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Bevel.hpp>

#include <GLWidgets/Paint/Painter.hpp>

#include <algorithm>

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

void paintScrollThumb(paint::Painter& p, const Rect& thumb, bool vertical, bool outerAtEnd, float wide, Color color) {
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
    p.fillRoundedRect(r, std::min(r.width, r.height) / 2, color);
}

} // namespace glw
