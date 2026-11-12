// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Bricks: rounded boxes in space (GLWidgets/Paint/Scene.hpp), each drawn as one quad around where it shows. For each
// pixel a ray from the camera's eye through it is turned into the brick's own frame, enters its bounds, and walks toward
// its surface by the distance to it (a distance field: the brick's shape as seen from above, a rounded rectangle, made
// solid with every edge rounded over its bevel). Where it meets it: its normal from the field's slope, lit; where it
// passes by, as near as it came: the part of the pixel it covers, so that its edges are smooth at any size and any turn.
//
// The light is of an environment with no image: a sky toward the one looking (z), a floor away from it, a soft key light
// above to the upper left (where the 2D styles' light is), a fill from the right; a rough surface sees them wider and
// fainter, a smooth one sharp. A dielectric reflects white, a little, more at grazing angles (Fresnel, Schlick's); a
// metal reflects its own color. Lit in linear light, then back to the display's curve with a soft shoulder (no hard
// clipping of highlights). A brick lit as a relief (material.w 1: a style's controls) is lit by the 2D reliefs' light
// instead (Painter.hlsl's Lighting), so that it looks as the reliefs of the style it stands in for.
//
// A brick of glass (material.w 2) shows what was drawn under it (the texture: the target copied at half its size before
// it, Renderer::backdropCaptures), clear, barely softened; toward its edges bent as a lens bends (what lies a little
// beyond the edge drawn in), its colors parting a little there; tinted by its color a little; a thin bright rim along its
// edge, brighter toward the light; its turned sides darker, a highlight where it turns toward the light. A look, not
// refraction computed.
//
// A shadow (material.z 2) is a brick's on the target (z = 0): a rounded rectangle (center.xy, halfSize.xy, its corner
// radius) blurred over `bevel` dips, as far as the target is (behind the bricks that stand over it). Drawn with the
// bricks, it needs no pipeline of its own.
//
// A decal (material.z 1) is a glyph's multi-channel distance field (the glyph atlas's fields) printed on the brick's top
// face: the ray meets the face's plane (in the brick's frame), where it does is a point of the decal's rectangle, read
// from the texture there (the median of its red, green and blue: 0.5 on the outline; material.xy how far its distances
// reach in the texture's units), sharp however near and however turned, lit as the face is.
//
// Vertex input (TEXCOORD<n>): 0 position (dips of the target), 1 center (xyz) and corner radius, 2 half size (xyz) and
// bevel, 3 rotation (a unit quaternion), 4 roughness, metal, kind, lit as a relief, 5 the eye (xyz) and pixels per dip,
// 6 a decal's rectangle on the top face, 7 its part of the texture, 8 color (sRGB, straight alpha). Resources as
// Painter.hlsl numbers them: the vertex shader's transform in space1, the fragment shader's texture and sampler in space2,
// its light in space3. Compiled by Tools/CompileShaders.py into Source/Paint/SceneShaders.hpp.

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
    VK_LOCATION(0) float2 position : TEXCOORD0;
    VK_LOCATION(1) float4 center : TEXCOORD1;
    VK_LOCATION(2) float4 halfSize : TEXCOORD2;
    VK_LOCATION(3) float4 rotation : TEXCOORD3;
    VK_LOCATION(4) float4 material : TEXCOORD4;
    VK_LOCATION(5) float4 eye : TEXCOORD5;
    VK_LOCATION(6) float4 decal : TEXCOORD6;
    VK_LOCATION(7) float4 decalUv : TEXCOORD7;
    VK_LOCATION(8) float4 color : TEXCOORD8;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 screen : TEXCOORD0;
    nointerpolation float4 center : TEXCOORD1;
    nointerpolation float4 halfSize : TEXCOORD2;
    nointerpolation float4 rotation : TEXCOORD3;
    nointerpolation float4 material : TEXCOORD4;
    nointerpolation float4 eye : TEXCOORD5;
    nointerpolation float4 decal : TEXCOORD6;
    nointerpolation float4 decalUv : TEXCOORD7;
    nointerpolation float4 color : TEXCOORD8;
    float2 backdropUv : TEXCOORD9;              // where the pixel is in the target, as a texture of it reads it
    nointerpolation float2 uvPerDip : TEXCOORD10;
};

