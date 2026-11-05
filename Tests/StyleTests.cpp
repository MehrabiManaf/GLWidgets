// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The styles of the themes: Studio (square, raised, quick), Fluid (rounded, flat; a press ripples, the selection
// slides), Plain (square, flat, nothing moving).
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <chrono>
#include <memory>

using namespace glw;
using namespace std::chrono_literals;

namespace {

// The tests' context in another theme, animations on while it lives.
struct Styled {
    UiContext ui;
    explicit Styled(ThemeStyle style) : ui(tests::testContext()) {
        ui.theme = Theme::dark(style);
        tests::testScheduler().setAnimationsEnabled(true);
    }
    ~Styled() { tests::testScheduler().setAnimationsEnabled(false); }
};

// A button alone in a root.
struct Pressed {
    UiRoot root;
    Button* button = nullptr;
    explicit Pressed(const UiContext& ui) : root(ui) {
        auto made = std::make_unique<Button>(Button::Props{.text = "Go"});
        button = made.get();
        root.setSize({200, 100});
        root.setContent(std::move(made));
        root.layout();
        tests::advance(1s);         // whatever came in, gone
    }
    Point middle() const { return button->mapToRoot({button->size().width / 2, button->size().height / 2}); }
    void press() {
        root.dispatch(platform::MouseMoved{1, middle()});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, middle()});
    }
    void release() { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, middle()}); }
};

} // namespace

TEST(theThemesStylesDifferAsTheySay) {
    const Theme studio = Theme::dark(ThemeStyle::Studio), fluid = Theme::dark(ThemeStyle::Fluid), plain = Theme::light(ThemeStyle::Plain);
    CHECK(studio.style == ThemeStyle::Studio && fluid.style == ThemeStyle::Fluid && plain.style == ThemeStyle::Plain);
    // Studio: square, raised, quick, no ripple
    CHECK(studio.button.cornerRadius == 0 && studio.button.bevel.gradient > 0 && studio.button.ripple.a == 0 && !studio.motion.still());
    // Fluid: rounded, flat, rippling, the selection sliding, slower
    // ... its buttons and fields lower, and pills (a radius past half their height: half of it)
    CHECK(fluid.button.cornerRadius >= 100 && fluid.lineEdit.cornerRadius >= 100 && fluid.dock.cornerRadius > 0);
    // ... as low as Studio's (3 dips above and below the text, panels' headers 30, rows 26), wider for their ends
    CHECK(fluid.button.padding.top == studio.button.padding.top && studio.button.padding.top == 3 && fluid.button.padding.start > studio.button.padding.start);
    CHECK(fluid.collapsiblePanel.headerHeight == 30 && studio.collapsiblePanel.headerHeight == 30 && studio.itemView.rowHeight == 26);
    // panels: Studio's a card with an edge and a shadow; Fluid's without a frame, its header raised, a shadow under it;
    // Plain's crisp, without a shadow; in every one the focus a lighter header (no ring)
    CHECK(studio.collapsiblePanel.shadow.a > 0 && studio.collapsiblePanel.border.a > 0 && plain.collapsiblePanel.shadow.a == 0);
    CHECK(fluid.collapsiblePanel.border.a == 0 && fluid.collapsiblePanel.shadow.a > 0 && fluid.collapsiblePanel.bevel.gradient > 0);
    for (const Theme* theme : {&studio, &fluid, &plain})
        CHECK(theme->collapsiblePanel.header.focused && *theme->collapsiblePanel.header.focused != theme->collapsiblePanel.header.normal);
    // flat: no light, no shadow (a bevel of no colors draws none)
    CHECK(plain.lineEdit.bevel.shadow.a == 0 && plain.button.bevel.highlight.a == 0 && Bevel{}.shadow.a == 0);
    CHECK(fluid.button.ripple.a > 0 && fluid.itemView.slideSelection && !fluid.itemView.rowLines);
    CHECK(fluid.motion.normal > studio.motion.normal && fluid.motion.popupSlide > studio.motion.popupSlide);
    // Plain: square, flat, no glow, nothing moving
    CHECK(plain.button.cornerRadius == 0 && plain.button.bevel.gradient == 0 && plain.button.focusRing.a == 0);
    CHECK(plain.motion.still() && plain.motion.ripple.count() == 0 && !plain.isDark() && studio.isDark());
    // a palette of one's own, in a style
    CHECK(Theme::fromPalette(Palette::light(ThemeStyle::Studio), ThemeStyle::Fluid).button.ripple.a > 0);
}

