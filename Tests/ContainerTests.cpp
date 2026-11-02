// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Scroll areas (bars, wheel, thumb, clipping, revealing the focus, right to left), splitters (shares, dragging, minimums,
// right to left) and check boxes.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>

using namespace glw;
namespace ui = glw::ui;

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 0.01f;
}

// Something of a fixed size.
struct Block : Widget {
    explicit Block(Size s) { setMinimumSize(s); }
};

platform::Event press(Point p, bool pressed = true) {
    return platform::MouseButtonChanged{1, platform::MouseButton::Left, pressed, 1, p};
}

// A root showing a scroll area of 10 blocks 30 high (300 in all) in 200 x 100.
struct Scrolled {
    UiRoot root{tests::testContext()};
    ScrollArea* area = nullptr;
    Widget* first = nullptr;

    explicit Scrolled(LayoutDirection direction = LayoutDirection::LeftToRight) {
        ui::Children blocks;
        for (int i = 0; i < 10; ++i)
            blocks.push_back(std::make_unique<Block>(Size{0, 30}));
        root.setSize({200, 100});
        root.setLayoutDirection(direction);
        root.setContent(ui::ScrollArea({}, ui::VStack({}, std::move(blocks))).bind(area));
        root.layout();
        first = area->content()->children()[0].get();
    }
};

} // namespace

TEST(scrollBarWhenContentIsLonger) {
    Scrolled s;
    const float bar = tests::testContext().theme.scrollBar.width;
    CHECK(near(s.area->viewport().width, 200 - bar));       // the bar takes its width
    CHECK(near(s.area->content()->bounds().width, 200 - bar));
    CHECK(near(s.area->maximumOffset().y, 200));
    CHECK(s.root.widgetAt({200 - bar / 2, 50}) == s.area);   // the bar: not the content under it
    CHECK(s.root.widgetAt({50, 50}) != s.area);
}

TEST(wheelScrollsAndStops) {
    Scrolled s;
    s.root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {50, 50}});    // toward the user: down
    CHECK(near(s.area->offset().y, 48));
    CHECK(near(s.first->mapToRoot({}).y, -48));            // the content moved up
    for (int i = 0; i < 10; ++i)
        s.root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {50, 50}});
    CHECK(near(s.area->offset().y, 200));                  // no further than the end
    s.root.dispatch(platform::MouseWheelScrolled{1, {0, 1}, {50, 50}});
    CHECK(near(s.area->offset().y, 152));
}

TEST(draggingTheThumb) {
    Scrolled s;
    const float bar = tests::testContext().theme.scrollBar.width;
    const Point thumb{200 - bar / 2, 5};
    s.root.dispatch(press(thumb));
    s.root.dispatch(platform::MouseMoved{1, {thumb.x, thumb.y + 20}});
    s.root.dispatch(press({thumb.x, thumb.y + 20}, false));
    // the thumb is a third of the track (100 of 300): 20 of its 66.7 dips of room are 30 % of the 200 to scroll
    CHECK(near(s.area->offset().y, 20.0f / (100.0f - 100.0f / 3) * 200));
}

TEST(tabRevealsTheFocus) {
    UiRoot root(tests::testContext());
    root.setSize({200, 100});
    ui::Children buttons;
    for (int i = 0; i < 8; ++i)
        buttons.push_back(ui::Button({.text = "Button"}).minimumSize({0, 40}));
    ScrollArea* area = nullptr;
    root.setContent(ui::ScrollArea({}, ui::VStack({}, std::move(buttons))).bind(area));
    root.layout();
    for (int i = 0; i < 8; ++i) {
        root.dispatch(platform::KeyChanged{1, platform::Key::Tab, platform::KeyModifiers::None, true, false});
        const Widget* focus = root.focusWidget();
        const float top = focus->mapToRoot({}).y;
        CHECK(top >= area->viewport().top() - 0.01f && top + focus->size().height <= area->viewport().bottom() + 0.01f);
    }
    CHECK(area->offset().y > 0);
}

