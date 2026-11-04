// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Icon.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Rhi/Device.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <thorvg.h>

namespace glw {

namespace {

// ThorVG started once, for the life of the program, on the calling thread (no threads of its own: icons are small).
void startThorVG() {
    struct Started {
        Started() { tvg::Initializer::init(0); }
        ~Started() { tvg::Initializer::term(); }
    };
    static Started started;
}

} // namespace

struct Icon::Data {
    std::uint64_t id = 0;               // what the atlas knows it by (an address may be another icon's later)
    tvg::Picture* svg = nullptr;        // parsed once, kept (a reference of its own) to be drawn at any size
    float width = 0, height = 0;        // its own size (the viewBox's)
    Painted painted;
    bool monochrome = false;

    ~Data() {
        if (svg)
            svg->unref();
    }
};

namespace {

std::uint64_t nextId() {
    static std::atomic<std::uint64_t> next{1};
    return next++;
}

} // namespace

Icon Icon::fromSvg(std::string_view svg, bool monochrome) {
    startThorVG();
    tvg::Picture* picture = tvg::Picture::gen();
    picture->ref();
    float width = 0, height = 0;
    if (picture->load(svg.data(), static_cast<std::uint32_t>(svg.size()), "svg", nullptr, true) != tvg::Result::Success) {
        picture->unref();
        throw Error("Icon: not an SVG document it can read");
    }
    picture->size(&width, &height);
    if (width <= 0 || height <= 0) {
        picture->unref();
        throw Error("Icon: an SVG document without a size (width, height or viewBox)");
    }
    auto data = std::make_shared<Data>();
    data->id = nextId();
    data->svg = picture;
    data->width = width;
    data->height = height;
    data->monochrome = monochrome;
    Icon icon;
    icon.data_ = std::move(data);
    return icon;
}

Icon Icon::fromSvgFile(const std::string& path, bool monochrome) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw Error("Icon: cannot read " + path);
    std::stringstream text;
    text << in.rdbuf();
    return fromSvg(text.str(), monochrome);
}

Icon Icon::fromPainter(Painted paint) {
    auto data = std::make_shared<Data>();
    data->id = nextId();
    data->painted = std::move(paint);
    data->monochrome = true;
    Icon icon;
    icon.data_ = std::move(data);
    return icon;
}

bool Icon::monochrome() const {
    return data_ && data_->monochrome;
}

void Icon::draw(PaintContext& context, const Rect& rect, Color color) const {
    if (!data_ || rect.empty())
        return;
    paint::Painter& p = context.painter;
    if (data_->painted) {
        data_->painted(p, rect, color);
        return;
    }
    IconAtlas* atlas = context.ui.icons;
    if (!atlas)
        return;
    // on the target's pixels: its size in pixels is what it is rasterized at
    const paint::Transform& t = p.transform();
    const float ppd = p.pixelsPerDip();
    const Rect target = t.map(rect);
    const int w = std::max(1, static_cast<int>(std::lround(target.width * ppd)));
    const int h = std::max(1, static_cast<int>(std::lround(target.height * ppd)));
    const float x = std::round(target.x * ppd), y = std::round(target.y * ppd);
    const PixelRect image = atlas->image(*this, w, h);
    if (image.empty())
        return;
    const PixelSize size = atlas->size();
    const Rect uv{static_cast<float>(image.x) / size.width, static_cast<float>(image.y) / size.height,
                  static_cast<float>(image.width) / size.width, static_cast<float>(image.height) / size.height};
    const Rect local = t.unmap(Rect{x / ppd, y / ppd, w / ppd, h / ppd});
    p.drawImage(local, atlas->texture(), data_->monochrome ? color : Color{1, 1, 1, color.a}, uv);
}

// ---- atlas ----

namespace {

struct Key {
    std::uint64_t icon;
    int width, height;
    bool operator==(const Key&) const = default;
};

struct KeyHash {
    std::size_t operator()(const Key& k) const {
        return std::hash<std::uint64_t>()(k.icon) ^ (static_cast<std::size_t>(k.width) << 20) ^ static_cast<std::size_t>(k.height);
    }
};

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

struct IconAtlas::Impl {
    paint::TextureId texture = 0;
    int size = 0;
    std::vector<std::byte> pixels;          // RGBA, premultiplied
    std::unordered_map<Key, PixelRect, KeyHash> images;
    std::vector<Shelf> shelves;
    int nextShelf = 0;
    PixelRect dirty;
    bool cleared = false;
    std::unique_ptr<rhi::Texture> gpu;
    std::vector<std::byte> staging;
    std::unique_ptr<tvg::SwCanvas> canvas;  // ThorVG's CPU rasterizer, aimed at `scratch` each time
    std::vector<std::uint32_t> scratch;

