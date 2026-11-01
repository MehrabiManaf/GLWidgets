// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The shader of draw lists (GLWidgets/Paint/DrawList.hpp): everything a Painter records is drawn by this one pair, with
// premultiplied alpha blending. An engine drawing GLWidgets' draw lists itself uses it as it is.
//
// Three kinds of vertices, by shape.z:
//   0  textured: the color times the texture at uv (a white texel for tessellated lines and polygons, an image). The
//      anti-aliasing of lines and polygons is in their geometry (a fringe whose color fades to 0).
//   1  box: a rounded rectangle as a signed distance field. uv is the point relative to the box's center and halfSize its
//      half size, in dips; shape.x is the corner radius, shape.y the border width (0: filled), shape.w the blur (0: a
//      one-pixel anti-aliased edge).
//   2  mask: the color times the red channel of the texture at uv, a coverage (the glyph atlas, one channel).
//
// Resources are numbered as SDL's GPU API wants them (SDL_CreateGPUShader): the vertex shader's uniform buffer in space1
// (descriptor set 1 in SPIR-V), the fragment shader's texture and sampler in space2 (set 2, one combined image sampler).
// Vertex inputs are TEXCOORD<location>. Compiled by Tools/CompileShaders.py into Source/Paint/PainterShaders.hpp.

#ifdef __spirv__
#define VK_LOCATION(n) [[vk::location(n)]]
#define VK_BINDING(index, set) [[vk::binding(index, set)]]
#define VK_COMBINED [[vk::combinedImageSampler]]
#else
#define VK_LOCATION(n)
#define VK_BINDING(index, set)
#define VK_COMBINED
#endif

struct VertexInput {
    VK_LOCATION(0) float2 position : TEXCOORD0;     // dips of the target
    VK_LOCATION(1) float2 uv : TEXCOORD1;
    VK_LOCATION(2) float2 halfSize : TEXCOORD2;
    VK_LOCATION(3) float4 shape : TEXCOORD3;        // corner radius, border width, kind, blur
    VK_LOCATION(4) float4 color : TEXCOORD4;        // sRGB, straight alpha
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 halfSize : TEXCOORD1;
    float4 shape : TEXCOORD2;
    float4 color : TEXCOORD3;                       // premultiplied
};

// dips to clip space: position * scale + offset
VK_BINDING(0, 1) cbuffer Transform : register(b0, space1) {
    float2 scale;
    float2 offset;
};

VK_COMBINED VK_BINDING(0, 2) Texture2D<float4> image : register(t0, space2);
VK_COMBINED VK_BINDING(0, 2) SamplerState imageSampler : register(s0, space2);

VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position * scale + offset, 0.0, 1.0);
    output.uv = input.uv;
    output.halfSize = input.halfSize;
    output.shape = input.shape;
    output.color = float4(input.color.rgb * input.color.a, input.color.a);
    return output;
}

// Distance from p to a box of half size b with corners of radius r, centered on the origin: negative inside.
float roundedBox(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// How much of a pixel at distance d is inside, with an edge `width` wide on each side of d = 0.
float coverage(float d, float width) {
    return 1.0 - smoothstep(-width, width, d);
}

float4 fragmentMain(VertexOutput input) : SV_Target {
    // every kind computed, then one chosen: derivatives (fwidth, the texture's) need uniform control flow
    float4 texel = image.Sample(imageSampler, input.uv);
    float4 textured = input.color * texel;
    float4 mask = input.color * texel.r;

    float d = roundedBox(input.uv, input.halfSize, input.shape.x);
    float edge = max(fwidth(d) * 0.5, input.shape.w);
    float inside = coverage(d, edge);
    float hole = input.shape.y > 0.0 ? coverage(d + input.shape.y, edge) : 0.0;
    float4 box = input.color * (inside * (1.0 - hole));

    return input.shape.z > 1.5 ? mask : input.shape.z > 0.5 ? box : textured;
}
