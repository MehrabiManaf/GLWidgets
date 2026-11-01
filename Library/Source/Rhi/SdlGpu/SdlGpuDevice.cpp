// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Rhi/SdlGpu/SdlGpuDevice.hpp"

#include "Platform/Sdl/SdlPlatform.hpp"
#include "Platform/Sdl/SdlWindow.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>

#include <SDL3/SDL_error.h>

#include <cstring>
#include <string>
#include <vector>

namespace glw::rhi {

std::unique_ptr<Device> createDevice(platform::Platform& platform, const DeviceDesc& desc) {
    GLW_EXPECTS(dynamic_cast<platform::sdl::SdlPlatform*>(&platform) != nullptr);
    return std::make_unique<sdlgpu::SdlGpuDevice>(desc);
}

namespace sdlgpu {

namespace {

// The formats GLWidgets ships its shaders in (Tools/CompileShaders.py): SDL chooses among the GPU APIs of the system that
// take one of them - Direct3D 12 (DXBC), Vulkan (SPIR-V), Metal (MSL).
constexpr SDL_GPUShaderFormat shaderFormats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_MSL;

std::string sdlError(const char* what) {
    return std::string(what) + ": " + SDL_GetError();
}

SDL_GPUTextureFormat toSdl(TextureFormat format) {
    switch (format) {
    case TextureFormat::Unknown: return SDL_GPU_TEXTUREFORMAT_INVALID;
    case TextureFormat::RGBA8: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case TextureFormat::BGRA8: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case TextureFormat::RGBA8Srgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::BGRA8Srgb: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::R8: return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    case TextureFormat::RGBA16Float: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::RGB10A2: return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

TextureFormat fromSdl(SDL_GPUTextureFormat format) {
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBA8;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM: return TextureFormat::BGRA8;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB: return TextureFormat::RGBA8Srgb;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB: return TextureFormat::BGRA8Srgb;
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM: return TextureFormat::R8;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT: return TextureFormat::RGBA16Float;
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM: return TextureFormat::RGB10A2;
    default: return TextureFormat::Unknown;
    }
}

SDL_GPUVertexElementFormat toSdl(VertexFormat format) {
    switch (format) {
    case VertexFormat::Float: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    case VertexFormat::Float2: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    case VertexFormat::Float3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    case VertexFormat::Float4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    case VertexFormat::UByte4Norm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    }
    return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
}

SDL_GPUPrimitiveType toSdl(Topology topology) {
    switch (topology) {
    case Topology::TriangleList: return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    case Topology::TriangleStrip: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    case Topology::LineList: return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case Topology::LineStrip: return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case Topology::PointList: return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    }
    return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
}

SDL_GPUColorTargetBlendState toSdl(BlendMode blend) {
    SDL_GPUColorTargetBlendState state{};
    if (blend == BlendMode::None)
        return state;
    state.enable_blend = true;
    state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    const SDL_GPUBlendFactor destination =
        blend == BlendMode::Additive ? SDL_GPU_BLENDFACTOR_ONE : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    state.dst_color_blendfactor = destination;
    state.dst_alpha_blendfactor = destination;
    state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    return state;
}

SDL_GPUSamplerAddressMode toSdl(AddressMode mode) {
    switch (mode) {
    case AddressMode::ClampToEdge: return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    case AddressMode::Repeat: return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    case AddressMode::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    }
    return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
}

template <class T, class Base>
T& as(Base& resource) {
    auto* result = dynamic_cast<T*>(&resource);
    GLW_EXPECTS(result != nullptr);     // made by another device implementation
    return *result;
}

} // namespace

// ---- device ----

SdlGpuDevice::SdlGpuDevice(const DeviceDesc& desc) {
    device_ = SDL_CreateGPUDevice(shaderFormats, desc.debug, nullptr);
    if (!device_ && desc.debug)      // the validation layers are not installed: without them
        device_ = SDL_CreateGPUDevice(shaderFormats, false, nullptr);
    if (!device_)
        throw Error(sdlError("no GPU device (Vulkan, Direct3D 12 or Metal) - SDL_CreateGPUDevice"));
}

SdlGpuDevice::~SdlGpuDevice() {
    GLW_ASSERT(surfaces_ == 0);
    SDL_WaitForGPUIdle(device_);
    SDL_DestroyGPUDevice(device_);
}

std::string_view SdlGpuDevice::backendName() const {
    const char* driver = SDL_GetGPUDeviceDriver(device_);
    return driver ? driver : "";
}

std::unique_ptr<Surface> SdlGpuDevice::createSurface(platform::Window& window) {
    return std::make_unique<SdlGpuSurface>(*this, as<platform::sdl::SdlWindow>(window));
}

std::unique_ptr<Buffer> SdlGpuDevice::createBuffer(const BufferDesc& desc) {
    GLW_EXPECTS(desc.size > 0 && desc.size <= UINT32_MAX);
    SDL_GPUBufferCreateInfo info{};
    if (hasAny(desc.usage, BufferUsage::Vertex))
        info.usage |= SDL_GPU_BUFFERUSAGE_VERTEX;
    if (hasAny(desc.usage, BufferUsage::Index))
        info.usage |= SDL_GPU_BUFFERUSAGE_INDEX;
    if (hasAny(desc.usage, BufferUsage::Storage))
        info.usage |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    info.size = static_cast<Uint32>(desc.size);
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &info);
    if (!buffer)
        throw Error(sdlError("SDL_CreateGPUBuffer"));
    return std::make_unique<SdlGpuBuffer>(device_, buffer, desc.size);
}

std::unique_ptr<Texture> SdlGpuDevice::createTexture(const TextureDesc& desc) {
    GLW_EXPECTS(!desc.size.empty() && desc.format != TextureFormat::Unknown);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = toSdl(desc.format);
    if (hasAny(desc.usage, TextureUsage::Sampled))
        info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if (hasAny(desc.usage, TextureUsage::RenderTarget))
        info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = static_cast<Uint32>(desc.size.width);
    info.height = static_cast<Uint32>(desc.size.height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture)
        throw Error(sdlError("SDL_CreateGPUTexture"));
    return std::make_unique<SdlGpuTexture>(device_, texture, desc);
}

std::unique_ptr<Sampler> SdlGpuDevice::createSampler(const SamplerDesc& desc) {
    SDL_GPUSamplerCreateInfo info{};
    const SDL_GPUFilter filter = desc.filter == Filter::Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = toSdl(desc.address);
    info.address_mode_v = toSdl(desc.address);
    info.address_mode_w = toSdl(desc.address);
    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(device_, &info);
    if (!sampler)
        throw Error(sdlError("SDL_CreateGPUSampler"));
    return std::make_unique<SdlGpuSampler>(device_, sampler);
}

std::unique_ptr<Shader> SdlGpuDevice::createShader(const ShaderDesc& desc) {
    const SDL_GPUShaderFormat available = SDL_GetGPUShaderFormats(device_);
    std::span<const std::uint8_t> code;
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    std::string entryPoint(desc.entryPoint);
    if ((available & SDL_GPU_SHADERFORMAT_SPIRV) && !desc.code.spirv.empty()) {
        code = desc.code.spirv;
        format = SDL_GPU_SHADERFORMAT_SPIRV;
    } else if ((available & SDL_GPU_SHADERFORMAT_DXBC) && !desc.code.dxbc.empty()) {
        code = desc.code.dxbc;
        format = SDL_GPU_SHADERFORMAT_DXBC;
    } else if ((available & SDL_GPU_SHADERFORMAT_MSL) && !desc.code.msl.empty()) {
        code = desc.code.msl;
        format = SDL_GPU_SHADERFORMAT_MSL;
        if (entryPoint == "main")
            entryPoint = "main0";
    } else {
        throw Error("a shader has no code for " + std::string(backendName()) + " (Tools/CompileShaders.py)");
    }

    SDL_GPUShaderCreateInfo info{};
    info.code_size = code.size();
    info.code = code.data();
    info.entrypoint = entryPoint.c_str();
    info.format = format;
    info.stage = desc.stage == ShaderStage::Vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = desc.samplers;
    info.num_storage_textures = desc.storageTextures;
    info.num_storage_buffers = desc.storageBuffers;
    info.num_uniform_buffers = desc.uniformBuffers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device_, &info);
    if (!shader)
        throw Error(sdlError("SDL_CreateGPUShader"));
    return std::make_unique<SdlGpuShader>(device_, shader);
}

std::unique_ptr<Pipeline> SdlGpuDevice::createPipeline(const PipelineDesc& desc) {
    GLW_EXPECTS(desc.vertexShader && desc.fragmentShader && desc.colorFormat != TextureFormat::Unknown);

    SDL_GPUVertexBufferDescription vertexBuffer{};
    vertexBuffer.slot = 0;
    vertexBuffer.pitch = desc.vertexStride;
    vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    std::vector<SDL_GPUVertexAttribute> attributes;
    for (const VertexAttribute& a : desc.vertexAttributes)
        attributes.push_back(SDL_GPUVertexAttribute{a.location, 0, toSdl(a.format), a.offset});

    SDL_GPUColorTargetDescription target{};
    target.format = toSdl(desc.colorFormat);
    target.blend_state = toSdl(desc.blend);

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = as<const SdlGpuShader>(*desc.vertexShader).handle();
    info.fragment_shader = as<const SdlGpuShader>(*desc.fragmentShader).handle();
    if (!attributes.empty()) {
        info.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes.data();
        info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
    }
    info.primitive_type = toSdl(desc.topology);
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device_, &info);
    if (!pipeline)
        throw Error(sdlError("SDL_CreateGPUGraphicsPipeline"));
    return std::make_unique<SdlGpuPipeline>(device_, pipeline);
}

