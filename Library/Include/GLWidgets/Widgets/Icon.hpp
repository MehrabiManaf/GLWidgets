// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Icons: from SVG (rasterized at the size in pixels they show at, once, into an atlas every window shares: sharp at any
// scale) or painted by a function (an arrow, a cross). A monochrome icon is its shapes' coverage, shown in the color it is
// given (the theme's text: it follows the theme); another keeps its colors, faded by the given color's alpha (disabled).
//
//   const glw::Icon save = glw::Icon::fromSvg(R"(<svg viewBox="0 0 16 16"><path d="..."/></svg>)", true);
//   save.draw(context, {x, y, 16, 16}, theme.palette.text);
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace glw::paint {
class Painter;
class Renderer;
} // namespace glw::paint

namespace glw::rhi {
class CommandList;
class Device;
} // namespace glw::rhi

namespace glw {

struct PaintContext;

class GLW_EXPORT Icon {
public:
    using Painted = std::function<void(paint::Painter& painter, const Rect& rect, Color color)>;

    Icon() = default;           // none: draws nothing
    // Throws Error when `svg` is not an SVG document it can read.
    static Icon fromSvg(std::string_view svg, bool monochrome = false);
    static Icon fromSvgFile(const std::string& path, bool monochrome = false);
    static Icon fromPainter(Painted paint);

    bool empty() const { return !data_; }
    bool monochrome() const;
    // Into `rect` (the painter's coordinates), on the target's pixels.
    void draw(PaintContext& context, const Rect& rect, Color color) const;

    friend bool operator==(const Icon& a, const Icon& b) { return a.data_ == b.data_; }

private:
    friend class IconAtlas;
    struct Data;
    std::shared_ptr<const Data> data_;
};

// The SVG icons rasterized, in one RGBA texture (premultiplied), shared by every window (UiContext::icons). When it is
// full it starts again empty: upload says so, and what was painted with it must be painted again.
class GLW_EXPORT IconAtlas {
public:
    explicit IconAtlas(paint::TextureId texture, int size = 1024);
    ~IconAtlas();
    IconAtlas(const IconAtlas&) = delete;
    IconAtlas& operator=(const IconAtlas&) = delete;

    // `icon` at `width` x `height` pixels: where it is in the atlas (rasterized the first time); empty when it cannot be.
    PixelRect image(const Icon& icon, int width, int height);
    paint::TextureId texture() const;
    PixelSize size() const;

    // GLWidgets' renderer: makes or updates the texture with what changed. True when the atlas started again since the last
    // upload: paint again.
    bool upload(rhi::Device& device, rhi::CommandList& commands, paint::Renderer& renderer);
    // Another renderer: the pixels (four bytes each, rows of size().width), the part changed since clearDirty.
    std::span<const std::byte> pixels() const;
    PixelRect dirty() const;
    void clearDirty();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw
