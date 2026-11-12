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

#include <cstddef>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

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
    std::unique_ptr<rhi::Buffer> sceneVertices_;
};

// The environment bricks are lit by (Scene.hlsl's): how bright, and whether its sky and floor are of a dark theme.
struct Environment {
    float exposure = 1;
    bool dark = true;
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
    // draws as white. `filter`: how it is sampled between its pixels (Nearest: each pixel a sharp square however large it is
    // drawn: an image of cells, a heat map's).
    TextureId addTexture(const rhi::Texture* texture = nullptr, rhi::Filter filter = rhi::Filter::Linear);
    void setTexture(TextureId id, const rhi::Texture* texture);
    void removeTexture(TextureId id);

    // The light reliefs are lit by (Painter::fillRelief), from the next draw on.
    void setLighting(const Lighting& lighting) { lighting_ = lighting; }
    const Lighting& lighting() const { return lighting_; }
    // ... and bricks (Painter::drawBrick).
    void setEnvironment(const Environment& environment) { environment_ = environment; }
    const Environment& environment() const { return environment_; }

    // Outside passes, before the pass that draws `list`.
    void upload(rhi::CommandList& commands, const DrawList& list, RenderBuffers& buffers);
    // Inside the pass of a target of `format`, `size` pixels, with `scale` pixels per dip. `depth`: the pass has a depth
    // buffer (beginSurfacePass with depth): bricks are tested against it and written to it, so that those that pass through
    // one another meet where they do; the rest is drawn over what came before, as without it. A list that tests its
    // bricks by depth (DrawList::depth) wants one.
    // The commands from `first` to before `end`; glass reads `backdrop` (the target as it was, blurred) where it is.
    void draw(rhi::CommandList& commands, const DrawList& list, const RenderBuffers& buffers, rhi::TextureFormat format,
              PixelSize size, float scale, bool depth = false, std::size_t first = 0,
              std::size_t end = std::numeric_limits<std::size_t>::max(), const rhi::Texture* backdrop = nullptr);

    // Where the glass of `list` (its commands that read backdropTexture) wants what was drawn before it captured and
    // blurred: before the first of them, and before each that something drawn since the last capture is under. The
    // indices of those commands, in order; none without glass. A target with glass is drawn into a texture of its own,
    // the passes cut there (the target copied and blurred between them: Application does it).
    static std::vector<std::size_t> backdropCaptures(const DrawList& list);

private:
    const rhi::Pipeline& pipeline(rhi::TextureFormat format, bool depth);
    const rhi::Pipeline& scenePipeline(rhi::TextureFormat format, bool depth);
    struct Bound {
        const rhi::Texture* texture = nullptr;
        rhi::Filter filter = rhi::Filter::Linear;
    };
    Bound texture(TextureId id) const;

    rhi::Device& device_;
    std::unique_ptr<rhi::Shader> vertexShader_;
    std::unique_ptr<rhi::Shader> fragmentShader_;
    std::unique_ptr<rhi::Shader> sceneVertexShader_;
    std::unique_ptr<rhi::Shader> sceneFragmentShader_;
    std::unique_ptr<rhi::Sampler> sampler_;
    std::unique_ptr<rhi::Sampler> nearest_;
    std::unique_ptr<rhi::Texture> white_;
    bool whiteUploaded_ = false;
    // by format, and whether the pass has a depth buffer (the format's value times two, plus one when it does)
    std::unordered_map<int, std::unique_ptr<rhi::Pipeline>> pipelines_;
    std::unordered_map<int, std::unique_ptr<rhi::Pipeline>> scenePipelines_;
    std::unordered_map<TextureId, Bound> textures_;
    TextureId nextTexture_ = 1;
    Lighting lighting_;
    Environment environment_;
};

} // namespace glw::paint
