// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The Painter's solids (Scene.hpp): bricks, their shadows and what is printed on them, and what was drawn moved in space.
#include <GLWidgets/Paint/Painter.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace glw::paint {

namespace {

// `color` (packColor's) with its alpha times `by`.
void scaleAlpha(std::uint32_t& color, float by) {
    const auto a = static_cast<std::uint32_t>(std::lround(static_cast<float>(color >> 24) * std::clamp(by, 0.0f, 1.0f)));
    color = (color & 0x00ffffffu) | a << 24;
}

// The smallest rectangle around `points` seen from the camera (project), `pad` dips larger on each side.
template <class Each>
Rect around(const Painter& painter, float pad, Each&& each) {
    float left = 1e30f, top = 1e30f, right = -1e30f, bottom = -1e30f;
    each([&](Float3 p) {
        const Point s = painter.project(p);
        left = std::min(left, s.x);
        top = std::min(top, s.y);
        right = std::max(right, s.x);
        bottom = std::max(bottom, s.y);
    });
    return {left - pad, top - pad, right - left + 2 * pad, bottom - top + 2 * pad};
}

Float3 centerOf(const SceneVertex& v) {
    return {v.center[0], v.center[1], v.center[2]};
}
Quaternion rotationOf(const SceneVertex& v) {
    return {v.rotation[0], v.rotation[1], v.rotation[2], v.rotation[3]};
}
void setPlace(SceneVertex& v, Float3 center, Quaternion q) {
    v.center[0] = center.x;
    v.center[1] = center.y;
    v.center[2] = center.z;
    v.rotation[0] = q.x;
    v.rotation[1] = q.y;
    v.rotation[2] = q.z;
    v.rotation[3] = q.w;
}

// How much of `area` a brick covers (1: the slab the rest stands on).
float largeIn(const SceneVertex& v, const Rect& area) {
    return std::clamp(v.halfSize[0] * v.halfSize[1] * 4 / std::max(1.0f, area.width * area.height), 0.0f, 1.0f);
}

// The order a panel's pieces come in (Painter::arriveFrom, riseIn): 0 its slab (its largest brick, nearly as large as
// it), 1 its tab (a brick joined at its bottom) with what is drawn over the slab's top (its tab bar), 2 the rest.
struct Pieces {
    float slabTop = -1e30f;         // (none: no slab, all of it the rest)
    int stageOf(const SceneVertex& v, const Rect& area) const {
        if (largeIn(v, area) >= 0.5f)
            return 0;
        const auto sides = static_cast<unsigned>(v.decal[0] + 0.5f);
        return v.material[2] < 0.5f || v.material[2] > 1.5f ? ((sides & 2u) != 0 ? 1 : 2) : 2;
    }
    int stageOf(float y) const { return y < slabTop ? 1 : 2; }
};

Pieces piecesOf(const std::vector<SceneVertex>& vertices, std::size_t first, const Rect& area) {
    Pieces pieces;
    for (std::size_t i = first; i + 3 < vertices.size(); i += 4) {
        const SceneVertex& v = vertices[i];
        if (v.material[2] < 0.5f && largeIn(v, area) >= 0.5f)
            pieces.slabTop = std::max(pieces.slabTop, v.center[1] - v.halfSize[1]);
    }
    return pieces;
}

} // namespace

Point Painter::project(Float3 p) const {
    const Float3& e = camera_.eye;
    // onto the target (z = 0) along the line from the eye (a point at or above the eye: as far as it goes)
    const float s = e.z / std::max(e.z - p.z, e.z * 1e-3f);
    return {e.x + (p.x - e.x) * s, e.y + (p.y - e.y) * s};
}

SceneVertex Painter::sceneVertex(const Brick& brick, float kind) const {
    const Transform& t = transform();
    const Point c = t.map(Point{brick.center.x, brick.center.y});
    const Float3 half = brick.halfSize * t.scale;
    SceneVertex v{};
    setPlace(v, {c.x, c.y, brick.center.z * t.scale}, brick.rotation);
    const float bevel = std::clamp(brick.bevel * t.scale, 0.0f, std::min({half.x, half.y, half.z}));
    v.bevel = bevel;
    v.cornerRadius = std::clamp(brick.cornerRadius * t.scale, bevel, std::min(half.x, half.y));
    v.halfSize[0] = half.x;
    v.halfSize[1] = half.y;
    v.halfSize[2] = half.z;
    v.material[0] = std::clamp(brick.roughness, 0.02f, 1.0f);
    v.material[1] = std::clamp(brick.metal, 0.0f, 1.0f);
    v.material[2] = kind;
    v.material[3] = brick.glass ? 2.0f : brick.reliefLight ? 1.0f : 0.0f;
    v.eye[0] = camera_.eye.x;
    v.eye[1] = camera_.eye.y;
    v.eye[2] = camera_.eye.z;
    v.pixelsPerDip = pixelsPerDip_;
    v.color = packColor(brick.color);
    // its joined sides (a brick's decal fields: it has none)
    const Joined& j = brick.joined;
    v.decal[0] = static_cast<float>((j.top ? 1 : 0) | (j.bottom ? 2 : 0) | (j.left ? 4 : 0) | (j.right ? 8 : 0));
    v.decal[1] = (j.topFrom - brick.halfSize.x) * t.scale;
    v.decal[2] = (j.topTo - brick.halfSize.x) * t.scale;
    return v;
}