std::unique_ptr<CommandList> SdlGpuDevice::beginCommands() {
    return std::make_unique<SdlGpuCommandList>(*this);
}

void SdlGpuDevice::waitIdle() {
    SDL_WaitForGPUIdle(device_);
}

// ---- surface ----

SdlGpuSurface::SdlGpuSurface(SdlGpuDevice& device, platform::sdl::SdlWindow& window) : device_(device), window_(window) {
    if (!SDL_ClaimWindowForGPUDevice(device_.handle(), window_.handle()))
        throw Error(sdlError("SDL_ClaimWindowForGPUDevice"));
    device_.surfaceCreated();
}

SdlGpuSurface::~SdlGpuSurface() {
    // the swap chain goes with the claim: nothing the GPU has to do may still use it
    SDL_WaitForGPUIdle(device_.handle());
    SDL_ReleaseWindowFromGPUDevice(device_.handle(), window_.handle());
    device_.surfaceDestroyed();
}

platform::Window& SdlGpuSurface::window() const {
    return window_;
}

TextureFormat SdlGpuSurface::format() const {
    return fromSdl(SDL_GetGPUSwapchainTextureFormat(device_.handle(), window_.handle()));
}

SDL_Window* SdlGpuSurface::windowHandle() const {
    return window_.handle();
}

