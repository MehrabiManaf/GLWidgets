// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ImageStore.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Rhi/Device.hpp>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace glw {

struct ImageStore::Entry {
    paint::TextureId texture = 0;
    PixelSize size;
    std::vector<std::uint32_t> pixels;
    PixelRect dirty;                            // what changed since the last upload
    bool remake = false;                        // its size changed: a texture again
    bool released = false;                      // its holder gone: its texture to go
    std::unique_ptr<rhi::Texture> gpu;
};

struct ImageStore::Impl {
    std::function<paint::TextureId(rhi::Filter)> newTexture;
    paint::TextureId next = paint::TextureId{1} << 48;     // its own ids, far from the renderer's
    std::vector<std::unique_ptr<Entry>> entries;
    std::vector<std::byte> staging;
};

namespace {

PixelRect united(PixelRect a, PixelRect b) {
    if (a.width <= 0 || a.height <= 0)
        return b;
    if (b.width <= 0 || b.height <= 0)
        return a;
    const int x = std::min(a.x, b.x), y = std::min(a.y, b.y);
    return {x, y, std::max(a.x + a.width, b.x + b.width) - x, std::max(a.y + a.height, b.y + b.height) - y};
}

} // namespace

ImageStore::ImageStore(std::function<paint::TextureId(rhi::Filter)> newTexture) : impl_(std::make_unique<Impl>()) {
    impl_->newTexture = std::move(newTexture);
}

ImageStore::~ImageStore() = default;

ImageStore::Image ImageStore::make(rhi::Filter filter) {
    auto entry = std::make_unique<Entry>();
    entry->texture = impl_->newTexture ? impl_->newTexture(filter) : impl_->next++;
    Entry* made = entry.get();
    impl_->entries.push_back(std::move(entry));
    return Image(this, made);
}

std::uint32_t ImageStore::pixel(Color c) {
    const auto byte = [](float v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    const float a = std::clamp(c.a, 0.0f, 1.0f);
    return byte(c.r * a) | byte(c.g * a) << 8 | byte(c.b * a) << 16 | byte(a) << 24;
}

std::size_t ImageStore::count() const {
    return static_cast<std::size_t>(
        std::count_if(impl_->entries.begin(), impl_->entries.end(), [](const auto& e) { return !e->released; }));
}

void ImageStore::upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer) {
    uploaded_ = 0;
    auto& entries = impl_->entries;
    for (auto it = entries.begin(); it != entries.end();) {
        Entry& e = **it;
        if (e.released) {
            // (the GPU lets its texture go once it is done with it)
            renderer.removeTexture(e.texture);
            it = entries.erase(it);
            continue;
        }
        if (e.remake || (!e.gpu && !e.size.empty())) {
            e.gpu = e.size.empty() ? nullptr : device.createTexture({e.size, rhi::TextureFormat::RGBA8, rhi::TextureUsage::Sampled});
            renderer.setTexture(e.texture, e.gpu.get());
            e.dirty = {0, 0, e.size.width, e.size.height};
            e.remake = false;
        }
        if (e.gpu && e.dirty.width > 0 && e.dirty.height > 0) {
            const PixelRect r = e.dirty;
            std::vector<std::byte>& staging = impl_->staging;
            staging.resize(static_cast<std::size_t>(r.width) * r.height * 4);
            for (int y = 0; y < r.height; ++y)
                std::memcpy(&staging[static_cast<std::size_t>(y) * r.width * 4],
                            &e.pixels[static_cast<std::size_t>(r.y + y) * e.size.width + r.x], static_cast<std::size_t>(r.width) * 4);
            commands.upload(*e.gpu, r, staging);
            uploaded_ += staging.size();
        }
        e.dirty = {};
        ++it;
    }
}

// ---- an image ----

ImageStore::Image::Image(Image&& other) noexcept
    : store_(std::exchange(other.store_, nullptr)), entry_(std::exchange(other.entry_, nullptr)) {}

ImageStore::Image& ImageStore::Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        release();
        store_ = std::exchange(other.store_, nullptr);
        entry_ = std::exchange(other.entry_, nullptr);
    }
    return *this;
}

ImageStore::Image::~Image() {
    release();
}

void ImageStore::Image::release() {
    if (entry_) {
        entry_->released = true;
        // without a renderer nothing uploads: gone now
        if (!store_->impl_->newTexture)
            std::erase_if(store_->impl_->entries, [&](const auto& e) { return e.get() == entry_; });
    }
    entry_ = nullptr;
    store_ = nullptr;
}

paint::TextureId ImageStore::Image::texture() const {
    return entry_ ? entry_->texture : paint::whiteTexture;
}

PixelSize ImageStore::Image::size() const {
    return entry_ ? entry_->size : PixelSize{};
}

void ImageStore::Image::resize(PixelSize size) {
    GLW_EXPECTS(entry_);
    size = {std::max(0, size.width), std::max(0, size.height)};
    if (size == entry_->size)
        return;
    entry_->size = size;
    entry_->pixels.assign(static_cast<std::size_t>(size.width) * size.height, 0);
    entry_->remake = true;
}

std::span<std::uint32_t> ImageStore::Image::row(int y) {
    GLW_EXPECTS(entry_ && y >= 0 && y < entry_->size.height);
    return {entry_->pixels.data() + static_cast<std::size_t>(y) * entry_->size.width, static_cast<std::size_t>(entry_->size.width)};
}

std::span<const std::uint32_t> ImageStore::Image::row(int y) const {
    GLW_EXPECTS(entry_ && y >= 0 && y < entry_->size.height);
    return {entry_->pixels.data() + static_cast<std::size_t>(y) * entry_->size.width, static_cast<std::size_t>(entry_->size.width)};
}

void ImageStore::Image::changed(PixelRect rect) {
    if (!entry_)
        return;
    const int x = std::clamp(rect.x, 0, entry_->size.width), y = std::clamp(rect.y, 0, entry_->size.height);
    const int right = std::clamp(rect.x + rect.width, x, entry_->size.width), bottom = std::clamp(rect.y + rect.height, y, entry_->size.height);
    entry_->dirty = united(entry_->dirty, {x, y, right - x, bottom - y});
}

void ImageStore::Image::changed() {
    if (entry_)
        changed({0, 0, entry_->size.width, entry_->size.height});
}

} // namespace glw