// dips to clip space: position * scale + offset (as Painter.hlsl's)
VK_BINDING(0, 1) cbuffer Transform : register(b0, space1) {
    float2 scale;
    float2 offset;
};

VK_COMBINED VK_BINDING(0, 2) Texture2D<float4> image : register(t0, space2);
VK_COMBINED VK_BINDING(0, 2) SamplerState imageSampler : register(s0, space2);

// environment: x how bright the light is (1: as it is), y 1 in a dark theme and 0 in a light one (the floor's and the
// sky's tone). Then the 2D reliefs' light (Painter.hlsl's Lighting), for bricks lit as they are (material.w 1).
VK_BINDING(0, 3) cbuffer Environment : register(b0, space3) {
    float4 environment;
    float4 light;               // xyz: toward the light; w: the ambient light
    float4 reliefMaterial;      // x: diffuse, y: specular, z: shininess, w: -
    float4 finish;              // x: how metal (its highlight of its color)
};

VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position * scale + offset, 0.0, 1.0);
    output.screen = input.position;
    output.center = input.center;
    output.halfSize = input.halfSize;
    output.rotation = input.rotation;
    output.material = input.material;
    output.eye = input.eye;
    output.decal = input.decal;
    output.decalUv = input.decalUv;
    output.color = input.color;
    output.backdropUv = float2(output.position.x * 0.5 + 0.5, 0.5 - output.position.y * 0.5);
    output.uvPerDip = abs(scale) * 0.5;
    return output;
}

