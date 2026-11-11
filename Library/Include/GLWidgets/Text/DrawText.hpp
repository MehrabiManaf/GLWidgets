// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Text through a painter: each glyph of a layout as a quad of the atlas, placed on the target's pixels (to a quarter of a
// pixel horizontally, whole pixels vertically) and rasterized at the size it has there; under a zoom (the transform's
// scale not 1), from its distance field when the atlas has fields (GlyphAtlas::fieldGlyph), sharp at any size without a
// bitmap for each.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <span>

namespace glw::paint {
class Painter;
} // namespace glw::paint

namespace glw::text {

class GlyphAtlas;
class TextLayout;

// A part of a text in a color of its own: [begin, end) in its UTF-8 bytes.
struct ColorSpan {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    Color color;
};

// Draws `layout` with its top left corner at `origin` (the painter's coordinates).
GLW_EXPORT void drawText(paint::Painter& painter, GlyphAtlas& atlas, const TextLayout& layout, Point origin, Color color);
// ... each glyph in the color of the span its character is in (sorted, not overlapping), `color` out of them: a text
// highlighted (code). Glyphs out of the painter's clip are passed over.
GLW_EXPORT void drawText(paint::Painter& painter, GlyphAtlas& atlas, const TextLayout& layout, Point origin, Color color,
                         std::span<const ColorSpan> spans);

} // namespace glw::text
