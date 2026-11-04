// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Canvases: items zoomed and panned (painted, under the mouse and mapped through the view), zoomed at the mouse, dragged
// by the middle and right buttons, a context menu asked for; only the items that show painted; framed; an item raised while
// it has the mouse; Bezier curves flattened.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Canvas.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <vector>

using namespace glw;

namespace {

bool near(double a, double b, double within = 0.01) {
    return std::abs(a - b) < within;
}

bool near(Point a, Point b) {
    return near(a.x, b.x) && near(a.y, b.y);
}

// A box 80 x 40 that counts its paints and keeps what the mouse did to it.
class Box final : public Widget {
public:
    int painted = 0;
    std::vector<Point> moves;
    int presses = 0;

    Box() { setMinimumSize({80, 40}); }

protected:
    void paint(PaintContext&) override { ++painted; }
    void onMouse(MouseEvent& event) override {
        if (event.button != platform::MouseButton::Left)
            return;
        if (event.type == MouseEventType::Press)
            ++presses;
        if (event.type == MouseEventType::Move)
            moves.push_back(event.position);
        event.handled = event.type != MouseEventType::Wheel;
    }
};

struct Canvased {
    UiRoot root{tests::testContext()};
    Canvas* canvas = nullptr;

    Canvased() {
        auto made = std::make_unique<Canvas>();
        canvas = made.get();
        root.setSize({400, 300});
        root.setContent(std::move(made));
    }
    void button(platform::MouseButton b, bool down, Point at) {
        root.dispatch(platform::MouseButtonChanged{1, b, down, 1, at});
    }
    void move(Point to) { root.dispatch(platform::MouseMoved{1, to}); }
    int paint() {
        paint::DrawList list;
        root.paint(list);
        return 0;
    }
};

} // namespace

TEST(aCanvasZoomsAtTheMouseAndPans) {
    Canvased c;
    Box& box = c.canvas->addItem(std::make_unique<Box>(), {100, 50});
    c.root.layout();
    CHECK(near(box.mapToRoot({0, 0}), {100, 50}) && near(box.size().width, 80));
    CHECK(c.root.widgetAt({120, 60}) == &box && c.root.widgetAt({190, 60}) == c.canvas);

    // a step of the wheel at the box's corner: the corner stays, the box grows
    c.root.dispatch(platform::MouseWheelScrolled{1, {0, 1}, {100, 50}});
    CHECK(near(c.canvas->zoom(), 1.15f));
    CHECK(near(box.mapToRoot({0, 0}), {100, 50}) && near(box.mapToRoot({80, 40}), {192, 96}));
    CHECK(near(box.mapFromRoot({192, 96}), {80, 40}));
    CHECK(c.root.widgetAt({190, 90}) == &box);                  // in it now, zoomed
    CHECK(near(c.canvas->toCanvas({192, 96}), {180, 90}));

    // the middle button drags the view
    const Point origin = c.canvas->origin();
    c.button(platform::MouseButton::Middle, true, {300, 200});
    c.move({350, 220});
    c.button(platform::MouseButton::Middle, false, {350, 220});
    CHECK(near(c.canvas->origin(), origin + Point{50, 20}));
    CHECK(near(box.mapToRoot({0, 0}), {150, 70}));

    // the right button: let go where it was pressed, a menu asked for at that point of the canvas; dragged, it pans
    std::optional<Point> asked;
    c.canvas->setOnContextRequested([&](Point at) { asked = at; });
    c.button(platform::MouseButton::Right, true, {150, 70});
    c.button(platform::MouseButton::Right, false, {150, 70});
    CHECK(asked && near(*asked, {100, 50}));
    asked.reset();
    c.button(platform::MouseButton::Right, true, {300, 200});
    c.move({280, 200});
    c.button(platform::MouseButton::Right, false, {280, 200});
    CHECK(!asked && near(box.mapToRoot({0, 0}), {130, 70}));

    // not past its range
    c.canvas->setZoom(100);
    CHECK(near(c.canvas->zoom(), c.canvas->maximumZoom()));
}

TEST(aCanvasPaintsTheItemsThatShowOnly) {
    Canvased c;
    // ten thousand boxes, a grid of 100 x 100, 200 apart
    std::vector<Box*> boxes;
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x)
            boxes.push_back(&c.canvas->addItem(std::make_unique<Box>(), {x * 200.0f, y * 200.0f}));
    }
    c.root.layout();
    const auto painted = [&] {
        int count = 0;
        for (Box* box : boxes)
            count += std::exchange(box->painted, 0);
        return count;
    };
    c.paint();
    // 400 x 300 of the canvas: 2 x 2 of them, and those whose overhang reaches in
    const int near1 = painted();
    CHECK(near1 > 0 && near1 <= 9);
    // a tenth: 4000 x 3000, some 20 x 15 of them
    c.canvas->setZoom(0.1f, Point{0, 0});
    c.paint();
    const int far = painted();
    CHECK(far >= 300 && far <= 400);
    // panned into the middle: as many as before, others
    c.canvas->setZoom(1, Point{0, 0});
    c.canvas->centerOn({10000, 10000});
    c.paint();
    CHECK(painted() <= 9 && boxes[50 * 100 + 50]->bounds().x == 10000);
}