// ---- command list ----

SdlGpuCommandList::SdlGpuCommandList(SdlGpuDevice& device) : device_(device) {
    commands_ = SDL_AcquireGPUCommandBuffer(device_.handle());
    if (!commands_)
        throw Error(sdlError("SDL_AcquireGPUCommandBuffer"));
}

SdlGpuCommandList::~SdlGpuCommandList() {
    if (!commands_)
        return;
    // a command buffer that acquired a swap chain image cannot be cancelled: submitted, as CommandList says
    if (pass_)
        SDL_EndGPURenderPass(pass_);
    SDL_SubmitGPUCommandBuffer(commands_);
}

SDL_GPUTransferBuffer* SdlGpuCommandList::stage(std::span<const std::byte> data) {
    GLW_EXPECTS(!data.empty() && data.size() <= UINT32_MAX);
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = static_cast<Uint32>(data.size());
    SDL_GPUTransferBuffer* staging = SDL_CreateGPUTransferBuffer(device_.handle(), &info);
    if (!staging)
        throw Error(sdlError("SDL_CreateGPUTransferBuffer"));
    void* memory = SDL_MapGPUTransferBuffer(device_.handle(), staging, false);
    if (!memory) {
        SDL_ReleaseGPUTransferBuffer(device_.handle(), staging);
        throw Error(sdlError("SDL_MapGPUTransferBuffer"));
    }
    std::memcpy(memory, data.data(), data.size());
    SDL_UnmapGPUTransferBuffer(device_.handle(), staging);
    return staging;
}

