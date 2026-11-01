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
    const paint::Transform& transform = painter.transform();
    const float pixelsPerDip = painter.pixelsPerDip();
    const float pixelSize = layout.style().size * transform.scale * pixelsPerDip;
    if (pixelSize < 1 || color.a <= 0)
        return;
    const PixelSize atlasSize = atlas.size();
    for (const PositionedGlyph& g : layout.glyphs()) {
        // the pen on the target's pixels: to a quarter of a pixel across, on a whole pixel down (the baseline stays sharp)
        const Point pen = transform.map(origin + g.position) * pixelsPerDip;
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
        painter.drawMask(target, atlas.texture(), color, uv);
    }
}

} // namespace glw::text
