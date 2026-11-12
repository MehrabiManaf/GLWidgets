// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Paint/Painter.hpp>

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>
#include <cmath>

namespace glw::paint {

namespace {

constexpr float boxKind = static_cast<float>(VertexKind::Box);
constexpr float reliefKind = static_cast<float>(VertexKind::Relief);
constexpr float ringReliefKind = static_cast<float>(VertexKind::RingRelief);
constexpr float glassGlow = 9;           // dips a glossy glass relief's glow reaches out of it (Painter.hlsl's kind 6)
constexpr float turn = 6.28318530718f;

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
    const Point middle = bounds.center();
    camera_.eye = {middle.x, middle.y, std::max(1000.0f, 1.6f * std::max(bounds.width, bounds.height))};
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
    box(t.map(rect), radius * t.scale, 0, 0, color, color);
}

void Painter::fillRoundedRect(const Rect& rect, float radius, Color top, Color bottom) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, 0, 0, top, bottom);
}

void Painter::strokeRoundedRect(const Rect& rect, float radius, float width, Color top, Color bottom) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, std::max(width * t.scale, 1e-3f), 0, top, bottom);
}

void Painter::strokeRoundedRect(const Rect& rect, float radius, float width, Color color) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, std::max(width * t.scale, 1e-3f), 0, color, color);
}

void Painter::drawShadow(const Rect& rect, float radius, float blur, Color color) {
    const Transform& t = transform();
    box(t.map(rect), radius * t.scale, 0, blur * t.scale, color, color);
}