void SdlGpuCommandList::upload(Buffer& buffer, std::size_t offset, std::span<const std::byte> data, bool discard) {
    GLW_EXPECTS(commands_ && !pass_);
    GLW_EXPECTS(offset + data.size() <= buffer.size());
    SDL_GPUTransferBuffer* staging = stage(data);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands_);
    const SDL_GPUTransferBufferLocation source{staging, 0};
    const SDL_GPUBufferRegion destination{as<SdlGpuBuffer>(buffer).handle(), static_cast<Uint32>(offset),
                                          static_cast<Uint32>(data.size())};
    SDL_UploadToGPUBuffer(copy, &source, &destination, discard);
    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(device_.handle(), staging);
}

void SdlGpuCommandList::upload(Texture& texture, PixelRect region, std::span<const std::byte> pixels) {
    GLW_EXPECTS(commands_ && !pass_ && !region.empty());
    GLW_EXPECTS(pixels.size() == static_cast<std::size_t>(region.width) * region.height * bytesPerPixel(texture.format()));
    SDL_GPUTransferBuffer* staging = stage(pixels);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands_);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.pixels_per_row = static_cast<Uint32>(region.width);
    source.rows_per_layer = static_cast<Uint32>(region.height);
    SDL_GPUTextureRegion destination{};
    destination.texture = as<SdlGpuTexture>(texture).handle();
    destination.x = static_cast<Uint32>(region.x);
    destination.y = static_cast<Uint32>(region.y);
    destination.w = static_cast<Uint32>(region.width);
    destination.h = static_cast<Uint32>(region.height);
    destination.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(device_.handle(), staging);
}

void SdlGpuCommandList::beginPass(SDL_GPUTexture* target, Color clear) {
    SDL_GPUColorTargetInfo color{};
    color.texture = target;
    color.clear_color = SDL_FColor{clear.r, clear.g, clear.b, clear.a};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    pass_ = SDL_BeginGPURenderPass(commands_, &color, 1, nullptr);
    if (!pass_)
        throw Error(sdlError("SDL_BeginGPURenderPass"));
}

std::optional<PixelSize> SdlGpuCommandList::beginSurfacePass(Surface& surface, Color clear) {
    GLW_EXPECTS(commands_ && !pass_);
    auto& target = as<SdlGpuSurface>(surface);
    GLW_EXPECTS(&target.device() == &device_);

    // waits while too many frames are in flight: what keeps the CPU from running ahead of the GPU
    SDL_GPUTexture* image = nullptr;
    Uint32 width = 0, height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands_, target.windowHandle(), &image, &width, &height))
        throw Error(sdlError("SDL_WaitAndAcquireGPUSwapchainTexture"));
    if (!image)
        return std::nullopt;
    beginPass(image, clear);
    return PixelSize{static_cast<int>(width), static_cast<int>(height)};
}

void SdlGpuCommandList::beginTexturePass(Texture& target, Color clear) {
    GLW_EXPECTS(commands_ && !pass_ && hasAny(target.usage(), TextureUsage::RenderTarget));
    beginPass(as<SdlGpuTexture>(target).handle(), clear);
}