void Painter::sceneQuad(const Rect& quad, SceneVertex v, TextureId texture, bool anyTexture) {
    if (quad.empty() || culled(quad))
        return;
    DrawCommand& cmd = command(texture, anyTexture, true);
    const auto base = static_cast<std::uint32_t>(list_.sceneVertices.size());
    for (Point corner : {Point{quad.left(), quad.top()}, Point{quad.right(), quad.top()}, Point{quad.right(), quad.bottom()},
                         Point{quad.left(), quad.bottom()}}) {
        v.position = corner;
        list_.sceneVertices.push_back(v);
    }
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

void Painter::placeQuad(SceneVertex* four) const {
    const SceneVertex& v = four[0];
    if (v.material[2] > 1.5f)
        return;             // a shadow lies on the target, where it is
    const Float3 center = centerOf(v);
    const Quaternion q = rotationOf(v);
    Rect quad;
    if (v.material[2] > 0.5f) {
        // a decal: its rectangle on the top face
        quad = around(*this, 2.0f / pixelsPerDip_, [&](auto take) {
            for (Point c : {Point{v.decal[0], v.decal[1]}, Point{v.decal[2], v.decal[1]}, Point{v.decal[2], v.decal[3]},
                            Point{v.decal[0], v.decal[3]}})
                take(center + q.rotate({c.x, c.y, v.halfSize[2]}));
        });
    } else {
        const Float3 ax = q.rotate({v.halfSize[0], 0, 0}), ay = q.rotate({0, v.halfSize[1], 0}), az = q.rotate({0, 0, v.halfSize[2]});
        quad = around(*this, 1.5f / pixelsPerDip_, [&](auto take) {
            for (int k = 0; k < 8; ++k)
                take(center + ax * ((k & 1) ? 1.0f : -1.0f) + ay * ((k & 2) ? 1.0f : -1.0f) + az * ((k & 4) ? 1.0f : -1.0f));
        });
    }
    const Point corners[4] = {{quad.left(), quad.top()}, {quad.right(), quad.top()}, {quad.right(), quad.bottom()}, {quad.left(), quad.bottom()}};
    for (int k = 0; k < 4; ++k)
        four[k].position = corners[k];
}

void Painter::drawBrick(const Brick& brick) {
    if (brick.color.a <= 0 || brick.halfSize.x <= 0 || brick.halfSize.y <= 0 || brick.halfSize.z <= 0)
        return;
    if (brick.shadow > 0)
        drawBrickShadow(brick);
    const SceneVertex v = sceneVertex(brick, brickKind);
    // the quad around where it shows: its corners seen from the camera, and a pixel more for its anti-aliased edge
    const Float3 center = centerOf(v);
    const Quaternion& q = brick.rotation;
    const Float3 ax = q.rotate({v.halfSize[0], 0, 0}), ay = q.rotate({0, v.halfSize[1], 0}), az = q.rotate({0, 0, v.halfSize[2]});
    const Rect quad = around(*this, 1.5f / pixelsPerDip_, [&](auto take) {
        for (int k = 0; k < 8; ++k)
            take(center + ax * ((k & 1) ? 1.0f : -1.0f) + ay * ((k & 2) ? 1.0f : -1.0f) + az * ((k & 4) ? 1.0f : -1.0f));
    });
    // of glass: it reads what is under it
    if (brick.glass)
        sceneQuad(quad, v, brick.joined.onto ? joinedBackdropTexture : backdropTexture, false);
    else
        sceneQuad(quad, v, whiteTexture, true);
}

void Painter::drawBrickShadow(const Brick& brick) {
    if (brick.shadow <= 0 || brick.color.a <= 0)
        return;
    const SceneVertex b = sceneVertex(brick, brickKind);
    const Float3 center = centerOf(b);
    const Quaternion& q = brick.rotation;
    const Float3 ax = q.rotate({b.halfSize[0], 0, 0}), ay = q.rotate({0, b.halfSize[1], 0}), az = q.rotate({0, 0, b.halfSize[2]});
    const Float3 reach{std::abs(ax.x) + std::abs(ay.x) + std::abs(az.x), std::abs(ax.y) + std::abs(ay.y) + std::abs(az.y),
                       std::abs(ax.z) + std::abs(ay.z) + std::abs(az.z)};
    // on the target: under it, away from the light (above, to the upper left), softer and fainter as it is higher
    const float height = std::max(0.0f, center.z - reach.z) + reach.z * 0.5f;
    const Point at{center.x + 0.28f * height + 0.5f, center.y + 0.42f * height + 1.0f};
    const float blur = 1.5f + 0.55f * height;
    const float alpha = brick.shadow * brick.color.a * std::clamp(1.0f - height / 400.0f, 0.25f, 1.0f) * (brick.glass ? 0.6f : 1.0f);
    SceneVertex v = b;
    setPlace(v, {at.x, at.y, 0}, {});
    v.cornerRadius = std::min(b.cornerRadius, std::min(reach.x, reach.y));
    v.halfSize[0] = reach.x;
    v.halfSize[1] = reach.y;
    v.bevel = blur;
    v.material[2] = shadowKind;
    v.color = packColor(Color{0, 0, 0, alpha});
    // drawn by the scene's shader, as the bricks are (one pipeline for both; behind those over it)
    sceneQuad(Rect{at.x - reach.x, at.y - reach.y, 2 * reach.x, 2 * reach.y}.inflated(blur + 1.0f / pixelsPerDip_), v, whiteTexture, true);
}

void Painter::drawBrickDecal(const Brick& brick, const Rect& rect, TextureId texture, const Rect& uv, Point range, Color color) {
    if (color.a <= 0 || rect.empty())
        return;
    const float scale = transform().scale;
    SceneVertex v = sceneVertex(brick, decalKind);
    const Rect on{rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale};
    v.decal[0] = on.left();
    v.decal[1] = on.top();
    v.decal[2] = on.right();
    v.decal[3] = on.bottom();
    v.decalUv[0] = uv.left();
    v.decalUv[1] = uv.top();
    v.decalUv[2] = uv.right();
    v.decalUv[3] = uv.bottom();
    v.material[0] = range.x;
    v.material[1] = range.y;
    v.material[3] = 0;
    v.color = packColor(color);
    // the quad around where it shows: its corners on the top face, seen from the camera, and two pixels more
    const Float3 center = centerOf(v);
    const Quaternion& q = brick.rotation;
    const Rect quad = around(*this, 2.0f / pixelsPerDip_, [&](auto take) {
        for (Point c : {Point{on.left(), on.top()}, Point{on.right(), on.top()}, Point{on.right(), on.bottom()}, Point{on.left(), on.bottom()}})
            take(center + q.rotate({c.x, c.y, v.halfSize[2]}));
    });
    sceneQuad(quad, v, texture, false);
}

void Painter::arcBrick(Point center, float inner, float outer, float start, float sweep, float inset, float height, float bevel, Color color,
                       float corner) {
    constexpr float turn = 6.28318530718f;
    inner = std::max(0.0f, inner);
    const bool whole = sweep >= turn - 1e-4f;
    const float half = std::min(sweep, turn) / 2;
    const float gap = whole ? 0.0f : std::max(0.0f, inset);
    // in its own frame (the sector's middle along +x): what its arcs and sides reach
    float lowX = 1e30f, highX = -1e30f, highY = 0;
    const auto reach = [&](float r, float angle) {
        lowX = std::min(lowX, r * std::cos(angle));
        highX = std::max(highX, r * std::cos(angle));
        highY = std::max(highY, std::abs(r * std::sin(angle)));
    };
    for (float r : {inner, outer}) {
        for (float angle : {-half, 0.0f, half})
            reach(r, angle);
        if (half > turn / 4)
            reach(r, turn / 4);
        if (half >= turn / 2 - 1e-4f)
            reach(r, turn / 2);
    }
    const float middleX = (lowX + highX) / 2;
    // its body down into what it is on, as deep as a relief's brick is (no more than half as deep as it is across)
    const float depth = std::min(height * reliefBricks_->depth, (outer - inner) * 0.5f);
    const float middle = start + half;
    const Point axis{std::cos(middle), std::sin(middle)};
    const Brick brick{.center = {center.x + axis.x * middleX, center.y + axis.y * middleX, -depth / 2},
                      .halfSize = {(highX - lowX) / 2, highY, depth / 2},
                      .rotation = Quaternion::axisAngle({0, 0, 1}, middle),
                      .cornerRadius = corner,
                      .bevel = reliefBricks_->bevel >= 0 ? reliefBricks_->bevel : bevel,
                      .color = color,
                      .roughness = reliefBricks_->roughness,
                      .shadow = 0,
                      .reliefLight = true};
    if (brick.halfSize.x <= 0 || brick.halfSize.y <= 0 || depth <= 0)
        return;
    SceneVertex v = sceneVertex(brick, brickKind);
    // the ring in its frame (Scene.hlsl): its radii, its half angle and its sides' inset (sixteenths of a dip), where its
    // center is; its corners as round as the ring's are, not as its bounds
    const float scale = transform().scale;
    v.decalUv[0] = inner * scale;
    v.decalUv[1] = outer * scale;
    v.decalUv[2] = whole ? -1.0f : half + 8.0f * std::round(gap * scale * 16);
    v.decalUv[3] = -middleX * scale;
    v.cornerRadius = std::clamp(corner * scale, v.bevel, std::max(v.bevel, (outer - inner) * scale / 2));
    const Float3 c = centerOf(v);
    const Quaternion& q = brick.rotation;
    const Float3 ax = q.rotate({v.halfSize[0], 0, 0}), ay = q.rotate({0, v.halfSize[1], 0}), az = q.rotate({0, 0, v.halfSize[2]});
    const Rect quad = around(*this, 1.5f / pixelsPerDip_, [&](auto take) {
        for (int k = 0; k < 8; ++k)
            take(c + ax * ((k & 1) ? 1.0f : -1.0f) + ay * ((k & 2) ? 1.0f : -1.0f) + az * ((k & 4) ? 1.0f : -1.0f));
    });
    sceneQuad(quad, v, whiteTexture, true);
}

// ---- what was drawn, moved ----

Painter::Mark Painter::markHere() const {
    return {list_.vertices.size(), list_.sceneVertices.size(), list_.commands.size()};
}

Painter::Mark Painter::mark() {
    // a command of its own from here: what follows is not added to one that came before
    list_.commands.push_back(DrawCommand{static_cast<std::uint32_t>(list_.indices.size()), 0, clips_.back(), whiteTexture, false});
    return {list_.vertices.size(), list_.sceneVertices.size(), list_.commands.size() - 1};
}

void Painter::fallIn(const Mark& mark, float t, const Rect& area, std::optional<Rect> from) {
    if (from && !from->empty() && !area.empty())
        arriveFrom(mark, t, area, *from);
    else
        arrive(mark, t, area, false);
}

void Painter::arriveFrom(const Mark& mark, float t, const Rect& area, const Rect& from) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1)
        return;
    // where a point of the area was: the area stretched onto `from`
    // where a point of the area was: the area made as small as fits in `from` (the same both ways: nothing stretched, text
    // as it is, smaller), in its middle
    const float fit = std::min(from.width / area.width, from.height / area.height);
    const float sx = fit, sy = fit;
    const Point fromMiddle = from.center(), areaMiddle = area.center();
    const auto was = [&](Point p) { return Point{fromMiddle.x + (p.x - areaMiddle.x) * fit, fromMiddle.y + (p.y - areaMiddle.y) * fit}; };
    const Float3& eye = camera_.eye;
    // where a point `z` high must be to show at `seen` from the camera: each starts exactly where it was showing
    const auto under = [&](Point seen, float z) {
        const float k = (eye.z - z) / eye.z;
        return Point{eye.x + (seen.x - eye.x) * k, eye.y + (seen.y - eye.y) * k};
    };
    const auto along = [](Point a, Point b, float e) { return Point{a.x + (b.x - a.x) * e, a.y + (b.y - a.y) * e}; };
    constexpr float pi = 3.14159265f;
    // How a piece comes, `large` of the area (the slab the rest stands on: 1): put together, the slab first, then one row
    // after another down the area; each from where it was, a little nearer the camera, along an arc (up a little, then
    // down), quick at first and slowing, a slight tilt of its own that sways once and settles; landed, a small bounce and
    // a pop of its size.
    const float lifted = eye.z * 0.08f;             // where it starts (nearer the camera)
    const float hop = eye.z * 0.035f;               // the arc's height over that
    const float bounce = std::min(5.0f, eye.z * 0.006f);
    constexpr float span = 0.6f, landed = 0.8f;     // of t a piece takes; of that, when it lands (then the bounce)
    struct Motion {
        float e = 1;            // of the way from where it was (its place and size), eased
        float z = 0;            // over where it is
        float sway = 0;         // its tilt along its way, radians
        float glint = 0;        // ... toward the light
        float pop = 1;          // its size, times
    };
    // the slab first, then its tab (and its bar), then the rest a row after another
    const Pieces pieces = piecesOf(list_.sceneVertices, mark.sceneVertices, area);
    const auto motion = [&](int stage, float y, float large) {
        const float rows = std::clamp((y - area.y) / area.height, 0.0f, 1.0f);
        const float delay = stage == 0 ? 0.0f : stage == 1 ? 0.14f : 0.26f + 0.14f * rows;
        const float u = std::clamp((t - delay) / span, 0.0f, 1.0f);
        const float s = std::min(1.0f, u / landed);
        Motion m;
        m.e = 1 - (1 - s) * (1 - s) * (1 - s);
        const float small = 1 - large;
        m.z = lifted * (1 - m.e) + hop * std::sin(pi * s) * small;
        // leaning a little into its way (its leading edge lower), flat as it lands, a hair the other way after; turned
        // toward the light on the way (a highlight across it), back as it lands
        m.sway = (0.07f * (1 - m.e) - 0.015f * std::sin(pi * s) * s) * small;
        m.glint = 0.26f * std::sin(pi * s) * (0.25f + 0.75f * small);
        if (u > landed) {
            const float b = (u - landed) / (1 - landed);
            m.z += bounce * std::sin(pi * b) * (1 - b) * small;
            m.pop = 1 + 0.035f * std::sin(pi * b) * small;
        }
        return m;
    };

    // the 2D of it: with what it is on, seen where it is on the way (its text as small as the rest); a shadow (a blurred
    // box) not shown on the way, coming as what casts it lands
    for (std::size_t i = mark.vertices; i < list_.vertices.size(); ++i) {
        Vertex& v = list_.vertices[i];
        const Motion m = motion(pieces.stageOf(v.position.y), v.position.y, 0);
        const Point at = along(under(was(v.position), lifted), v.position, m.e);
        v.position = project({at.x, at.y, m.z});
        if (v.kind == static_cast<float>(VertexKind::Box) && v.blur > 0.5f)
            scaleAlpha(v.color, std::clamp((m.e - 0.9f) / 0.1f, 0.0f, 1.0f));
    }
    float order = 0;
    for (std::size_t i = mark.sceneVertices; i + 3 < list_.sceneVertices.size(); i += 4) {
        SceneVertex* four = &list_.sceneVertices[i];
        const Point c{four[0].center[0], four[0].center[1]};
        const float large = largeIn(four[0], area);
        const Motion m = motion(pieces.stageOf(four[0], area), c.y, large);
        const Point start = was(c);
        if (four[0].material[2] > 1.5f) {
            // a shadow: under its brick on the way, darker as it comes down
            const Point at = along(start, c, m.e);
            for (int k = 0; k < 4; ++k) {
                four[k].center[0] += at.x - c.x;
                four[k].center[1] += at.y - c.y;
                four[k].position = {four[k].position.x + at.x - c.x, four[k].position.y + at.y - c.y};
                scaleAlpha(four[k].color, 0.3f + 0.7f * m.e);
            }
            continue;
        }
        const Point at = along(under(start, lifted), c, m.e);       // (its top face, as its 2D, where it was)
        // as large as it was showing there (the area stretched onto `from`; nearer the camera, it shows larger), growing to
        // its own size as it comes, a pop of it as it lands
        const float nearer = (eye.z - lifted) / eye.z;
        const float fromX = sx * nearer, fromY = sy * nearer;
        const float wide = four[0].halfSize[0] * (fromX + (1 - fromX) * m.e) * m.pop;
        const float tall = four[0].halfSize[1] * (fromY + (1 - fromY) * m.e) * m.pop;
        // its tilt: about the line across its way (none when it does not move)
        const Float3 way{c.x - start.x, c.y - start.y, 0};
        const Quaternion lean = length(way) > 1 ? Quaternion::axisAngle({-way.y, way.x, 0}, m.sway) : Quaternion{};
        const Quaternion tilt = towardLight(m.glint) * lean;
        for (int k = 0; k < 4; ++k) {
            four[k].halfSize[0] = wide;
            four[k].halfSize[1] = tall;
            four[k].cornerRadius = std::min(four[k].cornerRadius, std::min(wide, tall));
            four[k].bevel = std::min(four[k].bevel, four[k].cornerRadius);
            setPlace(four[k], {at.x, at.y, four[k].center[2] + m.z + order}, tilt * rotationOf(four[k]));
        }
        placeQuad(four);
        order += 0.01f;
    }
    // cut by no clip on the way
    for (std::size_t c = mark.commands; c < list_.commands.size(); ++c)
        list_.commands[c].clip = clips_.front();
}

