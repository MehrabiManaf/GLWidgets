// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The shader of draw lists (GLWidgets/Paint/DrawList.hpp): everything a Painter records is drawn by this one pair, with
// premultiplied alpha blending. An engine drawing GLWidgets' draw lists itself uses it as it is.
//
// Four kinds of vertices, by shape.z:
//   0  textured: the color times the texture at uv (a white texel for tessellated lines and polygons, an image). The
//      anti-aliasing of lines and polygons is in their geometry (a fringe whose color fades to 0).
//   1  box: a rounded rectangle as a signed distance field. uv is the point relative to the box's center and halfSize its
//      half size, in dips; shape.x is the corner radius, shape.y the border width (0: filled), shape.w the blur (0: a
//      one-pixel anti-aliased edge).
//   2  mask: the color times the red channel of the texture at uv, a coverage (the glyph atlas, one channel).
//   3  relief: a rounded rectangle as box is, raised shape.w dips above what it is on (below it when negative: sunk),
//      its edge rounding down to it over shape.y dips (a quarter circle's profile), lit by the light of Lighting: its
//      normal from the slope of that height, a diffuse and a specular term, scaled so that its flat top shows its color
//      as it is; sunk, the shadow its rim casts on its floor, away from the light. A metal's highlight is of its color,
//      and it is brushed: fine lines along it. One anti-aliased pixel at its edge.
//
// Resources are numbered as SDL's GPU API wants them (SDL_CreateGPUShader): the vertex shader's uniform buffer in space1
// (descriptor set 1 in SPIR-V), the fragment shader's texture and sampler in space2 (set 2, one combined image sampler),
// its uniform buffer (the light) in space3 (set 3).
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

// the light of reliefs (GLWidgets/Paint/DrawList.hpp's Lighting)
VK_BINDING(0, 3) cbuffer Lighting : register(b0, space3) {
    float4 light;       // xyz: toward the light (x right, y down the screen, z out of it); w: the ambient light
    float4 material;    // x: diffuse, y: specular, z: shininess, w: how dark a rim's shadow in a sunk relief is
    float4 finish;      // x: how metal it is (its highlight of its color, not white); y: how brushed (lines along it)
};

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

// The direction in which roundedBox grows at p: out of the box, across its nearest edge or out of its corner.
float2 roundedBoxGradient(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    float2 g = q.x > 0.0 && q.y > 0.0 ? normalize(q) : (q.x > q.y ? float2(1.0, 0.0) : float2(0.0, 1.0));
    return g * float2(p.x < 0.0 ? -1.0 : 1.0, p.y < 0.0 ? -1.0 : 1.0);
}

// A relief's color (premultiplied `color`) at p, `d` inside its edge: lit by its slope.
float3 relief(float2 p, float2 b, float r, float bevel, float height, float d, float4 color) {
    // its height: up from its edge over `bevel` (a quarter circle), flat beyond; its normal from its slope
    float t = saturate(-d / bevel);
    float angle = t * 1.5707963;
    float slope = height * 1.5707963 * cos(angle) / bevel;
    float3 n = normalize(float3(slope * roundedBoxGradient(p, b, r), 1.0));
    float3 toward = normalize(light.xyz);
    // diffuse, its flat top as its color is; specular, a highlight where it turns toward the light (none on its top)
    float top = light.w + material.x * toward.z;
    float shade = (light.w + material.x * saturate(dot(n, toward))) / max(top, 1e-3);
    float3 halfway = normalize(toward + float3(0.0, 0.0, 1.0));
    float shine = material.y * max(0.0, pow(saturate(dot(n, halfway)), material.z) - pow(saturate(halfway.z), material.z));
    // sunk: what the rim hides of the light (the floor and the walls toward it), as deep as it is there
    float depth = height < 0.0 ? -height * sin(angle) : 0.0;
    float2 exit = p + toward.xy / max(toward.z, 0.2) * depth;
    float hidden = depth > 0.0 ? saturate(roundedBox(exit, b, r) / 1.5 + 0.5) : 0.0;
    shade *= 1.0 - material.w * hidden;
    // brushed: each line half a dip high a little lighter or darker than the next, along it
    float grain = frac(sin(floor(p.y * 2.0 + 0.5) * 12.9898) * 43758.5453) - 0.5;
    shade *= 1.0 + finish.y * grain;
    // a metal's highlight of its own color (straight, from premultiplied), a plastic's white
    float3 straight = color.rgb / max(color.a, 1e-3);
    float3 highlight = lerp(float3(1.0, 1.0, 1.0), straight, finish.x);
    return color.rgb * shade + shine * highlight * color.a;
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
    bool lit = input.shape.z > 2.5;
    float edge = max(fwidth(d) * 0.5, lit ? 0.0 : input.shape.w);
    float inside = coverage(d, edge);
    float hole = !lit && input.shape.y > 0.0 ? coverage(d + input.shape.y, edge) : 0.0;
    float4 box = input.color * (inside * (1.0 - hole));
    float3 shaded = relief(input.uv, input.halfSize, input.shape.x, max(input.shape.y, 0.5), input.shape.w, d, input.color);
    float4 raised = float4(shaded, input.color.a) * inside;

    return lit ? raised : input.shape.z > 1.5 ? mask : input.shape.z > 0.5 ? box : textured;
}
