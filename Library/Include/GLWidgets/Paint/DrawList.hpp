// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What a Painter records and a Renderer draws: vertices, 32-bit indices of triangle lists, and commands that refer to
// textures by id. No GPU object, so anything with the shader of Library/Shaders/Painter.hlsl and one pipeline (the vertex
// layout below, premultiplied alpha blending) can draw it: GLWidgets' Renderer, or an engine's own renderer.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace glw::paint {

// A texture as a draw list knows it: the renderer maps ids to textures. 0 is a white texel.
using TextureId = std::uint64_t;
inline constexpr TextureId whiteTexture = 0;

enum class VertexKind : std::uint8_t { Textured = 0, Box = 1 };

// What the fields mean for each kind is in Painter.hlsl. The layout is the shader's input: TEXCOORD0 position, 1 uv,
// 2 halfSize, 3 (radius, border, kind, blur) as one float4, 4 color as four normalized bytes.
struct Vertex {
    Point position;                 // dips of the target
    Point uv;
    Point halfSize;
    float radius = 0;
    float border = 0;
    float kind = 0;                 // a VertexKind
    float blur = 0;
    std::uint32_t color = 0;        // packColor
};
static_assert(sizeof(Vertex) == 44 && offsetof(Vertex, radius) == 24 && offsetof(Vertex, color) == 40);

struct DrawCommand {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    Rect clip;                      // dips of the target
    TextureId texture = whiteTexture;
};

struct DrawList {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<DrawCommand> commands;

    void clear() {
        vertices.clear();
        indices.clear();
        commands.clear();
    }
    bool empty() const { return indices.empty(); }
};

// A color as a vertex has it: RGBA8, straight alpha, red in the lowest byte.
inline std::uint32_t packColor(const Color& c) {
    auto channel = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return channel(c.r) | channel(c.g) << 8 | channel(c.b) << 16 | channel(c.a) << 24;
}

} // namespace glw::paint