void Painter::riseIn(const Mark& mark, float t, const Rect& area) {
    arrive(mark, t, area, true);
}

void Painter::arrive(const Mark& mark, float t, const Rect& area, bool fromBehind) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1)
        return;
    const float top = area.y, tall = std::max(1.0f, area.height), wide = std::max(1.0f, area.width);
    // when a place starts to come: falling, from the top of the area down; rising, a panel's pieces in the reverse of
    // the order they land in (Pieces): the rest a row after another, then its tab, then its slab
    const Pieces pieces = piecesOf(list_.sceneVertices, mark.sceneVertices, area);
    const auto risingDelay = [&](int stage, float y) {
        return stage == 2 ? 0.12f * std::clamp((y - top) / tall, 0.0f, 1.0f) : stage == 1 ? 0.2f : 0.3f;
    };
    const auto delayAt = [&](Point at) {
        return fromBehind ? risingDelay(pieces.stageOf(at.y), at.y) : 0.40f * std::clamp((at.y - top) / tall, 0.0f, 1.0f);
    };
    constexpr float span = 0.55f;           // of t a brick takes to come (its delay at most 0.4, and a little of its own)
    constexpr float landed = 0.8f;          // falling: of its span when it lands (then a little bounce)
    const auto progress = [&](float delay) { return std::clamp((t - delay) / span, 0.0f, 1.0f); };
    const auto smooth = [](float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * (3 - 2 * x);
    };

    // the 2D of it (text, lines): where it is, showing as what is under it comes (landed, or near)
    for (std::size_t i = mark.vertices; i < list_.vertices.size(); ++i) {
        Vertex& v = list_.vertices[i];
        const float u = progress(delayAt(v.position));
        scaleAlpha(v.color, smooth(fromBehind ? (u - 0.6f) / 0.4f : (u - landed) / (1 - landed)));
    }

    const float far = camera_.eye.z;
    // each brick a hair nearer than those before it (a hundredth of a dip): where two meet in a plane (a control's top on
    // its slab's), the later one shows, as it does without a depth test
    float order = 0;
    for (std::size_t i = mark.sceneVertices; i + 3 < list_.sceneVertices.size(); i += 4) {
        SceneVertex* four = &list_.sceneVertices[i];
        // its own motion, by where it is (a brick, its shadow and what is printed on it share it)
        const float cx = four[0].center[0], cy = four[0].center[1];
        std::uint32_t seed = static_cast<std::uint32_t>(std::lround(cx * 7.0f)) * 73856093u ^ static_cast<std::uint32_t>(std::lround(cy * 7.0f)) * 19349663u;
        const auto next = [&] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);       // 0 to 1
        };
        // one nearly as large as the area (the slab the rest stands on) comes first, flat; a smaller one (a row as wide as
        // the area too) turns as it comes
        const float large = std::clamp(four[0].halfSize[0] * four[0].halfSize[1] * 4 / (wide * tall), 0.0f, 1.0f);
        const float free = std::clamp(1 - large * 1.6f, 0.0f, 1.0f);
        const float delay = fromBehind ? risingDelay(pieces.stageOf(four[0], area), cy) : (delayAt({cx, cy}) + 0.05f * next()) * free;
        const float u = progress(delay);
        if (four[0].material[2] > 1.5f) {
            // a shadow: coming as its brick does
            for (int k = 0; k < 4; ++k)
                scaleAlpha(four[k].color, u * u);
            continue;
        }
        const Float3 axis{next() * 2 - 1, next() * 2 - 1, next() - 0.5f};
        const Float3 turnAbout = length(axis) > 0.01f ? axis : Float3{1, 0, 0};
        float rise = 0, angle = 0, alpha = 1;
        Point aside{};
        if (!fromBehind) {
            // dropped: high, then down faster and faster, a small bounce where it lands, its turn undone as it comes
            const float left = 1 - std::min(1.0f, u / landed);
            const float fall = left * left;
            const float height = far * 0.4f * (0.35f + 0.65f * free);
            rise = height * fall;
            if (u > landed) {
                const float b = (u - landed) / (1 - landed);
                rise = std::min(6.0f, height * 0.02f) * std::sin(3.14159265f * b) * (1 - b);
            }
            angle = (0.5f + next() * 0.7f) * free * 6.2831853f * fall;
            aside = {(next() * 2 - 1) * 70 * fall * free, (next() * 2 - 1) * 45 * fall * free};
            alpha = std::min(1.0f, u * 8);
        } else {
            // from far behind: nearer and nearer, slowing as it comes, its tilt undone
            const float left = 1 - u;
            const float behind = left * left * left;
            rise = -far * 0.9f * behind;
            angle = 0.35f * free * behind;
            alpha = smooth(u / 0.35f);
        }
        // rising: turned toward the light on the way too (a highlight across it), flat where it comes to
        const float glint = fromBehind ? 0.26f * std::sin(3.14159265f * u) * (0.25f + 0.75f * free) : 0.0f;
        const Quaternion spin = towardLight(glint) * Quaternion::axisAngle(turnAbout, angle);
        for (int k = 0; k < 4; ++k) {
            SceneVertex& v = four[k];
            setPlace(v, {cx + aside.x, cy + aside.y, v.center[2] + rise + order}, spin * rotationOf(v));
            scaleAlpha(v.color, alpha);
        }
        placeQuad(four);
        order += 0.01f;
    }
    // its bricks cut by no clip while they come
    for (std::size_t c = mark.commands; c < list_.commands.size(); ++c) {
        if (list_.commands[c].scene)
            list_.commands[c].clip = clips_.front();
    }
}