TEST(aButtonRipplesWhenPressedInFluid) {
    Styled fluid(ThemeStyle::Fluid);
    Pressed p(fluid.ui);
    p.press();
    CHECK(tests::testScheduler().animating());
    tests::advance(200ms);
    p.release();
    tests::advance(1s);
    CHECK(!tests::testScheduler().animating());
}

TEST(nothingMovesInPlain) {
    Styled plain(ThemeStyle::Plain);
    Pressed p(plain.ui);
    // hovered, pressed, let go: at once, nothing animating
    p.press();
    CHECK(!tests::testScheduler().animating());
    p.release();
    p.root.layout();
    CHECK(!tests::testScheduler().animating());
}

TEST(aListsSelectionSlidesInFluid) {
    Styled fluid(ThemeStyle::Fluid);
    UiRoot root(fluid.ui);
    auto made = std::make_unique<ListView>();
    ListView& list = *made;
    list.setModel(std::make_shared<StringListModel>(std::vector<LocalizedText>{"a", "b", "c", "d"}));
    root.setSize({200, 300});
    root.setContent(std::move(made));
    root.layout();
    list.setCurrent(0);
    tests::advance(1s);
    // to another row: on its way there, then there
    list.setCurrent(3);
    CHECK(tests::testScheduler().animating() && list.isSelected(3));
    tests::advance(1s);
    CHECK(!tests::testScheduler().animating());
}

TEST(aStyleChangedEasesItsShapes) {
    const Theme studio = Theme::dark(ThemeStyle::Studio), fluid = Theme::dark(ThemeStyle::Fluid);
    // halfway: corners half as round (a pill's as round as a control is high: a sixteen dips' at most), controls between
    const Theme half = Theme::blend(studio, fluid, 0.5f);
    CHECK(half.style == ThemeStyle::Fluid);
    CHECK(half.button.cornerRadius == 8 && half.dock.cornerRadius == 5);
    CHECK(half.button.padding.start == 13 && half.collapsiblePanel.headerHeight == 30);
    // bevels fading: half of Studio's toward Plain's flat ones
    const Theme flatter = Theme::blend(studio, Theme::dark(ThemeStyle::Plain), 0.5f);
    CHECK(flatter.button.bevel.gradient > 0 && flatter.button.bevel.gradient < studio.button.bevel.gradient);
    // at either end: the themes themselves
    const Theme start = Theme::blend(studio, fluid, 0), end = Theme::blend(studio, fluid, 1);
    CHECK(start.button.cornerRadius == 0 && start.palette.window == studio.palette.window);
    CHECK(end.button.cornerRadius == fluid.button.cornerRadius && end.palette.window == fluid.palette.window);
    // popups: round in Fluid
    CHECK(fluid.frame.popupRadius > 0 && studio.frame.popupRadius == 0);
}

TEST(studioIsTheDefault) {
    CHECK(Theme::dark().style == ThemeStyle::Studio && Theme::light().style == ThemeStyle::Studio && Theme{}.style == ThemeStyle::Studio);
    // Fluid: its splitters seen; its buttons raised as its panels' headers
    const Theme fluid = Theme::dark(ThemeStyle::Fluid);
    CHECK(fluid.splitter.handle.a > 0 && fluid.button.bevel.gradient > 0 && fluid.button.bevel.gradient == fluid.collapsiblePanel.bevel.gradient);
}

