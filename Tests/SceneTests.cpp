// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Solids (Paint/Scene.hpp): bricks as a Painter records them, what was drawn moved in space, and a style that draws its
// raised controls as bricks (Solid).
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Paint/Scene.hpp>
#include <GLWidgets/Text/DrawText.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>

using namespace glw;
using namespace glw::paint;

namespace {

bool near(float a, float b, float within = 1e-4f) {
    return std::abs(a - b) <= within;
}

} // namespace

TEST(aQuaternionTurnsAsItSays) {
    // a quarter turn about z: x to y (y down the screen: clockwise)
    const Float3 v = Quaternion::axisAngle({0, 0, 1}, std::numbers::pi_v<float> / 2).rotate({1, 0, 0});
    CHECK(near(v.x, 0) && near(v.y, 1) && near(v.z, 0));
    // then back
    const Quaternion q = Quaternion::tilt(0.3f, -0.7f, 1.1f);
    const Float3 w = q.inverse().rotate(q.rotate({2, 3, 4}));
    CHECK(near(w.x, 2) && near(w.y, 3) && near(w.z, 4));
}

TEST(aBrickIsAQuadOfItsOwnAfterItsShadow) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.fillRect({0, 0, 100, 100}, {1, 0, 0, 1});
        p.drawBrick({.center = {400, 300, 5}, .halfSize = {40, 15, 5}});
        p.fillRect({0, 0, 10, 10}, {0, 1, 0, 1});
    }
    // its shadow, then it: four vertices of the scene's each, in a command of their own between the painter's
    CHECK(list.sceneVertices.size() == 8 && list.sceneVertices[0].material[2] == shadowKind && list.sceneVertices[4].material[2] == brickKind);
    CHECK(list.commands.size() == 3 && !list.commands[0].scene && list.commands[1].scene && !list.commands[2].scene);
    CHECK(list.commands[1].indexCount == 12 && list.vertices.size() == 4 * 2);
    // the quad around it: at least as large as it is, centered near it (the camera above the middle)
    float left = 1e9f, right = -1e9f;
    for (std::size_t k = 4; k < 8; ++k) {
        const SceneVertex& v = list.sceneVertices[k];
        left = std::min(left, v.position.x);
        right = std::max(right, v.position.x);
        CHECK(near(v.center[0], 400) && near(v.halfSize[0], 40) && v.eye[2] >= 1000);
    }
    CHECK(left <= 360 && right >= 440 && right - left < 90);
}

TEST(aBrickSeenFromTheCameraLooksLargerAsItIsHigher) {
    DrawList list;
    Painter p(list, {0, 0, 800, 600}, 1);
    // at the target: where its dips are; above it: away from the middle, larger
    CHECK(near(p.project({100, 100, 0}).x, 100) && near(p.project({100, 100, 0}).y, 100));
    const Point high = p.project({100, 100, 200});
    CHECK(high.x < 100 && high.y < 100);
    // out of the clip: nothing recorded
    p.pushClip({0, 0, 100, 100});
    p.drawBrick({.center = {500, 500, 5}, .halfSize = {10, 10, 5}, .shadow = 0});
    p.popClip();
    CHECK(list.sceneVertices.empty());
}

TEST(textOnABrickIsDecalsOfItsGlyphsDistanceFields) {
    static text::FontLibrary fonts;
    text::GlyphAtlas atlas(1, 512, 9, 512);
    const text::TextLayout layout("Ok", text::TextStyle{{&fonts.defaultFont()}, 14});
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        const Brick brick{.center = {400, 300, 5}, .halfSize = {40, 15, 5}, .rotation = Quaternion::tilt(0.3f, 0.2f)};
        p.drawBrick(brick);
        text::drawTextOnBrick(p, atlas, layout, brick, {-10, -8}, {0, 0, 0, 1});
    }
    // its shadow, the brick, then a decal for each glyph (the fields' texture), with the brick's place and turn, and how
    // far the field's distances reach
    CHECK(list.sceneVertices.size() == 4 * 4);
    const SceneVertex& decal = list.sceneVertices[8];
    CHECK(decal.material[2] == decalKind && decal.center[0] == 400 && decal.rotation[3] < 1 && decal.material[0] > 0 && decal.material[1] > 0);
    bool decals = false;
    for (const DrawCommand& c : list.commands)
        decals = decals || (c.scene && c.texture == 9);
    CHECK(decals);
    // an atlas without fields: nothing printed
    text::GlyphAtlas bitmaps(1, 512);
    DrawList none;
    {
        Painter p(none, {0, 0, 800, 600}, 1);
        text::drawTextOnBrick(p, bitmaps, layout, Brick{}, {}, {0, 0, 0, 1});
    }
    CHECK(none.sceneVertices.empty());
}

