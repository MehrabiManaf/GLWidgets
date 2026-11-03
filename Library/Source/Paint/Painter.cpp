// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Paint/Painter.hpp>

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>
#include <cmath>

namespace glw::paint {

namespace {

constexpr float boxKind = static_cast<float>(VertexKind::Box);

Vertex texturedVertex(Point position, Point uv, std::uint32_t color, VertexKind kind = VertexKind::Textured) {
    Vertex v;
    v.position = position;
    v.uv = uv;
    v.kind = static_cast<float>(kind);
    v.color = color;
    return v;
}

Point normalized(Point p) {
    const float length = std::sqrt(p.x * p.x + p.y * p.y);
    return length > 0 ? Point{p.x / length, p.y / length} : Point{};
}

// The offset of a joint between two segments with unit normals a and b: their mean, lengthened so that the sides of both
// segments meet (a miter), at most twice as long as a normal.
Point miter(Point a, Point b) {
    Point m = (a + b) * 0.5f;
    const float d2 = m.x * m.x + m.y * m.y;
    if (d2 > 1e-6f)
        m = m * std::min(1.0f / d2, 4.0f);
    return m;
}

} // namespace

Painter::Painter(DrawList& list, Rect bounds, float pixelsPerDip) : list_(list), pixelsPerDip_(pixelsPerDip) {
    GLW_EXPECTS(pixelsPerDip > 0);
    clips_.push_back(bounds);
    transforms_.push_back(Transform{});
}

Painter::~Painter() {
    GLW_ASSERT(clips_.size() == 1 && transforms_.size() == 1);      // every push popped
}

// ---- boxes ----

void Painter::fillRect(const Rect& rect, Color color) {
    fillRoundedRect(rect, 0, color);
}

void Painter::fillRoundedRect(const Rect& rect, float radius, Color color) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, 0, 0, color);
}

void Painter::strokeRoundedRect(const Rect& rect, float radius, float width, Color color) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, std::max(width * t.scale, 1e-3f), 0, color);
}

void Painter::drawShadow(const Rect& rect, float radius, float blur, Color color) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, 0, blur * t.scale, color);
}

void Painter::fillCircle(Point center, float radius, Color color) {
    fillRoundedRect({center.x - radius, center.y - radius, 2 * radius, 2 * radius}, radius, color);
}

void Painter::strokeCircle(Point center, float radius, float width, Color color) {
    strokeRoundedRect({center.x - radius, center.y - radius, 2 * radius, 2 * radius}, radius, width, color);
}

void Painter::box(const Rect& rect, float radius, float border, float blur, Color color) {
    if (rect.empty() || color.a <= 0)
        return;
    // room around the box for its blur and its anti-aliased edge
    const Rect quad = rect.inflated(blur + 1.0f / pixelsPerDip_);
    if (culled(quad))
        return;
    const Point center = rect.center();
    const Point half{rect.width / 2, rect.height / 2};
    radius = std::clamp(radius, 0.0f, std::min(half.x, half.y));
    const std::uint32_t packed = packColor(color);

    DrawCommand& cmd = command(whiteTexture, true);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    for (Point corner : {Point{quad.left(), quad.top()}, Point{quad.right(), quad.top()}, Point{quad.right(), quad.bottom()},
                         Point{quad.left(), quad.bottom()}}) {
        Vertex v;
        v.position = corner;
        v.uv = corner - center;
        v.halfSize = half;
        v.radius = radius;
        v.border = border;
        v.kind = boxKind;
        v.blur = blur;
        v.color = packed;
        list_.vertices.push_back(v);
    }
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

// ---- lines and polygons ----

void Painter::drawLine(Point a, Point b, float width, Color color) {
    const Point points[] = {a, b};
    drawPolyline(points, width, color);
}

void Painter::drawPolyline(std::span<const Point> points, float width, Color color) {
    if (points.size() < 2 || color.a <= 0 || width <= 0)
        return;
    transformed(points);
    const std::size_t n = points_.size();
    const float fringe = 1.0f / pixelsPerDip_;
    const float w = width * transform().scale;

    // a line thinner than a pixel is drawn a pixel wide, fainter
    float core = (w - fringe) / 2;
    if (w < fringe) {
        color.a *= w / fringe;
        core = 0;
    }
    const std::uint32_t solid = packColor(color);
    const std::uint32_t clear = packColor({color.r, color.g, color.b, 0});

    normals_.resize(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Point d = normalized(points_[i + 1] - points_[i]);
        normals_[i] = Point{-d.y, d.x};
    }

    DrawCommand& cmd = command(whiteTexture, false);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    // four vertices across each point: fringe, side, side, fringe
    for (std::size_t i = 0; i < n; ++i) {
        const Point m = i == 0 ? normals_[0] : i == n - 1 ? normals_[n - 2] : miter(normals_[i - 1], normals_[i]);
        const Point outer = m * (core + fringe);
        const Point inner = m * core;
        const Point p = points_[i];
        list_.vertices.push_back(texturedVertex(p + outer, {}, clear));
        list_.vertices.push_back(texturedVertex(p + inner, {}, solid));
        list_.vertices.push_back(texturedVertex(p - inner, {}, solid));
        list_.vertices.push_back(texturedVertex(p - outer, {}, clear));
    }
    // three quads between consecutive points: the outer fringe, the core, the other fringe
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const auto a = base + static_cast<std::uint32_t>(4 * i);
        const auto b = a + 4;
        for (std::uint32_t k = 0; k < 3; ++k)
            list_.indices.insert(list_.indices.end(), {a + k, a + k + 1, b + k + 1, a + k, b + k + 1, b + k});
    }
    cmd.indexCount += static_cast<std::uint32_t>(18 * (n - 1));
}