void Painter::place(const Mark& mark, Point pivot, const Lifted& lifted) {
    const Float3 at{pivot.x, pivot.y, 0};
    const Float3 up{lifted.shift.x, lifted.shift.y, lifted.lift};
    const float scale = lifted.scale;
    const auto moved = [&](Float3 p) { return at + up + lifted.turn.rotate((p - at) * scale); };
    const bool faded = lifted.alpha < 1;
    for (std::size_t i = mark.vertices; i < list_.vertices.size(); ++i) {
        Vertex& v = list_.vertices[i];
        v.position = project(moved({v.position.x, v.position.y, 0}));
        if (faded)
            scaleAlpha(v.color, lifted.alpha);
    }
    for (std::size_t i = mark.sceneVertices; i + 3 < list_.sceneVertices.size(); i += 4) {
        SceneVertex* four = &list_.sceneVertices[i];
        if (four[0].material[2] > 1.5f) {
            // a shadow lies on the target, where it is (fading as its brick does)
            if (faded)
                for (int k = 0; k < 4; ++k)
                    scaleAlpha(four[k].color, lifted.alpha);
            continue;
        }
        const Float3 center = moved(centerOf(four[0]));
        const Quaternion q = lifted.turn * rotationOf(four[0]);
        for (int k = 0; k < 4; ++k) {
            setPlace(four[k], center, q);
            if (scale != 1) {
                for (float& h : four[k].halfSize)
                    h *= scale;
                four[k].cornerRadius *= scale;
                four[k].bevel *= scale;
                for (float& d : four[k].decal)
                    d *= four[k].material[2] > 0.5f ? scale : 1.0f;      // (a decal's rectangle on its face)
                if (four[k].material[2] < 0.5f && four[k].decalUv[1] > 0) {
                    // a brick bent around a ring: its radii, its sides' inset and where its ring's center is
                    float* arc = four[k].decalUv;
                    arc[0] *= scale;
                    arc[1] *= scale;
                    arc[3] *= scale;
                    if (arc[2] >= 0) {
                        const float steps = std::floor(arc[2] / 8);
                        arc[2] = arc[2] - steps * 8 + 8 * std::round(steps * scale);
                    }
                }
            }
            if (faded)
                scaleAlpha(four[k].color, lifted.alpha);
        }
        placeQuad(four);
    }
}