TEST(aCanvasFramesItsItems) {
    Canvased c;
    c.canvas->addItem(std::make_unique<Box>(), {-500, -100});
    c.canvas->addItem(std::make_unique<Box>(), {1420, 900});
    // before it has a size: when it has one
    c.canvas->frameItems();
    c.root.layout();
    const Rect all = c.canvas->itemsBounds();
    CHECK(near(all.x, -500) && near(all.right(), 1500) && near(all.bottom(), 940));
    const Rect shown = c.canvas->visibleArea();
    CHECK(shown.x <= all.x && shown.right() >= all.right() && shown.y <= all.y && shown.bottom() >= all.bottom());
    CHECK(c.canvas->zoom() < 1);
    CHECK(c.canvas->itemsIn({-600, -200, 200, 200}).size() == 1);
    // small: not zoomed past 1
    Canvased d;
    d.canvas->addItem(std::make_unique<Box>(), {0, 0});
    d.root.layout();
    d.canvas->frameItems();
    CHECK(near(d.canvas->zoom(), 1) && near(d.canvas->fromCanvas({40, 20}), {200, 150}));
}

TEST(anItemRaisedKeepsTheMouse) {
    Canvased c;
    Box& under = c.canvas->addItem(std::make_unique<Box>(), {0, 0});
    Box& over = c.canvas->addItem(std::make_unique<Box>(), {40, 20});
    c.root.layout();
    CHECK(c.root.widgetAt({50, 30}) == &over);
    c.button(platform::MouseButton::Left, true, {10, 10});
    CHECK(under.presses == 1);
    c.canvas->raiseItem(under);
    c.root.layout();
    CHECK(c.root.widgetAt({50, 30}) == &under);
    c.move({60, 30});
    c.button(platform::MouseButton::Left, false, {60, 30});
    CHECK(under.moves.size() == 1 && near(under.moves[0], {60, 30}));
    c.canvas->lowerItem(under);
    c.root.layout();
    CHECK(c.root.widgetAt({50, 30}) == &over);
    std::unique_ptr<Widget> taken = c.canvas->removeItem(over);
    CHECK(taken.get() == &over && c.canvas->children().size() == 1);
}

TEST(bezierCurvesAreFlattenedToTheirTolerance) {
    std::vector<Point> points;
    paint::flattenBezier({0, 0}, {100, 0}, {0, 100}, {100, 100}, 0.25f, points);
    CHECK(near(points.front(), {0, 0}) && near(points.back(), {100, 100}));
    const std::size_t fine = points.size();
    points.clear();
    paint::flattenBezier({0, 0}, {100, 0}, {0, 100}, {100, 100}, 4, points);
    CHECK(points.size() < fine && points.size() >= 3);
    // straight: one segment
    points.clear();
    paint::flattenBezier({0, 0}, {10, 10}, {20, 20}, {30, 30}, 0.25f, points);
    CHECK(points.size() == 2);
    // the middle of the curve is on it: t = 1/2 of this one is (50, 50)
    points.clear();
    paint::flattenBezier({0, 0}, {100, 0}, {0, 100}, {100, 100}, 0.01f, points);
    bool middle = false;
    for (Point p : points)
        middle = middle || (std::abs(p.x - 50) < 1 && std::abs(p.y - 50) < 1);
    CHECK(middle);
}

TEST(aBoxInAGradientHasItsColorsAtItsTopAndBottom) {
    paint::DrawList list;
    {
        paint::Painter p(list, {0, 0, 200, 200}, 1);
        p.fillRoundedRect({10, 10, 100, 100}, 4, Color{1, 0, 0, 1}, Color{0, 0, 1, 1});
        p.fillRoundedRect({10, 120, 100, 20}, 4, Color{0, 1, 0, 1});
    }
    // four vertices each, the quad a pixel larger than the box: its colors carried on past its edges
    CHECK(list.vertices.size() == 8);
    const auto red = [](std::uint32_t c) { return c & 0xff; };
    const auto blue = [](std::uint32_t c) { return (c >> 16) & 0xff; };
    CHECK(red(list.vertices[0].color) == 255 && blue(list.vertices[0].color) == 0);
    CHECK(blue(list.vertices[2].color) == 255 && red(list.vertices[2].color) == 0);
    CHECK(list.vertices[4].color == list.vertices[6].color);
    // fading to transparent: premultiplied, so its color is kept, not faded toward black
    list.clear();
    {
        paint::Painter p(list, {0, 0, 200, 200}, 1);
        p.fillRoundedRect({0, 10, 100, 100}, 0, Color{1, 1, 1, 1}, Color{1, 1, 1, 0});
    }
    CHECK(red(list.vertices[0].color) == 255 && (list.vertices[0].color >> 24) == 255);
    CHECK((list.vertices[2].color >> 24) == 0);
}
