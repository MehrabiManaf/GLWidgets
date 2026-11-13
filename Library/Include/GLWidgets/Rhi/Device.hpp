// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The render hardware interface: what GLWidgets asks of the GPU, whatever API does it. Widgets never use it; the paint
// layer does, and so may applications for what widgets do not draw (a 3D scene, a chart with millions of points). The
// implementation is SDL3's GPU API (Source/Rhi/SdlGpu): Vulkan, Direct3D 12 or Metal, as the system has. Another API
// (OpenGL, Direct3D 11) would be another implementation of these classes.
//
// One device draws into every window of the application, so what it holds (textures, the glyph atlas...) is shared by all of
// them: a panel moved to another window does not upload anything again. It is used on the main thread, as the platform.
//
// A frame: beginCommands; uploads; for each target a pass (beginSurfacePass or beginTexturePass, draws, endPass), each window
// at most once a frame; then submit, which presents what was drawn into the windows.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Rhi/Resources.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace glw::platform {
class Platform;
class Window;
} // namespace glw::platform

namespace glw::rhi {

// What a device draws into a window: a swap chain. It must not outlive its device nor its window.
class GLW_EXPORT Surface {
public:
    virtual ~Surface();
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    virtual platform::Window& window() const = 0;
    // The format of its images: what the pipelines drawing into it are made for.
    virtual TextureFormat format() const = 0;
    // Blocks until its next image can be drawn into at once: the frames in flight done, presented at the display's
    // refresh. Called before the input of a frame is read, so that the frame shows the newest input.
    virtual void waitUntilReady() = 0;

protected:
    Surface() = default;
};

// The commands of a frame. Destroyed without submit, it submits what it has.
class GLW_EXPORT CommandList {
public:
    virtual ~CommandList();
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;

    // ---- outside passes ----

    // Copies `data` into `buffer` at `offset`. `discard`: what the buffer held before is not needed any more, so the GPU
    // may go on reading it for the frames in flight while this frame gets new memory (no waiting).
    virtual void upload(Buffer& buffer, std::size_t offset, std::span<const std::byte> data, bool discard = false) = 0;
    // Copies pixels into `region` of `texture`: rows of region.width pixels of its format, one after the other.
    virtual void upload(Texture& texture, PixelRect region, std::span<const std::byte> pixels) = 0;

    // Starts drawing into the next image of `surface`, cleared to `clear`, and gives its size. None when there is nothing to
    // draw into now (the window is minimized, the swap chain is being remade): nothing to draw then, and no endPass.
    // `depth`: with a depth buffer of the surface's own (as large as its image, cleared to the farthest), for pipelines
    // made with depthFormat Depth32Float.
    virtual std::optional<PixelSize> beginSurfacePass(Surface& surface, Color clear, bool depth = false) = 0;
    // Starts drawing into `target` (made with TextureUsage::RenderTarget), cleared to `clear` (none: drawn over what it
    // has). `depth`: its depth buffer (a Depth32Float texture made with TextureUsage::DepthTarget, as large), cleared to the
    // farthest when `clearDepth`, kept for a pass that goes on drawing into the same target.
    virtual void beginTexturePass(Texture& target, std::optional<Color> clear, Texture* depth = nullptr, bool clearDepth = true) = 0;
    // The next image of `surface`, to blit into (blitToSurface) rather than draw into, and its size. None as
    // beginSurfacePass says.
    virtual std::optional<PixelSize> acquireSurface(Surface& surface) = 0;
    // All of `source` (made with TextureUsage::Sampled) onto all of `target` (made with TextureUsage::RenderTarget),
    // scaled with linear filtering. Outside passes.
    virtual void blit(const Texture& source, Texture& target) = 0;
    // ... onto the image of `surface` acquired (acquireSurface), presented with it.
    virtual void blitToSurface(const Texture& source, Surface& surface) = 0;

    // Sends the commands to the GPU and presents the surfaces drawn. Nothing can be added after.
    virtual void submit() = 0;

    // ---- inside a pass ----

    virtual void endPass() = 0;

    virtual void setPipeline(const Pipeline& pipeline) = 0;
    virtual void setVertexBuffer(const Buffer& buffer, std::size_t offset = 0) = 0;
    virtual void setIndexBuffer(const Buffer& buffer, IndexType type, std::size_t offset = 0) = 0;
    // The texture and sampler of the fragment shader's sampler `slot`.
    virtual void setTexture(std::uint32_t slot, const Texture& texture, const Sampler& sampler) = 0;
    // The data of uniform buffer `slot` for the draws that follow (std140 layout, a multiple of 16 bytes).
    virtual void setVertexUniforms(std::uint32_t slot, std::span<const std::byte> data) = 0;
    virtual void setFragmentUniforms(std::uint32_t slot, std::span<const std::byte> data) = 0;
    // A pass starts with the whole target as viewport and no scissor.
    virtual void setViewport(PixelRect viewport) = 0;
    virtual void setScissor(PixelRect scissor) = 0;

    virtual void draw(std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0, std::int32_t vertexOffset = 0) = 0;

protected:
    CommandList() = default;
};

struct DeviceDesc {
    bool debug = false;             // the validation of the GPU API, when the system has it (slow)
    // The frames the CPU may submit before the GPU has done the first of them (1 to 3). One keeps what shows closest
    // to the input (an interface: what is dragged follows the mouse); more keep the CPU and the GPU busier at once (a
    // game drawing much), each a refresh of delay.
    int framesInFlight = 1;
};

class GLW_EXPORT Device {
public:
    virtual ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // The GPU API in use: "vulkan", "direct3d12", "metal".
    virtual std::string_view backendName() const = 0;

    virtual std::unique_ptr<Surface> createSurface(platform::Window& window) = 0;

    // Each throws Error when the GPU API refuses (a shader without code for this API...).
    virtual std::unique_ptr<Buffer> createBuffer(const BufferDesc& desc) = 0;
    virtual std::unique_ptr<Texture> createTexture(const TextureDesc& desc) = 0;
    virtual std::unique_ptr<Sampler> createSampler(const SamplerDesc& desc) = 0;
    virtual std::unique_ptr<Shader> createShader(const ShaderDesc& desc) = 0;
    virtual std::unique_ptr<Pipeline> createPipeline(const PipelineDesc& desc) = 0;

    virtual std::unique_ptr<CommandList> beginCommands() = 0;

    // Waits until the GPU has done everything submitted.
    virtual void waitIdle() = 0;

    // Waits until the GPU has done everything submitted, then gives the pixels of all of `texture` (not a surface's image):
    // its rows one after the other, the top one first, in its format. Slow, the GPU waited for: a screenshot, a test, not
    // each frame.
    virtual std::vector<std::byte> readPixels(const Texture& texture) = 0;

protected:
    Device() = default;
};

// A device for the windows of `platform`, which must outlive it. Throws Error when the system has no GPU API GLWidgets can use.
GLW_EXPORT std::unique_ptr<Device> createDevice(platform::Platform& platform, const DeviceDesc& desc = {});

} // namespace glw::rhi
