// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Text/GlyphAtlas.hpp>

#include "Text/DistanceField.hpp"
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
    std::uint32_t quarterPixels = 0;    // the size, in quarters of a pixel (a field's: 0)
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

// A row of a page, filled from the left with glyphs at most `height` high.
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

// A texture of the atlas and its pixels in memory: the glyphs' coverage (one byte a pixel), or their distance fields
// (four).
struct Page {
    paint::TextureId texture = 0;
    rhi::TextureFormat format = rhi::TextureFormat::R8;
    int size = 0;
    int bytes = 1;                      // a pixel's
    std::vector<std::byte> pixels;
    std::vector<Shelf> shelves;
    int nextShelf = 0;                  // the top of the space below the shelves
    PixelRect dirty;
    bool cleared = false;
    std::unique_ptr<rhi::Texture> gpu;
    std::vector<std::byte> staging;     // the dirty part, rows one after the other

    void make(paint::TextureId id, rhi::TextureFormat f, int s, int b) {
        texture = id;
        format = f;
        size = s;
        bytes = b;
        pixels.assign(static_cast<std::size_t>(s) * s * b, std::byte{0});
        dirty = {0, 0, s, s};
    }

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
        shelves.clear();
        nextShelf = 0;
        dirty = {0, 0, size, size};
        cleared = true;
    }

    std::byte* at(int x, int y) { return &pixels[(static_cast<std::size_t>(y) * size + x) * bytes]; }

    void upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer) {
        if (!gpu) {
            gpu = device.createTexture({{size, size}, format, rhi::TextureUsage::Sampled});
            renderer.setTexture(texture, gpu.get());
            dirty = {0, 0, size, size};
        }
        if (dirty.empty())
            return;
        const PixelRect& r = dirty;
        const auto row = static_cast<std::size_t>(r.width) * bytes;
        staging.resize(row * r.height);
        for (int y = 0; y < r.height; ++y)
            std::memcpy(&staging[y * row], at(r.x, r.y + y), row);
        commands.upload(*gpu, r, staging);
        dirty = {};
    }
};

// ---- a glyph's outline, to the contours of its field ----

// Its curves flattened into lines of about this long (field pixels): off by a hundredth of a pixel or so at most.
constexpr double flatStep = 0.75;

struct Outline {
    std::vector<detail::FieldContour>* contours = nullptr;
    double scale = 1;               // from the font's units to the field's pixels
    detail::FieldPoint position;

    detail::FieldPoint point(const FT_Vector* v) const { return {static_cast<double>(v->x) * scale, static_cast<double>(v->y) * scale}; }
    void add(detail::FieldPoint p, bool joint) {
        p.joint = joint;
        contours->back().push_back(p);
        position = p;
    }
};

double distance(detail::FieldPoint a, detail::FieldPoint b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

int moveTo(const FT_Vector* to, void* user) {
    auto& o = *static_cast<Outline*>(user);
    o.contours->emplace_back();
    o.add(o.point(to), true);
    return 0;
}

int lineTo(const FT_Vector* to, void* user) {
    auto& o = *static_cast<Outline*>(user);
    o.add(o.point(to), true);
    return 0;
}

int conicTo(const FT_Vector* control, const FT_Vector* to, void* user) {
    auto& o = *static_cast<Outline*>(user);
    const detail::FieldPoint p0 = o.position, c = o.point(control), p1 = o.point(to);
    const int n = std::clamp(static_cast<int>(std::ceil((distance(p0, c) + distance(c, p1)) / flatStep)), 1, 64);
    for (int i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / n, u = 1 - t;
        o.add({u * u * p0.x + 2 * u * t * c.x + t * t * p1.x, u * u * p0.y + 2 * u * t * c.y + t * t * p1.y}, i == n);
    }
    return 0;
}

int cubicTo(const FT_Vector* first, const FT_Vector* second, const FT_Vector* to, void* user) {
    auto& o = *static_cast<Outline*>(user);
    const detail::FieldPoint p0 = o.position, c0 = o.point(first), c1 = o.point(second), p1 = o.point(to);
    const int n = std::clamp(static_cast<int>(std::ceil((distance(p0, c0) + distance(c0, c1) + distance(c1, p1)) / flatStep)), 1, 96);
    for (int i = 1; i <= n; ++i) {
        const double t = static_cast<double>(i) / n, u = 1 - t;
        const double a = u * u * u, b = 3 * u * u * t, d = 3 * u * t * t, e = t * t * t;
        o.add({a * p0.x + b * c0.x + d * c1.x + e * p1.x, a * p0.y + b * c0.y + d * c1.y + e * p1.y}, i == n);
    }
    return 0;
}

} // namespace

struct GlyphAtlas::Impl {
    Page coverage;
    Page fields;                        // none (size 0) without a texture for it
    std::unordered_map<Key, Glyph, KeyHash> glyphs;
    std::unordered_map<Key, FieldGlyph, KeyHash> fieldGlyphs;
};

GlyphAtlas::GlyphAtlas(paint::TextureId texture, int size, paint::TextureId fieldTexture, int fieldSize) : impl_(std::make_unique<Impl>()) {
    GLW_EXPECTS(size > 0 && fieldSize > 0);
    impl_->coverage.make(texture, rhi::TextureFormat::R8, size, 1);
    if (fieldTexture != 0)
        impl_->fields.make(fieldTexture, rhi::TextureFormat::RGBA8, fieldSize, 4);
}

GlyphAtlas::~GlyphAtlas() = default;

