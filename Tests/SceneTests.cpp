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
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/CollapsiblePanel.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
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

TEST(theGlassStyleIsPlasticOfGlass) {
    // Plastic's reliefs and shapes, of glass: nothing in space, nothing arriving
    const Theme glass = Theme::dark(ThemeStyle::Glass), plastic = Theme::dark(ThemeStyle::Plastic);
    CHECK(themeStyleNamed("glass") == ThemeStyle::Glass);
    CHECK(glass.glassReliefs && !glass.reliefBricks && !plastic.glassReliefs && !Theme::dark(ThemeStyle::Solid).glassReliefs);
    CHECK(glass.button.cornerRadius == plastic.button.cornerRadius && glass.button.bevel.relief == plastic.button.bevel.relief);
    CHECK(glass.dock.landing.count() == 0 && glass.dock.rising.count() == 0);
}

TEST(aGlassReliefReadsWhatIsUnderItAndIsJoinedAsABrickIs) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.setGlassReliefs(GlassReliefs{.from = 0.5f});
        p.fillRect({0, 0, 800, 600}, {0.2f, 0.2f, 0.2f, 1});
        p.fillRelief({0, 30, 400, 300}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1});                  // a slab: glass
        p.fillRelief({16, 8, 88, 30}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}, {.bottom = true, .onto = true});   // its tab
        p.fillRelief({20, 60, 80, 26}, 4, 1.6f, 2, {0.5f, 0.5f, 0.5f, 1});                       // a button on it
        p.fillRelief({20, 100, 80, 26}, 4, 0.3f, 1, {0.5f, 0.5f, 0.5f, 1});                      // too low: as it is
        p.fillRelief({20, 140, 80, 26}, 4, -1.4f, 2, {0.1f, 0.1f, 0.1f, 1});                     // sunk: as it is
    }
    CHECK(list.sceneVertices.empty());
    int glass = 0, plain = 0;
    for (std::size_t i = 4; i < list.vertices.size(); i += 4) {
        glass += list.vertices[i].kind == static_cast<float>(VertexKind::GlassRelief) ? 1 : 0;
        plain += list.vertices[i].kind == static_cast<float>(VertexKind::Relief) ? 1 : 0;
    }
    CHECK(glass == 3 && plain == 2);
    // a copy before the slab, none for its tab (joined onto it), another for the button on the slab
    CHECK(Renderer::backdropCaptures(list).size() == 2);
}

TEST(whatFallsInFromSomewhereStartsWhereItWasShowing) {
    // a brick and a text at their places in `area`; from `from` (twice as wide, elsewhere): at the start, seen from the
    // camera, where they were there (the area as small as fits in it, in its middle: here as large); at the end where
    // they are
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
    CHECK(near(seen.x, 600, 0.5f) && near(seen.y, 350, 0.5f));
    // a little nearer the camera than where it lands, then where it is
    CHECK(brick.center[2] > 10);
    const auto [end, unused] = landed(1);
    // its text's first corner (as it is at the end) where it was too
    const Point corner = end.vertices[0].position;
    CHECK(near(start.vertices[0].position.x, 600 + (corner.x - 200), 0.5f) && near(start.vertices[0].position.y, 350 + (corner.y - 150), 0.5f));
    CHECK(near(end.sceneVertices[0].center[0], 200) && near(end.sceneVertices[0].center[2], -5));
}

TEST(aTabJoinedOntoItsStackIsOnePieceOfGlassWithIt) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.setReliefBricks(ReliefBricks{.glass = true, .glassFrom = 0.5f});
        // the stack's slab, its top joined to the tab between 20 and 100; the tab over it, its bottom joined onto it
        p.fillRelief({0, 30, 400, 300}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}, {.top = true, .topFrom = 20, .topTo = 100});
        p.fillRelief({16, 8, 88, 30}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}, {.bottom = true, .onto = true});
    }
    // the slab's joined sides on its brick (its top, from its middle); the tab's bottom
    const SceneVertex& slab = list.sceneVertices[0];
    CHECK(slab.decal[0] == 1 && near(slab.decal[1], 20 - 200) && near(slab.decal[2], 100 - 200));
    CHECK(list.sceneVertices[4].decal[0] == 2);
    // one copy only, before the slab: the tab reads what the slab reads (not the slab through it)
    std::size_t joined = 0;
    for (const DrawCommand& c : list.commands)
        joined += c.texture == joinedBackdropTexture ? 1 : 0;
    CHECK(joined == 1 && Renderer::backdropCaptures(list).size() == 1);
}

