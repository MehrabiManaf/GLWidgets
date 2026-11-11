// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Text/DrawText.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Text/TextLayout.hpp>

#include <algorithm>
#include <cmath>

namespace glw::text {

void drawText(paint::Painter& painter, GlyphAtlas& atlas, const TextLayout& layout, Point origin, Color color) {
    drawText(painter, atlas, layout, origin, color, {});
}

void drawText(paint::Painter& painter, GlyphAtlas& atlas, const TextLayout& layout, Point origin, Color color,
              std::span<const ColorSpan> spans) {
    const paint::Transform& transform = painter.transform();
    const float pixelsPerDip = painter.pixelsPerDip();
    const float pixelSize = layout.style().size * transform.scale * pixelsPerDip;
    if (pixelSize < 1 || color.a <= 0)
        return;
    const PixelSize atlasSize = atlas.size();
    // zoomed (a canvas's text): distance fields, made once and sharp at any scale, instead of a bitmap for each size
    const bool fields = atlas.hasFields() && std::abs(transform.scale - 1.0f) > 1e-3f && pixelSize >= 5;
    const PixelSize fieldSize = fields ? atlas.fieldAtlasSize() : PixelSize{1, 1};
    const float fieldScale = pixelSize / GlyphAtlas::fieldEm;       // the field's pixels to the target's
    const Point fieldRange{GlyphAtlas::fieldRange / static_cast<float>(fieldSize.width),
                           GlyphAtlas::fieldRange / static_cast<float>(fieldSize.height)};
    // the clip on the target, in pixels: a long line's glyphs out of it are not even looked up
    const Rect clip = painter.clip();
    const float clipLeft = clip.x * pixelsPerDip, clipRight = clip.right() * pixelsPerDip;
    const float reach = pixelSize * 2;                  // what a glyph may cover past its pen
    for (const PositionedGlyph& g : layout.glyphs()) {
        Color shown = color;
        if (!spans.empty()) {
            // the last span starting at or before its character
            const auto it = std::upper_bound(spans.begin(), spans.end(), g.cluster, [](std::uint32_t c, const ColorSpan& s) { return c < s.begin; });
            if (it != spans.begin() && g.cluster < (it - 1)->end)
                shown = (it - 1)->color;
        }
        // the pen on the target's pixels: to a quarter of a pixel across, on a whole pixel down (the baseline stays sharp)
        const Point pen = transform.map(origin + g.position) * pixelsPerDip;
        if (pen.x + reach < clipLeft || pen.x - reach > clipRight)
            continue;
        if (fields) {
            // where it is, not moved to the pixels: a field is as sharp anywhere
            const GlyphAtlas::FieldGlyph& field = atlas.fieldGlyph(*g.font, g.glyph);
            if (field.rect.empty())
                continue;
            const Rect target{(pen.x + field.left * fieldScale) / pixelsPerDip, (pen.y - field.top * fieldScale) / pixelsPerDip,
                              field.rect.width * fieldScale / pixelsPerDip, field.rect.height * fieldScale / pixelsPerDip};
            const Rect uv{static_cast<float>(field.rect.x) / fieldSize.width, static_cast<float>(field.rect.y) / fieldSize.height,
                          static_cast<float>(field.rect.width) / fieldSize.width, static_cast<float>(field.rect.height) / fieldSize.height};
            painter.drawFieldMask(target, atlas.fieldTexture(), shown, uv, fieldRange);
            continue;
        }
        const float column = std::floor(pen.x);
        const int subpixel = std::min(3, static_cast<int>((pen.x - column) * 4));
        const GlyphAtlas::Glyph& image = atlas.glyph(*g.font, g.glyph, pixelSize, subpixel);
        if (image.rect.empty())
            continue;
        const float x = column + static_cast<float>(image.left);
        const float y = std::round(pen.y) - static_cast<float>(image.top);
        const Rect target{x / pixelsPerDip, y / pixelsPerDip, image.rect.width / pixelsPerDip, image.rect.height / pixelsPerDip};
        const Rect uv{static_cast<float>(image.rect.x) / atlasSize.width, static_cast<float>(image.rect.y) / atlasSize.height,
                      static_cast<float>(image.rect.width) / atlasSize.width,
                      static_cast<float>(image.rect.height) / atlasSize.height};
        painter.drawMask(target, atlas.texture(), shown, uv);
    }
}

} // namespace glw::text
