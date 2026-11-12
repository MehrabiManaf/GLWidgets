// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Paint/Renderer.hpp>

#include "Paint/PainterShaders.hpp"
#include "Paint/SceneShaders.hpp"

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

namespace glw::paint {

namespace {

constexpr rhi::VertexAttribute vertexAttributes[] = {
    {0, rhi::VertexFormat::Float2, offsetof(Vertex, position)},
    {1, rhi::VertexFormat::Float2, offsetof(Vertex, uv)},
    {2, rhi::VertexFormat::Float2, offsetof(Vertex, halfSize)},
    {3, rhi::VertexFormat::Float4, offsetof(Vertex, radius)},       // radius, border, kind, blur
    {4, rhi::VertexFormat::UByte4Norm, offsetof(Vertex, color)},
};

constexpr rhi::VertexAttribute sceneAttributes[] = {
    {0, rhi::VertexFormat::Float2, offsetof(SceneVertex, position)},
    {1, rhi::VertexFormat::Float4, offsetof(SceneVertex, center)},          // center, corner radius
    {2, rhi::VertexFormat::Float4, offsetof(SceneVertex, halfSize)},        // half size, bevel
    {3, rhi::VertexFormat::Float4, offsetof(SceneVertex, rotation)},
    {4, rhi::VertexFormat::Float4, offsetof(SceneVertex, material)},
    {5, rhi::VertexFormat::Float4, offsetof(SceneVertex, eye)},             // eye, pixels per dip
    {6, rhi::VertexFormat::Float4, offsetof(SceneVertex, decal)},
    {7, rhi::VertexFormat::Float4, offsetof(SceneVertex, decalUv)},
    {8, rhi::VertexFormat::UByte4Norm, offsetof(SceneVertex, color)},
};

// A buffer at least `needed` bytes big: `buffer` when it is, else a new one with room to grow.
void reserve(rhi::Device& device, std::unique_ptr<rhi::Buffer>& buffer, std::size_t needed, rhi::BufferUsage usage) {
    if (buffer && buffer->size() >= needed)
        return;
    const std::size_t size = std::max<std::size_t>({needed + needed / 2, buffer ? buffer->size() * 2 : 0, 64 * 1024});
    buffer = device.createBuffer({size, usage});
}

// `clip` (dips) in the pixels of a target `size` big, within it.
PixelRect toPixels(const Rect& clip, float scale, PixelSize size) {
    const int left = std::clamp(static_cast<int>(std::floor(clip.left() * scale)), 0, size.width);
    const int top = std::clamp(static_cast<int>(std::floor(clip.top() * scale)), 0, size.height);
    const int right = std::clamp(static_cast<int>(std::ceil(clip.right() * scale)), 0, size.width);
    const int bottom = std::clamp(static_cast<int>(std::ceil(clip.bottom() * scale)), 0, size.height);
    return {left, top, right - left, bottom - top};
}

} // namespace

RenderBuffers::RenderBuffers() = default;
RenderBuffers::~RenderBuffers() = default;
RenderBuffers::RenderBuffers(RenderBuffers&&) noexcept = default;
RenderBuffers& RenderBuffers::operator=(RenderBuffers&&) noexcept = default;

Renderer::Renderer(rhi::Device& device) : device_(device) {
    rhi::ShaderDesc vertex;
    vertex.stage = rhi::ShaderStage::Vertex;
    vertex.code = {shaders::painterVertexSpirv, shaders::painterVertexDxbc, shaders::painterVertexMsl};
    vertex.entryPoint = "vertexMain";
    vertex.uniformBuffers = 1;
    vertexShader_ = device_.createShader(vertex);

    rhi::ShaderDesc fragment;
    fragment.stage = rhi::ShaderStage::Fragment;
    fragment.code = {shaders::painterFragmentSpirv, shaders::painterFragmentDxbc, shaders::painterFragmentMsl};
    fragment.entryPoint = "fragmentMain";
    fragment.samplers = 1;
    fragment.uniformBuffers = 1;        // the light
    fragmentShader_ = device_.createShader(fragment);

    rhi::ShaderDesc sceneVertex;
    sceneVertex.stage = rhi::ShaderStage::Vertex;
    sceneVertex.code = {shaders::sceneVertexSpirv, shaders::sceneVertexDxbc, shaders::sceneVertexMsl};
    sceneVertex.entryPoint = "vertexMain";
    sceneVertex.uniformBuffers = 1;
    sceneVertexShader_ = device_.createShader(sceneVertex);
    rhi::ShaderDesc sceneFragment;
    sceneFragment.stage = rhi::ShaderStage::Fragment;
    sceneFragment.code = {shaders::sceneFragmentSpirv, shaders::sceneFragmentDxbc, shaders::sceneFragmentMsl};
    sceneFragment.entryPoint = "fragmentMain";
    sceneFragment.samplers = 1;          // a decal's texture
    sceneFragment.uniformBuffers = 1;    // the environment, and the light of reliefs
    sceneFragmentShader_ = device_.createShader(sceneFragment);

    sampler_ = device_.createSampler({rhi::Filter::Linear, rhi::AddressMode::ClampToEdge});
    nearest_ = device_.createSampler({rhi::Filter::Nearest, rhi::AddressMode::ClampToEdge});
    white_ = device_.createTexture({{1, 1}, rhi::TextureFormat::RGBA8, rhi::TextureUsage::Sampled});
}

Renderer::~Renderer() = default;

TextureId Renderer::addTexture(const rhi::Texture* texture, rhi::Filter filter) {
    const TextureId id = nextTexture_++;
    textures_.emplace(id, Bound{texture, filter});
    return id;
}

void Renderer::setTexture(TextureId id, const rhi::Texture* texture) {
    GLW_EXPECTS(textures_.contains(id));
    textures_[id].texture = texture;
}

void Renderer::removeTexture(TextureId id) {
    textures_.erase(id);
}

void Renderer::upload(rhi::CommandList& commands, const DrawList& list, RenderBuffers& buffers) {
    if (!whiteUploaded_) {
        const std::byte white[4] = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
        commands.upload(*white_, {0, 0, 1, 1}, white);
        whiteUploaded_ = true;
    }
    if (list.empty())
        return;
    const auto vertices = std::as_bytes(std::span(list.vertices));
    const auto indices = std::as_bytes(std::span(list.indices));
    reserve(device_, buffers.vertices_, vertices.size(), rhi::BufferUsage::Vertex);
    reserve(device_, buffers.indices_, indices.size(), rhi::BufferUsage::Index);
    // the whole content is new: the frames in flight keep reading the old one
    if (!vertices.empty())
        commands.upload(*buffers.vertices_, 0, vertices, true);
    commands.upload(*buffers.indices_, 0, indices, true);
    if (!list.sceneVertices.empty()) {
        const auto scene = std::as_bytes(std::span(list.sceneVertices));
        reserve(device_, buffers.sceneVertices_, scene.size(), rhi::BufferUsage::Vertex);
        commands.upload(*buffers.sceneVertices_, 0, scene, true);
    }
}

void Renderer::draw(rhi::CommandList& commands, const DrawList& list, const RenderBuffers& buffers, rhi::TextureFormat format,
                    PixelSize size, float scale, bool depth, std::size_t first, std::size_t end, const rhi::Texture* backdrop) {
    if (list.empty() || size.empty())
        return;
    GLW_EXPECTS(buffers.vertices_ && buffers.indices_ && scale > 0);     // upload first

    commands.setViewport({0, 0, size.width, size.height});
    // dips to clip space (Painter.hlsl's Transform, Scene.hlsl's)
    const float transform[4] = {2.0f * scale / size.width, -2.0f * scale / size.height, -1.0f, 1.0f};
    // the light of reliefs (Painter.hlsl's Lighting); of bricks (Scene.hlsl's Environment, then the light of reliefs)
    const float lighting[12] = {lighting_.x,       lighting_.y,        lighting_.z,         lighting_.ambient,
                                lighting_.diffuse, lighting_.specular, lighting_.shininess, lighting_.rimShadow,
                                lighting_.metal,   lighting_.brushed,  static_cast<float>(size.width), static_cast<float>(size.height)};
    const float environment[16] = {environment_.exposure, environment_.dark ? 1.0f : 0.0f, 0, 0,
                                   lighting_.x,           lighting_.y,                     lighting_.z, lighting_.ambient,
                                   lighting_.diffuse,     lighting_.specular,              lighting_.shininess, 0,
                                   lighting_.metal,       0,                               0, 0};
    commands.setIndexBuffer(*buffers.indices_, rhi::IndexType::UInt32);

    // each command with its pipeline (the painter's, or the scene's for bricks), what it reads bound again when it changes
    std::optional<bool> scene;
    const rhi::Texture* bound = nullptr;
    const rhi::Sampler* boundSampler = nullptr;
    end = std::min(end, list.commands.size());
    for (std::size_t c = first; c < end; ++c) {
        const DrawCommand& cmd = list.commands[c];
        if (cmd.indexCount == 0 || (cmd.scene && !buffers.sceneVertices_) || (!cmd.scene && list.vertices.empty()))
            continue;
        if (scene != cmd.scene) {
            scene = cmd.scene;
            commands.setPipeline(cmd.scene ? scenePipeline(format, depth) : pipeline(format, depth));
            commands.setVertexBuffer(cmd.scene ? *buffers.sceneVertices_ : *buffers.vertices_);
            commands.setVertexUniforms(0, std::as_bytes(std::span(transform)));
            if (cmd.scene)
                commands.setFragmentUniforms(0, std::as_bytes(std::span(environment)));
            else
                commands.setFragmentUniforms(0, std::as_bytes(std::span(lighting)));
            bound = nullptr;
            boundSampler = nullptr;
        }
        const PixelRect scissor = toPixels(cmd.clip, scale, size);
        if (scissor.empty())
            continue;
        commands.setScissor(scissor);
        const bool glass = cmd.texture == backdropTexture || cmd.texture == joinedBackdropTexture;
        const Bound tex = glass ? Bound{backdrop ? backdrop : white_.get(), rhi::Filter::Linear} : texture(cmd.texture);
        const rhi::Sampler* sampler = tex.filter == rhi::Filter::Nearest ? nearest_.get() : sampler_.get();
        if (tex.texture != bound || sampler != boundSampler) {
            commands.setTexture(0, *tex.texture, *sampler);
            bound = tex.texture;
            boundSampler = sampler;
        }
        commands.drawIndexed(cmd.indexCount, cmd.firstIndex, 0);
    }
}

const rhi::Pipeline& Renderer::pipeline(rhi::TextureFormat format, bool depth) {
    std::unique_ptr<rhi::Pipeline>& pipeline = pipelines_[static_cast<int>(format) * 2 + (depth ? 1 : 0)];
    if (!pipeline) {
        rhi::PipelineDesc desc;
        desc.vertexShader = vertexShader_.get();
        desc.fragmentShader = fragmentShader_.get();
        desc.vertexStride = sizeof(Vertex);
        desc.vertexAttributes = vertexAttributes;
        desc.blend = rhi::BlendMode::PremultipliedAlpha;
        desc.colorFormat = format;
        if (depth)
            desc.depthFormat = rhi::TextureFormat::Depth32Float;     // (drawn over what came before: not tested)
        pipeline = device_.createPipeline(desc);
    }
    return *pipeline;
}

const rhi::Pipeline& Renderer::scenePipeline(rhi::TextureFormat format, bool depth) {
    std::unique_ptr<rhi::Pipeline>& pipeline = scenePipelines_[static_cast<int>(format) * 2 + (depth ? 1 : 0)];
    if (!pipeline) {
        rhi::PipelineDesc desc;
        desc.vertexShader = sceneVertexShader_.get();
        desc.fragmentShader = sceneFragmentShader_.get();
        desc.vertexStride = sizeof(SceneVertex);
        desc.vertexAttributes = sceneAttributes;
        desc.blend = rhi::BlendMode::PremultipliedAlpha;
        desc.colorFormat = format;
        if (depth) {
            desc.depthFormat = rhi::TextureFormat::Depth32Float;
            desc.depthTest = desc.depthWrite = true;
        }
        pipeline = device_.createPipeline(desc);
    }
    return *pipeline;
}

std::vector<std::size_t> Renderer::backdropCaptures(const DrawList& list) {
    std::vector<std::size_t> captures;
    // what a command covers: its vertices' bounds, cut to its clip
    const auto bounds = [&](const DrawCommand& cmd) {
        float left = 1e30f, top = 1e30f, right = -1e30f, bottom = -1e30f;
        for (std::uint32_t i = cmd.firstIndex; i < cmd.firstIndex + cmd.indexCount; ++i) {
            const Point p = cmd.scene ? list.sceneVertices[list.indices[i]].position : list.vertices[list.indices[i]].position;
            left = std::min(left, p.x);
            top = std::min(top, p.y);
            right = std::max(right, p.x);
            bottom = std::max(bottom, p.y);
        }
        return Rect{left, top, right - left, bottom - top}.intersected(cmd.clip);
    };
    std::optional<Rect> drawn;          // since the last capture (none before the first)
    bool captured = false;
    for (std::size_t c = 0; c < list.commands.size(); ++c) {
        const DrawCommand& cmd = list.commands[c];
        if (cmd.indexCount == 0)
            continue;
        const Rect covers = bounds(cmd);
        // (glass joined onto the glass before it reads what that one read: none of its own, unless none was read yet)
        const bool joined = cmd.texture == joinedBackdropTexture;
        if ((cmd.texture == backdropTexture && (!captured || (drawn && drawn->intersects(covers)))) || (joined && !captured)) {
            captures.push_back(c);
            captured = true;
            drawn.reset();
        }
        if (!covers.empty())
            drawn = drawn ? drawn->united(covers) : covers;
    }
    return captures;
}

Renderer::Bound Renderer::texture(TextureId id) const {
    if (id != whiteTexture) {
        const auto it = textures_.find(id);
        if (it != textures_.end() && it->second.texture)
            return it->second;
    }
    return {white_.get(), rhi::Filter::Linear};
}

} // namespace glw::paint