TEST(aPanelLandsSlabThenTabThenTheRestAndRisesTheOtherWay) {
    // a slab, its tab (joined at its bottom), a control on the slab: how far each is from its place, part of the way in
    const Rect area{0, 0, 400, 300};
    const auto away = [&](float t, bool rising) {
        DrawList list;
        Painter p(list, {0, 0, 1000, 800}, 1);
        p.setReliefBricks(ReliefBricks{.depth = 1.5f, .bevel = 0});
        const Painter::Mark mark = p.mark();
        p.fillRelief({0, 30, 400, 270}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}, {.top = true, .topFrom = 10, .topTo = 90});
        p.fillRelief({10, 6, 80, 30}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1}, {.bottom = true, .onto = true});
        p.fillRelief({20, 60, 120, 26}, 4, 1.6f, 0, {0.5f, 0.5f, 0.5f, 1});
        if (rising)
            p.riseIn(mark, t, area);
        else
            p.fallIn(mark, t, area, Rect{500, 400, 400, 300});
        // (how far each is from the target's plane: in front of it landing, behind it rising)
        std::vector<float> far;
        for (std::size_t i = 0; i < list.sceneVertices.size(); i += 4)
            far.push_back(std::abs(list.sceneVertices[i].center[2]));
        return far;
    };
    // landing: the slab nearest its place (lowest), then the tab, then the control
    const std::vector<float> landing = away(0.25f, false);
    CHECK(landing.size() == 3 && landing[0] < landing[1] && landing[1] < landing[2]);
    // rising: the other way: the control nearest its place, then the tab, then the slab
    const std::vector<float> rising = away(0.3f, true);
    CHECK(rising.size() == 3 && rising[2] < rising[1] && rising[1] < rising[0]);
}

namespace {

// A widget that paints a rectangle of its color.
class Filled final : public Widget {
public:
    explicit Filled(Color color) : color_(color) {}

protected:
    void paint(PaintContext& context) override { context.painter.fillRect({0, 0, size().width, size().height}, color_); }

private:
    Color color_;
};

} // namespace

TEST(whatArrivesIsPaintedOverWhatComesAfterIt) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    auto row = std::make_unique<Widget>();
    row->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal));
    auto arriving = std::make_unique<Arriving>();
    Arriving& a = *arriving;
    row->addChild(std::move(arriving));
    auto blue = std::make_unique<Filled>(Color{0, 0, 1, 1});
    blue->setMinimumSize({100, 100});
    row->addChild(std::move(blue));
    root.setContent(std::move(row));
    root.setSize({400, 300});
    root.layout();
    const auto blueAt = [](const DrawList& list) {
        for (std::size_t i = 0; i < list.vertices.size(); ++i) {
            if (list.vertices[i].color == packColor({0, 0, 1, 1}))
                return i;
        }
        return list.vertices.size();
    };
    // still: in their order, the one after it over it
    DrawList list;
    root.paint(list);
    CHECK(blueAt(list) > 0 && blueAt(list) == list.vertices.size() - 4);
    // arriving: after it, over it (it comes from elsewhere, over what is there)
    a.at = Arrival{0.3f, false, Rect{200, 100, 100, 100}};
    root.invalidatePaint();
    root.paint(list);
    CHECK(blueAt(list) < list.vertices.size() - 4);
}

TEST(aPressGoesDownQuicklyAndSpringsBackPastItsPlace) {
    tests::testScheduler().setAnimationsEnabled(true);
    float shown = 0;
    Press press([&] {});
    const MotionStyle motion;
    press.to(&tests::testScheduler(), true, motion);
    tests::advance(motion.fast);
    CHECK(near(press.value(), 1, 1e-3f));
    // let go: up, a little past where it was (below 0: higher than it stands) on the way, then there
    press.to(&tests::testScheduler(), false, motion);
    float lowest = 1;
    for (int k = 0; k < 20; ++k) {
        tests::advance(motion.normal / 20);
        lowest = std::min(lowest, press.value());
    }
    shown = press.value();
    CHECK(lowest < -0.01f && near(shown, 0, 1e-3f));
    tests::testScheduler().setAnimationsEnabled(false);
}