void flattenBezier(Point from, Point c1, Point c2, Point to, float tolerance, std::vector<Point>& out) {
    // Wang's formula: segments enough for the curve's largest second difference
    const auto length = [](Point p) { return std::sqrt(p.x * p.x + p.y * p.y); };
    const float bend = std::max(length(from - c1 * 2 + c2), length(c1 - c2 * 2 + to));
    const int segments = std::clamp(static_cast<int>(std::ceil(std::sqrt(0.75f * bend / std::max(tolerance, 1e-4f)))), 1, 512);
    out.push_back(from);
    for (int i = 1; i < segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments), u = 1 - t;
        const float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        out.push_back({a * from.x + b * c1.x + c * c2.x + d * to.x, a * from.y + b * c1.y + c * c2.y + d * to.y});
    }
    out.push_back(to);
}

void Painter::drawBezier(Point from, Point c1, Point c2, Point to, float width, Color color) {
    curve_.clear();
    flattenBezier(from, c1, c2, to, 0.25f / (transform().scale * pixelsPerDip_), curve_);
    drawPolyline(curve_, width, color);
}

void Painter::fillConvexPolygon(std::span<const Point> points, Color color) {
    if (points.size() < 3 || color.a <= 0)
        return;
    transformed(points);
    const std::size_t n = points_.size();
    const float fringe = 1.0f / pixelsPerDip_;

    // which side is outside: the right of the edges when the signed area is positive
    float area = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const Point a = points_[i], b = points_[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }
    const float side = area >= 0 ? 1.0f : -1.0f;
    normals_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Point d = normalized(points_[(i + 1) % n] - points_[i]);
        normals_[i] = Point{d.y * side, -d.x * side};
    }

    const std::uint32_t solid = packColor(color);
    const std::uint32_t clear = packColor({color.r, color.g, color.b, 0});
    DrawCommand& cmd = command(whiteTexture, false);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    // two vertices at each point: half a fringe inside (solid), half a fringe outside (clear)
    for (std::size_t i = 0; i < n; ++i) {
        const Point m = miter(normals_[(i + n - 1) % n], normals_[i]) * (fringe / 2);
        list_.vertices.push_back(texturedVertex(points_[i] - m, {}, solid));
        list_.vertices.push_back(texturedVertex(points_[i] + m, {}, clear));
    }
    std::uint32_t count = 0;
    for (std::uint32_t i = 1; i + 1 < n; ++i) {     // the inside, as a fan
        list_.indices.insert(list_.indices.end(), {base, base + 2 * i, base + 2 * (i + 1)});
        count += 3;
    }
    for (std::uint32_t i = 0; i < n; ++i) {         // the fringe, a quad per edge
        const std::uint32_t a = base + 2 * i, b = base + 2 * static_cast<std::uint32_t>((i + 1) % n);
        list_.indices.insert(list_.indices.end(), {a, a + 1, b + 1, a, b + 1, b});
        count += 6;
    }
    cmd.indexCount += count;
}

// ---- images ----

void Painter::drawImage(const Rect& rect, TextureId texture, Color tint, const Rect& uv) {
    quad(transform().map(rect), texture, tint, uv, VertexKind::Textured);
}

void Painter::drawMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv) {
    quad(targetRect, texture, color, uv, VertexKind::Mask);
}

void Painter::quad(const Rect& r, TextureId texture, Color color, const Rect& uv, VertexKind kind) {
    if (r.empty() || color.a <= 0 || culled(r))
        return;
    const std::uint32_t packed = packColor(color);
    DrawCommand& cmd = command(texture, false);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    list_.vertices.push_back(texturedVertex({r.left(), r.top()}, {uv.left(), uv.top()}, packed, kind));
    list_.vertices.push_back(texturedVertex({r.right(), r.top()}, {uv.right(), uv.top()}, packed, kind));
    list_.vertices.push_back(texturedVertex({r.right(), r.bottom()}, {uv.right(), uv.bottom()}, packed, kind));
    list_.vertices.push_back(texturedVertex({r.left(), r.bottom()}, {uv.left(), uv.bottom()}, packed, kind));
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

// ---- state ----

void Painter::pushClip(const Rect& rect) {
    clips_.push_back(clips_.back().intersected(transform().map(rect)));
}

void Painter::popClip() {
    GLW_EXPECTS(clips_.size() > 1);
    clips_.pop_back();
}

void Painter::pushTransform(const Transform& t) {
    GLW_EXPECTS(t.scale > 0);
    transforms_.push_back(transform() * t);
}

void Painter::popTransform() {
    GLW_EXPECTS(transforms_.size() > 1);
    transforms_.pop_back();
}

DrawCommand& Painter::command(TextureId texture, bool anyTexture) {
    const Rect& clip = clips_.back();
    if (!list_.commands.empty()) {
        DrawCommand& last = list_.commands.back();
        if (last.clip == clip && (anyTexture || last.texture == texture))
            return last;
        if (last.indexCount == 0) {     // nothing in it yet: it can still change
            last.clip = clip;
            last.texture = anyTexture ? whiteTexture : texture;
            return last;
        }
    }
    list_.commands.push_back(
        DrawCommand{static_cast<std::uint32_t>(list_.indices.size()), 0, clip, anyTexture ? whiteTexture : texture});
    return list_.commands.back();
}

bool Painter::culled(const Rect& bounds) const {
    return bounds.intersected(clips_.back()).empty();
}

void Painter::transformed(std::span<const Point> points) {
    const Transform& t = transform();
    points_.resize(points.size());
    std::transform(points.begin(), points.end(), points_.begin(), [&](Point p) { return t.map(p); });
}

} // namespace glw::paint
