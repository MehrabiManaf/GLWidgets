// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Paint/Renderer.hpp>

#include "Paint/PainterShaders.hpp"

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace glw::paint {

namespace {

constexpr rhi::VertexAttribute vertexAttributes[] = {
    {0, rhi::VertexFormat::Float2, offsetof(Vertex, position)},
    {1, rhi::VertexFormat::Float2, offsetof(Vertex, uv)},
    {2, rhi::VertexFormat::Float2, offsetof(Vertex, halfSize)},
    {3, rhi::VertexFormat::Float4, offsetof(Vertex, radius)},       // radius, border, kind, blur
    {4, rhi::VertexFormat::UByte4Norm, offsetof(Vertex, color)},
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
    commands.upload(*buffers.vertices_, 0, vertices, true);
    commands.upload(*buffers.indices_, 0, indices, true);
}

void Renderer::draw(rhi::CommandList& commands, const DrawList& list, const RenderBuffers& buffers, rhi::TextureFormat format,
                    PixelSize size, float scale) {
    if (list.empty() || size.empty())
        return;
    GLW_EXPECTS(buffers.vertices_ && buffers.indices_ && scale > 0);     // upload first

    commands.setPipeline(pipeline(format));
    commands.setViewport({0, 0, size.width, size.height});
    // dips to clip space (Painter.hlsl's Transform)
    const float transform[4] = {2.0f * scale / size.width, -2.0f * scale / size.height, -1.0f, 1.0f};
    commands.setVertexUniforms(0, std::as_bytes(std::span(transform)));
    // the light of reliefs (Painter.hlsl's Lighting)
    const float lighting[12] = {lighting_.x,       lighting_.y,        lighting_.z,         lighting_.ambient,
                                lighting_.diffuse, lighting_.specular, lighting_.shininess, lighting_.rimShadow,
                                lighting_.metal,   lighting_.brushed,  0,                   0};
    commands.setFragmentUniforms(0, std::as_bytes(std::span(lighting)));
    commands.setVertexBuffer(*buffers.vertices_);
    commands.setIndexBuffer(*buffers.indices_, rhi::IndexType::UInt32);

    const rhi::Texture* bound = nullptr;
    const rhi::Sampler* boundSampler = nullptr;
    for (const DrawCommand& cmd : list.commands) {
        if (cmd.indexCount == 0)
            continue;
        const PixelRect scissor = toPixels(cmd.clip, scale, size);
        if (scissor.empty())
            continue;
        commands.setScissor(scissor);
        const Bound tex = texture(cmd.texture);
        const rhi::Sampler* sampler = tex.filter == rhi::Filter::Nearest ? nearest_.get() : sampler_.get();
        if (tex.texture != bound || sampler != boundSampler) {
            commands.setTexture(0, *tex.texture, *sampler);
            bound = tex.texture;
            boundSampler = sampler;
        }
        commands.drawIndexed(cmd.indexCount, cmd.firstIndex, 0);
    }
}

const rhi::Pipeline& Renderer::pipeline(rhi::TextureFormat format) {
    std::unique_ptr<rhi::Pipeline>& pipeline = pipelines_[format];
    if (!pipeline) {
        rhi::PipelineDesc desc;
        desc.vertexShader = vertexShader_.get();
        desc.fragmentShader = fragmentShader_.get();
        desc.vertexStride = sizeof(Vertex);
        desc.vertexAttributes = vertexAttributes;
        desc.blend = rhi::BlendMode::PremultipliedAlpha;
        desc.colorFormat = format;
        pipeline = device_.createPipeline(desc);
    }
    return *pipeline;
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
