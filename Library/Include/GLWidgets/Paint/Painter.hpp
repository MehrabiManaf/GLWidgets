// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Records 2D drawing into a DrawList. Coordinates are dips, through the current transform; what is drawn is cut to the
// current clip. A painter lives while one target is painted: it appends to its list, which may already hold commands.
//
// Boxes (rectangles, rounded rectangles, circles, their borders, shadows) are distance fields drawn by the shader: four
// vertices each, anti-aliased at any scale. Lines and polygons are tessellated here with a one-pixel anti-aliasing fringe.
// Bricks are solids in space (Scene.hpp), seen from a camera above the target: drawn, and what was drawn moved in space.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Paint/Scene.hpp>
#include <GLWidgets/Paint/Transform.hpp>

#include <optional>
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
    // down over `bevel` dips, lit by the light of the target (Lighting, the Renderer's): its flat top in `color`. Raised,
    // a brick instead while the painter draws them so (setReliefBricks).
    // `joined`: as a brick, its sides joined to another (Scene.hpp's Joined).
    void fillRelief(const Rect& rect, float radius, float height, float bevel, Color color, const Joined& joined = {});
    // ... a slab (a popup's): as a brick, as deep as it needs to be for the top's edge to round down over `edge` dips.
    void fillSlab(const Rect& rect, float radius, float height, float bevel, float edge, Color color);
    void fillCircle(Point center, float radius, Color color);
    void strokeCircle(Point center, float radius, float width, Color color);

    // ---- lines and polygons ----

    void drawLine(Point a, Point b, float width, Color color);
    // Segments through `points` joined with miters (limited to twice the width at sharp turns), ends cut square.
    void drawPolyline(std::span<const Point> points, float width, Color color);
    // The outline of a closed polygon (its last point back to its first: not repeated), joined at every corner as a
    // polyline's turns are - its first corner too, which a polyline back to its start leaves open.
    void drawPolygon(std::span<const Point> points, float width, Color color);
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
    // Its corners (where a side meets an arc) rounded over `corner` dips (no more than half as far as across it there).
    void fillArcRelief(Point center, float inner, float outer, float start, float sweep, float inset, float height, float bevel,
                       Color color, float corner = 0);

    // ---- images ----

    // `uv` is the part of the texture shown, in [0, 1].
    void drawImage(const Rect& rect, TextureId texture, Color tint = {1, 1, 1, 1}, const Rect& uv = {0, 0, 1, 1});
    // A coverage mask (the red channel of `texture`, as the glyph atlas has it) in `color`. `rect` is in the target's dips
    // and not transformed: what is drawn with it was placed on the target's pixels already (drawText does).
    void drawMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv);
    // A glyph's multi-channel distance field (GlyphAtlas::fieldGlyph) in `color`, sharp at any size: `range` how far its
    // distances reach, in `uv`'s units (across, down). `rect` as drawMask's.
    void drawFieldMask(const Rect& targetRect, TextureId texture, Color color, const Rect& uv, Point range);

    // ---- solids (Scene.hpp) ----

    // A brick where it is in space (its center's x and y through the current transform, its sizes and z scaled with it),
    // seen from the camera; its shadow on the target first (drawBrickShadow), unless its `shadow` is 0.
    void drawBrick(const Brick& brick);
    // Its shadow on the target, cast from the scene's light (above, to the upper left), softer as it is higher. Bricks of a
    // group (that may stand over one another): all their shadows first, then the bricks with `shadow` 0, so that a shadow
    // falls on the target and not on the bricks drawn before it.
    void drawBrickShadow(const Brick& brick);
    // A glyph's multi-channel distance field (GlyphAtlas::fieldGlyph) printed on a brick's top face (`brick` as drawBrick
    // takes it): `rect` on the face, from its middle (dips of the brick, before the transform), `uv` the part of `texture`,
    // `range` how far its distances reach in uv's units, in `color` lit as the face is. Drawn after its brick, it turns,
    // leans and moves with it, sharp at any size (text::drawTextOnBrick).
    void drawBrickDecal(const Brick& brick, const Rect& rect, TextureId texture, const Rect& uv, Point range, Color color);
    // Raised reliefs of glass, flat (none: of their color): what is under them showing through (GlassReliefs).
    void setGlassReliefs(std::optional<GlassReliefs> glass) { glassReliefs_ = glass; }
    // A relief `height` high is of glass (setGlassReliefs).
    bool glassAt(float height) const { return glassReliefs_ && height >= glassReliefs_->from; }
    // `rect` filled with `backdrop` (its pattern where it is on the target: the same wherever it is drawn).
    void fillBackdrop(const Rect& rect, const Backdrop& backdrop);
    // Raised reliefs (fillRelief with a height above 0) drawn as bricks (none: as reliefs).
    void setReliefBricks(std::optional<ReliefBricks> bricks) { reliefBricks_ = bricks; }
    const std::optional<ReliefBricks>& reliefBricks() const { return reliefBricks_; }
    // Its bricks tested against one another by depth (they may pass through one another: a pile fallen); without it they
    // are drawn in their order, as everything else.
    void setDepthTest(bool depth) { list_.depth = depth; }

    // What was drawn since a Mark (taken before it) can be moved in space afterwards.
    struct Mark {
        std::size_t vertices = 0, sceneVertices = 0, commands = 0;
    };
    Mark mark();
    // ... without a command of its own: for place only (not placeClips, unclip), so that many small pieces each moved
    // (a chart's bars, its tiles) are drawn in as few commands as they would be still.
    Mark markHere() const;
    // What was drawn since `mark` falling in from in front of the target, `t` of the way (0 to 1): each brick (and what
    // is printed on it) from near the camera, turning about an axis of its own, a little aside, down to where it is, one
    // after another from the top of `area` down; its shadow coming as it does; the 2D of it (text, lines) showing at the
    // end; bricks cut by no clip meanwhile.
    // From `from` (the target's dips: where it was), each point of it starts where it was there (`area` made as small as
    // fits in `from`, in its middle), a little nearer the camera, and moves down to its place with a slight tilt, the 2D
    // with it (its shadows coming as it lands), its slab first, then its tab, then the rest a row after another.
    void fallIn(const Mark& mark, float t, const Rect& area, std::optional<Rect> from = std::nullopt);
    // ... rising from far behind the target instead (small, toward the camera's middle), nearer and nearer, slowing as it
    // comes, one after another from the middle of `area` out; the 2D of each place showing as what is under it comes.
    void riseIn(const Mark& mark, float t, const Rect& area);
    // Other ways to come, all of it eased by itself from `t` 0 to 1, cut by no clip meanwhile:
    // from `from` (where it was, as fallIn takes it), swung down onto its place as a lid hinged on its top edge (a little
    // past flat, and back), growing to its size;
    void swingIn(const Mark& mark, float t, const Rect& area, const Rect& from);
    // ... its pieces scattered about where they were, each a little turned and nearer the camera, gathering into their
    // places one after another (its 2D showing as they come);
    void gatherIn(const Mark& mark, float t, const Rect& area, const Rect& from);
    // in its place, growing out of a smaller one a little behind the target, a little past its size and back, the light
    // catching it on the way;
    void popIn(const Mark& mark, float t, const Rect& area);
    // ... turned to face the one looking from nearly edge on, about its upright middle, as a card turns.
    void turnIn(const Mark& mark, float t, const Rect& area);
    // ... folded back on its `hinge` edge (away from the one looking), opening out onto its place, a little past it.
    void unfoldIn(const Mark& mark, float t, const Rect& area, Edge hinge);
    // What was drawn since `mark` moved in three dimensions about `pivot` (dips of the target, on the target's plane): its
    // 2D seen where it goes (its vertices projected), its bricks lifted and turned with it; shadows where they are.
    void place(const Mark& mark, Point pivot, const Lifted& lifted);
    // The camera bricks are seen from: by default above the middle of the target, as far above it as 1.6 times its longer
    // side (at least 1000 dips).
    void setCamera(Camera camera) { camera_ = camera; }
    const Camera& camera() const { return camera_; }
    // Where a point in space shows on the target, seen from the camera (the target's dips).
    Point project(Float3 point) const;

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
    // does not read the texture (a box), so any command with the clip in effect will do. `scene`: of bricks.
    DrawCommand& command(TextureId texture, bool anyTexture, bool scene = false);
    // A scene quad: `v` at each corner of `quad`, into the command of `texture`.
    void sceneQuad(const Rect& quad, SceneVertex v, TextureId texture, bool anyTexture);
    // fallIn's and riseIn's.
    void arrive(const Mark& mark, float t, const Rect& area, bool fromBehind);
    // What was drawn since `mark` cut by no clip (moving out of its place).
    void unclip(const Mark& mark);
    // ... cut where its clips went, moved as place moves it (what was cut where it is, cut as it moves).
    void placeClips(const Mark& mark, Point pivot, const Lifted& lifted);
    void arriveFrom(const Mark& mark, float t, const Rect& area, const Rect& from);
    // A brick's (a decal's) four scene vertices placed again around where it shows.
    void placeQuad(SceneVertex* four) const;
    // The scene vertex of `brick` through the current transform, of `kind` (its eye and pixels per dip set; no position).
    SceneVertex sceneVertex(const Brick& brick, float kind) const;
    // fillArcRelief's solid (with relief bricks): a brick bent around the ring, its top where the relief's is.
    void arcBrick(Point center, float inner, float outer, float start, float sweep, float inset, float height, float bevel, Color color,
                  float corner);
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
    void stroke(std::span<const Point> points, float width, Color color, bool closed);
    std::vector<Point> curve_;
    Camera camera_;
    std::optional<ReliefBricks> reliefBricks_;
    std::optional<GlassReliefs> glassReliefs_;
};

} // namespace glw::paint