float3 rotate(float4 q, float3 v) {
    float3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

float3 unrotate(float4 q, float3 v) {
    return rotate(float4(-q.xyz, q.w), v);
}

// Distance from p (the brick's frame) to the brick: half size h, its corners seen from above round over `corner`, all its
// edges over `bevel` (corner >= bevel).
float brickDistance(float3 p, float3 h, float corner, float bevel) {
    float2 q = abs(p.xy) - (h.xy - corner);
    float flat = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - (corner - bevel);
    float2 w = float2(flat, abs(p.z) - (h.z - bevel));
    return min(max(w.x, w.y), 0.0) + length(max(w, 0.0)) - bevel;
}

float3 brickNormal(float3 p, float3 h, float corner, float bevel, float e) {
    const float2 k = float2(1.0, -1.0);
    return normalize(k.xyy * brickDistance(p + k.xyy * e, h, corner, bevel) + k.yyx * brickDistance(p + k.yyx * e, h, corner, bevel) +
                     k.yxy * brickDistance(p + k.yxy * e, h, corner, bevel) + k.xxx * brickDistance(p + k.xxx * e, h, corner, bevel));
}

// ---- the environment ----

static const float3 keyLight = normalize(float3(-0.45, -0.62, 0.65));
static const float3 fillLight = normalize(float3(0.70, -0.10, 0.70));

// A soft light of `size` (the lobe's width, in 1 - cos), seen by a surface `roughness` rough: wider and fainter.
float lobe(float3 d, float3 toward, float size, float roughness) {
    float w = size + roughness * roughness * 1.2;
    return (size / w) * exp((dot(d, toward) - 1.0) / w);
}

// What comes from direction d (linear light), as a surface of `roughness` sees it.
float3 radiance(float3 d, float roughness) {
    float dark = environment.y;
    float3 zenith = lerp(float3(0.95, 0.97, 1.00), float3(0.78, 0.82, 0.90), dark);
    float3 horizon = lerp(float3(0.62, 0.63, 0.66), float3(0.36, 0.38, 0.43), dark);
    float3 ground = lerp(float3(0.30, 0.30, 0.31), float3(0.06, 0.065, 0.075), dark);
    float3 sky = d.z >= 0.0 ? lerp(horizon, zenith, sqrt(saturate(d.z))) : lerp(horizon, ground, saturate(-d.z * 2.5));
    float3 light = float3(1.0, 0.98, 0.95) * 14.0 * lobe(d, keyLight, 0.035, roughness) + float3(0.92, 0.96, 1.0) * 3.0 * lobe(d, fillLight, 0.09, roughness);
    return sky + light;
}

// What lights a surface facing n, from all around it.
float3 irradiance(float3 n) {
    float dark = environment.y;
    float3 sky = lerp(float3(0.62, 0.63, 0.66), float3(0.50, 0.52, 0.58), dark);
    float3 ground = lerp(float3(0.30, 0.30, 0.31), float3(0.16, 0.165, 0.18), dark);
    float3 ambient = lerp(ground, sky, n.z * 0.5 + 0.5);
    return ambient + float3(1.0, 0.98, 0.95) * 0.52 * saturate(dot(n, keyLight)) + float3(0.92, 0.96, 1.0) * 0.14 * saturate(dot(n, fillLight));
}

float3 toLinear(float3 c) {
    return pow(max(c, 0.0), 2.2);
}

// Back to the display's curve: linear up to 0.8, then a soft shoulder toward 1.
float3 toDisplay(float3 c) {
    float3 shoulder = 0.8 + 0.2 * (1.0 - exp(-(c - 0.8) / 0.2));
    c = lerp(c, shoulder, step(0.8, c));
    return pow(saturate(c), 1.0 / 2.2);
}

// Its color, and how far it is (along the ray, over four times the eye's height: all of what can show; a depth buffer's
// value, so that bricks passing through one another meet where they do).
struct FragmentOutput {
    float4 color : SV_Target;
    float depth : SV_Depth;
};

FragmentOutput fragmentMain(VertexOutput input) {
    FragmentOutput output;
    float3 center = input.center.xyz;
    float corner = input.center.w;
    float3 h = input.halfSize.xyz;
    float bevel = input.halfSize.w;
    float4 q = input.rotation;
    float3 eye = input.eye.xyz;
    float ppd = input.eye.w;

    // the ray through this pixel, in the brick's frame
    float3 onScreen = float3(input.screen, 0.0);
    float toScreen = length(onScreen - eye);
    float3 dir = (onScreen - eye) / toScreen;
    float3 ro = unrotate(q, eye - center);
    float3 rd = unrotate(q, dir);
    // dips a pixel is wide at a distance t along the ray
    float perPixel = 1.0 / (toScreen * ppd);

    // a decal: where the ray meets the top face's plane, its texture there (read before anything is discarded: its
    // derivatives need all the quad's pixels)
    float onFace = (h.z - ro.z) / (abs(rd.z) > 1e-6 ? rd.z : -1e-6);
    float2 local = (ro + rd * onFace).xy;
    float2 across = (local - input.decal.xy) / max(input.decal.zw - input.decal.xy, 1e-4);
    float2 uv = lerp(input.decalUv.xy, input.decalUv.zw, across);
    float3 texel = image.Sample(imageSampler, uv).rgb;
    float distance = max(min(texel.r, texel.g), min(max(texel.r, texel.g), texel.b)) - 0.5;
    float2 uvPerPixel = fwidth(uv);
    float fieldOnScreen = max(0.5 * dot(input.material.xy, 1.0 / max(uvPerPixel, 1e-6)), 1.0);
    if (input.material.z > 1.5) {
        // a shadow on the target
        float2 off = abs(input.screen - center.xy) - (h.xy - corner);
        float d = length(max(off, 0.0)) + min(max(off.x, off.y), 0.0) - corner;
        float blur = max(bevel, 0.5 / ppd);
        float shade = (1.0 - smoothstep(-blur, blur, d)) * input.color.a;
        if (shade <= 0.002)
            discard;
        output.color = float4(input.color.rgb * shade, shade);
        output.depth = saturate(toScreen / (eye.z * 4.0));
        return output;
    }
    if (input.material.z > 0.5) {
        // seen from above the face, within its rectangle
        if (rd.z >= 0.0 || onFace <= 0.0 || any(across < -0.02) || any(across > 1.02))
            discard;
        float ink = saturate(distance * fieldOnScreen + 0.5) * input.color.a;
        if (ink <= 0.002)
            discard;
        float3 face = rotate(q, float3(0.0, 0.0, 1.0));
        float3 printed = toDisplay(toLinear(input.color.rgb) * irradiance(face) * environment.x);
        output.color = float4(printed * ink, ink);
        // (a hair nearer than its face: over it, not fighting it)
        output.depth = saturate((onFace - 0.05) / (eye.z * 4.0));
        return output;
    }

    // where it enters and leaves the brick's bounds (two pixels larger, for its edge)
    float3 bound = h + 2.0 * length(ro) * perPixel;
    float3 inverse = 1.0 / ((step(0.0, rd) * 2.0 - 1.0) * max(abs(rd), 1e-6));
    float3 a = (-bound - ro) * inverse, b = (bound - ro) * inverse;
    float3 nearSide = min(a, b), farSide = max(a, b);
    float t0 = max(max(nearSide.x, nearSide.y), max(nearSide.z, 0.0));
    float t1 = min(min(farSide.x, farSide.y), farSide.z);
    if (t1 <= t0)
        discard;

    // the flat of its top face (most of what shows of a brick): met where the ray meets the face's plane, at once, its
    // normal the face's; elsewhere (its bevels, its sides, its edge) found by walking
    float tTop = (h.z - ro.z) / (rd.z < -1e-6 ? rd.z : -1e-6);
    float2 onTop = (ro + rd * tTop).xy;
    float2 inner = abs(onTop) - (h.xy - corner);
    float flatTop = length(max(inner, 0.0)) + min(max(inner.x, inner.y), 0.0) - (corner - bevel);
    float t = t0;
    float nearest = 1e9;
    float nearestT = t0;
    bool hit = false;
    float3 localNormal = float3(0.0, 0.0, 1.0);
    if (rd.z < -1e-6 && tTop >= t0 && flatTop < -tTop * perPixel) {
        hit = true;
        nearestT = tTop;
    } else {
        // toward its surface, by how far it is; as near as it came, in pixels
        [loop] for (int i = 0; i < 40; ++i) {
            float d = brickDistance(ro + rd * t, h, corner, bevel);
            float pixel = t * perPixel;
            float m = d / pixel;
            if (m < nearest) {
                nearest = m;
                nearestT = t;
            }
            if (d < pixel * 0.1) {
                hit = true;
                break;
            }
            t += max(d, pixel * 0.5);
            if (t > t1)
                break;
        }
        localNormal = brickNormal(ro + rd * nearestT, h, corner, bevel, max(nearestT * perPixel * 0.5, 0.01));
    }
    float coverage = hit ? 1.0 : saturate(1.0 - nearest);
    if (coverage <= 0.002)
        discard;

    // lit where it is (or where the ray passed nearest)
    float3 n = rotate(q, localNormal);
    float3 v = -dir;
    if (input.material.w > 1.5) {
        // glass, clear: what is under it (a copy at half size), barely softened; toward its edges bent as a lens bends
        // (what lies a little beyond the edge drawn in toward it), its colors parting a little there; a thin bright rim
        // along the edge, brighter on the side toward the light; its turned sides darker, a highlight where it turns
        // toward the light
        float3 hit = ro + rd * nearestT;
        // the top face's edge: how far in (dips), and which way is out
        float2 q2 = abs(hit.xy) - (h.xy - corner);
        float inside = -(length(max(q2, 0.0)) + min(max(q2.x, q2.y), 0.0) - corner);
        float2 outward = max(q2.x, q2.y) > 0.0 ? normalize(max(q2, 0.0) + 1e-5) : (q2.x > q2.y ? float2(1.0, 0.0) : float2(0.0, 1.0));
        outward *= sign(hit.xy + 1e-6);
        float2 out2 = rotate(q, float3(outward, 0.0)).xy;
        // a side joined to another (a tab to its stack): one piece there, no edge (decal.x its sides: 1 top, 2 bottom,
        // 4 left, 8 right; decal.yz where its top is joined, all of it when not apart)
        uint sides = (uint)(input.decal.x + 0.5);
        bool topOpen = (sides & 1u) != 0u && (input.decal.z <= input.decal.y || (hit.x >= input.decal.y && hit.x <= input.decal.z));
        float open = (outward.y < -0.5 && topOpen) || (outward.y > 0.5 && (sides & 2u) != 0u) || (outward.x < -0.5 && (sides & 4u) != 0u) ||
                             (outward.x > 0.5 && (sides & 8u) != 0u) ? 1.0 : 0.0;
        // its edge's zone: a fifth of its smaller side, 10 dips at most (small ones not all lens)
        float zone = max(min(10.0, 0.4 * min(h.x, h.y)), 2.0);
        float lens = (1.0 - smoothstep(0.0, zone, max(inside, 0.0))) * (1.0 - open);
        lens *= lens;
        // on its bevel and sides (turned away), bent by how far it turns
        float turned = (1.0 - saturate(n.z)) * (1.0 - open);
        float2 shift = (out2 * (lens * min(zone * 0.7, 7.0) + turned * 6.0)) * input.uvPerDip;
        uint width, height;
        image.GetDimensions(width, height);
        float2 texel = 1.0 / float2(max(width, 1u), max(height, 1u));
        float2 at = input.backdropUv + shift;
        float3 behind = float3(0.0, 0.0, 0.0);
        [unroll] for (int k = 0; k < 6; ++k) {
            float angle = k * 1.0471976 + 0.5;
            float2 soft = float2(cos(angle), sin(angle)) * 0.7 * texel;
            // red bent the least, blue the most: a faint fringe where it bends
            behind.r += image.SampleLevel(imageSampler, at + soft - shift * 0.06, 0).r;
            behind.g += image.SampleLevel(imageSampler, at + soft, 0).g;
            behind.b += image.SampleLevel(imageSampler, at + soft + shift * 0.08, 0).b;
        }
        behind /= 6.0;
        float facing = saturate(dot(n, v));
        float3 pane = lerp(behind * 0.97 + 0.03, input.color.rgb, 0.07) * lerp(0.80, 1.0, facing);
        // the rim: a thin line along the top face's edge, lit by how much it faces the light
        float3 toward = normalize(light.xyz);
        float lit = 0.78 + 0.22 * saturate(dot(out2, -normalize(toward.xy + 1e-5)) * 0.5 + 0.5);
        float edgeLine = exp(-max(inside, 0.0) / 1.2) * (inside > -bevel ? 1.0 : 0.0) * (1.0 - open);
        float rim = edgeLine * 0.36 * lit + pow(1.0 - facing, 3.0) * 0.30 * (1.0 - open);
        // a soft inner shade along the edge away from the light
        float shade = lens * 0.10 * (1.0 - lit);
        float3 halfway = normalize(toward + v);
        float shine = pow(saturate(dot(n, halfway)), 80.0) * 0.6;
        float3 glass = saturate(pane * (1.0 - shade) + (rim + shine) * float3(1.0, 1.0, 1.0));
        float alphaGlass = input.color.a * coverage;
        output.color = float4(glass * alphaGlass, alphaGlass);
        output.depth = saturate(nearestT / (eye.z * 4.0));
        return output;
    }
    if (input.material.w > 0.5) {
        // lit as the 2D styles' reliefs are (a style's controls): its flat top as its color is, its bevels and sides
        // lighter where they turn toward the light, darker away, a highlight where they turn toward it
        float3 toward = normalize(light.xyz);
        float top = light.w + reliefMaterial.x * toward.z;
        float shade = (light.w + reliefMaterial.x * saturate(dot(n, toward))) / max(top, 1e-3);
        float3 halfway = normalize(toward + float3(0.0, 0.0, 1.0));
        float shine = reliefMaterial.y * max(0.0, pow(saturate(dot(n, halfway)), reliefMaterial.z) - pow(saturate(halfway.z), reliefMaterial.z));
        float3 highlight = lerp(float3(1.0, 1.0, 1.0), input.color.rgb, finish.x);
        float3 lit = saturate(input.color.rgb * shade + shine * highlight);
        float alphaRelief = input.color.a * coverage;
        output.color = float4(lit * alphaRelief, alphaRelief);
        output.depth = saturate(nearestT / (eye.z * 4.0));
        return output;
    }
    float roughness = input.material.x, metal = input.material.y;
    float3 base = toLinear(input.color.rgb);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), base, metal);
    float nv = saturate(dot(n, v));
    float3 fresnel = f0 + (max(1.0 - roughness, f0) - f0) * pow(1.0 - nv, 5.0);
    float3 diffuse = base * (1.0 - metal) * irradiance(n);
    float3 specular = fresnel * radiance(reflect(-v, n), roughness);
    float3 color = toDisplay((diffuse * (1.0 - fresnel * (1.0 - metal)) + specular) * environment.x);

    float alpha = input.color.a * coverage;
    output.color = float4(color * alpha, alpha);
    output.depth = saturate(nearestT / (eye.z * 4.0));
    return output;
}
