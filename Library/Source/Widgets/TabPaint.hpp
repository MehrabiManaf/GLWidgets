// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How a stack of tabs over its content is drawn, in the theme's DockStyle: a dock's stacks (Docking.cpp) and a TabView's.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Widgets/Theme.hpp>

#include <optional>

namespace glw {

struct PaintContext;

namespace tabs {

constexpr float selectedTabTop = 3;     // dips between the bar's top and the top of the tab shown, among others

// A tab's face, `size` big at its own 0, 0: shown (`active`) or not, under the mouse or pressed. A raised stack's tab shown
// (islandBevel) is drawn by its stack (paintStack): nothing here.
void paintTab(PaintContext& context, const DockStyle& s, Size size, bool active, const VisualState& state);
// A tab's × in `area`, lit under the mouse.
void paintClose(PaintContext& context, const DockStyle& s, const Rect& area, bool hovered);
// The stack under its tabs, `size` big, its bar `bar` high: its background, its edge, the line under the bar; a raised
// stack's slab, and its tab shown (`activeTab`, in the stack's coordinates, cut to `row`, where the tabs show) joined to it.
void paintStack(PaintContext& context, const DockStyle& s, Size size, float bar, std::optional<Rect> activeTab, const Rect& row);

} // namespace tabs
} // namespace glw