const GlyphAtlas::Glyph& GlyphAtlas::glyph(const Font& font, std::uint32_t glyph, float pixelSize, int subpixel) {
    Impl& d = *impl_;
    Page& page = d.coverage;
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
                PixelRect rect = page.allocate(w, h);
                if (rect.empty()) {
                    page.clear();
                    d.glyphs.clear();
                    rect = page.allocate(w, h);
                }
                if (!rect.empty()) {
                    for (int row = 0; row < h; ++row) {
                        const unsigned char* source = bitmap.buffer + static_cast<std::ptrdiff_t>(row) * std::abs(bitmap.pitch);
                        std::memcpy(page.at(rect.x, rect.y + row), source, static_cast<std::size_t>(w));
                    }
                    page.dirty = united(page.dirty, rect);
                    result.rect = rect;
                }
            }
        }
    }
    return d.glyphs.emplace(key, result).first->second;
}

bool GlyphAtlas::hasFields() const {
    return impl_->fields.size > 0;
}

const GlyphAtlas::FieldGlyph& GlyphAtlas::fieldGlyph(const Font& font, std::uint32_t glyph) {
    Impl& d = *impl_;
    GLW_EXPECTS(hasFields());
    Page& page = d.fields;
    const Key key{&font, glyph, 0, 0};
    if (const auto it = d.fieldGlyphs.find(key); it != d.fieldGlyphs.end())
        return it->second;

    FieldGlyph result;
    const FT_Face face = font.impl().face;
    // its outline as it is in the font (unscaled, unhinted), to the field's pixels: fieldEm of them an em
    if (FT_Load_Glyph(face, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) == 0 &&
        face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        std::vector<detail::FieldContour> contours;
        Outline outline{&contours, static_cast<double>(fieldEm) / font.impl().unitsPerEm, {}};
        const FT_Outline_Funcs funcs{moveTo, lineTo, conicTo, cubicTo, 0, 0};
        FT_Outline_Decompose(&face->glyph->outline, &funcs, &outline);
        std::erase_if(contours, [](const detail::FieldContour& c) { return c.size() < 3; });
        if (!contours.empty()) {
            // whole pixels around it, and the field's range more on each side
            double left = 1e300, bottom = 1e300, right = -1e300, top = -1e300;
            for (const detail::FieldContour& c : contours)
                for (const detail::FieldPoint& q : c) {
                    left = std::min(left, q.x);
                    bottom = std::min(bottom, q.y);
                    right = std::max(right, q.x);
                    top = std::max(top, q.y);
                }
            left = std::floor(left);
            bottom = std::floor(bottom);
            const int w = static_cast<int>(std::ceil(right - left + 2 * fieldRange));
            const int h = static_cast<int>(std::ceil(top - bottom + 2 * fieldRange));
            if (w > 0 && h > 0 && w < page.size && h < page.size) {
                for (detail::FieldContour& c : contours)
                    for (detail::FieldPoint& q : c) {
                        q.x += fieldRange - left;
                        q.y += fieldRange - bottom;
                    }
                const std::vector<float> field = detail::multiChannelField(contours, w, h, fieldRange);
                PixelRect rect = page.allocate(w, h);
                if (rect.empty()) {
                    page.clear();
                    d.fieldGlyphs.clear();
                    rect = page.allocate(w, h);
                }
                if (!rect.empty()) {
                    // (the field's rows from the bottom up; the atlas's from the top down)
                    const auto byte = [](float v) { return static_cast<std::byte>(std::clamp(static_cast<int>(std::lround(v * 255.0f)), 0, 255)); };
                    for (int y = 0; y < h; ++y) {
                        std::byte* out = page.at(rect.x, rect.y + y);
                        const float* row = &field[static_cast<std::size_t>(h - 1 - y) * w * 3];
                        for (int x = 0; x < w; ++x) {
                            out[x * 4 + 0] = byte(row[x * 3 + 0]);
                            out[x * 4 + 1] = byte(row[x * 3 + 1]);
                            out[x * 4 + 2] = byte(row[x * 3 + 2]);
                            out[x * 4 + 3] = std::byte{255};
                        }
                    }
                    page.dirty = united(page.dirty, rect);
                    result.rect = rect;
                    result.left = static_cast<float>(left - fieldRange);
                    result.top = static_cast<float>(bottom - fieldRange) + static_cast<float>(h);
                }
            }
        }
    }
    return d.fieldGlyphs.emplace(key, result).first->second;
}

paint::TextureId GlyphAtlas::texture() const {
    return impl_->coverage.texture;
}

PixelSize GlyphAtlas::size() const {
    return {impl_->coverage.size, impl_->coverage.size};
}

paint::TextureId GlyphAtlas::fieldTexture() const {
    return impl_->fields.texture;
}

PixelSize GlyphAtlas::fieldAtlasSize() const {
    return {impl_->fields.size, impl_->fields.size};
}

bool GlyphAtlas::upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer) {
    Impl& d = *impl_;
    d.coverage.upload(device, commands, renderer);
    if (d.fields.size > 0)
        d.fields.upload(device, commands, renderer);
    const bool cleared = d.coverage.cleared || d.fields.cleared;
    d.coverage.cleared = d.fields.cleared = false;
    return cleared;
}

std::span<const std::byte> GlyphAtlas::pixels() const {
    return impl_->coverage.pixels;
}

PixelRect GlyphAtlas::dirty() const {
    return impl_->coverage.dirty;
}

std::span<const std::byte> GlyphAtlas::fieldPixels() const {
    return impl_->fields.pixels;
}

PixelRect GlyphAtlas::fieldDirty() const {
    return impl_->fields.dirty;
}

void GlyphAtlas::clearDirty() {
    impl_->coverage.dirty = {};
    impl_->fields.dirty = {};
}

} // namespace glw::text