    // A place for a w x h image, a pixel apart from its neighbours; empty when there is no room left.
    PixelRect allocate(int w, int h) {
        const int pw = w + 1, ph = h + 1;
        for (Shelf& shelf : shelves) {
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
        images.clear();
        shelves.clear();
        nextShelf = 0;
        dirty = {0, 0, size, size};
        cleared = true;
    }
};

IconAtlas::IconAtlas(paint::TextureId texture, int size) : impl_(std::make_unique<Impl>()) {
    GLW_EXPECTS(size > 0);
    impl_->texture = texture;
    impl_->size = size;
    impl_->pixels.assign(static_cast<std::size_t>(size) * size * 4, std::byte{0});
    impl_->dirty = {0, 0, size, size};
}

IconAtlas::~IconAtlas() = default;

PixelRect IconAtlas::image(const Icon& icon, int width, int height) {
    Impl& d = *impl_;
    if (!icon.data_ || !icon.data_->svg || width <= 0 || height <= 0 || width > d.size || height > d.size)
        return {};
    const Key key{icon.data_->id, width, height};
    if (const auto it = d.images.find(key); it != d.images.end())
        return it->second;
    PixelRect rect = d.allocate(width, height);
    if (rect.empty()) {
        d.clear();
        rect = d.allocate(width, height);
        if (rect.empty())
            return {};
    }
    // rasterized to fit, in the middle: premultiplied RGBA in memory, as ThorVG's ABGR8888 is on a little-endian machine
    // (a monochrome one made coverage only: white, as much as it covers)
    if (!d.canvas)
        d.canvas.reset(tvg::SwCanvas::gen());
    const Icon::Data& data = *icon.data_;
    const float scale = std::min(width / data.width, height / data.height);
    d.scratch.assign(static_cast<std::size_t>(width) * height, 0);
    d.canvas->target(d.scratch.data(), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(width),
                     static_cast<std::uint32_t>(height), tvg::ColorSpace::ABGR8888);
    data.svg->size(data.width * scale, data.height * scale);
    data.svg->translate((width - data.width * scale) / 2, (height - data.height * scale) / 2);
    d.canvas->add(data.svg);
    d.canvas->draw(true);
    d.canvas->sync();
    d.canvas->remove(data.svg);         // the icon's reference keeps it
    const bool mono = data.monochrome;
    for (int row = 0; row < height; ++row) {
        const std::uint32_t* source = &d.scratch[static_cast<std::size_t>(row) * width];
        std::byte* target = &d.pixels[(static_cast<std::size_t>(rect.y + row) * d.size + rect.x) * 4];
        for (int x = 0; x < width; ++x) {
            const std::uint32_t p = source[x];
            const auto a = static_cast<std::byte>(p >> 24);
            target[x * 4 + 0] = mono ? a : static_cast<std::byte>(p & 0xff);
            target[x * 4 + 1] = mono ? a : static_cast<std::byte>((p >> 8) & 0xff);
            target[x * 4 + 2] = mono ? a : static_cast<std::byte>((p >> 16) & 0xff);
            target[x * 4 + 3] = a;
        }
    }
    d.dirty = united(d.dirty, rect);
    d.images.emplace(key, rect);
    return rect;
}

paint::TextureId IconAtlas::texture() const {
    return impl_->texture;
}

PixelSize IconAtlas::size() const {
    return {impl_->size, impl_->size};
}

bool IconAtlas::upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer) {
    Impl& d = *impl_;
    if (!d.gpu) {
        d.gpu = device.createTexture({{d.size, d.size}, rhi::TextureFormat::RGBA8, rhi::TextureUsage::Sampled});
        renderer.setTexture(d.texture, d.gpu.get());
        d.dirty = {0, 0, d.size, d.size};
    }
    if (!d.dirty.empty()) {
        const PixelRect& r = d.dirty;
        d.staging.resize(static_cast<std::size_t>(r.width) * r.height * 4);
        for (int row = 0; row < r.height; ++row) {
            std::memcpy(&d.staging[static_cast<std::size_t>(row) * r.width * 4],
                        &d.pixels[(static_cast<std::size_t>(r.y + row) * d.size + r.x) * 4], static_cast<std::size_t>(r.width) * 4);
        }
        commands.upload(*d.gpu, r, d.staging);
    }
    clearDirty();
    return std::exchange(d.cleared, false);
}

std::span<const std::byte> IconAtlas::pixels() const {
    return impl_->pixels;
}

PixelRect IconAtlas::dirty() const {
    return impl_->dirty;
}

void IconAtlas::clearDirty() {
    impl_->dirty = {};
}

} // namespace glw