TEST(theSolidStylesControlsAreBricksOfTheirOwn) {
    const Theme solid = Theme::dark(ThemeStyle::Solid), plastic = Theme::dark(ThemeStyle::Plastic);
    CHECK(solid.table.headerGap > 0 && solid.table.slideSelection && solid.checkBox.checkDrop > 0);
    CHECK(solid.scrollBar.trackBevel.relief > 0 && solid.scrollBar.thumbBevel.relief > 0);
    CHECK(plastic.table.headerGap == 0 && plastic.checkBox.checkDrop == 0 && plastic.scrollBar.thumbBevel.relief == 0);
    // a thumb raised: a relief (a brick, with the painter's bricks), not a flat pill
    DrawList list;
    {
        Painter p(list, {0, 0, 400, 300}, 1);
        p.setReliefBricks(solid.reliefBricks);
        paintScrollThumb(p, {380, 20, 12, 80}, true, true, 1, {0.5f, 0.5f, 0.5f, 1}, 4, solid.scrollBar.thumbBevel);
    }
    CHECK(list.sceneVertices.size() == 4);
}

TEST(theSolidStylesPopupsAreSlabsThatComeInAndItsPanelsCascade) {
    const Theme solid = Theme::dark(ThemeStyle::Solid), plastic = Theme::dark(ThemeStyle::Plastic);
    CHECK(solid.menu.bevel.relief > 0 && solid.dialog.bevel.relief > 0 && solid.toolTip.bevel.relief > 0);
    CHECK(solid.motion.popupEntrance == PopupEntrance::Rise && solid.motion.dialogEntrance == PopupEntrance::Land);
    CHECK(solid.collapsiblePanel.cascade && !plastic.collapsiblePanel.cascade && plastic.motion.popupEntrance == PopupEntrance::Slide);
    // a popup's background: a slab (a brick, with the painter's bricks) where the bevel raises it; flat otherwise
    DrawList slab, flat;
    {
        Painter p(slab, {0, 0, 200, 300}, 1);
        p.setReliefBricks(solid.reliefBricks);
        paintPopupBackground(p, {0, 0, 200, 300}, 4, {0.2f, 0.2f, 0.2f, 1}, {0, 0, 0, 0}, solid.menu.bevel);
        Painter q(flat, {0, 0, 200, 300}, 1);
        paintPopupBackground(q, {0, 0, 200, 300}, 4, {0.2f, 0.2f, 0.2f, 1}, {0, 0, 0, 0}, plastic.menu.bevel);
    }
    CHECK(slab.sceneVertices.size() == 4 && flat.sceneVertices.empty() && flat.vertices.size() == 4);
}

TEST(aRootsContentArrivesAsAPopupComesIn) {
    tests::FakeHost host;
    UiContext context = tests::testContext();
    context.theme = Theme::dark(ThemeStyle::Solid);
    UiRoot root(context, &host);
    auto made = std::make_unique<Filled>(Color{0, 0, 1, 1});
    root.setContent(std::move(made));
    root.setSize({200, 100});
    root.layout();
    DrawList list;
    // rising from behind: its 2D not shown yet
    root.setContentArrival(Arrival{0.1f, true});
    root.paint(list);
    CHECK(!list.vertices.empty() && (list.vertices.front().color >> 24) == 0);
    // there: as it is
    root.setContentArrival(std::nullopt);
    root.paint(list);
    CHECK((list.vertices.front().color >> 24) == 255);
}

TEST(aCascadingPanelsWidgetsComeOutFromUnderItsHeaderInOrder) {
    tests::testScheduler().setAnimationsEnabled(true);
    tests::FakeHost host;
    UiContext context = tests::testContext();
    context.theme = Theme::dark(ThemeStyle::Solid);
    context.theme.motion.varied = false;            // (its widgets out from under its header, each time)
    UiRoot root(context, &host);
    auto made = std::make_unique<CollapsiblePanel>(CollapsiblePanel::Props{.title = "Panel", .expanded = true});
    CollapsiblePanel& panel = *made;
    for (const Color color : {Color{1, 0, 0, 1}, Color{0, 1, 0, 1}, Color{0, 0, 1, 1}}) {
        auto row = std::make_unique<Filled>(color);
        row->setMinimumSize({100, 30});
        panel.addContent(std::move(row));
    }
    root.setContent(std::move(made));
    root.setSize({300, 400});
    root.layout();
    DrawList list;
    const auto topOf = [&](Color color) {
        for (const Vertex& v : list.vertices) {
            if (v.color == packColor(color))
                return v.position.y;
        }
        return -1e9f;
    };
    root.paint(list);
    const float redAt = topOf({1, 0, 0, 1}), blueAt = topOf({0, 0, 1, 1});
    // closed, then opening: part of the way, the first nearer its place than the last (still under the header)
    panel.setExpanded(false, false);
    root.layout();
    panel.setExpanded(true);
    tests::advance(context.theme.motion.slow / 3);
    root.layout();
    root.paint(list);
    CHECK(redAt - topOf({1, 0, 0, 1}) < blueAt - topOf({0, 0, 1, 1}));
    tests::advance(context.theme.motion.slow);
    tests::testScheduler().setAnimationsEnabled(false);
}