TEST(stacksShrinkTheirChildren) {
    // a header and a long scrolled list in a stack too short for both: the list gets what the header leaves, and scrolls
    UiRoot root(tests::testContext());
    root.setSize({200, 150});
    ui::Children blocks;
    for (int i = 0; i < 10; ++i)
        blocks.push_back(std::make_unique<Block>(Size{0, 30}));
    Widget* header = nullptr;
    ScrollArea* area = nullptr;
    root.setContent(ui::VStack({}, ui::make<Block>(Size{0, 50}).bind(header), ui::ScrollArea({}, ui::VStack({}, std::move(blocks))).stretch(1).bind(area)));
    root.layout();
    CHECK(near(header->bounds().height, 50));       // at its minimum: it does not give any up
    CHECK(near(area->bounds().height, 100));
    CHECK(near(area->maximumOffset().y, 200));

    // without stretch the others give up length too, in proportion to what they have above their minimum
    UiRoot root2(tests::testContext());
    root2.setSize({200, 100});
    Widget* a = nullptr;
    Widget* b = nullptr;
    root2.setContent(ui::VStack({}, ui::make<Block>(Size{0, 0}).minimumSize({0, 20}).maximumSize({unbounded, 20}).bind(a),
                                ui::Label({.text = "x"}).minimumSize({0, 200}).maximumSize({unbounded, 200}).bind(b)));
    root2.layout();
    CHECK(near(a->bounds().height, 20));
    CHECK(near(b->bounds().height, 200));           // its minimum: the stack overflows rather than going below it
}

TEST(scrollBarOnTheLeftInRightToLeft) {
    Scrolled s(LayoutDirection::RightToLeft);
    const float bar = tests::testContext().theme.scrollBar.width;
    CHECK(near(s.area->viewport().x, bar));
    CHECK(s.root.widgetAt({bar / 2, 50}) == s.area);
}

TEST(splitterSharesAndDragging) {
    UiRoot root(tests::testContext());
    const float t = tests::testContext().theme.splitter.thickness;
    root.setSize({400 + t, 100});
    Widget* left = nullptr;
    Widget* right = nullptr;
    Splitter* splitter = nullptr;
    root.setContent(ui::Splitter({}, ui::make<Block>(Size{}).bind(left), ui::make<Block>(Size{120, 0}).bind(right)).bind(splitter));
    root.layout();
    CHECK(near(left->bounds().width, 200) && near(right->bounds().width, 200));
    CHECK(splitter->handles().size() == 1);

    // dragged 50 to the right
    const Point handle = splitter->handles()[0].center();
    root.dispatch(press(handle));
    root.dispatch(platform::MouseMoved{1, {handle.x + 50, handle.y}});
    root.dispatch(press({handle.x + 50, handle.y}, false));
    root.layout();
    CHECK(near(left->bounds().width, 250) && near(right->bounds().width, 150));

    // a wider splitter keeps the proportions
    root.setSize({800 + t, 100});
    root.layout();
    CHECK(near(left->bounds().width, 500) && near(right->bounds().width, 300));

    // no further than the other pane's minimum
    const Point again = splitter->handles()[0].center();
    root.dispatch(press(again));
    root.dispatch(platform::MouseMoved{1, {again.x + 1000, again.y}});
    root.dispatch(press({again.x + 1000, again.y}, false));
    root.layout();
    CHECK(near(right->bounds().width, 120));
}

TEST(splitterInRightToLeft) {
    UiRoot root(tests::testContext());
    root.setSize({406, 100});
    root.setLayoutDirection(LayoutDirection::RightToLeft);
    Widget* first = nullptr;
    root.setContent(ui::Splitter({}, ui::make<Block>(Size{}).bind(first), ui::make<Block>(Size{})));
    root.layout();
    CHECK(near(first->bounds().right(), 406));      // the first pane on the start side: the right
}

TEST(checkBoxes) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    root.setSize({300, 40});
    int toggles = 0;
    bool last = false;
    CheckBox* box = nullptr;
    root.setContent(ui::CheckBox({.text = "Show", .onToggled = [&](bool on) { ++toggles; last = on; }}).bind(box));
    root.layout();
    const Point inside = box->mapToRoot({5, box->size().height / 2});
    root.dispatch(press(inside));
    root.dispatch(press(inside, false));
    CHECK(box->checked() && toggles == 1 && last);
    root.dispatch(platform::KeyChanged{1, platform::Key::Space, platform::KeyModifiers::None, true, false});
    CHECK(!box->checked() && toggles == 2 && !last);
    box->setChecked(true);
    CHECK(box->checked() && toggles == 2);          // not by the user: not told
}
