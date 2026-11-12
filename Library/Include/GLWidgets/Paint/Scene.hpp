// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Solid things over the target: bricks (rounded boxes of three dimensions: a button, a table's cell, a key) that a
// Painter draws (drawBrick) as they are in space, moved and turned at will, under a camera above the target, lit by an
// environment of its own (Library/Shaders/Scene.hlsl: no image, no mesh: a sky, a floor, soft lights), its edges
// anti-aliased and its corners round at any size.
//
// Space: x to the right and y down, as the target's dips; z out of the screen, toward the one looking at it (the target
// at z = 0, what stands on it above). The camera looks down at the target from `eye.z` dips above it, so that what lies
// at z = 0 shows where its dips are, and what stands higher looks a little nearer and larger.
//
// A brick is drawn as one quad around where it shows: the fragment shader finds where a ray from the camera meets it (a
// distance field, a few steps from where the ray enters its bounds), its normal there, and how much of the pixel it covers
// from how near the ray passes its edge. Bricks are drawn in the order they come (as everything a Painter draws), so that
// those nearer the camera come last: they do not cut into one another, unless the painter tests them by depth
// (Painter::setDepthTest: a pile of them that may pass through one another).
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace glw::paint {

struct Float3 {
    float x = 0;
    float y = 0;
    float z = 0;

    friend Float3 operator+(Float3 a, Float3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Float3 operator-(Float3 a, Float3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Float3 operator*(Float3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
    friend bool operator==(const Float3&, const Float3&) = default;
};

inline float dot(Float3 a, Float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Float3 cross(Float3 a, Float3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Float3 a) {
    return std::sqrt(dot(a, a));
}

// A turn in space (a unit quaternion).
struct Quaternion {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;

    // `angle` radians about `axis` (any length but 0).
    static Quaternion axisAngle(Float3 axis, float angle) {
        const float n = length(axis);
        if (n <= 0)
            return {};
        const float s = std::sin(angle / 2) / n;
        return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle / 2)};
    }
    // Tipped `aboutX` radians about x (its bottom edge toward the camera when positive), then `aboutY` about y, then
    // turned `aboutZ` about z (clockwise as y goes down).
    static Quaternion tilt(float aboutX, float aboutY, float aboutZ = 0) {
        return axisAngle({0, 0, 1}, aboutZ) * axisAngle({0, 1, 0}, aboutY) * axisAngle({1, 0, 0}, aboutX);
    }
    // `b` first, then `a`.
    friend Quaternion operator*(Quaternion a, Quaternion b) {
        return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    }
    Float3 rotate(Float3 v) const {
        const Float3 q{x, y, z};
        const Float3 t = cross(q, v) * 2;
        return v + t * w + cross(q, t);
    }
    Quaternion inverse() const { return {-x, -y, -z, w}; }
    friend bool operator==(const Quaternion&, const Quaternion&) = default;
};

// A solid rounded box: where it is (its middle), how large (half its sides), how it is turned, how round (its corners as
// seen from above, its edges all around), of what (a color, how rough, how metal), and how dark a shadow it casts on the
// target (cast from the scene's light).
struct Brick {
    Float3 center;
    Float3 halfSize{20, 10, 4};
    Quaternion rotation;
    float cornerRadius = 4;         // of its corners as seen from above (at least its bevel)
    float bevel = 2;                // of all its edges (at most its half height)
    Color color{0.5f, 0.5f, 0.5f, 1};
    float roughness = 0.45f;        // 0: a mirror's; 1: chalk's
    float metal = 0;                // its reflections of its own color (1), or white (0: a plastic's, a paint's)
    float shadow = 0.35f;           // of the shadow under it; 0: none
    bool reliefLight = false;       // lit as the 2D reliefs are (Lighting), not by the environment: a style's controls
    // Of glass: what is under it shows through it, blurred (bent a little at its edges, as a thick pane bends what is
    // behind it), tinted by its color a little; its edges catching the light. Lit as reliefLight says otherwise.
    bool glass = false;
};

// Raised reliefs drawn as bricks instead (Painter::setReliefBricks): their tops where the reliefs are (at the target,
// z = 0), their bodies down into what they are on, `depth` times as deep as the reliefs are high (no more than half as
// deep as they are wide), lit as the reliefs are. Sunk reliefs (a field) stay as they are. Of glass (`glass`) those at
// least `glassFrom` high: the controls, not the low slabs they stand on (a stack's, a card's), which stay solid.
struct ReliefBricks {
    float depth = 5;
    float roughness = 0.32f;
    // How round the edges of their tops are, over how many dips (0: their tops flat to a sharp edge, their corners round
    // still as seen from above); negative: the reliefs' own bevels.
    float bevel = -1;
    bool glass = false;
    float glassFrom = 1;
    friend bool operator==(const ReliefBricks&, const ReliefBricks&) = default;
};

// How something drawn is moved in three dimensions: turned about its middle, lifted `lift` dips toward the camera,
// moved `shift` across the target; `alpha` of it showing (Painter::place).
struct Lifted {
    float lift = 0;
    Quaternion turn;
    Point shift;
    float alpha = 1;
};

// The camera: where it is (in the target's dips; z its height above the target).
struct Camera {
    Float3 eye;
};

// What a Painter records of a brick and Scene.hlsl draws: a vertex of the quad around where it shows, and all of the brick
// in each of the four (the shader's input: TEXCOORD0 position, 1 center and corner radius, 2 half size and bevel, 3
// rotation, 4 roughness, metal, kind (0 the brick, 1 a decal on its top face, 2 its shadow on the target), lit as a
// relief (1) or of glass (2), 5 the camera's eye and pixels per dip, 6 a decal's rectangle on the top face (from its
// middle, the brick's frame), 7 the part of the texture shown on it, 8 color). A decal is a glyph's distance field printed on the top face
// (text on a key): it turns and leans with it; its material's first two are how far its distances reach in the
// texture's units.
struct SceneVertex {
    Point position;                 // dips of the target
    float center[3];
    float cornerRadius;
    float halfSize[3];
    float bevel;
    float rotation[4];
    float material[4];
    float eye[3];
    float pixelsPerDip;
    float decal[4];                 // left, top, right, bottom on the top face
    float decalUv[4];               // left, top, right, bottom in the texture
    std::uint32_t color;            // packColor
};
static_assert(sizeof(SceneVertex) == 124 && offsetof(SceneVertex, color) == 120);

// The kinds of scene vertices (SceneVertex::material[2]).
inline constexpr float brickKind = 0;
inline constexpr float decalKind = 1;
inline constexpr float shadowKind = 2;

} // namespace glw::paint
