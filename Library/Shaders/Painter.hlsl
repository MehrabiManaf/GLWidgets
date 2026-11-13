// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The shader of draw lists (GLWidgets/Paint/DrawList.hpp): everything a Painter records is drawn by this one pair, with
// premultiplied alpha blending. An engine drawing GLWidgets' draw lists itself uses it as it is.
//
// Six kinds of vertices, by shape.z:
//   0  textured: the color times the texture at uv (a white texel for tessellated lines and polygons, an image). The
//      anti-aliasing of lines and polygons is in their geometry (a fringe whose color fades to 0).
//   1  box: a rounded rectangle as a signed distance field. uv is the point relative to the box's center and halfSize its
//      half size, in dips; shape.x is the corner radius, shape.y the border width (0: filled), shape.w the blur (0: a
//      one-pixel anti-aliased edge).
//   2  mask: the color times the red channel of the texture at uv, a coverage (the glyph atlas, one channel).
//   5  field: a glyph's multi-channel signed distance field (the glyph atlas's fields): the median of its red, green and
//      blue at uv is 0.5 on the outline; halfSize is how far its distances reach in uv's units, so that its edge is one
//      pixel wide on the screen at any size.
//   3  relief: a rounded rectangle as box is, raised shape.w dips above what it is on (below it when negative: sunk),
//      its edge rounding down to it over shape.y dips (a quarter circle's profile), lit by the light of Lighting: its
//      normal from the slope of that height, a diffuse and a specular term, scaled so that its flat top shows its color
//      as it is; sunk, the shadow its rim casts on its floor, away from the light. A metal's highlight is of its color,
//      and it is brushed: fine lines along it. One anti-aliased pixel at its edge.
//   6  relief of glass: a box relief (as 3) whose surface shows what was drawn under it (the texture: a copy of the
//      target, Renderer::backdropCaptures, read where the pixel is on the target, finish.zw its size in pixels), bent
//      toward its edge as a lens (along its slope, within its bevel and a little more), its colors parting a little
//      there; tinted by its color a little; a thin bright rim at its edge, a highlight where its slope turns toward the
//      light.
//      Open at its bottom (kind 6.125, glossy too 6.375): a tab joined to its stack, its lower part (10 dips and its
//      corners' radius) over the stack's top: no edge there nor along its sides there, one piece of glass with the stack.
//      Glossy (kind 6.25): a sheen over its upper part, as a glass button's, and its lit edges glowing out of it a
//      little (into the quad around it, added to what is under it), brightest toward the light.
//   7  backdrop: made as it is drawn, for glass to show (Backdrop): over the color, soft glowing pools of color and faint
//      sheens of light across, repeating every halfSize.x dips; uv the point on the target, halfSize.y how vivid,
//      shape.x the hue the pools' are around.
//   8  relief of glass of a sector of a ring: kind 4's shape, kind 6's glass (glossy: 8.25, its sheen along its outer
//      arc).
//   4  relief of a sector of a ring (a slice of a pie, of a donut), raised as 3 is: uv is the point in the sector's own
//      frame (its center at the origin, its middle along +x), halfSize its inner and outer radii; shape.x its half angle
//      and how far its sides are moved in, packed (half angle + 8 * sixteenths of a dip; negative: a whole ring); shape.y
//      its bevel and the radius of its corners, packed (bevel + 256 * eighths of a dip).
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
    float4 finish;      // x: how metal it is (its highlight of its color, not white); y: how brushed (lines along it);
                        // zw: the target's size in pixels (glass reads its copy of it)
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

