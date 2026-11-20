// SPDX-License-Identifier: MIT
#include "OpenGL.hpp"
#include "Platform/Sdl/SdlWindow.hpp"
#include "Platform/Sdl/SdlPlatform.hpp"
#include <GLWidgets/Core/Error.hpp>
#include <SDL3/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <spirv_glsl.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace glw::rhi::legacy {
namespace {

template<class Fn> Fn load(const char* name) {
    if (auto proc = SDL_GL_GetProcAddress(name))
        return reinterpret_cast<Fn>(proc);
    throw Error(std::string("OpenGL 3.3 missing ") + name);
}

struct Context {
    SDL_Window* hidden = nullptr;
    SDL_GLContext handle = nullptr;
    Context() {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        hidden = SDL_CreateWindow("GLWidgets OpenGL", 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        if (!hidden)
            throw Error(SDL_GetError());
        handle = SDL_GL_CreateContext(hidden);
        if (!handle) {
            const Error error(SDL_GetError());
            SDL_DestroyWindow(hidden);
            throw error;
        }
        SDL_GL_SetSwapInterval(1);
    }
    ~Context() { SDL_GL_DestroyContext(handle); SDL_DestroyWindow(hidden); }
    void bind(SDL_Window* window) {
        if (!SDL_GL_MakeCurrent(window, handle))
            throw Error(SDL_GetError());
    }
};

struct Api {
#define GL_PROC(name) decltype(&gl##name) name = load<decltype(&gl##name)>("gl" #name)
    GL_PROC(GenBuffers);
    GL_PROC(DeleteBuffers);
    GL_PROC(BindBuffer);
    GL_PROC(BufferData);
    GL_PROC(BufferSubData);
    GL_PROC(BindBufferBase);
    GL_PROC(GenTextures);
    GL_PROC(DeleteTextures);
    GL_PROC(BindTexture);
    GL_PROC(TexImage2D);
    GL_PROC(TexSubImage2D);
    GL_PROC(TexParameteri);
    GL_PROC(PixelStorei);
    GL_PROC(GetTexImage);
    GL_PROC(GenSamplers);
    GL_PROC(DeleteSamplers);
    GL_PROC(SamplerParameteri);
    GL_PROC(BindSampler);
    GL_PROC(CreateShader);
    GL_PROC(ShaderSource);
    GL_PROC(CompileShader);
    GL_PROC(GetShaderiv);
    GL_PROC(GetShaderInfoLog);
    GL_PROC(DeleteShader);
    GL_PROC(CreateProgram);
    GL_PROC(AttachShader);
    GL_PROC(LinkProgram);
    GL_PROC(GetProgramiv);
    GL_PROC(GetProgramInfoLog);
    GL_PROC(DeleteProgram);
    GL_PROC(UseProgram);
    GL_PROC(GetUniformBlockIndex);
    GL_PROC(UniformBlockBinding);
    GL_PROC(GetUniformLocation);
    GL_PROC(Uniform1i);
    GL_PROC(GenFramebuffers);
    GL_PROC(BindFramebuffer);
    GL_PROC(FramebufferTexture2D);
    GL_PROC(CheckFramebufferStatus);
    GL_PROC(DeleteFramebuffers);
    GL_PROC(BlitFramebuffer);
    GL_PROC(ClearColor);
    GL_PROC(ClearDepth);
    GL_PROC(Clear);
    GL_PROC(Enable);
    GL_PROC(Disable);
    GL_PROC(BlendFunc);
    GL_PROC(DepthFunc);
    GL_PROC(DepthMask);
    GL_PROC(Viewport);
    GL_PROC(Scissor);
    GL_PROC(GenVertexArrays);
    GL_PROC(BindVertexArray);
    GL_PROC(DeleteVertexArrays);
    GL_PROC(EnableVertexAttribArray);
    GL_PROC(DisableVertexAttribArray);
    GL_PROC(VertexAttribPointer);
    GL_PROC(DrawArrays);
    GL_PROC(DrawElements);
    GL_PROC(DrawElementsBaseVertex);
    GL_PROC(ActiveTexture);
    GL_PROC(Finish);
#undef GL_PROC
};

struct Format { GLint internal; GLenum layout, type; };
Format formatOf(TextureFormat f) {
    switch (f) {
    case TextureFormat::RGBA8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::BGRA8: return {GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case TextureFormat::RGBA8Srgb: return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::BGRA8Srgb: return {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case TextureFormat::R8: return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case TextureFormat::RGBA16Float: return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
    case TextureFormat::RGB10A2: return {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV};
    case TextureFormat::Depth32Float: return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
    default: throw Error("Unsupported OpenGL texture format");
    }
}

struct GlTexture final : Texture {
    Api& gl;
    TextureDesc desc;
    GLuint id{};
    GlTexture(Api& api, TextureDesc d) : gl(api), desc(d) {
        const auto f = formatOf(d.format);
        gl.GenTextures(1, &id);
        gl.BindTexture(GL_TEXTURE_2D, id);
        gl.TexImage2D(GL_TEXTURE_2D, 0, f.internal, d.size.width, d.size.height, 0, f.layout, f.type, nullptr);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }
    ~GlTexture() override { gl.DeleteTextures(1, &id); }
    PixelSize size() const override { return desc.size; }
    TextureFormat format() const override { return desc.format; }
    TextureUsage usage() const override { return desc.usage; }
};

struct GlBuffer final : Buffer {
    Api& gl;
    BufferDesc desc;
    GLuint id{};
    GlBuffer(Api& api, BufferDesc d) : gl(api), desc(d) {
        gl.GenBuffers(1, &id);
        gl.BindBuffer(GL_ARRAY_BUFFER, id);
        gl.BufferData(GL_ARRAY_BUFFER, d.size, nullptr, GL_DYNAMIC_DRAW);
    }
    ~GlBuffer() override { gl.DeleteBuffers(1, &id); }
    size_t size() const override { return desc.size; }
};
struct GlSampler final : Sampler {
    Api& gl;
    GLuint id{};
    GlSampler(Api& api, SamplerDesc desc) : gl(api) {
        gl.GenSamplers(1, &id);
        const GLint filter = desc.filter == Filter::Linear ? GL_LINEAR : GL_NEAREST;
        const GLint wrap = desc.address == AddressMode::Repeat ? GL_REPEAT : desc.address == AddressMode::MirroredRepeat ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
        gl.SamplerParameteri(id, GL_TEXTURE_MIN_FILTER, filter);
        gl.SamplerParameteri(id, GL_TEXTURE_MAG_FILTER, filter);
        gl.SamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap);
        gl.SamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap);
    }
    ~GlSampler() override { gl.DeleteSamplers(1, &id); }
};

std::string blockName(bool vertex, unsigned slot) {
    return (vertex ? "VertexBlock" : "FragmentBlock") + std::to_string(slot);
}
struct GlShader final : Shader {
    Api& gl;
    GLuint id{};
    unsigned blocks, samplers;
    GlShader(Api& api, const ShaderDesc& d) : gl(api), blocks(d.uniformBuffers), samplers(d.samplers) {
        if (d.code.spirv.empty() || d.code.spirv.size() % 4 || d.storageBuffers || d.storageTextures)
            throw Error("OpenGL 3.3 requires SPIR-V graphics shaders without storage resources");
        const bool vs = d.stage == ShaderStage::Vertex;
        std::vector<uint32_t> words(d.code.spirv.size()/4);
        std::memcpy(words.data(), d.code.spirv.data(), d.code.spirv.size());
        spirv_cross::CompilerGLSL compiler(std::move(words));
        compiler.set_entry_point(std::string(d.entryPoint), vs ? spv::ExecutionModelVertex : spv::ExecutionModelFragment);
        auto opts = compiler.get_common_options();
        opts.version = 330;
        opts.enable_420pack_extension = false;
        opts.vertex.flip_vert_y = true;
        opts.vertex.fixup_clipspace = true;
        compiler.set_common_options(opts);
        auto resources = compiler.get_shader_resources();
        for (const auto& r : vs ? resources.stage_outputs : resources.stage_inputs)
            compiler.set_name(r.id, "varyingAt" + std::to_string(compiler.get_decoration(r.id, spv::DecorationLocation)));
        for (const auto& r : resources.uniform_buffers)
            compiler.set_name(r.base_type_id, blockName(vs, compiler.get_decoration(r.id, spv::DecorationBinding)));
        compiler.build_combined_image_samplers();
        for (const auto& r : compiler.get_combined_image_samplers())
            compiler.set_name(r.combined_id, "texAt" + std::to_string(compiler.get_decoration(r.image_id, spv::DecorationBinding)));
        for (const auto& r : resources.sampled_images)
            compiler.set_name(r.id, "texAt" + std::to_string(compiler.get_decoration(r.id, spv::DecorationBinding)));
        const auto source = compiler.compile();
        const char* ptr = source.c_str();
        id = gl.CreateShader(vs ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        gl.ShaderSource(id, 1, &ptr, nullptr);
        gl.CompileShader(id);
        GLint ok = 0;
        gl.GetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            gl.GetShaderInfoLog(id, sizeof(log), nullptr, log);
            gl.DeleteShader(id);
            throw Error(std::string("OpenGL shader: ") + log);
        }
    }
    ~GlShader() override { gl.DeleteShader(id); }
};

constexpr unsigned stageSlots = 12; // guaranteed uniform blocks per shader stage in GL 3.3
struct GlPipeline final : Pipeline {
    Api& gl;
    GLuint id{};
    PipelineDesc desc;
    std::vector<VertexAttribute> attributes;
    GlPipeline(Api& api, const PipelineDesc& d) : gl(api), desc(d), attributes(d.vertexAttributes.begin(), d.vertexAttributes.end()) {
        auto& vs = static_cast<const GlShader&>(*d.vertexShader);
        auto& fs = static_cast<const GlShader&>(*d.fragmentShader);
        id = gl.CreateProgram();
        gl.AttachShader(id, vs.id);
        gl.AttachShader(id, fs.id);
        gl.LinkProgram(id);
        GLint ok = 0;
        gl.GetProgramiv(id, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            gl.GetProgramInfoLog(id, sizeof(log), nullptr, log);
            gl.DeleteProgram(id);
            throw Error(std::string("OpenGL program: ") + log);
        }
        gl.UseProgram(id);
        for (bool vertex : {true, false}) {
            for (unsigned i = 0; i < (vertex ? vs.blocks : fs.blocks); ++i) {
                GLuint index = gl.GetUniformBlockIndex(id, blockName(vertex, i).c_str());
                if (index != GL_INVALID_INDEX)
                    gl.UniformBlockBinding(id, index, (vertex ? 0 : stageSlots) + i);
            }
        }
        for (unsigned i = 0; i < fs.samplers; ++i)
            gl.Uniform1i(gl.GetUniformLocation(id, ("texAt" + std::to_string(i)).c_str()), i);
    }
    ~GlPipeline() override { gl.DeleteProgram(id); }
};

struct GlSurface final : Surface {
    Api& gl;
    platform::sdl::SdlWindow& win;
    std::unique_ptr<GlTexture> color, depth;
    GlSurface(Api& api, platform::Window& w) : gl(api), win(dynamic_cast<platform::sdl::SdlWindow&>(w)) {}
    platform::Window& window() const override { return win; }
    TextureFormat format() const override { return TextureFormat::RGBA8; }
    void waitUntilReady() override {}
    std::optional<PixelSize> prepare(bool withDepth) {
        const auto size = win.pixelSize();
        if (size.empty() || SDL_GetWindowFlags(win.handle()) & SDL_WINDOW_MINIMIZED) return std::nullopt;
        if (!color || color->size() != size) {
            color = std::make_unique<GlTexture>(gl, TextureDesc{size, format(), TextureUsage::RenderTarget});
            depth.reset();
        }
        if (withDepth && !depth)
            depth = std::make_unique<GlTexture>(gl, TextureDesc{size, TextureFormat::Depth32Float, TextureUsage::DepthTarget});
        return size;
    }
};


struct Commands final : CommandList {
    Api& gl;
    Context& context;
    GLuint read{}, write{}, vao{};
    std::array<GLuint, stageSlots * 2> uniforms{};
    const GlPipeline* pipeline{};
    GLuint vertex{};
    bool layoutDirty = true;
    std::vector<GLuint> enabledAttributes;
    size_t vertexOffset{}, indexOffset{};
    IndexType indexType{};
    bool submitted = false;
    std::vector<GlSurface*> windows;
    Commands(Api& api, Context& ctx) : gl(api), context(ctx) {
        gl.GenFramebuffers(1, &read);
        gl.GenFramebuffers(1, &write);
        gl.GenVertexArrays(1, &vao);
        gl.BindVertexArray(vao);
    }
    ~Commands() override {
        if (!submitted) {
            try { submit(); } catch (const std::exception& e) { SDL_Log("OpenGL submit: %s", e.what()); }
        }
        gl.DeleteBuffers(static_cast<GLsizei>(uniforms.size()), uniforms.data());
        gl.DeleteVertexArrays(1, &vao);
        gl.DeleteFramebuffers(1, &read);
        gl.DeleteFramebuffers(1, &write);
    }
    void upload(Buffer& b, size_t offset, std::span<const std::byte> data, bool discard) override {
        gl.BindBuffer(GL_ARRAY_BUFFER, static_cast<GlBuffer&>(b).id);
        (void)discard;
        gl.BufferSubData(GL_ARRAY_BUFFER, offset, data.size(), data.data());
    }
    void upload(Texture& t, PixelRect rect, std::span<const std::byte> data) override {
        const auto f = formatOf(t.format());
        gl.BindTexture(GL_TEXTURE_2D, static_cast<GlTexture&>(t).id);
        gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl.TexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width, rect.height, f.layout, f.type, data.data());
    }
    void attach(GLenum target, GLuint fbo, const Texture& color, Texture* depth = nullptr) {
        gl.BindFramebuffer(target, fbo);
        gl.FramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, static_cast<const GlTexture&>(color).id, 0);
        gl.FramebufferTexture2D(target, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth ? static_cast<GlTexture*>(depth)->id : 0, 0);
        if (gl.CheckFramebufferStatus(target) != GL_FRAMEBUFFER_COMPLETE) throw Error("OpenGL incomplete framebuffer");
    }
    std::optional<PixelSize> acquire(GlSurface& s, bool depth) {
        auto size = s.prepare(depth);
        if (size && std::find(windows.begin(), windows.end(), &s) == windows.end()) windows.push_back(&s);
        return size;
    }
    std::optional<PixelSize> acquireSurface(Surface& surface) override { return acquire(static_cast<GlSurface&>(surface), false); }
    std::optional<PixelSize> beginSurfacePass(Surface& surface, Color clear, bool depth) override {
        auto& s = static_cast<GlSurface&>(surface);
        const auto size = acquire(s, depth);
        if (size) beginTexturePass(*s.color, clear, depth ? s.depth.get() : nullptr, true);
        return size;
    }
    void beginTexturePass(Texture& tex, std::optional<Color> clear, Texture* depth, bool clearDepth) override {
        attach(GL_FRAMEBUFFER, write, tex, depth);
        gl.Enable(GL_FRAMEBUFFER_SRGB);
        gl.Disable(GL_SCISSOR_TEST);
        setViewport({0, 0, tex.size().width, tex.size().height});
        GLbitfield mask = 0;
        if (clear) {
            gl.ClearColor(clear->r, clear->g, clear->b, clear->a);
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (depth && clearDepth) {
            gl.DepthMask(GL_TRUE);
            gl.ClearDepth(1);
            mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (mask) gl.Clear(mask);
    }
    void endPass() override { gl.BindFramebuffer(GL_FRAMEBUFFER, 0); }
    void blit(const Texture& src, Texture& dst) override {
        attach(GL_READ_FRAMEBUFFER, read, src);
        attach(GL_DRAW_FRAMEBUFFER, write, dst);
        gl.Disable(GL_SCISSOR_TEST);
        gl.BlitFramebuffer(0, 0, src.size().width, src.size().height, 0, 0, dst.size().width, dst.size().height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    void blitToSurface(const Texture& src, Surface& dst) override { blit(src, *static_cast<GlSurface&>(dst).color); }
    void submit() override {
        if (submitted) return;
        submitted = true;
        for (auto* s : windows) {
            context.bind(s->win.handle());
            attach(GL_READ_FRAMEBUFFER, read, *s->color);
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            gl.Disable(GL_SCISSOR_TEST);
            gl.Disable(GL_FRAMEBUFFER_SRGB);
            const auto size = s->color->size();
            // Offscreen row zero is the RHI's top row. The native back buffer has its origin at the bottom.
            gl.BlitFramebuffer(0, 0, size.width, size.height, 0, size.height, size.width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            if (!SDL_GL_SwapWindow(s->win.handle())) throw Error(SDL_GetError());
        }
        context.bind(context.hidden);
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void setPipeline(const Pipeline& p) override {
        if (pipeline != &p) layoutDirty = true;
        pipeline = &static_cast<const GlPipeline&>(p);
        const auto& d = pipeline->desc;
        gl.UseProgram(pipeline->id);
        if (d.blend == BlendMode::None) gl.Disable(GL_BLEND);
        else {
            gl.Enable(GL_BLEND);
            gl.BlendFunc(GL_ONE, d.blend == BlendMode::Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        }
        if (d.depthTest || d.depthWrite) gl.Enable(GL_DEPTH_TEST); else gl.Disable(GL_DEPTH_TEST);
        gl.DepthFunc(d.depthTest ? GL_LEQUAL : GL_ALWAYS);
        gl.DepthMask(d.depthWrite ? GL_TRUE : GL_FALSE);
    }
    void setVertexBuffer(const Buffer& b, size_t offset) override {
        const GLuint id = static_cast<const GlBuffer&>(b).id;
        if (vertex != id || vertexOffset != offset) layoutDirty = true;
        vertex = id;
        vertexOffset = offset;
    }
    void setIndexBuffer(const Buffer& b, IndexType type, size_t offset) override {
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<const GlBuffer&>(b).id);
        indexType = type;
        indexOffset = offset;
    }
    void setTexture(uint32_t slot, const Texture& t, const Sampler& sampler) override {
        gl.ActiveTexture(GL_TEXTURE0 + slot);
        gl.BindTexture(GL_TEXTURE_2D, static_cast<const GlTexture&>(t).id);
        gl.BindSampler(slot, static_cast<const GlSampler&>(sampler).id);
    }
    void setUniform(unsigned binding, std::span<const std::byte> data) {
        auto& id = uniforms.at(binding);
        if (!id) gl.GenBuffers(1, &id);
        gl.BindBuffer(GL_UNIFORM_BUFFER, id);
        gl.BufferData(GL_UNIFORM_BUFFER, data.size(), data.data(), GL_STREAM_DRAW);
        gl.BindBufferBase(GL_UNIFORM_BUFFER, binding, id);
    }
    void setVertexUniforms(uint32_t slot, std::span<const std::byte> data) override { setUniform(slot, data); }
    void setFragmentUniforms(uint32_t slot, std::span<const std::byte> data) override { setUniform(stageSlots + slot, data); }
    void setViewport(PixelRect r) override { gl.Viewport(r.x, r.y, r.width, r.height); }
    void setScissor(PixelRect r) override { gl.Enable(GL_SCISSOR_TEST); gl.Scissor(r.x, r.y, r.width, r.height); }
    GLenum layout() {
        constexpr GLenum modes[]{GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_LINES, GL_LINE_STRIP, GL_POINTS};
        const GLenum mode = modes[static_cast<unsigned>(pipeline->desc.topology)];
        if (!layoutDirty) return mode;
        for (GLuint location : enabledAttributes) gl.DisableVertexAttribArray(location);
        enabledAttributes.clear();
        layoutDirty = false;
        gl.BindBuffer(GL_ARRAY_BUFFER, vertex);
        for (const auto& a : pipeline->attributes) {
            const bool packed = a.format == VertexFormat::UByte4Norm;
            const GLint count = packed ? 4 : static_cast<GLint>(a.format) + 1;
            gl.EnableVertexAttribArray(a.location);
            enabledAttributes.push_back(a.location);
            gl.VertexAttribPointer(a.location, count, packed ? GL_UNSIGNED_BYTE : GL_FLOAT, packed, pipeline->desc.vertexStride,
                                   reinterpret_cast<const void*>(vertexOffset + a.offset));
        }
        return mode;
    }
    void draw(uint32_t count, uint32_t first) override { gl.DrawArrays(layout(), first, count); }
    void drawIndexed(uint32_t count, uint32_t first, int32_t base) override {
        const bool small = indexType == IndexType::UInt16;
        const size_t offset = indexOffset + size_t(first) * (small ? 2 : 4);
        const auto mode = layout();
        if (base == 0)
            gl.DrawElements(mode, count, small ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, reinterpret_cast<const void*>(offset));
        else
            gl.DrawElementsBaseVertex(mode, count, small ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, reinterpret_cast<const void*>(offset), base);
    }
};

struct GlDevice final : Device {
    platform::sdl::SdlPlatform& platform;
    Context context;
    Api gl;
    explicit GlDevice(platform::sdl::SdlPlatform& p) : platform(p) { platform.openGlWindows = true; }
    ~GlDevice() override { platform.openGlWindows = false; }
    std::string_view backendName() const override { return "opengl"; }
    std::unique_ptr<Surface> createSurface(platform::Window& w) override { return std::make_unique<GlSurface>(gl, w); }
    std::unique_ptr<Buffer> createBuffer(const BufferDesc& d) override { return std::make_unique<GlBuffer>(gl, d); }
    std::unique_ptr<Texture> createTexture(const TextureDesc& d) override { return std::make_unique<GlTexture>(gl, d); }
    std::unique_ptr<Sampler> createSampler(const SamplerDesc& d) override { return std::make_unique<GlSampler>(gl, d); }
    std::unique_ptr<Shader> createShader(const ShaderDesc& d) override { return std::make_unique<GlShader>(gl, d); }
    std::unique_ptr<Pipeline> createPipeline(const PipelineDesc& d) override { return std::make_unique<GlPipeline>(gl, d); }
    std::unique_ptr<CommandList> beginCommands() override { return std::make_unique<Commands>(gl, context); }
    void waitIdle() override { gl.Finish(); }
    std::vector<std::byte> readPixels(const Texture& t) override {
        const auto f = formatOf(t.format());
        std::vector<std::byte> out(size_t(t.size().width) * t.size().height * bytesPerPixel(t.format()));
        gl.BindTexture(GL_TEXTURE_2D, static_cast<const GlTexture&>(t).id);
        gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
        gl.GetTexImage(GL_TEXTURE_2D, 0, f.layout, f.type, out.data());
        return out;
    }
};
} // namespace

std::unique_ptr<Device> create(platform::Platform& platform) {
    return std::make_unique<GlDevice>(dynamic_cast<platform::sdl::SdlPlatform&>(platform));
}
} // namespace glw::rhi::legacy