TEST(theSolidStylesGroovesAndKeys) {
    const Theme solid = Theme::dark(ThemeStyle::Solid), plastic = Theme::dark(ThemeStyle::Plastic);
    CHECK(solid.lineEdit.stepperBevel.relief > 0 && solid.splitter.grooveWidth > 0 && solid.splitter.groove.relief > 0);
    CHECK(solid.statusBar.bevel.relief > 0 && solid.node.dragLift > 0);
    CHECK(plastic.lineEdit.stepperBevel.relief == 0 && plastic.splitter.grooveWidth == 0 && plastic.statusBar.bevel.relief == 0 &&
          plastic.node.dragLift == 0);
}

TEST(everyWayToComeStartsAwayAndEndsWhereItIs) {
    const Rect area{100, 100, 300, 200}, from{600, 400, 150, 100};
    const auto drawn = [&](auto&& motion, float t) {
        DrawList list;
        Painter p(list, {0, 0, 1000, 800}, 1);
        p.setReliefBricks(ReliefBricks{.depth = 2.5f, .bevel = 0});
        const Painter::Mark mark = p.mark();
        p.fillRelief({100, 130, 300, 170}, 4, 0.8f, 1.5f, {0.3f, 0.3f, 0.3f, 1});       // a slab
        p.fillRelief({120, 160, 120, 26}, 4, 1.6f, 0, {0.5f, 0.5f, 0.5f, 1});           // a control on it
        p.fillRect({130, 168, 40, 10}, {1, 1, 1, 1});                                    // its text
        motion(p, mark, t);
        return list;
    };
    const auto swing = [&](Painter& p, const Painter::Mark& m, float t) { p.swingIn(m, t, area, from); };
    const auto gather = [&](Painter& p, const Painter::Mark& m, float t) { p.gatherIn(m, t, area, from); };
    const auto pop = [&](Painter& p, const Painter::Mark& m, float t) { p.popIn(m, t, area); };
    const auto turn = [&](Painter& p, const Painter::Mark& m, float t) { p.turnIn(m, t, area); };
    const auto unfold = [&](Painter& p, const Painter::Mark& m, float t) { p.unfoldIn(m, t, area, Edge::Top); };
    const DrawList still = drawn([](Painter&, const Painter::Mark&, float) {}, 1);
    const auto check = [&](auto&& motion, bool moves) {
        const DrawList early = drawn(motion, 0.2f), end = drawn(motion, 1);
        // at the end: as it is drawn still
        CHECK(near(end.sceneVertices[4].center[0], still.sceneVertices[4].center[0]) &&
              near(end.sceneVertices[4].center[2], still.sceneVertices[4].center[2]));
        CHECK(near(end.vertices[0].position.x, still.vertices[0].position.x));
        // on the way: the control elsewhere, or as large otherwise, or turned
        const SceneVertex& a = early.sceneVertices[4];
        const SceneVertex& b = still.sceneVertices[4];
        const bool away = std::abs(a.center[0] - b.center[0]) + std::abs(a.center[1] - b.center[1]) + std::abs(a.center[2] - b.center[2]) > 2 ||
                          std::abs(a.halfSize[0] - b.halfSize[0]) > 1 || std::abs(a.rotation[3] - 1) > 1e-3f;
        CHECK(away == moves);
    };
    check(swing, true);
    check(gather, true);
    check(pop, true);
    check(turn, true);
    check(unfold, true);
    // unfolding from its top edge: what is below it still turned up toward it (the text's bottom higher than it is)
    const DrawList early = drawn(unfold, 0.1f);
    float bottom = -1e9f;
    for (std::size_t i = early.vertices.size() - 4; i < early.vertices.size(); ++i)
        bottom = std::max(bottom, early.vertices[i].position.y);
    CHECK(bottom < 174 && bottom > 140);
}

