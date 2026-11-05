// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The look of the theme's controls, for widgets of one's own to look like them: a raised control (a button), a sunken one
// (a field), the glow of the focus. Each paints in `rect`, of the widget's coordinates, what its Bevel says.
//
//   paintFocusGlow(p, r, s.cornerRadius, focused ? s.focusRing : Color{});    // first: what is drawn next covers its inside
//   paintRaised(p, r, s.cornerRadius, s.background.resolve(state), s.border.resolve(state), s.bevel, state.pressed);
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Theme.hpp>

#include <functional>

namespace glw {

namespace paint {
class Painter;
}

// `c` mixed toward white by `amount` (toward black when it is negative), its alpha kept.
GLW_EXPORT Color lit(Color c, float amount);

// A control standing out of what it is on: its shadow (when its background is opaque), its background lighter at its top,
// the light line inside along its top, `outline` around it. Pressed, sunk: darker at its top, a shadow inside instead.
GLW_EXPORT void paintRaised(paint::Painter& painter, const Rect& rect, float radius, Color background, Color outline,
                            const Bevel& bevel, bool pressed = false);
// A control sunk into what it is on: the light line under its bottom edge, its background, a shadow inside along its top,
// `outline` around it.
GLW_EXPORT void paintSunken(paint::Painter& painter, const Rect& rect, float radius, Color background, Color outline,
                            const Bevel& bevel);
// The soft glow around a control that has the focus, `glow` its color (transparent: none). It is painted before the
// control, which covers its inside.
GLW_EXPORT void paintFocusGlow(paint::Painter& painter, const Rect& rect, float radius, Color glow);

// A scroll bar's thumb in `thumb` (its place along the bar, the bar's width across): rounded, thin by the bar's outer edge
// (the end side's when `outerAtEnd`: the right of a vertical bar in left to right, the bottom of a horizontal one) while
// `wide` is 0, as wide as the bar less a margin at 1 (the mouse over the bar, the thumb dragged), eased between.
// Its corners `radius` round, at most half as much as it is thick (a pill).
GLW_EXPORT void paintScrollThumb(paint::Painter& painter, const Rect& thumb, bool vertical, bool outerAtEnd, float wide, Color color,
                                 float radius = 0);

// A press's ripple (a style's that has one: ButtonStyle::ripple): out of where the control was pressed to its ends, as high
// as it is and as round (inside it, a pill's round ends too), fading as it goes. Started on a press, painted over the
// control's background, under what is on it.
class GLW_EXPORT Ripple {
public:
    explicit Ripple(std::function<void()> onStep);
    // Out of `at` (the control's coordinates), over `duration`: none without one, or with animations off.
    void start(Scheduler* scheduler, Point at, Duration duration);
    bool running() const { return progress_.running(); }
    // In `rect` of corners `radius` round, in `color` (transparent: none).
    void paint(paint::Painter& painter, const Rect& rect, float radius, Color color) const;

private:
    Animation progress_;
    Point at_;
};

} // namespace glw