// Distance from p to a sector of a ring around the origin, its middle along +x: from the radius radii.x to radii.y, its
// half angle and how far its sides are moved in packed in `sides` (see kind 4; negative: a whole ring, no sides), its
// corners (where a side meets an arc) rounded over `corner` dips: no more than half as far as across it there.
// `g`: the direction in which it grows at p.
float ringSector(float2 p, float2 radii, float sides, float corner, out float2 g) {
    float len = length(p);
    float2 radial = len > 1e-4 ? p / len : float2(1.0, 0.0);
    // its arcs: out of the outer one, into the hole
    float outer = len - radii.y;
    float within = radii.x - len;
    float arcs = max(outer, within);
    float2 arcsOut = outer > within ? radial : -radial;
    // its sides' outward normals; a sector up to a half turn is inside both, a wider one inside either
    bool ring = sides < 0.0;
    float steps = floor(max(sides, 0.0) / 8.0);
    float angle = max(sides, 0.0) - steps * 8.0;
    float inset = steps / 16.0;
    float2 up = float2(-sin(angle), cos(angle));
    float2 down = float2(up.x, -up.y);
    float d1 = dot(p, up) + inset;
    float d2 = dot(p, down) + inset;
    bool narrow = angle <= 1.5707963;
    float side = ring ? -1e6 : (narrow ? max(d1, d2) : min(d1, d2));
    float2 sideOut = (narrow ? d1 > d2 : d1 < d2) ? up : down;
    // the corners: as a rounded box's, the arcs and a side across each other there
    float r = ring ? 0.0 : min(corner, 0.5 * (radii.y - radii.x));
    r = max(0.0, narrow ? min(r, len * sin(angle) - inset) : r);
    float2 q = float2(arcs, side) + r;
    float2 beyond = max(q, 0.0);
    float outside = length(beyond);
    g = outside > 1e-5 ? normalize(arcsOut * beyond.x + sideOut * beyond.y) : (q.x > q.y ? arcsOut : sideOut);
    return outside + min(max(q.x, q.y), 0.0) - r;
}

// A relief's color (premultiplied `color`) at p, `d` inside its edge, its edge's outward direction `g` there: lit by its
// slope. `hidden`: how much of the light its rim hides there (sunk); `across`: where p is across its brushed lines (a
// box's, along it: its height; a ring's, around it: its radius).
float3 relief(float2 p, float2 g, float bevel, float height, float d, float hidden, float across, float4 color) {
    // its height: up from its edge over `bevel` (a quarter circle), flat beyond; its normal from its slope
    float t = saturate(-d / bevel);
    float angle = t * 1.5707963;
    float slope = height * 1.5707963 * cos(angle) / bevel;
    float3 n = normalize(float3(slope * g, 1.0));
    float3 toward = normalize(light.xyz);
    // diffuse, its flat top as its color is; specular, a highlight where it turns toward the light (none on its top)
    float top = light.w + material.x * toward.z;
    float shade = (light.w + material.x * saturate(dot(n, toward))) / max(top, 1e-3);
    float3 halfway = normalize(toward + float3(0.0, 0.0, 1.0));
    float shine = material.y * max(0.0, pow(saturate(dot(n, halfway)), material.z) - pow(saturate(halfway.z), material.z));
    shade *= 1.0 - material.w * hidden;
    // brushed: each line half a dip wide a little lighter or darker than the next (a ring's turned: around it)
    float grain = frac(sin(floor(across * 2.0 + 0.5) * 12.9898) * 43758.5453) - 0.5;
    shade *= 1.0 + finish.y * grain;
    // a metal's highlight of its own color (straight, from premultiplied), a plastic's white
    float3 straight = color.rgb / max(color.a, 1e-3);
    float3 highlight = lerp(float3(1.0, 1.0, 1.0), straight, finish.x);
    return color.rgb * shade + shine * highlight * color.a;
}

