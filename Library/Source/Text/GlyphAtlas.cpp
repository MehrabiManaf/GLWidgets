// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Text/GlyphAtlas.hpp>

#include "Text/FontImpl.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Rhi/Device.hpp>

#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace glw::text {

namespace {

struct Key {
    const Font* font = nullptr;
    std::uint32_t glyph = 0;
    std::uint32_t quarterPixels = 0;    // the size, in quarters of a pixel
    std::uint32_t subpixel = 0;

    friend bool operator==(const Key&, const Key&) = default;
};

struct KeyHash {
    std::size_t operator()(const Key& k) const {
        std::size_t h = std::hash<const void*>()(k.font);
        h = h * 31 + k.glyph;
        h = h * 31 + k.quarterPixels;
        return h * 4 + k.subpixel;
    }
};

// A row of the atlas, filled from the left with glyphs at most `height` high.
struct Shelf {
    int y = 0;
    int height = 0;
    int x = 0;
};

PixelRect united(const PixelRect& a, const PixelRect& b) {
    if (a.empty())
        return b;
    const int left = std::min(a.x, b.x), top = std::min(a.y, b.y);
    const int right = std::max(a.x + a.width, b.x + b.width), bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

} // namespace

struct GlyphAtlas::Impl {
    paint::TextureId texture = 0;
    int size = 0;
    std::vector<std::byte> pixels;
    std::unordered_map<Key, Glyph, KeyHash> glyphs;
    std::vector<Shelf> shelves;
    int nextShelf = 0;                  // the top of the space below the shelves
    PixelRect dirty;
    bool cleared = false;
    std::unique_ptr<rhi::Texture> gpu;
    std::vector<std::byte> staging;     // the dirty part, rows one after the other

    // A place for a w x h bitmap (with a pixel of space right and below it, so that filtering does not take in its
    // neighbours), or an empty rect when there is no room left.
    PixelRect allocate(int w, int h) {
        const int pw = w + 1, ph = h + 1;
        for (Shelf& shelf : shelves) {
            // a shelf not much taller than the glyph, so that small glyphs do not waste tall rows
            if (shelf.height >= ph && shelf.height <= ph + ph / 2 + 2 && shelf.x + pw <= size) {
                const PixelRect r{shelf.x, shelf.y, w, h};
                shelf.x += pw;
                return r;
            }
        }
        if (nextShelf + ph > size || pw > size)
            return {};
        shelves.push_back(Shelf{nextShelf, ph, pw});
        const PixelRect r{0, nextShelf, w, h};
        nextShelf += ph;
        return r;
    }

    void clear() {
        std::fill(pixels.begin(), pixels.end(), std::byte{0});
        glyphs.clear();
        shelves.clear();
        nextShelf = 0;
        dirty = {0, 0, size, size};
        cleared = true;
    }
};

GlyphAtlas::GlyphAtlas(paint::TextureId texture, int size) : impl_(std::make_unique<Impl>()) {
    GLW_EXPECTS(size > 0);
    impl_->texture = texture;
    impl_->size = size;
    impl_->pixels.assign(static_cast<std::size_t>(size) * size, std::byte{0});
    impl_->dirty = {0, 0, size, size};
}

GlyphAtlas::~GlyphAtlas() = default;

const GlyphAtlas::Glyph& GlyphAtlas::glyph(const Font& font, std::uint32_t glyph, float pixelSize, int subpixel) {
    Impl& d = *impl_;
    const Key key{&font, glyph, static_cast<std::uint32_t>(std::max(1L, std::lround(pixelSize * 4))),
                  static_cast<std::uint32_t>(std::clamp(subpixel, 0, 3))};
    if (const auto it = d.glyphs.find(key); it != d.glyphs.end())
        return it->second;

    Glyph result;
    const FT_Face face = font.impl().face;
    // 26.6 fixed point at 72 dots per inch: a point is a pixel, and a quarter of a pixel is 16
    FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(key.quarterPixels) * 16, 72, 72);
    // light hinting: vertical only, so the horizontal placement stays the shaper's
    if (FT_Load_Glyph(face, glyph, FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP) == 0) {
        if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
            FT_Outline_Translate(&face->glyph->outline, static_cast<FT_Pos>(key.subpixel) * 16, 0);
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LIGHT) == 0) {
            const FT_Bitmap& bitmap = face->glyph->bitmap;
            result.left = face->glyph->bitmap_left;
            result.top = face->glyph->bitmap_top;
            const int w = static_cast<int>(bitmap.width), h = static_cast<int>(bitmap.rows);
            if (w > 0 && h > 0) {
                PixelRect rect = d.allocate(w, h);
                if (rect.empty()) {
                    d.clear();
                    rect = d.allocate(w, h);
                }
                if (!rect.empty()) {
                    for (int row = 0; row < h; ++row) {
                        const unsigned char* source = bitmap.buffer + static_cast<std::ptrdiff_t>(row) * std::abs(bitmap.pitch);
                        std::memcpy(&d.pixels[static_cast<std::size_t>(rect.y + row) * d.size + rect.x], source,
                                    static_cast<std::size_t>(w));
                    }
                    d.dirty = united(d.dirty, rect);
                    result.rect = rect;
                }
            }
        }
    }
    return d.glyphs.emplace(key, result).first->second;
}

paint::TextureId GlyphAtlas::texture() const {
    return impl_->texture;
}

PixelSize GlyphAtlas::size() const {
    return {impl_->size, impl_->size};
}

bool GlyphAtlas::upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer) {
    Impl& d = *impl_;
    if (!d.gpu) {
        d.gpu = device.createTexture({{d.size, d.size}, rhi::TextureFormat::R8, rhi::TextureUsage::Sampled});
        renderer.setTexture(d.texture, d.gpu.get());
        d.dirty = {0, 0, d.size, d.size};
    }
    if (!d.dirty.empty()) {
        const PixelRect& r = d.dirty;
        d.staging.resize(static_cast<std::size_t>(r.width) * r.height);
        for (int row = 0; row < r.height; ++row) {
            std::memcpy(&d.staging[static_cast<std::size_t>(row) * r.width],
                        &d.pixels[static_cast<std::size_t>(r.y + row) * d.size + r.x], static_cast<std::size_t>(r.width));
        }
        commands.upload(*d.gpu, r, d.staging);
    }
    clearDirty();
    return std::exchange(d.cleared, false);
}

std::span<const std::byte> GlyphAtlas::pixels() const {
    return impl_->pixels;
}

PixelRect GlyphAtlas::dirty() const {
    return impl_->dirty;
}

void GlyphAtlas::clearDirty() {
    impl_->dirty = {};
}

} // namespace glw::text
