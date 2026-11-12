// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What a Painter records and a Renderer draws: vertices, 32-bit indices of triangle lists, and commands that refer to
// textures by id. No GPU object, so anything with the shader of Library/Shaders/Painter.hlsl and one pipeline (the vertex
// layout below, premultiplied alpha blending) can draw it: GLWidgets' Renderer, or an engine's own renderer.
//
// Bricks (Scene.hpp) are commands of their own (DrawCommand::scene): their indices are of the scene's vertices, drawn
// with Library/Shaders/Scene.hlsl and its layout. A list without them needs the first shader only.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Paint/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace glw::paint {

// A texture as a draw list knows it: the renderer maps ids to textures. 0 is a white texel.
using TextureId = std::uint64_t;
inline constexpr TextureId whiteTexture = 0;
// What was drawn under a glass brick (Brick::glass), blurred: no texture of its own, the renderer's copy of the target as
// it is when the brick comes (Renderer::backdropCaptures).
inline constexpr TextureId backdropTexture = ~TextureId{0};
// ... the same as the glass it is joined onto (drawn just before it) reads: no new copy for it.
inline constexpr TextureId joinedBackdropTexture = ~TextureId{1};

enum class VertexKind : std::uint8_t { Textured = 0, Box = 1, Mask = 2, Relief = 3, RingRelief = 4, Field = 5, GlassRelief = 6 };

// Raised reliefs of glass (Painter::setGlassReliefs): those at least `from` high show what was drawn under them through
// their surface, bent along their sloped edges as a lens (Painter.hlsl's kind 6).
struct GlassReliefs {
    float from = 0.5f;
    friend bool operator==(const GlassReliefs&, const GlassReliefs&) = default;
};

// The light reliefs are lit by (Painter::fillRelief): the Renderer's (setLighting), the fragment shader's uniforms
// (Painter.hlsl's Lighting). One for all of a target.
struct Lighting {
    float x = -0.42f;               // toward the light: from the left, above (y down the screen), in front (out of it)
    float y = -0.62f;
    float z = 0.66f;
    float ambient = 0.60f;          // what lights it from everywhere
    float diffuse = 0.40f;          // ... from the light, as a surface faces it
    float specular = 0.16f;         // a highlight where it turns toward the light
    float shininess = 18;           // ... how tight
    float rimShadow = 0.40f;        // a sunk relief's floor, where its rim hides the light: this much darker
    float metal = 0;                // how metal its surfaces are: their highlights of their own color (0: white, a plastic's)
    float brushed = 0;              // ... how brushed: fine lines along them, this much lighter and darker
};
static_assert(sizeof(Lighting) == 40);

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
    bool scene = false;             // of bricks: its indices are of sceneVertices, drawn with Scene.hlsl
};

struct DrawList {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<DrawCommand> commands;
    std::vector<SceneVertex> sceneVertices;
    bool depth = false;             // its bricks tested against one another by depth (Painter::setDepthTest)

    void clear() {
        vertices.clear();
        indices.clear();
        commands.clear();
        sceneVertices.clear();
        depth = false;
    }
    bool empty() const { return indices.empty(); }
};

// A color as a vertex has it: RGBA8, straight alpha, red in the lowest byte.
inline std::uint32_t packColor(const Color& c) {
    auto channel = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return channel(c.r) | channel(c.g) << 8 | channel(c.b) << 16 | channel(c.a) << 24;
}

} // namespace glw::paint
