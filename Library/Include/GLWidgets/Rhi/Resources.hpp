// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What a device makes (Device.hpp): buffers, textures, samplers, shaders and pipelines, and what describes them.
// A resource must not outlive its device. It may be destroyed right after the frame that used it is submitted: the GPU
// API keeps what the GPU still uses.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace glw::rhi {

// ---- textures ----

enum class TextureFormat : std::uint8_t {
    Unknown,
    RGBA8,              // 8 bits per channel, unsigned normalized
    BGRA8,
    RGBA8Srgb,          // the same, read and written through the sRGB curve
    BGRA8Srgb,
    R8,
    RGBA16Float,
    RGB10A2,
};

// Bytes per pixel of `format` (0 for Unknown).
GLW_EXPORT std::size_t bytesPerPixel(TextureFormat format);

enum class TextureUsage : std::uint8_t {
    Sampled      = 1 << 0,  // read by shaders
    RenderTarget = 1 << 1,  // drawn into (CommandList::beginTexturePass)
};
constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr bool hasAny(TextureUsage set, TextureUsage flags) {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flags)) != 0;
}

struct TextureDesc {
    PixelSize size;
    TextureFormat format = TextureFormat::RGBA8;
    TextureUsage usage = TextureUsage::Sampled;
};

class GLW_EXPORT Texture {
public:
    virtual ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    virtual PixelSize size() const = 0;
    virtual TextureFormat format() const = 0;
    virtual TextureUsage usage() const = 0;

protected:
    Texture() = default;
};

// ---- buffers ----

enum class BufferUsage : std::uint8_t {
    Vertex  = 1 << 0,
    Index   = 1 << 1,
    Storage = 1 << 2,       // read by shaders
};
constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr bool hasAny(BufferUsage set, BufferUsage flags) {
    return (static_cast<std::uint8_t>(set) & static_cast<std::uint8_t>(flags)) != 0;
}

struct BufferDesc {
    std::size_t size = 0;   // bytes
    BufferUsage usage = BufferUsage::Vertex;
};

class GLW_EXPORT Buffer {
public:
    virtual ~Buffer();
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    virtual std::size_t size() const = 0;

protected:
    Buffer() = default;
};

// ---- samplers ----

enum class Filter : std::uint8_t { Nearest, Linear };
enum class AddressMode : std::uint8_t { ClampToEdge, Repeat, MirroredRepeat };

struct SamplerDesc {
    Filter filter = Filter::Linear;
    AddressMode address = AddressMode::ClampToEdge;
};

class GLW_EXPORT Sampler {
public:
    virtual ~Sampler();
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

protected:
    Sampler() = default;
};

// ---- shaders ----

enum class ShaderStage : std::uint8_t { Vertex, Fragment };

// A shader compiled ahead of time for each GPU API (Tools/CompileShaders.py): the device takes the one it uses. Empty
// when a shader was not compiled for an API.
struct ShaderCode {
    std::span<const std::uint8_t> spirv;    // Vulkan
    std::span<const std::uint8_t> dxbc;     // Direct3D 12 (shader model 5.1)
    std::span<const std::uint8_t> msl;      // Metal (source text, as SPIRV-Cross writes it)
};

// The resources a shader declares, as SDL's GPU API wants them numbered (Library/Shaders/Painter.hlsl says how).
struct ShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    ShaderCode code;
    std::string_view entryPoint = "main";   // "main" is "main0" in MSL (SPIRV-Cross renames it): the device knows
    std::uint32_t samplers = 0;
    std::uint32_t uniformBuffers = 0;
    std::uint32_t storageBuffers = 0;
    std::uint32_t storageTextures = 0;
};

class GLW_EXPORT Shader {
public:
    virtual ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

protected:
    Shader() = default;
};

// ---- pipelines ----

enum class VertexFormat : std::uint8_t { Float, Float2, Float3, Float4, UByte4Norm };

struct VertexAttribute {
    std::uint32_t location = 0;             // TEXCOORD<location> in HLSL
    VertexFormat format = VertexFormat::Float4;
    std::uint32_t offset = 0;               // bytes into the vertex
};

enum class Topology : std::uint8_t { TriangleList, TriangleStrip, LineList, LineStrip, PointList };

enum class BlendMode : std::uint8_t {
    None,                   // replaces
    PremultipliedAlpha,     // source + destination * (1 - source alpha)
    Additive,               // source + destination
};

// A whole drawing state: shaders, vertex layout, topology and what it draws into. One vertex buffer.
struct PipelineDesc {
    const Shader* vertexShader = nullptr;
    const Shader* fragmentShader = nullptr;
    std::uint32_t vertexStride = 0;         // bytes from one vertex to the next
    std::span<const VertexAttribute> vertexAttributes;
    Topology topology = Topology::TriangleList;
    BlendMode blend = BlendMode::PremultipliedAlpha;
    TextureFormat colorFormat = TextureFormat::Unknown;    // of the surface or texture drawn into
};

class GLW_EXPORT Pipeline {
public:
    virtual ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

protected:
    Pipeline() = default;
};

enum class IndexType : std::uint8_t { UInt16, UInt32 };

} // namespace glw::rhi