void Painter::placeClips(const Mark& mark, Point pivot, const Lifted& lifted) {
    const Float3 at{pivot.x, pivot.y, 0};
    const Float3 up{lifted.shift.x, lifted.shift.y, lifted.lift};
    const auto moved = [&](Point p) { return project(at + up + lifted.turn.rotate((Float3{p.x, p.y, 0} - at) * lifted.scale)); };
    for (std::size_t c = mark.commands; c < list_.commands.size(); ++c) {
        // (turned, a clip is a quadrilateral: halfway between the rectangle around it and the one within it, cutting what
        // was cut nearly where it was, and what shows nearly none of it)
        const Rect& r = list_.commands[c].clip;
        const Point a = moved({r.left(), r.top()}), b = moved({r.right(), r.top()});
        const Point d = moved({r.right(), r.bottom()}), e = moved({r.left(), r.bottom()});
        const float left = (std::max(std::min(a.x, b.x), std::min(e.x, d.x)) + std::min({a.x, b.x, d.x, e.x})) / 2;
        const float right = (std::min(std::max(a.x, b.x), std::max(e.x, d.x)) + std::max({a.x, b.x, d.x, e.x})) / 2;
        const float top = (std::max(std::min(a.y, e.y), std::min(b.y, d.y)) + std::min({a.y, b.y, d.y, e.y})) / 2;
        const float bottom = (std::min(std::max(a.y, e.y), std::max(b.y, d.y)) + std::max({a.y, b.y, d.y, e.y})) / 2;
        list_.commands[c].clip = Rect{left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)}.intersected(clips_.front());
    }
}

