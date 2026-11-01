// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Records 2D drawing into a DrawList. Coordinates are dips, through the current transform; what is drawn is cut to the
// current clip. A painter lives while one target is painted: it appends to its list, which may already hold commands.
//
// Boxes (rectangles, rounded rectangles, circles, their borders, shadows) are distance fields drawn by the shader: four
// vertices each, anti-aliased at any scale. Lines and polygons are tessellated here with a one-pixel anti-aliasing fringe.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>

#include <span>
#include <vector>

namespace glw::paint {

// A uniform scale then a translation: what a canvas needs to zoom and pan. No rotation, so boxes and clips stay aligned
// with the axes. Widths (of lines, borders) and radii scale with it.
struct Transform {
    float scale = 1;
    Point translation;

    Point map(Point p) const { return {p.x * scale + translation.x, p.y * scale + translation.y}; }
    Rect map(const Rect& r) const {
        return {r.x * scale + translation.x, r.y * scale + translation.y, r.width * scale, r.height * scale};
    }
    // `inner` first, then this one
    Transform operator*(const Transform& inner) const { return {scale * inner.scale, map(inner.translation)}; }
};

class GLW_EXPORT Painter {
public:
    // Paints into `list` for a target `bounds` big (dips; the first clip) with `pixelsPerDip` pixels per dip.
    Painter(DrawList& list, Rect bounds, float pixelsPerDip);
    ~Painter();
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;

    // ---- boxes ----

    void fillRect(const Rect& rect, Color color);
    void fillRoundedRect(const Rect& rect, float radius, Color color);
    // A border `width` wide inside `rect`.
    void strokeRoundedRect(const Rect& rect, float radius, float width, Color color);
    // A blurred rounded rectangle: the shadow of something of that shape (offset the rectangle to cast it).
    void drawShadow(const Rect& rect, float radius, float blur, Color color);
    void fillCircle(Point center, float radius, Color color);
    void strokeCircle(Point center, float radius, float width, Color color);

    // ---- lines and polygons ----

    void drawLine(Point a, Point b, float width, Color color);
    // Segments through `points` joined with miters (limited to twice the width at sharp turns), ends cut square.
    void drawPolyline(std::span<const Point> points, float width, Color color);
    // A convex polygon, its points in order (either direction).
    void fillConvexPolygon(std::span<const Point> points, Color color);

    // ---- images ----

    // `uv` is the part of the texture shown, in [0, 1].
    void drawImage(const Rect& rect, TextureId texture, Color tint = {1, 1, 1, 1}, const Rect& uv = {0, 0, 1, 1});
    // A coverage mask (the red channel of `texture`, as the glyph atlas has it) in `color`. `rect` is in the target's dips
    // and not transformed: what is drawn with it was placed on the target's pixels already (drawText does).
    void drawMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv);

    // ---- state ----

    // Cuts what follows to `rect` (intersected with the clip in effect) until popClip.
    void pushClip(const Rect& rect);
    void popClip();
    // Applies `transform` to what follows (after the transforms in effect) until popTransform.
    void pushTransform(const Transform& transform);
    void popTransform();

    const Transform& transform() const { return transforms_.back(); }
    float pixelsPerDip() const { return pixelsPerDip_; }

private:
    // The command what is added next goes into: the last one when its clip and texture fit. `anyTexture`: what is added
    // does not read the texture (a box), so any command with the clip in effect will do.
    DrawCommand& command(TextureId texture, bool anyTexture);
    // A box in the target's dips.
    void box(const Rect& rect, float radius, float border, float blur, Color color);
    // A textured quad in the target's dips.
    void quad(const Rect& rect, TextureId texture, Color color, const Rect& uv, VertexKind kind);
    bool culled(const Rect& bounds) const;
    void transformed(std::span<const Point> points);       // into points_

    DrawList& list_;
    float pixelsPerDip_;
    std::vector<Rect> clips_;
    std::vector<Transform> transforms_;
    std::vector<Point> points_;         // scratch, so that a polyline does not allocate each time
    std::vector<Point> normals_;
};

} // namespace glw::paint
