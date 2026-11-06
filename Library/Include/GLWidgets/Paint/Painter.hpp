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
#include <GLWidgets/Paint/Transform.hpp>

#include <span>
#include <vector>

namespace glw::paint {

// The points of a cubic Bezier curve from `from` to `to`, pulled toward `c1` then `c2`: as many as it takes for its
// segments to stay within `tolerance` of the curve (in its units). Appended to `out`, `from` and `to` included.
GLW_EXPORT void flattenBezier(Point from, Point c1, Point c2, Point to, float tolerance, std::vector<Point>& out);

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
    // ... in a vertical gradient: `top` along the rectangle's top, `bottom` along its bottom, eased linearly between.
    void fillRoundedRect(const Rect& rect, float radius, Color top, Color bottom);
    void strokeRoundedRect(const Rect& rect, float radius, float width, Color top, Color bottom);
    // A blurred rounded rectangle: the shadow of something of that shape (offset the rectangle to cast it).
    void drawShadow(const Rect& rect, float radius, float blur, Color color);
    // A rounded rectangle as a relief: `height` dips above what it is on (below it when negative: sunk), its edge rounding
    // down over `bevel` dips, lit by the light of the target (Lighting, the Renderer's): its flat top in `color`.
    void fillRelief(const Rect& rect, float radius, float height, float bevel, Color color);
    void fillCircle(Point center, float radius, Color color);
    void strokeCircle(Point center, float radius, float width, Color color);

    // ---- lines and polygons ----

    void drawLine(Point a, Point b, float width, Color color);
    // Segments through `points` joined with miters (limited to twice the width at sharp turns), ends cut square.
    void drawPolyline(std::span<const Point> points, float width, Color color);
    // A cubic Bezier curve (flattenBezier), its segments within a quarter of a pixel of it at the scale it is drawn.
    void drawBezier(Point from, Point c1, Point c2, Point to, float width, Color color);
    // A convex polygon, its points in order (either direction).
    void fillConvexPolygon(std::span<const Point> points, Color color);
    // The area between a line through `points` (left to right) and the level `baseline` (a y): a chart's area under its
    // line. `far` where the line is farthest from the baseline, `near` at the baseline, eased between. Its edge along the
    // line has a fringe; its sides and the baseline none (what is beside it, a line over it, covers them).
    void fillArea(std::span<const Point> points, float baseline, Color far, Color near);
    // The area between two lines (each left to right, their points at any x: a band, a range): `top` at its highest,
    // `bottom` at its lowest, eased between. Its edges along the lines have fringes; its ends none.
    void fillBand(std::span<const Point> upper, std::span<const Point> lower, Color top, Color bottom);
    // A sector of a ring (a slice of a pie, of a donut): around `center` from the radius `inner` (0: to the center) to
    // `outer`, from the angle `start` over `sweep` (radians: 0 to the right, growing clockwise as y goes down). Its two
    // sides are moved in by `inset` dips each, parallel: sectors side by side `2 * inset` apart all along. `innerColor` at
    // the inner radius, `outerColor` at the outer, eased between. A whole turn is a ring, without sides.
    // Vertical: `innerColor` at the top of the ring, `outerColor` at its bottom instead (lit from above).
    enum class ArcShading : std::uint8_t { Radial, Vertical };
    void fillArc(Point center, float inner, float outer, float start, float sweep, float inset, Color innerColor, Color outerColor,
                 ArcShading shading = ArcShading::Radial);
    void fillArc(Point center, float inner, float outer, float start, float sweep, float inset, Color color) {
        fillArc(center, inner, outer, start, sweep, inset, color, color);
    }
    // ... a relief (as fillRelief is): `height` dips above what it is on, its edges rounding down over `bevel` dips, lit by
    // the light of the target; its flat top in `color`. A slice of a pie as a control of a lit style is.
    void fillArcRelief(Point center, float inner, float outer, float start, float sweep, float inset, float height, float bevel,
                       Color color);

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
    // The clip in effect, in the target's dips.
    const Rect& clip() const { return clips_.back(); }
    float pixelsPerDip() const { return pixelsPerDip_; }

private:
    // The command what is added next goes into: the last one when its clip and texture fit. `anyTexture`: what is added
    // does not read the texture (a box), so any command with the clip in effect will do.
    DrawCommand& command(TextureId texture, bool anyTexture);
    // A box in the target's dips.
    void box(const Rect& rect, float radius, float border, float blur, Color top, Color bottom);
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
    std::vector<Point> curve_;
};

} // namespace glw::paint