TEST(theSolidStyleVariesHowThingsCome) {
    CHECK(Theme::dark(ThemeStyle::Solid).motion.varied && !Theme::dark(ThemeStyle::Plastic).motion.varied);
    // one of the ways each time: all of them, in a while
    bool seen[3] = {};
    for (int i = 0; i < 200; ++i) {
        const int way = randomWay(3);
        CHECK(way >= 0 && way < 3);
        seen[std::clamp(way, 0, 2)] = true;
    }
    CHECK(seen[0] && seen[1] && seen[2] && randomWay(1) == 0);
}

TEST(aReliefArcIsABrickBentAroundItsRingWhereReliefsAreBricks) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.setReliefBricks(ReliefBricks{.depth = 2.5f, .bevel = 0});
        p.fillArcRelief({400, 300}, 60, 100, 0, 1.0f, 2, 1.6f, 1, {0.5f, 0.5f, 0.5f, 1}, 3);
    }
    // one brick, nothing flat: the ring's radii, its sides (half a radian), its ring's center behind its middle
    CHECK(list.sceneVertices.size() == 4 && list.vertices.empty());
    const SceneVertex& v = list.sceneVertices[0];
    CHECK(near(v.decalUv[0], 60) && near(v.decalUv[1], 100) && v.decalUv[2] > 0.49f && v.decalUv[3] < 0);
    const float out = std::hypot(v.center[0] - 400, v.center[1] - 300);
    CHECK(out > 50 && out < 100 && near(out, -v.decalUv[3], 0.01f) && v.center[2] < 0);
    // moved, twice as large: its ring with it
    DrawList moved;
    {
        Painter p(moved, {0, 0, 800, 600}, 1);
        p.setReliefBricks(ReliefBricks{.depth = 2.5f, .bevel = 0});
        const Painter::Mark mark = p.markHere();
        p.fillArcRelief({400, 300}, 60, 100, 0, 1.0f, 2, 1.6f, 1, {0.5f, 0.5f, 0.5f, 1}, 3);
        p.place(mark, {400, 300}, {.scale = 2});
    }
    CHECK(near(moved.sceneVertices[0].decalUv[1], 200, 0.01f) && near(moved.sceneVertices[0].decalUv[3], 2 * v.decalUv[3], 0.01f));
}

TEST(aBackdropIsMadeWhereItIsDrawnAndGlossyGlassGlowsOutOfItsEdges) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.fillBackdrop({0, 0, 800, 600}, Backdrop{.base = {0.1f, 0.1f, 0.12f, 1}});
    }
    CHECK(list.vertices.size() == 4 && list.vertices[0].kind == static_cast<float>(VertexKind::Backdrop));
    CHECK(near(list.vertices[2].uv.x, 800) && near(list.vertices[2].uv.y, 600) && near(list.vertices[0].halfSize.x, 760));
    const auto glass = [](bool glossy) {
        DrawList drawn;
        Painter p(drawn, {0, 0, 800, 600}, 1);
        p.setGlassReliefs(GlassReliefs{.from = 0.5f, .glossy = glossy});
        p.fillRelief({100, 100, 80, 30}, 4, 1.6f, 1.5f, {0.4f, 0.4f, 0.45f, 1});
        return drawn;
    };
    const DrawList clear = glass(false), glossy = glass(true);
    CHECK(clear.vertices[0].kind == static_cast<float>(VertexKind::GlassRelief));
    CHECK(near(glossy.vertices[0].kind, static_cast<float>(VertexKind::GlassRelief) + 0.25f));
    // its glow: room for it around it
    CHECK(glossy.vertices[0].position.x < clear.vertices[0].position.x - 6 && Theme::dark(ThemeStyle::Glass).backdrop.has_value());
}

TEST(aReliefArcOfGlassReadsWhatIsUnderIt) {
    DrawList list;
    {
        Painter p(list, {0, 0, 800, 600}, 1);
        p.setGlassReliefs(GlassReliefs{.from = 0.5f, .glossy = true});
        p.fillArcRelief({400, 300}, 60, 100, 0, 1.0f, 2, 1.6f, 1, {0.2f, 0.5f, 0.9f, 1}, 3);
    }
    CHECK(list.vertices.size() == 4 && near(list.vertices[0].kind, static_cast<float>(VertexKind::GlassRingRelief) + 0.25f));
    CHECK(!Renderer::backdropCaptures(list).empty());
}
