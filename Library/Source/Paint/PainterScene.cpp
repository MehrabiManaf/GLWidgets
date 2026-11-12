// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The Painter's solids (Scene.hpp): bricks, their shadows and what is printed on them, and what was drawn moved in space.
#include <GLWidgets/Paint/Painter.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

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
        sceneQuad(quad, v, backdropTexture, false);
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

// ---- what was drawn, moved ----

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
    const float sx = from.width / area.width, sy = from.height / area.height;
    const auto was = [&](Point p) { return Point{from.x + (p.x - area.x) * sx, from.y + (p.y - area.y) * sy}; };
    // a row a little after the one above it; each eased from where it was to its place, coming down from a little nearer
    // the camera as it goes
    const auto delayAt = [&](float y) { return 0.06f * std::clamp((y - area.y) / area.height, 0.0f, 1.0f); };
    constexpr float span = 0.94f;
    const auto eased = [&](float delay) {
        const float u = std::clamp((t - delay) / span, 0.0f, 1.0f);
        return u < 0.5f ? 4 * u * u * u : 1 - std::pow(-2 * u + 2, 3.0f) / 2;      // in and out, cubic
    };
    const float height = camera_.eye.z * 0.08f;
    const auto along = [](Point a, Point b, float e) { return Point{a.x + (b.x - a.x) * e, a.y + (b.y - a.y) * e}; };
    // where a point `height` high must be to show at `seen` from the camera: it starts exactly where it was showing
    const Float3& eye = camera_.eye;
    const auto under = [&](Point seen, float z) {
        const float k = (eye.z - z) / eye.z;
        return Point{eye.x + (seen.x - eye.x) * k, eye.y + (seen.y - eye.y) * k};
    };

    // the 2D of it: with what it is on, seen where it is on the way, stretched as it is (a line, a box); a glyph (a quad of
    // a mask or a field) moved as one, its own size kept
    for (std::size_t i = mark.vertices; i < list_.vertices.size(); ++i) {
        Vertex& v = list_.vertices[i];
        const bool glyph = (v.kind == static_cast<float>(VertexKind::Mask) || v.kind == static_cast<float>(VertexKind::Field)) &&
                           i + 3 < list_.vertices.size() && list_.vertices[i + 3].kind == v.kind;
        if (glyph) {
            const Point middle{(list_.vertices[i].position.x + list_.vertices[i + 2].position.x) / 2,
                               (list_.vertices[i].position.y + list_.vertices[i + 2].position.y) / 2};
            const float e = eased(delayAt(middle.y));
            const float z = height * (1 - e);
            const Point at = along(under(was(middle), height), middle, e);
            for (std::size_t k = i; k < i + 4; ++k) {
                const Point p = list_.vertices[k].position;
                list_.vertices[k].position = project({at.x + p.x - middle.x, at.y + p.y - middle.y, z});
            }
            i += 3;
            continue;
        }
        const float e = eased(delayAt(v.position.y));
        const float z = height * (1 - e);
        const Point at = along(under(was(v.position), height), v.position, e);
        v.position = project({at.x, at.y, z});
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
        const float e = eased(delayAt(c.y));
        const Point start = was(c);
        if (four[0].material[2] > 1.5f) {
            const Point at = along(start, c, e);
            // a shadow: under its brick on the way, darker as it comes down
            for (int k = 0; k < 4; ++k) {
                four[k].center[0] += at.x - c.x;
                four[k].center[1] += at.y - c.y;
                four[k].position = {four[k].position.x + at.x - c.x, four[k].position.y + at.y - c.y};
                scaleAlpha(four[k].color, 0.3f + 0.7f * e);
            }
            continue;
        }
        const Point at = along(under(start, height), c, e);           // (its top face, as its 2D, where it was)
        // as large as it was showing there (the area stretched onto `from`; nearer the camera, it shows larger), growing to
        // its own size as it comes
        const float nearer = (eye.z - height) / eye.z;
        const float fromX = sx * nearer, fromY = sy * nearer;
        const float wide = four[0].halfSize[0] * (fromX + (1 - fromX) * e), tall = four[0].halfSize[1] * (fromY + (1 - fromY) * e);
        for (int k = 0; k < 4; ++k) {
            four[k].halfSize[0] = wide;
            four[k].halfSize[1] = tall;
            four[k].cornerRadius = std::min(four[k].cornerRadius, std::min(wide, tall));
            four[k].bevel = std::min(four[k].bevel, four[k].cornerRadius);
        }
        // a slight tilt of its own, undone as it lands
        const Float3 axis{next() * 2 - 1, next() * 2 - 1, 0};
        const Quaternion tilt = Quaternion::axisAngle(length(axis) > 0.01f ? axis : Float3{1, 0, 0}, 0.07f * (1 - e));
        for (int k = 0; k < 4; ++k)
            setPlace(four[k], {at.x, at.y, four[k].center[2] + height * (1 - e) + order}, tilt * rotationOf(four[k]));
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
    const Point middle = area.center();
    const float reach = std::max(1.0f, std::hypot(wide, tall) / 2);
    // when a place starts to come: falling, from the top of the area down; rising, from its middle out
    const auto delayAt = [&](Point at) {
        return fromBehind ? 0.35f * std::clamp(std::hypot(at.x - middle.x, at.y - middle.y) / reach, 0.0f, 1.0f)
                          : 0.40f * std::clamp((at.y - top) / tall, 0.0f, 1.0f);
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
        const float delay = (delayAt({cx, cy}) + 0.05f * next()) * free;
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
            angle = 0.6f * free * behind;
            alpha = smooth(u / 0.35f);
        }
        const Quaternion spin = Quaternion::axisAngle(turnAbout, angle);
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
    const auto moved = [&](Float3 p) { return at + up + lifted.turn.rotate(p - at); };
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
            if (faded)
                scaleAlpha(four[k].color, lifted.alpha);
        }
        placeQuad(four);
    }
}

} // namespace glw::paint
