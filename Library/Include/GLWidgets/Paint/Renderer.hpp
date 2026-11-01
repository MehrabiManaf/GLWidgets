// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Draws draw lists with the RHI. For each target drawn in a frame (a window, a texture): upload its list into its
// RenderBuffers before the target's pass, then draw it inside the pass.
//
//   renderer.upload(*commands, list, buffers);
//   if (auto size = commands->beginSurfacePass(surface, clear)) {
//       renderer.draw(*commands, list, buffers, surface.format(), *size, window.scale());
//       commands->endPass();
//   }
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Rhi/Device.hpp>

#include <memory>
#include <unordered_map>

namespace glw::paint {

// The GPU buffers of one target's draw list. They grow as the lists do and are reused from frame to frame.
class GLW_EXPORT RenderBuffers {
public:
    RenderBuffers();
    ~RenderBuffers();
    RenderBuffers(RenderBuffers&&) noexcept;
    RenderBuffers& operator=(RenderBuffers&&) noexcept;

private:
    friend class Renderer;
    std::unique_ptr<rhi::Buffer> vertices_;
    std::unique_ptr<rhi::Buffer> indices_;
};

class GLW_EXPORT Renderer {
public:
    // Throws Error when the device's GPU API has no code of the painter's shader (Tools/CompileShaders.py).
    explicit Renderer(rhi::Device& device);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // An id for draw lists to refer to `texture`, which must live while the id is used (not owned). None yet: lists may use
    // the id before setTexture gives it its texture (the glyph atlas is painted with before it is uploaded); until then it
    // draws as white.
    TextureId addTexture(const rhi::Texture* texture = nullptr);
    void setTexture(TextureId id, const rhi::Texture* texture);
    void removeTexture(TextureId id);

    // Outside passes, before the pass that draws `list`.
    void upload(rhi::CommandList& commands, const DrawList& list, RenderBuffers& buffers);
    // Inside the pass of a target of `format`, `size` pixels, with `scale` pixels per dip.
    void draw(rhi::CommandList& commands, const DrawList& list, const RenderBuffers& buffers, rhi::TextureFormat format,
              PixelSize size, float scale);

private:
    const rhi::Pipeline& pipeline(rhi::TextureFormat format);
    const rhi::Texture& texture(TextureId id) const;

    rhi::Device& device_;
    std::unique_ptr<rhi::Shader> vertexShader_;
    std::unique_ptr<rhi::Shader> fragmentShader_;
    std::unique_ptr<rhi::Sampler> sampler_;
    std::unique_ptr<rhi::Texture> white_;
    bool whiteUploaded_ = false;
    std::unordered_map<rhi::TextureFormat, std::unique_ptr<rhi::Pipeline>> pipelines_;
    std::unordered_map<TextureId, const rhi::Texture*> textures_;
    TextureId nextTexture_ = 1;
};

} // namespace glw::paint