void Painter::fillRelief(const Rect& rect, float radius, float height, float bevel, Color color, const Joined& joined) {
    if (reliefBricks_ && height > 0 && !rect.empty()) {
        // a solid: its top where the relief's is, its body down into what it is on
        const float depth = std::min(height * reliefBricks_->depth, std::min(rect.width, rect.height) * 0.5f);
        const Point c = rect.center();
        drawBrick({.center = {c.x, c.y, -depth / 2}, .halfSize = {rect.width / 2, rect.height / 2, depth / 2}, .cornerRadius = radius,
                   .bevel = reliefBricks_->bevel >= 0 ? reliefBricks_->bevel : bevel, .color = color,
                   .roughness = reliefBricks_->roughness, .shadow = 0, .reliefLight = true,
                   .glass = reliefBricks_->glass && height >= reliefBricks_->glassFrom, .joined = joined});
        return;
    }
    const Transform& t = transform();
    const Rect r = t.map(rect);
    if (r.empty() || color.a <= 0)
        return;
    // of glass: it reads what is under it (as what it is joined onto reads, when it is); glossy, its glow around it too
    const bool glass = glassReliefs_ && height >= glassReliefs_->from;
    const bool glossy = glass && glassReliefs_->glossy;
    const Rect quad = r.inflated(glossy ? glassGlow * t.scale : 1.0f / pixelsPerDip_);
    if (culled(quad))
        return;
    const Point center = r.center();
    const Point half{r.width / 2, r.height / 2};
    DrawCommand& cmd = glass ? command(joined.onto ? joinedBackdropTexture : backdropTexture, false) : command(whiteTexture, true);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    const std::uint32_t packed = packColor(color);
    for (Point corner : {Point{quad.left(), quad.top()}, Point{quad.right(), quad.top()}, Point{quad.right(), quad.bottom()},
                         Point{quad.left(), quad.bottom()}}) {
        Vertex v;
        v.position = corner;
        v.uv = corner - center;
        v.halfSize = half;
        v.radius = std::clamp(radius * t.scale, 0.0f, std::min(half.x, half.y));
        v.border = std::max(bevel * t.scale, 0.5f);         // the bevel
        // (glossy glass: its kind and a quarter, Painter.hlsl's kind 6)
        v.kind = glass ? static_cast<float>(VertexKind::GlassRelief) + (glossy ? 0.25f : 0.0f) : reliefKind;
        v.blur = height * t.scale;                          // the height
        v.color = packed;
        list_.vertices.push_back(v);
    }
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

void Painter::fillSlab(const Rect& rect, float radius, float height, float bevel, float edge, Color color) {
    if (!reliefBricks_ || height <= 0 || edge <= 0 || rect.empty()) {
        fillRelief(rect, radius, height, bevel, color);
        return;
    }
    // its top where the relief's is, its body down into what it is on, its edge rounding down over `edge` (a brick's
    // bevel is no more than half as deep as it is)
    const float depth = std::min(std::max(height * reliefBricks_->depth, edge * 2.4f), std::min(rect.width, rect.height) * 0.5f);
    const Point c = rect.center();
    drawBrick({.center = {c.x, c.y, -depth / 2}, .halfSize = {rect.width / 2, rect.height / 2, depth / 2}, .cornerRadius = std::max(radius, edge * 1.6f),
               .bevel = edge, .color = color, .roughness = reliefBricks_->roughness, .shadow = 0, .reliefLight = true});
}

void Painter::fillBackdrop(const Rect& rect, const Backdrop& backdrop) {
    const Rect r = transform().map(rect);
    if (r.empty() || culled(r))
        return;
    DrawCommand& cmd = command(whiteTexture, true);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    const std::uint32_t packed = packColor(backdrop.base);
    const float scale = transform().scale;
    for (Point corner : {Point{r.left(), r.top()}, Point{r.right(), r.top()}, Point{r.right(), r.bottom()}, Point{r.left(), r.bottom()}}) {
        Vertex v;
        v.position = corner;
        v.uv = corner;                              // where it is: its pattern
        v.halfSize = {std::max(backdrop.tile * scale, 16.0f), backdrop.vivid};
        v.radius = backdrop.hue;
        v.kind = static_cast<float>(VertexKind::Backdrop);
        v.color = packed;
        list_.vertices.push_back(v);
    }
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

void Painter::fillCircle(Point center, float radius, Color color) {
    const Transform& t = transform();
    box(t.map(Rect{center.x - radius, center.y - radius, 2 * radius, 2 * radius}), radius * t.scale, 0, 0, color, color);
}

void Painter::strokeCircle(Point center, float radius, float width, Color color) {
    const Transform& t = transform();
    box(t.map(Rect{center.x - radius, center.y - radius, 2 * radius, 2 * radius}), radius * t.scale,
        std::max(width * t.scale, 1e-3f), 0, color, color);
}

void Painter::box(const Rect& rect, float radius, float border, float blur, Color top, Color bottom) {
    if (rect.empty() || (top.a <= 0 && bottom.a <= 0))
        return;
    // room around the box for its blur and its anti-aliased edge
    const Rect quad = rect.inflated(blur + 1.0f / pixelsPerDip_);
    if (culled(quad))
        return;
    const Point center = rect.center();
    const Point half{rect.width / 2, rect.height / 2};
    radius = std::clamp(radius, 0.0f, std::min(half.x, half.y));
    // the gradient carried on to the quad's edges, past the box's by its margin (vertex colors are interpolated)
    const bool flat = top == bottom;
    const auto at = [&](float y) {
        if (flat)
            return packColor(top);
        const float f = (y - rect.top()) / rect.height;
        // premultiplied, as the shader interpolates: a color fading to transparent keeps its hue
        const Color a{top.r * top.a, top.g * top.a, top.b * top.a, top.a};
        const Color b{bottom.r * bottom.a, bottom.g * bottom.a, bottom.b * bottom.a, bottom.a};
        Color c = mix(a, b, std::clamp(f, -1.0f, 2.0f));
        c.a = std::clamp(c.a, 0.0f, 1.0f);
        if (c.a > 0)
            c = {c.r / c.a, c.g / c.a, c.b / c.a, c.a};
        return packColor(c);
    };
    const std::uint32_t topColor = at(quad.top()), bottomColor = at(quad.bottom());

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
        v.color = corner.y < center.y ? topColor : bottomColor;
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
    // a segment of no length (a point twice) goes the way of the one before it (after it, at the start)
    for (std::size_t i = 1; i + 1 < n; ++i)
        if (normals_[i].x == 0 && normals_[i].y == 0)
            normals_[i] = normals_[i - 1];
    for (std::size_t i = n - 1; i-- > 1;)
        if (normals_[i - 1].x == 0 && normals_[i - 1].y == 0)
            normals_[i - 1] = normals_[i];

    DrawCommand& cmd = command(whiteTexture, false);
    std::uint32_t count = 0;
    // four vertices across a point, offset by `m` (a normal, or a miter): fringe, side, side, fringe
    const auto across = [&](Point p, Point m) {
        const auto at = static_cast<std::uint32_t>(list_.vertices.size());
        const Point outer = m * (core + fringe);
        const Point inner = m * core;
        list_.vertices.push_back(texturedVertex(p + outer, {}, clear));
        list_.vertices.push_back(texturedVertex(p + inner, {}, solid));
        list_.vertices.push_back(texturedVertex(p - inner, {}, solid));
        list_.vertices.push_back(texturedVertex(p - outer, {}, clear));
        return at;
    };
    // three quads between two of those: the outer fringe, the core, the other fringe
    const auto between = [&](std::uint32_t a, std::uint32_t b) {
        for (std::uint32_t k = 0; k < 3; ++k)
            list_.indices.insert(list_.indices.end(), {a + k, a + k + 1, b + k + 1, a + k, b + k + 1, b + k});
        count += 18;
    };
    std::uint32_t previous = across(points_[0], normals_[0]);
    for (std::size_t i = 1; i < n; ++i) {
        const Point p = points_[i];
        if (i == n - 1) {
            between(previous, across(p, normals_[n - 2]));
            break;
        }
        const Point a = normals_[i - 1], b = normals_[i];
        const Point mean = (a + b) * 0.5f;
        const float d2 = mean.x * mean.x + mean.y * mean.y;
        if (d2 >= 0.25f) {
            // a turn of 120 degrees or less: mitered (the sides meet, at most twice as far out as a normal)
            const std::uint32_t at = across(p, mean * (1.0f / d2));
            between(previous, at);
            previous = at;
        } else {
            // sharper (a jagged signal turning back): beveled. A miter would be far out, and one held in pinches the
            // line to nothing there: the end of one segment and the start of the next apart, the wedge between filled
            const std::uint32_t end = across(p, a), start = across(p, b);
            between(previous, end);
            between(end, start);
            previous = start;
        }
    }
    cmd.indexCount += count;
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

void Painter::fillArea(std::span<const Point> points, float baseline, Color far, Color near) {
    if (points.size() < 2 || (far.a <= 0 && near.a <= 0))
        return;
    transformed(points);
    const std::size_t n = points_.size();
    const float base = transform().map(Point{0, baseline}).y;
    const float fringe = 1.0f / pixelsPerDip_;
    // the color at a height: `far` at the point farthest from the baseline, `near` at it
    float reach = 0;
    for (const Point& p : points_)
        if (std::abs(p.y - base) > std::abs(reach))
            reach = p.y - base;
    const auto colorAt = [&](float y) {
        const float t = reach != 0 ? std::clamp((y - base) / reach, 0.0f, 1.0f) : 1.0f;
        return mix(near, far, t);
    };
    DrawCommand& cmd = command(whiteTexture, false);
    const auto first = static_cast<std::uint32_t>(list_.vertices.size());
    const std::uint32_t atBase = packColor(near);
    // three at each point: its fringe (away from the baseline), the point, the baseline under it
    for (const Point& p : points_) {
        const Color c = colorAt(p.y);
        const float out = p.y <= base ? -fringe : fringe;
        list_.vertices.push_back(texturedVertex({p.x, p.y + out}, {}, packColor(withAlpha(c, 0))));
        list_.vertices.push_back(texturedVertex(p, {}, packColor(c)));
        list_.vertices.push_back(texturedVertex({p.x, base}, {}, atBase));
    }
    for (std::uint32_t i = 0; i + 1 < n; ++i) {
        const std::uint32_t a = first + 3 * i, b = a + 3;
        list_.indices.insert(list_.indices.end(), {a, a + 1, b + 1, a, b + 1, b, a + 1, a + 2, b + 2, a + 1, b + 2, b + 1});
    }
    cmd.indexCount += static_cast<std::uint32_t>(12 * (n - 1));
}

void Painter::fillBand(std::span<const Point> upper, std::span<const Point> lower, Color top, Color bottom) {
    if (upper.size() < 2 || lower.size() < 2 || (top.a <= 0 && bottom.a <= 0))
        return;
    const Transform& t = transform();
    curve_.resize(upper.size());
    std::transform(upper.begin(), upper.end(), curve_.begin(), [&](Point p) { return t.map(p); });
    transformed(lower);
    float highest = curve_[0].y, lowest = curve_[0].y;
    for (const std::vector<Point>* chain : {&curve_, &points_})
        for (const Point& p : *chain) {
            highest = std::min(highest, p.y);
            lowest = std::max(lowest, p.y);
        }
    const auto colorAt = [&](float y, bool solid) {
        const Color c = mix(top, bottom, lowest > highest ? (y - highest) / (lowest - highest) : 0.0f);
        return packColor(solid ? c : withAlpha(c, 0));
    };
    const float fringe = 1.0f / pixelsPerDip_;
    DrawCommand& cmd = command(whiteTexture, false);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    const auto nu = static_cast<std::uint32_t>(curve_.size()), nl = static_cast<std::uint32_t>(points_.size());
    // the upper line: its fringe above each point, the point; the lower: the point, its fringe below
    for (const Point& p : curve_) {
        list_.vertices.push_back(texturedVertex({p.x, p.y - fringe}, {}, colorAt(p.y, false)));
        list_.vertices.push_back(texturedVertex(p, {}, colorAt(p.y, true)));
    }
    const std::uint32_t below = base + 2 * nu;
    for (const Point& p : points_) {
        list_.vertices.push_back(texturedVertex(p, {}, colorAt(p.y, true)));
        list_.vertices.push_back(texturedVertex({p.x, p.y + fringe}, {}, colorAt(p.y, false)));
    }
    const auto u = [&](std::uint32_t i) { return base + 2 * i + 1; };
    const auto l = [&](std::uint32_t j) { return below + 2 * j; };
    std::uint32_t count = 0;
    // between them: on along whichever line's next point comes first (both left to right)
    for (std::uint32_t i = 0, j = 0; i + 1 < nu || j + 1 < nl;) {
        if (j + 1 >= nl || (i + 1 < nu && curve_[i + 1].x <= points_[j + 1].x)) {
            list_.indices.insert(list_.indices.end(), {u(i), u(i + 1), l(j)});
            ++i;
        } else {
            list_.indices.insert(list_.indices.end(), {u(i), l(j + 1), l(j)});
            ++j;
        }
        count += 3;
    }
    for (std::uint32_t i = 0; i + 1 < nu; ++i) {
        const std::uint32_t a = base + 2 * i, b = a + 2;
        list_.indices.insert(list_.indices.end(), {a, a + 1, b + 1, a, b + 1, b});
        count += 6;
    }
    for (std::uint32_t j = 0; j + 1 < nl; ++j) {
        const std::uint32_t a = below + 2 * j, b = a + 2;
        list_.indices.insert(list_.indices.end(), {a, a + 1, b + 1, a, b + 1, b});
        count += 6;
    }
    cmd.indexCount += count;
}

void Painter::fillArc(Point center, float inner, float outer, float start, float sweep, float inset, Color innerColor,
                      Color outerColor, ArcShading shading) {
    if (sweep <= 0 || outer <= inner || outer <= 0 || (innerColor.a <= 0 && outerColor.a <= 0))
        return;
    const Transform& t = transform();
    const Point c = t.map(center);
    const bool whole = sweep >= turn - 1e-4f;
    sweep = std::min(sweep, turn);
    float r0 = std::max(0.0f, inner) * t.scale;
    const float r1 = outer * t.scale;
    const float gap = whole ? 0.0f : std::max(0.0f, inset) * t.scale;
    if (gap > 0) {
        // where its sides, moved in, meet: not nearer the center
        r0 = std::max(r0, gap / std::sin(std::min(sweep, turn / 2) / 2));
        if (r0 >= r1)
            return;
    }
    if (culled({c.x - r1, c.y - r1, 2 * r1, 2 * r1}))
        return;
    const float fringe = 1.0f / pixelsPerDip_;
    // the angle a side is moved in by at a radius
    const auto shift = [&](float r) { return gap > 0 && r > 0 ? std::asin(std::min(1.0f, gap / r)) : 0.0f; };
    const float in0 = shift(r0), out0 = shift(r1);
    // segments enough for the outer arc to stay within a quarter of a pixel of the circle
    const float pixels = r1 * pixelsPerDip_;
    const float step = pixels > 0.25f ? 2 * std::acos(std::max(0.0f, 1 - 0.25f / pixels)) : sweep;
    const int segments = std::clamp(static_cast<int>(std::ceil(sweep / std::max(step, 1e-3f))), 1, 1024);
    // its inner edge a point (a pie's slice): no fringe there
    const bool point = r0 <= fringe;

    // a vertex's color: by its side (radial), or by its height in the ring (vertical); clear in a fringe
    const bool vertical = shading == ArcShading::Vertical;
    const auto colorAt = [&](Point at, bool innerSide, bool solid) {
        Color color = vertical ? mix(innerColor, outerColor, std::clamp((at.y - (c.y - r1)) / (2 * r1), 0.0f, 1.0f))
                               : innerSide ? innerColor : outerColor;
        return packColor(solid ? color : withAlpha(color, 0));
    };
    DrawCommand& cmd = command(whiteTexture, false);
    const auto first = static_cast<std::uint32_t>(list_.vertices.size());
    const auto along = [](float angle) { return Point{std::cos(angle), std::sin(angle)}; };
    const auto put = [&](Point at, bool innerSide, bool solid) { list_.vertices.push_back(texturedVertex(at, {}, colorAt(at, innerSide, solid))); };
    // four across each step: the inner fringe, the inner edge, the outer edge, the outer fringe
    for (int k = 0; k <= segments; ++k) {
        const float f = static_cast<float>(k) / static_cast<float>(segments);
        const Point din = along(start + in0 + (sweep - 2 * in0) * f), dout = along(start + out0 + (sweep - 2 * out0) * f);
        put(c + din * (point ? r0 : std::max(0.0f, r0 - fringe / 2)), true, point);
        put(c + din * (point ? r0 : r0 + fringe / 2), true, true);
        put(c + dout * (r1 - fringe / 2), false, true);
        put(c + dout * (r1 + fringe / 2), false, false);
    }
    std::uint32_t count = 0;
    const auto strip = [&](std::uint32_t a, std::uint32_t b) {
        for (std::uint32_t k = 0; k < 3; ++k)
            list_.indices.insert(list_.indices.end(), {a + k, a + k + 1, b + k + 1, a + k, b + k + 1, b + k});
        count += 18;
    };
    for (std::uint32_t k = 0; k < static_cast<std::uint32_t>(segments); ++k)
        strip(first + 4 * k, first + 4 * (k + 1));
    if (!whole) {
        // the sides' fringes: each column of an end moved out of the sector by a fringe, clear
        const auto cap = [&](std::uint32_t column, Point out) {
            const auto at = static_cast<std::uint32_t>(list_.vertices.size());
            for (std::uint32_t k = 0; k < 4; ++k) {
                const Point moved = list_.vertices[column + k].position + out * fringe;
                put(moved, k < 2, false);
            }
            strip(column, at);
        };
        const float end = start + sweep;
        cap(first, {std::sin(start), -std::cos(start)});
        cap(first + 4 * static_cast<std::uint32_t>(segments), {-std::sin(end), std::cos(end)});
    }
    cmd.indexCount += count;
}

void Painter::fillArcRelief(Point center, float inner, float outer, float start, float sweep, float inset, float height,
                            float bevel, Color color, float corner) {
    if (sweep <= 0 || outer <= inner || outer <= 0 || color.a <= 0)
        return;
    if (reliefBricks_ && height > 0) {
        arcBrick(center, inner, outer, start, sweep, inset, height, bevel, color, corner);
        return;
    }
    const Transform& t = transform();
    const Point c = t.map(center);
    const float r0 = std::max(0.0f, inner) * t.scale, r1 = outer * t.scale;
    if (culled({c.x - r1, c.y - r1, 2 * r1, 2 * r1}))
        return;
    const bool whole = sweep >= turn - 1e-4f;
    const float half = std::min(sweep, turn) / 2;
    const float gap = whole ? 0.0f : std::clamp(inset * t.scale, 0.0f, 255.0f);
    // in its own frame (its middle along +x): what its arcs and sides reach, and a pixel more
    float lowX = 0, highX = 0, lowY = 0, highY = 0;
    bool any = false;
    const auto reach = [&](float r, float angle) {
        const float x = r * std::cos(angle), y = r * std::sin(angle);
        lowX = any ? std::min(lowX, x) : x;
        highX = any ? std::max(highX, x) : x;
        lowY = any ? std::min(lowY, y) : y;
        highY = any ? std::max(highY, y) : y;
        any = true;
    };
    for (float r : {r0, r1}) {
        for (float angle : {-half, 0.0f, half})
            reach(r, angle);
        if (half > turn / 4) {
            reach(r, turn / 4);
            reach(r, -turn / 4);
        }
        if (half >= turn / 2 - 1e-4f)
            reach(r, turn / 2);
    }
    // of glass: it reads what is under it; glossy, its glow around it too
    const bool glass = glassReliefs_ && height >= glassReliefs_->from;
    const bool glossy = glass && glassReliefs_->glossy;
    const float margin = (glossy ? glassGlow * t.scale : 1.0f / pixelsPerDip_) + 0.5f;
    lowX -= margin;
    lowY -= margin;
    highX += margin;
    highY += margin;
    const float middle = start + half;
    const Point axis{std::cos(middle), std::sin(middle)}, across{-axis.y, axis.x};
    DrawCommand& cmd = glass ? command(backdropTexture, false) : command(whiteTexture, true);
    const auto base = static_cast<std::uint32_t>(list_.vertices.size());
    const std::uint32_t packed = packColor(color);
    // its half angle and how far its sides are in, packed (see Painter.hlsl's kind 4); a whole ring, no sides
    const float sides = whole ? -1.0f : half + 8.0f * std::round(gap * 16);
    // its bevel and its corners' radius, packed too (eighths of a dip above a bevel under 256: exact enough in a float)
    const float corners = std::round(std::clamp(corner * t.scale, 0.0f, 120.0f) * 8);
    const float bevelAndCorners = std::clamp(bevel * t.scale, 0.5f, 255.0f) + 256.0f * corners;
    for (Point local : {Point{lowX, lowY}, Point{highX, lowY}, Point{highX, highY}, Point{lowX, highY}}) {
        Vertex v;
        v.position = c + axis * local.x + across * local.y;
        v.uv = local;
        v.halfSize = {r0, r1};
        v.radius = sides;
        v.border = bevelAndCorners;
        v.kind = glass ? static_cast<float>(VertexKind::GlassRingRelief) + (glossy ? 0.25f : 0.0f) : ringReliefKind;
        v.blur = height * t.scale;                         // the height
        v.color = packed;
        list_.vertices.push_back(v);
    }
    list_.indices.insert(list_.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    cmd.indexCount += 6;
}

// ---- images ----

void Painter::drawImage(const Rect& rect, TextureId texture, Color tint, const Rect& uv) {
    quad(transform().map(rect), texture, tint, uv, VertexKind::Textured);
}

void Painter::drawMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv) {
    quad(targetRect, texture, color, uv, VertexKind::Mask);
}

void Painter::drawFieldMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv, Point range) {
    if (targetRect.empty() || color.a <= 0 || culled(targetRect))
        return;
    const std::size_t first = list_.vertices.size();
    quad(targetRect, texture, color, uv, VertexKind::Field);
    for (std::size_t i = first; i < list_.vertices.size(); ++i)
        list_.vertices[i].halfSize = range;
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

DrawCommand& Painter::command(TextureId texture, bool anyTexture, bool scene) {
    const Rect& clip = clips_.back();
    if (!list_.commands.empty()) {
        DrawCommand& last = list_.commands.back();
        if (last.clip == clip && last.scene == scene && (anyTexture || last.texture == texture))
            return last;
        if (last.indexCount == 0) {     // nothing in it yet: it can still change
            last.clip = clip;
            last.texture = anyTexture ? whiteTexture : texture;
            last.scene = scene;
            return last;
        }
    }
    list_.commands.push_back(
        DrawCommand{static_cast<std::uint32_t>(list_.indices.size()), 0, clip, anyTexture ? whiteTexture : texture, scene});
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