void Painter::unclip(const Mark& mark) {
    for (std::size_t c = mark.commands; c < list_.commands.size(); ++c)
        list_.commands[c].clip = clips_.front();
}

namespace {

constexpr float pi = 3.14159265f;

float outCubic(float u) {
    u = std::clamp(u, 0.0f, 1.0f);
    const float v = 1 - u;
    return 1 - v * v * v;
}

// Fast, past the end by about `over` (1.7: some 10%), back.
float outBack(float u, float over) {
    u = std::clamp(u, 0.0f, 1.0f);
    const float v = u - 1;
    return 1 + (over + 1) * v * v * v + over * v * v;
}

float smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3 - 2 * x);
}

} // namespace

void Painter::swingIn(const Mark& mark, float t, const Rect& area, const Rect& from) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1 || area.empty() || from.empty())
        return;
    // as small as fits where it was, in its middle; swung down about its top edge, from its bottom toward the one looking
    const float fit = std::min(from.width / area.width, from.height / area.height);
    const Point hinge{area.center().x, area.y};
    const Point startHinge{from.center().x, from.center().y + (area.y - area.center().y) * fit};
    const float e = outCubic(t);
    const float swing = outBack(t, 1.6f);
    const Lifted lifted{.lift = camera_.eye.z * 0.05f * (1 - e),
                        .turn = Quaternion::axisAngle({1, 0, 0}, 0.95f * (1 - swing)) * towardLight(0.16f * std::sin(pi * e)),
                        .shift = {(startHinge.x - hinge.x) * (1 - e), (startHinge.y - hinge.y) * (1 - e)},
                        .alpha = std::min(1.0f, t * 8),
                        .scale = fit + (1 - fit) * e};
    placeClips(mark, hinge, lifted);
    place(mark, hinge, lifted);
}