// A color of hue h (0 to 1 around the wheel), saturation s, value v.
float3 hsv(float h, float s, float v) {
    float3 k = saturate(abs(frac(h + float3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0);
    return v * lerp(float3(1.0, 1.0, 1.0), k, s);
}

// Kind 7's pattern at p (dips), repeating every `tile`: pools of color (where each is in the tile, how wide, its hue from
// `hue`) glowing over `base`, as dark or light as it is; and faint sheens across (their ways whole turns in a tile, so that
// they repeat with it).
float3 backdrop(float2 p, float tile, float vivid, float hue, float3 base) {
    static const float4 pools[6] = {float4(0.18, 0.22, 0.30, 0.00), float4(0.70, 0.12, 0.26, 0.10), float4(0.52, 0.58, 0.34, -0.08),
                                    float4(0.10, 0.78, 0.24, 0.16), float4(0.86, 0.70, 0.22, -0.14), float4(0.40, 0.92, 0.18, 0.05)};
    float2 q = p / tile;
    float dark = 1.0 - saturate(dot(base, float3(0.2126, 0.7152, 0.0722)) * 1.6);
    float3 c = base;
    [unroll] for (int k = 0; k < 6; ++k) {
        float2 d = q - pools[k].xy;
        d -= round(d);
        float r = length(d) / pools[k].z;
        float glow = exp(-r * r * 2.0);
        float3 color = hsv(hue + pools[k].w, lerp(0.45, 0.75, dark), lerp(0.95, 0.85, dark));
        c = lerp(c, color, glow * lerp(0.32, 0.42, dark) * vivid);
    }
    // sheens: soft bright bands along two ways across
    float a = 0.5 + 0.5 * cos(6.2831853 * dot(q, float2(2.0, -1.0)));
    float b = 0.5 + 0.5 * cos(6.2831853 * dot(q, float2(1.0, 3.0)) + 1.3);
    c += (pow(a, 18.0) * 0.06 + pow(b, 24.0) * 0.035) * vivid * float3(1.0, 1.0, 1.0);
    return saturate(c);
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

    // a sector's bevel and corners, packed (kind 4)
    bool sector = (input.shape.z > 3.5 && input.shape.z < 4.5) || input.shape.z > 7.5;
    float corners = sector ? floor(input.shape.y / 256.0) : 0.0;
    float shapeY = input.shape.y - corners * 256.0;
    float2 arcSlope;
    float arc = ringSector(input.uv, input.halfSize, input.shape.x, corners / 8.0, arcSlope);
    float d = sector ? arc : roundedBox(input.uv, input.halfSize, input.shape.x);
    bool lit = input.shape.z > 2.5;
    float edge = max(fwidth(d) * 0.5, lit ? 0.0 : input.shape.w);
    float inside = coverage(d, edge);
    float hole = !lit && input.shape.y > 0.0 ? coverage(d + input.shape.y, edge) : 0.0;
    float4 box = input.color * (inside * (1.0 - hole));
    // sunk (a box): what its rim hides of the light (the floor and the walls toward it), as deep as it is there: where a
    // ray toward the light leaves it
    float bevel = max(shapeY, 0.5);
    float depth = input.shape.w < 0.0 ? -input.shape.w * sin(saturate(-d / bevel) * 1.5707963) : 0.0;
    float3 toward = normalize(light.xyz);
    float2 exit = input.uv + toward.xy / max(toward.z, 0.2) * depth;
    float hidden = depth > 0.0 && !sector ? saturate(roundedBox(exit, input.halfSize, input.shape.x) / 1.5 + 0.5) : 0.0;
    float2 slope = sector ? arcSlope : roundedBoxGradient(input.uv, input.halfSize, input.shape.x);
    float3 shaded = relief(input.uv, slope, bevel, input.shape.w, d, hidden, sector ? length(input.uv) : input.uv.y, input.color);
    float4 raised = float4(shaded, input.color.a) * inside;

    // a distance field: its edge a pixel wide however large it shows
    float distance = max(min(texel.r, texel.g), min(max(texel.r, texel.g), texel.b)) - 0.5;
    float2 perPixel = fwidth(input.uv);

    if (input.shape.z > 6.5 && input.shape.z < 7.5) {
        float3 made = backdrop(input.uv, input.halfSize.x, input.halfSize.y, input.shape.x, input.color.rgb / max(input.color.a, 1e-3));
        return float4(made * input.color.a, input.color.a);
    }
    if (input.shape.z > 5.5) {
        // glass: what is under it, read where the pixel is (bent toward its edge, a lens), barely softened
        // (a sector's: across its ring)
        float2 out2 = sector ? arcSlope : roundedBoxGradient(input.uv, input.halfSize, input.shape.x);
        // glossy (a quarter in its kind), and open at its bottom (an eighth: a tab joined to its stack, its lower part
        // `joinDepth` and its corners' radius over the stack's top): no edge there, nor along its sides over the stack (one
        // piece of glass with it), its sheen as the stack's top's
        float kindPart = frac(input.shape.z);
        bool glossy = kindPart > 0.2;
        bool open = !sector && ((kindPart > 0.1 && kindPart < 0.2) || kindPart > 0.3);
        const float joinDepth = 10.0;
        float joinLine = input.halfSize.y - joinDepth - input.shape.x;
        float far = 64.0;
        float edged = !open ? d : input.uv.y > joinLine ? -100.0
                                : roundedBox(input.uv - float2(0.0, far), input.halfSize + float2(0.0, far), input.shape.x);
        float across = sector ? 0.5 * (input.halfSize.y - input.halfSize.x) : min(input.halfSize.x, input.halfSize.y);
        float zone = bevel + min(6.0, 0.15 * across);
        float bend = 1.0 - smoothstep(0.0, zone, max(-edged, 0.0));
        bend *= bend;
        float2 dipsPerPixel = max(perPixel, 1e-4);
        float2 shift = out2 * bend * min(zone * 0.5, 3.0) / dipsPerPixel;             // pixels, outward
        float2 target = max(finish.zw, 1.0);
        float3 behind = float3(0.0, 0.0, 0.0);
        [unroll] for (int k = 0; k < 6; ++k) {
            float a = k * 1.0471976 + 0.5;
            float2 at = input.position.xy + float2(cos(a), sin(a)) * 1.2;
            // red bent the least, blue the most: a faint fringe where it bends
            behind.r += image.SampleLevel(imageSampler, (at + shift * 0.97) / target, 0).r;
            behind.g += image.SampleLevel(imageSampler, (at + shift) / target, 0).g;
            behind.b += image.SampleLevel(imageSampler, (at + shift * 1.04) / target, 0).b;
        }
        behind /= 6.0;
        float3 straight = input.color.rgb / max(input.color.a, 1e-3);
        // tinted by its color a little; much more when it is a color (a chart's bar, a checked box), so that it reads
        float most = max(straight.r, max(straight.g, straight.b)), least = min(straight.r, min(straight.g, straight.b));
        float colored = smoothstep(0.15, 0.6, (most - least) / max(most, 1e-3));
        float3 pane = lerp(behind * 0.97 + 0.03, straight, lerp(0.07, 0.62, colored));
        // its edge: a thin bright line, a little brighter toward the light; a highlight where its slope turns toward it
        float facing = 0.78 + 0.22 * saturate(dot(out2, -normalize(toward.xy + 1e-5)) * 0.5 + 0.5);
        float rim = exp(-max(-edged, 0.0) / 1.0) * 0.22 * facing;
        float t = saturate(-edged / bevel);
        float3 n = normalize(float3(input.shape.w * 1.5707963 * cos(t * 1.5707963) / bevel * out2, 1.0));
        float3 halfway = normalize(toward + float3(0.0, 0.0, 1.0));
        float shine = max(0.0, pow(saturate(dot(n, halfway)), 60.0) - pow(saturate(halfway.z), 60.0)) * 0.8;
        float shade = bend * 0.035 * (1.0 - facing) / 0.22;
        // glossy: a sheen over its upper part (fading down to its middle, inside its rim), and its lit edges glowing out
        // of it, added to what is under it
        // (a sector's: in from its outer arc; open at its bottom: all of it as bright as its stack's top)
        float down = sector ? saturate((input.halfSize.y - length(input.uv)) / max(input.halfSize.y - input.halfSize.x, 1.0))
                     : open ? 0.0
                            : saturate((input.uv.y + input.halfSize.y) / max(2.0 * input.halfSize.y, 1.0));
        float sheen = glossy ? (1.0 - smoothstep(0.0, 0.55, down)) * smoothstep(0.0, 2.5, -edged) * 0.16 : 0.0;
        float3 glass = saturate(pane * (1.0 - shade) + rim * (glossy ? 1.15 : 1.0) + shine * (glossy ? 1.4 : 1.0) + sheen);
        float alpha = input.color.a * inside;
        float lightSide = saturate(dot(out2, -normalize(toward.xy + 1e-5)) * 0.6 + 0.4);
        float glow = glossy && !(open && input.uv.y > joinLine) ? exp(-max(d, 0.0) / 3.2) * (1.0 - inside) * 0.11 * lightSide * input.color.a : 0.0;
        float3 glowColor = lerp(float3(1.0, 1.0, 1.0), straight, 0.35);
        return float4(glass * alpha + glowColor * glow, alpha);
    }
    float onScreen = max(0.5 * dot(input.halfSize, 1.0 / max(perPixel, 1e-6)), 1.0);
    float4 field = input.color * saturate(distance * onScreen + 0.5);

    return input.shape.z > 4.5 ? field : lit ? raised : input.shape.z > 1.5 ? mask : input.shape.z > 0.5 ? box : textured;
}