TEST(raisedReliefsAreBricksWhenThePainterSaysSo) {
    // a raised relief: a brick lit as a relief, its top at the target; a sunk one: a relief still
    DrawList list;
    {
        Painter p(list, {0, 0, 400, 300}, 1);
        p.setReliefBricks(ReliefBricks{.depth = 5});
        p.fillRelief({10, 10, 80, 26}, 5, 1.6f, 2.5f, {0.3f, 0.3f, 0.3f, 1});
        p.fillRelief({10, 50, 80, 26}, 5, -1.4f, 2, {0.1f, 0.1f, 0.1f, 1});
    }
    CHECK(list.sceneVertices.size() == 4 && list.sceneVertices[0].material[3] == 1);
    CHECK(near(list.sceneVertices[0].center[2] + list.sceneVertices[0].halfSize[2], 0));
    CHECK(near(list.sceneVertices[0].halfSize[2] * 2, 1.6f * 5, 1e-3f));
    CHECK(list.vertices.size() == 4 && !list.depth);
    CHECK(near(list.sceneVertices[0].bevel, 2.5f));        // the relief's own bevel
    // a bevel of their own: 0, their tops flat to a sharp edge, their corners round still
    DrawList sharp;
    {
        Painter p(sharp, {0, 0, 400, 300}, 1);
        p.setReliefBricks(ReliefBricks{.bevel = 0});
        p.fillRelief({10, 10, 80, 26}, 5, 1.6f, 2.5f, {0.3f, 0.3f, 0.3f, 1});
    }
    CHECK(sharp.sceneVertices[0].bevel == 0 && near(sharp.sceneVertices[0].cornerRadius, 5));
    // without: reliefs both
    DrawList flat;
    {
        Painter p(flat, {0, 0, 400, 300}, 1);
        p.fillRelief({10, 10, 80, 26}, 5, 1.6f, 2.5f, {0.3f, 0.3f, 0.3f, 1});
    }
    CHECK(flat.sceneVertices.empty() && flat.vertices.size() == 4);
}

TEST(whatIsPlacedLiftedIsSeenLargerAndItsBricksGoWithIt) {
    DrawList list;
    Painter p(list, {0, 0, 800, 600}, 1);
    p.setReliefBricks(ReliefBricks{});
    const Painter::Mark mark = p.mark();
    p.fillRelief({100, 100, 80, 30}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});
    p.fillRect({110, 110, 20, 10}, {1, 1, 1, 1});
    const auto wide = [&] {
        float left = 1e9f, right = -1e9f;
        for (const Vertex& v : list.vertices) {
            left = std::min(left, v.position.x);
            right = std::max(right, v.position.x);
        }
        return right - left;
    };
    const float before = wide();
    const float centerBefore = list.sceneVertices.back().center[2];
    p.place(mark, {140, 115}, {.lift = 20});
    // nearer the camera: the 2D of it wider about the pivot, the brick up by as much
    CHECK(wide() > before + 0.01f);
    CHECK(near(list.sceneVertices.back().center[2], centerBefore + 20, 1e-3f));
}

TEST(whatFallsInComesFromInFrontAndIsWhereItIsAtTheEnd) {
    const auto fallen = [](float t) {
        DrawList list;
        Painter p(list, {0, 0, 800, 600}, 1);
        p.pushClip({0, 0, 200, 200});
        const Painter::Mark mark = p.mark();
        p.drawBrick({.center = {100, 100, 5}, .halfSize = {40, 15, 5}, .shadow = 0});
        p.fillRect({90, 95, 20, 10}, {1, 1, 1, 1});
        p.fallIn(mark, t, {0, 0, 200, 200});
        p.popClip();
        return list;
    };
    // early: the brick high above where it lands, cut by no clip; the 2D of it not there yet
    const DrawList early = fallen(0.1f);
    CHECK(early.sceneVertices[0].center[2] > 100);
    CHECK(std::any_of(early.commands.begin(), early.commands.end(), [](const DrawCommand& c) { return c.scene && c.clip.width == 800; }));
    CHECK((early.vertices[0].color >> 24) == 0);
    // at the end: where it is, the 2D of it as it is
    const DrawList end = fallen(1);
    CHECK(near(end.sceneVertices[0].center[2], 5) && (end.vertices[0].color >> 24) == 255);
}