void Painter::gatherIn(const Mark& mark, float t, const Rect& area, const Rect& from) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1 || area.empty() || from.empty())
        return;
    const float fit = std::min(from.width / area.width, from.height / area.height);
    const Point fromMiddle = from.center(), areaMiddle = area.center();
    const auto was = [&](Point p) { return Point{fromMiddle.x + (p.x - areaMiddle.x) * fit, fromMiddle.y + (p.y - areaMiddle.y) * fit}; };
    // its 2D where it is, showing as its pieces come (its shadows last)
    for (std::size_t i = mark.vertices; i < list_.vertices.size(); ++i) {
        Vertex& v = list_.vertices[i];
        const bool shadow = v.kind == static_cast<float>(VertexKind::Box) && v.blur > 0.5f;
        scaleAlpha(v.color, smooth01((t - (shadow ? 0.85f : 0.68f)) / (shadow ? 0.15f : 0.3f)));
    }
    float order = 0;
    for (std::size_t i = mark.sceneVertices; i + 3 < list_.sceneVertices.size(); i += 4) {
        SceneVertex* four = &list_.sceneVertices[i];
        const Point c{four[0].center[0], four[0].center[1]};
        std::uint32_t seed = static_cast<std::uint32_t>(std::lround(c.x * 7.0f)) * 73856093u ^ static_cast<std::uint32_t>(std::lround(c.y * 7.0f)) * 19349663u;
        const auto next = [&] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);       // 0 to 1
        };
        // the slab where it was, first; the rest scattered about there, coming one after another
        const float large = largeIn(four[0], area);
        const float free = large >= 0.5f ? 0.0f : 1.0f;
        const float direction = next() * 2 * pi;
        const float away = (35 + 55 * next()) * free;
        const float high = (25 + 70 * next()) * free;
        // tilted only (turned in its plane, a long one looks a stick thrown), the longer the less
        const Float3 axis{next() * 2 - 1, next() * 2 - 1, 0};
        const float aspect = std::max(four[0].halfSize[0], four[0].halfSize[1]) / std::max(1.0f, std::min(four[0].halfSize[0], four[0].halfSize[1]));
        const float spin = (next() * 2 - 1) * 0.45f * std::min(1.0f, 3 / aspect) * free;
        const float delay = 0.32f * next() * free;
        const float u = std::clamp((t - delay) / 0.68f, 0.0f, 1.0f);
        const float e = outCubic(u);
        const Point start = was(c) + Point{std::cos(direction) * away, std::sin(direction) * away};
        const Point at{start.x + (c.x - start.x) * e, start.y + (c.y - start.y) * e};
        if (four[0].material[2] > 1.5f) {
            for (int k = 0; k < 4; ++k) {
                four[k].center[0] += at.x - c.x;
                four[k].center[1] += at.y - c.y;
                four[k].position = {four[k].position.x + at.x - c.x, four[k].position.y + at.y - c.y};
                scaleAlpha(four[k].color, e * e);
            }
            continue;
        }
        const float size = fit + (1 - fit) * e;
        const Quaternion turn = Quaternion::axisAngle(length(axis) > 0.01f ? axis : Float3{1, 0, 0}, spin * (1 - e)) *
                                towardLight(0.2f * std::sin(pi * e) * free);
        for (int k = 0; k < 4; ++k) {
            four[k].halfSize[0] *= size;
            four[k].halfSize[1] *= size;
            four[k].cornerRadius = std::min(four[k].cornerRadius, std::min(four[k].halfSize[0], four[k].halfSize[1]));
            four[k].bevel = std::min(four[k].bevel, four[k].cornerRadius);
            setPlace(four[k], {at.x, at.y, four[k].center[2] + high * (1 - e) + order}, turn * rotationOf(four[k]));
            scaleAlpha(four[k].color, smooth01(u / 0.2f));
        }
        placeQuad(four);
        order += 0.01f;
    }
    unclip(mark);
}