TEST(plasticAndMetalDrawTheirControlsAsLitReliefs) {
    const Theme plastic = Theme::dark(ThemeStyle::Plastic);
    // raised buttons, sunk fields, a light from the upper left
    CHECK(plastic.button.bevel.relief > 0 && plastic.lineEdit.bevel.relief > 0 && plastic.collapsiblePanel.bevel.relief > 0);
    CHECK(plastic.lighting.x < 0 && plastic.lighting.y < 0 && plastic.lighting.z > 0 && Theme::dark().button.bevel.relief == 0);
    // low reliefs; item views' headers and highlights raised too (not in the other styles)
    CHECK(plastic.button.bevel.relief < 2 && plastic.button.bevel.roundness < 3);
    CHECK(plastic.table.headerBackground.a > 0 && plastic.table.headerBevel.relief > 0 && plastic.itemView.rowBevel.relief > 0);
    CHECK(Theme::dark().itemView.rowBevel.relief == 0);
    // a raised control, a sunk one: reliefs (the vertices of a lit kind), up and down
    paint::DrawList list;
    {
        paint::Painter p(list, {0, 0, 200, 200}, 1);
        paintRaised(p, {10, 10, 80, 26}, 7, {0.3f, 0.3f, 0.3f, 1}, {0, 0, 0, 0}, plastic.button.bevel);
        paintSunken(p, {10, 50, 80, 26}, 6, {0.1f, 0.1f, 0.1f, 1}, {0, 0, 0, 0}, plastic.lineEdit.bevel);
    }
    int up = 0, down = 0;
    for (const paint::Vertex& v : list.vertices) {
        if (v.kind == static_cast<float>(paint::VertexKind::Relief)) {
            up += v.blur > 0 ? 1 : 0;
            down += v.blur < 0 ? 1 : 0;
        }
    }
    CHECK(up == 4 && down == 4);
    // eased to it: its relief and its light between
    const Theme half = Theme::blend(Theme::dark(), plastic, 0.5f);
    CHECK(half.button.bevel.relief == plastic.button.bevel.relief / 2 && half.lighting.specular > 0);
    // plastic: a white, tight highlight; metal: its highlight of its color, broad, its surfaces brushed, its own grays
    const Theme metal = Theme::dark(ThemeStyle::Metal);
    CHECK(plastic.lighting.metal == 0 && plastic.lighting.brushed == 0 && plastic.lighting.shininess > metal.lighting.shininess);
    CHECK(metal.lighting.metal > 0.5f && metal.lighting.brushed > 0 && metal.button.bevel.relief == plastic.button.bevel.relief);
    CHECK(metal.palette.window != plastic.palette.window && Theme::light(ThemeStyle::Metal).isDark() == false);
}

TEST(viewsHeadersAndNodesLookAsTheStylesControls) {
    // every style: its views' headers behind their titles, raised as its controls are (flat in Plain)
    const Theme studio = Theme::dark(ThemeStyle::Studio), fluid = Theme::dark(ThemeStyle::Fluid), plain = Theme::dark(ThemeStyle::Plain);
    const Theme plastic = Theme::dark(ThemeStyle::Plastic), metal = Theme::dark(ThemeStyle::Metal);
    for (const Theme* theme : {&studio, &fluid, &plain, &plastic, &metal})
        CHECK(theme->itemView.headerBackground.a > 0 && theme->table.headerBackground.a > 0);
    CHECK(studio.table.headerBevel.gradient > 0 && fluid.table.headerBevel.gradient > 0 && plain.table.headerBevel.gradient == 0);
    CHECK(plastic.table.headerBevel.relief > 0);
    // nodes: square-ish and raised in Studio, round in Fluid, square and flat in Plain, reliefs in Plastic and Metal
    CHECK(studio.node.cornerRadius < fluid.node.cornerRadius && plain.node.cornerRadius == 0 && plain.node.shadow.a == 0);
    CHECK(studio.node.bevel.gradient > 0 && plastic.node.bevel.relief > 0 && metal.node.bodyBevel.relief > 0);
}

TEST(tabsLookAsTheStylesControls) {
    // Studio, Plain, Plastic and Metal: the tab shown joined to its content (Plastic's and Metal's a relief of its color);
    // Fluid: standing on the bar, raised
    const Theme studio = Theme::dark(ThemeStyle::Studio), fluid = Theme::dark(ThemeStyle::Fluid), plain = Theme::dark(ThemeStyle::Plain);
    const Theme plastic = Theme::dark(ThemeStyle::Plastic), metal = Theme::dark(ThemeStyle::Metal);
    CHECK(studio.dock.tabsJoined && plain.dock.tabsJoined && plastic.dock.tabsJoined && metal.dock.tabsJoined);
    CHECK(studio.majorTab.joined && plain.majorTab.joined && plastic.majorTab.joined && metal.majorTab.joined);
    CHECK(!fluid.dock.tabsJoined && !fluid.majorTab.joined && plastic.dock.selectedTab == plastic.dock.background);
    CHECK(fluid.dock.tabRadius >= 100 && fluid.dock.tabBevel.gradient > 0 && plastic.dock.tabBevel.relief > 0 && plastic.majorTab.bevel.relief > 0);
    CHECK(plain.dock.tabRadius == 0 && plain.dock.selectedTab == plain.dock.background);
    // Plastic and Metal: stacks and cards raised, slabs, the tab shown a folder's tab of its stack
    CHECK(plastic.dock.islandBevel.relief > 0 && metal.card.bevel.relief > 0 && studio.dock.islandBevel.relief == 0 && fluid.card.bevel.relief == 0);
}
