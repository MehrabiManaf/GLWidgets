// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The RHI on SDL3's GPU API. It works with the SDL platform only: a surface is a window SDL's device claimed.
#include <GLWidgets/Rhi/Device.hpp>

#include <SDL3/SDL_gpu.h>

namespace glw::platform::sdl {
class SdlWindow;
} // namespace glw::platform::sdl

namespace glw::rhi::sdlgpu {

class SdlGpuDevice final : public Device {
public:
    explicit SdlGpuDevice(const DeviceDesc& desc);
    ~SdlGpuDevice() override;

    SDL_GPUDevice* handle() const { return device_; }

    std::string_view backendName() const override;
    std::unique_ptr<Surface> createSurface(platform::Window& window) override;
    std::unique_ptr<Buffer> createBuffer(const BufferDesc& desc) override;
    std::unique_ptr<Texture> createTexture(const TextureDesc& desc) override;
    std::unique_ptr<Sampler> createSampler(const SamplerDesc& desc) override;
    std::unique_ptr<Shader> createShader(const ShaderDesc& desc) override;
    std::unique_ptr<Pipeline> createPipeline(const PipelineDesc& desc) override;
    std::unique_ptr<CommandList> beginCommands() override;
    void waitIdle() override;

    // SdlGpuSurface: the surfaces there are, which must all be gone before the device
    void surfaceCreated() { ++surfaces_; }
    void surfaceDestroyed() { --surfaces_; }

private:
    SDL_GPUDevice* device_ = nullptr;
    int surfaces_ = 0;
};

class SdlGpuSurface final : public Surface {
public:
    SdlGpuSurface(SdlGpuDevice& device, platform::sdl::SdlWindow& window);
    ~SdlGpuSurface() override;

    platform::Window& window() const override;
    TextureFormat format() const override;
    SdlGpuDevice& device() const { return device_; }
    SDL_Window* windowHandle() const;

private:
    SdlGpuDevice& device_;
    platform::sdl::SdlWindow& window_;
};

class SdlGpuBuffer final : public Buffer {
public:
    SdlGpuBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer, std::size_t size) : device_(device), buffer_(buffer), size_(size) {}
    ~SdlGpuBuffer() override { SDL_ReleaseGPUBuffer(device_, buffer_); }

    std::size_t size() const override { return size_; }
    SDL_GPUBuffer* handle() const { return buffer_; }

private:
    SDL_GPUDevice* device_;
    SDL_GPUBuffer* buffer_;
    std::size_t size_;
};

class SdlGpuTexture final : public Texture {
public:
    SdlGpuTexture(SDL_GPUDevice* device, SDL_GPUTexture* texture, const TextureDesc& desc) : device_(device), texture_(texture), desc_(desc) {}
    ~SdlGpuTexture() override { SDL_ReleaseGPUTexture(device_, texture_); }

    PixelSize size() const override { return desc_.size; }
    TextureFormat format() const override { return desc_.format; }
    TextureUsage usage() const override { return desc_.usage; }
    SDL_GPUTexture* handle() const { return texture_; }

private:
    SDL_GPUDevice* device_;
    SDL_GPUTexture* texture_;
    TextureDesc desc_;
};

class SdlGpuSampler final : public Sampler {
public:
    SdlGpuSampler(SDL_GPUDevice* device, SDL_GPUSampler* sampler) : device_(device), sampler_(sampler) {}
    ~SdlGpuSampler() override { SDL_ReleaseGPUSampler(device_, sampler_); }

    SDL_GPUSampler* handle() const { return sampler_; }

private:
    SDL_GPUDevice* device_;
    SDL_GPUSampler* sampler_;
};

class SdlGpuShader final : public Shader {
public:
    SdlGpuShader(SDL_GPUDevice* device, SDL_GPUShader* shader) : device_(device), shader_(shader) {}
    ~SdlGpuShader() override { SDL_ReleaseGPUShader(device_, shader_); }

    SDL_GPUShader* handle() const { return shader_; }

private:
    SDL_GPUDevice* device_;
    SDL_GPUShader* shader_;
};

class SdlGpuPipeline final : public Pipeline {
public:
    SdlGpuPipeline(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* pipeline) : device_(device), pipeline_(pipeline) {}
    ~SdlGpuPipeline() override { SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_); }

    SDL_GPUGraphicsPipeline* handle() const { return pipeline_; }

private:
    SDL_GPUDevice* device_;
    SDL_GPUGraphicsPipeline* pipeline_;
};

class SdlGpuCommandList final : public CommandList {
public:
    explicit SdlGpuCommandList(SdlGpuDevice& device);
    ~SdlGpuCommandList() override;

    void upload(Buffer& buffer, std::size_t offset, std::span<const std::byte> data, bool discard) override;
    void upload(Texture& texture, PixelRect region, std::span<const std::byte> pixels) override;
    std::optional<PixelSize> beginSurfacePass(Surface& surface, Color clear) override;
    void beginTexturePass(Texture& target, Color clear) override;
    void submit() override;

    void endPass() override;
    void setPipeline(const Pipeline& pipeline) override;
    void setVertexBuffer(const Buffer& buffer, std::size_t offset) override;
    void setIndexBuffer(const Buffer& buffer, IndexType type, std::size_t offset) override;
    void setTexture(std::uint32_t slot, const Texture& texture, const Sampler& sampler) override;
    void setVertexUniforms(std::uint32_t slot, std::span<const std::byte> data) override;
    void setFragmentUniforms(std::uint32_t slot, std::span<const std::byte> data) override;
    void setViewport(PixelRect viewport) override;
    void setScissor(PixelRect scissor) override;
    void draw(std::uint32_t vertexCount, std::uint32_t firstVertex) override;
    void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex, std::int32_t vertexOffset) override;

private:
    // a transfer buffer holding `data`, released once recorded (SDL keeps it until the GPU is done with it)
    SDL_GPUTransferBuffer* stage(std::span<const std::byte> data);
    void beginPass(SDL_GPUTexture* target, Color clear);

    SdlGpuDevice& device_;
    SDL_GPUCommandBuffer* commands_ = nullptr;      // none once submitted
    SDL_GPURenderPass* pass_ = nullptr;
};

} // namespace glw::rhi::sdlgpu