void Painter::popIn(const Mark& mark, float t, const Rect& area) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1 || area.empty())
        return;
    const float grow = outBack(t, 1.7f);
    const Lifted lifted{.lift = -60 * (1 - outCubic(t)),
                        .turn = towardLight(0.18f * std::sin(pi * t)),
                        .alpha = smooth01(t / 0.4f),
                        .scale = 0.55f + 0.45f * grow};
    placeClips(mark, area.center(), lifted);
    place(mark, area.center(), lifted);
}

void Painter::turnIn(const Mark& mark, float t, const Rect& area) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1 || area.empty())
        return;
    const float turned = outBack(t, 1.3f);
    const Lifted lifted{.lift = -40 * (1 - outCubic(t)), .turn = Quaternion::axisAngle({0, 1, 0}, 1.2f * (1 - turned)), .alpha = smooth01(t / 0.3f)};
    placeClips(mark, area.center(), lifted);
    place(mark, area.center(), lifted);
}

void Painter::unfoldIn(const Mark& mark, float t, const Rect& area, Edge hinge) {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t >= 1 || area.empty())
        return;
    // turned about its hinge, its far edge back into the window, opening toward the one looking (past flat a little, and
    // back), the light catching it on the way
    const float angle = 1.1f * (1 - outBack(t, 1.4f));
    Point pivot;
    Quaternion turn;
    switch (hinge) {
    case Edge::Top:
        pivot = {area.center().x, area.y};
        turn = Quaternion::axisAngle({1, 0, 0}, -angle);
        break;
    case Edge::Bottom:
        pivot = {area.center().x, area.bottom()};
        turn = Quaternion::axisAngle({1, 0, 0}, angle);
        break;
    case Edge::Left:
        pivot = {area.x, area.center().y};
        turn = Quaternion::axisAngle({0, 1, 0}, angle);
        break;
    case Edge::Right:
        pivot = {area.right(), area.center().y};
        turn = Quaternion::axisAngle({0, 1, 0}, -angle);
        break;
    }
    const Lifted lifted{.turn = turn * towardLight(0.14f * std::sin(pi * outCubic(t))), .alpha = smooth01(t / 0.25f)};
    placeClips(mark, pivot, lifted);
    place(mark, pivot, lifted);
}

} // namespace glw::paint
