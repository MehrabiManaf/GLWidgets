// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The scroll bar every scrolling widget has: its thumb where its offset says, dragged; its track pressed for a page (nine
// tenths, eased, from where it was going); the mouse over it; mirrored across in right to left; and the wheel through
// wheelScroll (a step from where it was going, not taken at an end, so that what is around it scrolls). A text edit's bar
// across.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/ScrollBar.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace glw;

namespace {

bool near(double a, double b, double within = 0.01) {
    return std::abs(a - b) <= within;
}

// A bar and what it asked for.
struct Barred {
    std::vector<std::pair<double, bool>> scrolled;
    int repainted = 0;
    ScrollBar bar;

    explicit Barred(Orientation orientation)
        : bar(orientation, [this](double offset, bool eased) { scrolled.emplace_back(offset, eased); }, [this] { ++repainted; }) {}

    MouseEvent mouse(MouseEventType type, Point at) const {
        MouseEvent e;
        e.type = type;
        e.position = at;
        return e;
    }
};

} // namespace

TEST(aScrollBarsThumbIsWhereItsOffsetSays) {
    Barred b(Orientation::Vertical);
    // a content four times what shows, a quarter of the way down: the thumb a quarter long, a quarter of its room down
    b.bar.place({100, 0, 8, 400}, 1600, 400, 300, 300, false);
    CHECK(b.bar.shown());
    const Rect thumb = b.bar.thumb();
    CHECK(near(thumb.height, 100) && near(thumb.y, 300.0 / 1200 * 300) && near(thumb.x, 100));
    // all of it shows: no bar
    b.bar.place({100, 0, 8, 400}, 400, 400, 0, 0, false);
    CHECK(!b.bar.shown() && b.bar.thumb().empty() && b.bar.track().empty());
    // a short thumb: no shorter than the style's least
    b.bar.place({100, 0, 8, 400}, 1'000'000, 400, 0, 0, false, 24);
    CHECK(near(b.bar.thumb().height, 24));
    // across in right to left: from the right
    Barred a(Orientation::Horizontal);
    a.bar.place({0, 300, 400, 8}, 1600, 400, 0, 0, true);
    CHECK(near(a.bar.thumb().right(), 400) && near(a.bar.thumb().width, 100));
}

TEST(aScrollBarsThumbIsDraggedAndItsTrackPages) {
    Barred b(Orientation::Vertical);
    b.bar.place({100, 0, 8, 400}, 1600, 400, 0, 0, false);
    // over its thumb: hovered
    CHECK(!b.bar.mouse(b.mouse(MouseEventType::Move, {104, 50})) && b.bar.hovered());
    // its thumb pressed and dragged half its room down: half the offset's, at once
    CHECK(b.bar.mouse(b.mouse(MouseEventType::Press, {104, 50})) && b.bar.dragging());
    CHECK(b.bar.mouse(b.mouse(MouseEventType::Move, {104, 50 + 150})));
    CHECK(!b.scrolled.empty() && near(b.scrolled.back().first, 600) && !b.scrolled.back().second);
    CHECK(b.bar.mouse(b.mouse(MouseEventType::Release, {104, 200})) && !b.bar.dragging());
    // its track pressed under the thumb: a page down from where it was going, nine tenths of what shows, eased
    b.bar.place({100, 0, 8, 400}, 1600, 400, 0, 100, false);
    CHECK(b.bar.mouse(b.mouse(MouseEventType::Press, {104, 380})));
    CHECK(near(b.scrolled.back().first, 100 + 0.9 * 400) && b.scrolled.back().second);
    // pressed again on the way: a page further
    b.bar.mouse(b.mouse(MouseEventType::Release, {104, 380}));
    CHECK(b.bar.mouse(b.mouse(MouseEventType::Press, {104, 380})) && near(b.scrolled.back().first, 100 + 2 * 0.9 * 400));
    // pressed elsewhere: not its
    CHECK(!b.bar.mouse(b.mouse(MouseEventType::Press, {50, 50})));
    // the mouse gone: not hovered
    b.bar.leave();
    CHECK(!b.bar.hovered());
}

TEST(theWheelScrollsFromWhereItWasGoingAndNotPastTheEnds) {
    SmoothScroll smooth([](double) {});
    // a step from 0 down
    const WheelScroll down = wheelScroll(smooth, nullptr, 0, 100, false, 1000);
    CHECK(down.moved && down.aimed && near(down.target, 100));
    // at the top, up: nothing moves (what is around it scrolls)
    const WheelScroll up = wheelScroll(smooth, nullptr, 0, -100, false, 1000);
    CHECK(!up.moved && near(up.target, 0));
    // a touchpad's fraction: as it comes, not aimed
    const WheelScroll fine = wheelScroll(smooth, nullptr, 500, 7, true, 1000);
    CHECK(fine.moved && !fine.aimed && near(fine.target, 507));
    CHECK(!wheelScroll(smooth, nullptr, 1000, 5, true, 1000).moved);
}

TEST(aScrollAreasWheelAtItsEndIsNotTaken) {
    // a scroll area in a scroll area: the inner one at its end, the outer one scrolls
    UiRoot root{tests::testContext()};
    auto outer = std::make_unique<ScrollArea>();
    ScrollArea* outerArea = outer.get();
    auto column = std::make_unique<Widget>();
    column->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 0.0f));
    auto inner = std::make_unique<ScrollArea>();
    ScrollArea* innerArea = inner.get();
    auto tall = std::make_unique<Widget>();
    tall->setMinimumSize({100, 600});
    inner->setContent(std::move(tall));
    inner->setMinimumSize({200, 200});
    inner->setMaximumSize({unbounded, 200});
    column->add(std::move(inner));
    auto filler = std::make_unique<Widget>();
    filler->setMinimumSize({100, 1000});
    column->add(std::move(filler));
    outer->setContent(std::move(column));
    root.setSize({400, 300});
    root.setContent(std::move(outer));
    root.layout();
    // over the inner one: it scrolls down
    root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {100, 100}});
    root.layout();
    CHECK(innerArea->offset().y > 0 && outerArea->offset().y == 0);
    // at its end: the outer one
    innerArea->setOffset({0, innerArea->maximumOffset().y});
    root.layout();
    root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {100, 100}});
    root.layout();
    CHECK(outerArea->offset().y > 0);
}

TEST(aTextEditHasABarAcrossForItsLongLines) {
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<TextEdit>();
    TextEdit* edit = made.get();
    edit->setText("short\n" + std::string(400, 'x') + "\nshort");
    root.setSize({400, 300});
    root.setContent(std::move(made));
    root.layout();
    paint::DrawList list;
    root.paint(list);           // its lines laid out: the long one known
    root.layout();
    root.paint(list);
    CHECK(edit->horizontalOffset() == 0);
    // its thumb, at the start of the bar along the bottom, dragged to the end
    const float y = 300 - 4;
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, {60, y}});
    root.dispatch(platform::MouseMoved{1, {200, y}});
    root.dispatch(platform::MouseMoved{1, {900, y}});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, {900, y}});
    CHECK(edit->horizontalOffset() > 1000);
}