TEST(theSolidStyleIsDataTheWidgetsAlreadyKnow) {
    // its raised controls bricks, a camera nearer than the painter's own, panels landing; the other styles none of it
    const Theme solid = Theme::dark(ThemeStyle::Solid);
    CHECK(solid.reliefBricks.has_value() && solid.reliefBricks->bevel == 0 && solid.cameraDistance > 0 && solid.dock.landing.count() > 0);
    CHECK(themeStyleNamed("solid") == ThemeStyle::Solid);
    for (ThemeStyle other : {ThemeStyle::Studio, ThemeStyle::Fluid, ThemeStyle::Plain, ThemeStyle::Plastic, ThemeStyle::Metal}) {
        const Theme t = Theme::light(other);
        CHECK(!t.reliefBricks && t.cameraDistance == 0 && t.dock.landing.count() == 0);
    }
    // Plastic's shapes and light, as Plastic is
    const Theme plastic = Theme::dark(ThemeStyle::Plastic);
    CHECK(solid.button.bevel.relief == plastic.button.bevel.relief && solid.lighting.specular == plastic.lighting.specular);
    // blended toward it: its bricks are the target's (what is on or off is not mixed)
    CHECK(Theme::blend(plastic, solid, 0.3f).reliefBricks.has_value() && !Theme::blend(solid, plastic, 0.3f).reliefBricks);
}

namespace {

// A widget whose painting is arriving (Widget::arrival): a raised relief, a text-like quad.
class Arriving final : public Widget {
public:
    std::optional<Arrival> at;

protected:
    std::optional<Arrival> arrival() const override { return at; }
    void paint(PaintContext& context) override {
        context.painter.fillRelief({10, 10, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});
        context.painter.fillRect({20, 15, 30, 10}, {1, 1, 1, 1});
    }
};

} // namespace

TEST(aRootPaintsWhatIsArrivingFallingInAndItsThemesBricks) {
    tests::FakeHost host;
    UiContext context = tests::testContext();
    context.theme = Theme::dark(ThemeStyle::Solid);
    UiRoot root(context, &host);
    auto made = std::make_unique<Arriving>();
    Arriving& w = *made;
    root.setContent(std::move(made));
    root.setSize({400, 300});
    root.layout();
    DrawList list;
    // the theme's raised relief a brick; nothing arriving: not tested by depth
    root.paint(list);
    CHECK(list.sceneVertices.size() == 4 && !list.depth);
    // seen from the theme's camera: its distance times the longer side
    CHECK(near(list.sceneVertices[0].eye[2], context.theme.cameraDistance * 400, 1e-2f));
    // arriving: falling in, tested by depth
    w.at = Arrival{0.2f, false};
    root.invalidatePaint();
    root.paint(list);
    CHECK(list.depth && list.sceneVertices[0].center[2] > 0);
    // in a style without bricks: what arrives is its 2D, fading in
    context.theme = Theme::dark(ThemeStyle::Studio);
    UiRoot flat(context, &host);
    auto other = std::make_unique<Arriving>();
    other->at = Arrival{0.2f, false};
    flat.setContent(std::move(other));
    flat.setSize({400, 300});
    flat.layout();
    flat.paint(list);
    CHECK(list.sceneVertices.empty() && !list.vertices.empty() && (list.vertices.back().color >> 24) == 0);
}

TEST(whatRisesInComesFromBehindAndShowsItsTextAsItComes) {
    const auto risen = [](float t) {
        DrawList list;
        Painter p(list, {0, 0, 800, 600}, 1);
        const Painter::Mark mark = p.mark();
        p.drawBrick({.center = {400, 300, 5}, .halfSize = {40, 15, 5}, .shadow = 0});
        p.fillRect({390, 295, 20, 10}, {1, 1, 1, 1});
        p.riseIn(mark, t, {0, 0, 800, 600});
        return list;
    };
    // early: far behind the target (seen small), the 2D of it not there yet; at the end where it is
    const DrawList early = risen(0.15f);
    CHECK(early.sceneVertices[0].center[2] < -100 && (early.vertices[0].color >> 24) == 0);
    const DrawList end = risen(1);
    CHECK(near(end.sceneVertices[0].center[2], 5) && (end.vertices[0].color >> 24) == 255);
}

