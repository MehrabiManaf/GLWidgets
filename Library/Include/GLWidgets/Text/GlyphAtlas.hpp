// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Glyphs rasterized (FreeType) into one texture, one channel of coverage, shared by all text of all windows. A glyph is
// kept per font, size in pixels (in quarters of a pixel) and horizontal position within a pixel (in quarters), so text is
// placed to a quarter of a pixel: the joined letters of Persian meet without gaps.
//
// Glyphs for text that zooms (a canvas's, scaled by a transform) are kept as distance fields instead, in a second texture
// of four channels (made when a texture id is given for it): each glyph once, at fieldEm pixels an em, its outline's
// multi-channel signed distance field (Text/DistanceField.hpp, its corners kept sharp), drawn at any size by
// Painter::drawFieldMask. Small text is sharper as hinted bitmaps; text at its size (scale 1) stays one.
//
// The atlas is in memory; what changed goes to the GPU with upload (GLWidgets' renderer) or, for an engine drawing draw lists
// itself, through pixels / dirty / clearDirty. When it is full it starts again empty: upload then says so, and what was
// painted with it in that frame must be painted again.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace glw::rhi {
class CommandList;
class Device;
} // namespace glw::rhi

namespace glw::paint {
class Renderer;
} // namespace glw::paint

namespace glw::text {

class Font;

class GLW_EXPORT GlyphAtlas {
public:
    // `texture`: the id draw lists use for it (Renderer::addTexture). `size`: its width and height in pixels.
    // `fieldTexture`, `fieldSize`: the distance fields' (none: no fields, zoomed text as bitmaps).
    explicit GlyphAtlas(paint::TextureId texture, int size = 2048, paint::TextureId fieldTexture = 0, int fieldSize = 1024);
    ~GlyphAtlas();
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    struct Glyph {
        PixelRect rect;             // in the atlas; empty for a glyph with nothing to draw (a space)
        int left = 0;               // from the pen position to the bitmap's left edge, pixels
        int top = 0;                // from the baseline up to the bitmap's top edge, pixels
    };
    // `glyph` of `font` at `pixelSize` pixels per em, its pen `subpixel` quarters of a pixel right of a pixel's edge (0-3):
    // rasterized the first time it is asked for.
    const Glyph& glyph(const Font& font, std::uint32_t glyph, float pixelSize, int subpixel);

    paint::TextureId texture() const;
    PixelSize size() const;

    // ---- distance fields ----

    static constexpr float fieldEm = 48;        // a field's pixels an em
    static constexpr float fieldRange = 6;      // ... how far its distances reach, in all (3 inside and 3 out)
    struct FieldGlyph {
        PixelRect rect;             // in the fields' texture; empty for a glyph with nothing to draw
        float left = 0;             // from the pen to its left edge, its pixels (fieldEm an em)
        float top = 0;              // from the baseline up to its top edge
    };
    bool hasFields() const;
    // `glyph` of `font` as a field: made the first time it is asked for.
    const FieldGlyph& fieldGlyph(const Font& font, std::uint32_t glyph);
    paint::TextureId fieldTexture() const;
    PixelSize fieldAtlasSize() const;

    // ---- to the GPU ----

    // GLWidgets' renderer: makes or updates the texture with what changed. Outside passes, before the passes that draw
    // with the atlas. True when the atlas was full and started again since the last upload: paint again.
    bool upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer);

    // Another renderer: the pixels (one byte of coverage each, rows of size().width), the part changed since clearDirty.
    std::span<const std::byte> pixels() const;
    PixelRect dirty() const;
    // ... the fields' (four bytes a pixel: red, green, blue, alpha).
    std::span<const std::byte> fieldPixels() const;
    PixelRect fieldDirty() const;
    void clearDirty();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw::text
