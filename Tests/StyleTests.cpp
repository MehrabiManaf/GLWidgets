// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The styles of the themes: Studio (square, raised, quick), Fluid (rounded, flat; a press ripples, the selection
// slides), Plain (square, flat, nothing moving).
#include "Check.hpp"
#include "TestUi.hpp"

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
    CHECK(fluid.button.padding.top < studio.button.padding.top && fluid.collapsiblePanel.headerHeight < studio.collapsiblePanel.headerHeight);
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
    CHECK(half.button.padding.top == 4 && half.collapsiblePanel.headerHeight == 33);
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