TEST(aGlassBrickReadsWhatWasDrawnUnderIt) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.setReliefBricks(ReliefBricks{.glass = true, .glassFrom = 1});
        p.fillRect({0, 0, 800, 600}, {0.2f, 0.2f, 0.2f, 1});                // what it is on
        p.fillRelief({0, 0, 800, 600}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}); // a slab: low, solid
        p.fillRelief({10, 10, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});    // a button: glass
        p.fillRect({20, 15, 30, 10}, {1, 1, 1, 1});                            // its text, over it
        p.fillRelief({200, 10, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});   // another, beside it
        p.fillRect({100, 100, 300, 30}, {0, 0, 1, 1});                         // something drawn where...
        p.fillRelief({120, 100, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});  // ... a third comes over it
    }
    // the slab solid, the buttons glass (lit as glass, reading the backdrop)
    CHECK(list.sceneVertices.size() == 4 * 4 && list.sceneVertices[0].material[3] == 1 && list.sceneVertices[4].material[3] == 2);
    std::size_t glass = 0;
    for (const DrawCommand& c : list.commands)
        glass += c.texture == backdropTexture ? 1 : 0;
    CHECK(glass == 3);
    // captured before the first, not before the one beside it (nothing new under it: its neighbour's text is not), again
    // before the third (something drawn under it since)
    const std::vector<std::size_t> captures = Renderer::backdropCaptures(list);
    CHECK(captures.size() == 2 && list.commands[captures[0]].texture == backdropTexture && list.commands[captures[1]].texture == backdropTexture);
    CHECK(near(list.sceneVertices[list.indices[list.commands[captures[1]].firstIndex]].center[0], 160));
    // none without glass
    DrawList solid;
    {
        Painter p(solid, {0, 0, 800, 600}, 1);
        p.setReliefBricks(ReliefBricks{});
        p.fillRelief({10, 10, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});
    }
    CHECK(Renderer::backdropCaptures(solid).empty());
}

TEST(theGlassStyleIsSolidOfGlass) {
    const Theme glass = Theme::dark(ThemeStyle::Glass), solid = Theme::dark(ThemeStyle::Solid);
    CHECK(themeStyleNamed("glass") == ThemeStyle::Glass);
    CHECK(glass.reliefBricks && glass.reliefBricks->glass && solid.reliefBricks && !solid.reliefBricks->glass);
    CHECK(glass.button.cornerRadius == solid.button.cornerRadius && glass.dock.landing == solid.dock.landing && glass.dock.rising.count() > 0);
}

TEST(whatFallsInFromSomewhereStartsWhereItWasShowing) {
    // a brick and a text at their places in `area`; from `from` (twice as wide, elsewhere): at the start, seen from the
    // camera, where they were there; at the end where they are
    const Rect area{100, 100, 200, 100}, from{400, 300, 400, 100};
    const auto landed = [&](float t) {
        DrawList list;
        Painter p(list, {0, 0, 1000, 800}, 1);
        const Painter::Mark mark = p.mark();
        p.drawBrick({.center = {200, 150, -5}, .halfSize = {40, 15, 5}, .shadow = 0});
        p.fillRect({190, 145, 20, 10}, {1, 1, 1, 1});
        p.fallIn(mark, t, area, from);
        return std::pair{std::move(list), p.camera()};
    };
    const auto [start, camera] = landed(0);
    const SceneVertex& brick = start.sceneVertices[0];
    // its top face's middle, seen from the camera
    const float topZ = brick.center[2] + brick.halfSize[2];
    const float k = camera.eye.z / (camera.eye.z - topZ);
    const Point seen{camera.eye.x + (brick.center[0] - camera.eye.x) * k, camera.eye.y + (brick.center[1] - camera.eye.y) * k};
    CHECK(near(seen.x, 400 + (200 - 100) * 2, 0.5f) && near(seen.y, 300 + 50, 0.5f));
    // a little nearer the camera than where it lands, then where it is
    CHECK(brick.center[2] > 10);
    const auto [end, unused] = landed(1);
    // its text's first corner (as it is at the end) where it was too
    const Point corner = end.vertices[0].position;
    CHECK(near(start.vertices[0].position.x, 400 + (corner.x - 100) * 2, 0.5f) && near(start.vertices[0].position.y, 300 + (corner.y - 100), 0.5f));
    CHECK(near(end.sceneVertices[0].center[0], 200) && near(end.sceneVertices[0].center[2], -5));
}