void SdlGpuCommandList::submit() {
    GLW_EXPECTS(commands_ && !pass_);
    SDL_GPUCommandBuffer* commands = commands_;
    commands_ = nullptr;
    if (!SDL_SubmitGPUCommandBuffer(commands))
        throw Error(sdlError("SDL_SubmitGPUCommandBuffer"));
}

void SdlGpuCommandList::endPass() {
    GLW_EXPECTS(pass_ != nullptr);
    SDL_EndGPURenderPass(pass_);
    pass_ = nullptr;
}

void SdlGpuCommandList::setPipeline(const Pipeline& pipeline) {
    GLW_EXPECTS(pass_ != nullptr);
    SDL_BindGPUGraphicsPipeline(pass_, as<const SdlGpuPipeline>(pipeline).handle());
}

void SdlGpuCommandList::setVertexBuffer(const Buffer& buffer, std::size_t offset) {
    GLW_EXPECTS(pass_ != nullptr);
    const SDL_GPUBufferBinding binding{as<const SdlGpuBuffer>(buffer).handle(), static_cast<Uint32>(offset)};
    SDL_BindGPUVertexBuffers(pass_, 0, &binding, 1);
}

void SdlGpuCommandList::setIndexBuffer(const Buffer& buffer, IndexType type, std::size_t offset) {
    GLW_EXPECTS(pass_ != nullptr);
    const SDL_GPUBufferBinding binding{as<const SdlGpuBuffer>(buffer).handle(), static_cast<Uint32>(offset)};
    SDL_BindGPUIndexBuffer(pass_, &binding,
                           type == IndexType::UInt16 ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
}

void SdlGpuCommandList::setTexture(std::uint32_t slot, const Texture& texture, const Sampler& sampler) {
    GLW_EXPECTS(pass_ != nullptr);
    const SDL_GPUTextureSamplerBinding binding{as<const SdlGpuTexture>(texture).handle(), as<const SdlGpuSampler>(sampler).handle()};
    SDL_BindGPUFragmentSamplers(pass_, slot, &binding, 1);
}

void SdlGpuCommandList::setVertexUniforms(std::uint32_t slot, std::span<const std::byte> data) {
    GLW_EXPECTS(commands_ != nullptr);
    SDL_PushGPUVertexUniformData(commands_, slot, data.data(), static_cast<Uint32>(data.size()));
}

void SdlGpuCommandList::setFragmentUniforms(std::uint32_t slot, std::span<const std::byte> data) {
    GLW_EXPECTS(commands_ != nullptr);
    SDL_PushGPUFragmentUniformData(commands_, slot, data.data(), static_cast<Uint32>(data.size()));
}

void SdlGpuCommandList::setViewport(PixelRect viewport) {
    GLW_EXPECTS(pass_ != nullptr);
    const SDL_GPUViewport v{static_cast<float>(viewport.x), static_cast<float>(viewport.y), static_cast<float>(viewport.width),
                            static_cast<float>(viewport.height), 0.0f, 1.0f};
    SDL_SetGPUViewport(pass_, &v);
}

void SdlGpuCommandList::setScissor(PixelRect scissor) {
    GLW_EXPECTS(pass_ != nullptr);
    const SDL_Rect r{scissor.x, scissor.y, scissor.width, scissor.height};
    SDL_SetGPUScissor(pass_, &r);
}

void SdlGpuCommandList::draw(std::uint32_t vertexCount, std::uint32_t firstVertex) {
    GLW_EXPECTS(pass_ != nullptr);
    SDL_DrawGPUPrimitives(pass_, vertexCount, 1, firstVertex, 0);
}

void SdlGpuCommandList::drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex, std::int32_t vertexOffset) {
    GLW_EXPECTS(pass_ != nullptr);
    SDL_DrawGPUIndexedPrimitives(pass_, indexCount, 1, firstIndex, vertexOffset, 0);
}

} // namespace sdlgpu

} // namespace glw::rhi
