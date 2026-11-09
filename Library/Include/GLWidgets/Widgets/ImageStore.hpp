// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Images a widget makes itself, pixel by pixel (a heat map's cells, a spectrogram, a picture decoded), each a texture the
// painter draws with: what changed of it goes to the GPU once a frame, before the windows are drawn (the application's
// UiContext::images). A widget holds an Image, writes into its rows, says what it changed, and draws it with
// Painter::drawImage(rect, image.texture(), ...); the image goes when it does.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Rhi/Resources.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace glw {

namespace rhi {
class CommandList;
class Device;
} // namespace rhi

namespace paint {
class Renderer;
} // namespace paint

class GLW_EXPORT ImageStore {
    struct Entry;

public:
    // `newTexture`: an id the renderer draws with (the application's: Renderer::addTexture); none (no renderer, tests):
    // ids of its own.
    explicit ImageStore(std::function<paint::TextureId(rhi::Filter)> newTexture = {});
    ~ImageStore();
    ImageStore(const ImageStore&) = delete;
    ImageStore& operator=(const ImageStore&) = delete;

    // One of its images, held by what draws it: gone with it.
    class GLW_EXPORT Image {
    public:
        Image() = default;
        Image(Image&& other) noexcept;
        Image& operator=(Image&& other) noexcept;
        ~Image();

        explicit operator bool() const { return entry_ != nullptr; }
        paint::TextureId texture() const;
        PixelSize size() const;
        // Its size; all of it clear (transparent) when it changes.
        void resize(PixelSize size);
        // A row of its pixels to write into: premultiplied RGBA, its red in the lowest byte (pixel()). Then changed().
        std::span<std::uint32_t> row(int y);
        std::span<const std::uint32_t> row(int y) const;
        // What was written, to go to the GPU (united with what was before it this frame).
        void changed(PixelRect rect);
        void changed();

    private:
        friend class ImageStore;
        Image(ImageStore* store, Entry* entry) : store_(store), entry_(entry) {}
        void release();
        ImageStore* store_ = nullptr;
        Entry* entry_ = nullptr;
    };

    // A new image, empty; Nearest: each of its pixels a sharp square however large it is drawn.
    Image make(rhi::Filter filter = rhi::Filter::Linear);
    // A color as a pixel of an image.
    static std::uint32_t pixel(Color color);

    // Each image's new pixels to the GPU (its texture made again when its size changed), those released gone; once a
    // frame, outside passes.
    void upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer);
    // How many it has; how many bytes of pixels went to the GPU at the last upload.
    std::size_t count() const;
    std::size_t uploaded() const { return uploaded_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::size_t uploaded_ = 0;
};

} // namespace glw
