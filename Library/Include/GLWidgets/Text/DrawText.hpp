// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Text through a painter: each glyph of a layout as a quad of the atlas, placed on the target's pixels (to a quarter of a
// pixel horizontally, whole pixels vertically) and rasterized at the size it has there, so text is sharp at any scale and
// zoom.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

namespace glw::paint {
class Painter;
} // namespace glw::paint

namespace glw::text {

class GlyphAtlas;
class TextLayout;

// Draws `layout` with its top left corner at `origin` (the painter's coordinates).
GLW_EXPORT void drawText(paint::Painter& painter, GlyphAtlas& atlas, const TextLayout& layout, Point origin, Color color);

} // namespace glw::text
