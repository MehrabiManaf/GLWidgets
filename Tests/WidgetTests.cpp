// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Widget trees in a UiRoot, without a window or a GPU: layout, mirroring, the routes of events, focus, the window's hit
// areas.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace glw;
using platform::HitArea;

namespace {

using tests::FakeHost;

UiContext& context() {
    return tests::testContext();
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.01f;
}

// A widget of a fixed size that writes down the events it gets.
struct Probe : Widget {
    std::string name;
    std::vector<std::string>* log;
    bool handlePress = false;
    bool hoveredNow = false;
    Probe(std::string n, std::vector<std::string>* l, Size s = {}) : name(std::move(n)), log(l) { setMinimumSize(s); }
    void onPreviewMouse(MouseEvent& e) override {
        if (e.type == MouseEventType::Press)
            log->push_back("preview " + name);
    }
    void onMouse(MouseEvent& e) override {
        if (e.type == MouseEventType::Press) {
            log->push_back("press " + name);
            e.handled = handlePress;
        } else if (e.type == MouseEventType::Release) {
            log->push_back("release " + name);
        } else if (e.type == MouseEventType::Move) {
            log->push_back("move " + name);
        }
    }
    void onHoverChanged(bool h) override { hoveredNow = h; }
};

platform::Event press(Point p) {
    return platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, p};
}

platform::Event release(Point p) {
    return platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, p};
}

std::unique_ptr<Widget> stack(Orientation orientation, float spacing = 0) {
    auto w = std::make_unique<Widget>();
    w->setLayout(std::make_unique<StackLayout>(orientation, spacing));
    return w;
}

} // namespace

TEST(verticalStackWithStretch) {
    UiRoot root(context());
    root.setSize({100, 300});
    auto content = stack(Orientation::Vertical, 10);
    std::vector<std::string> log;
    Widget& a = content->add(std::make_unique<Probe>("a", &log, Size{0, 50}));
    Widget& b = content->add(std::make_unique<Probe>("b", &log, Size{0, 50}));
    Widget& c = content->add(std::make_unique<Probe>("c", &log, Size{0, 50}));
    b.setStretch(1);
    c.setStretch(3);
    root.setContent(std::move(content));
    root.layout();
    // 300 - 3 x 50 - 2 x 10 = 130 left: b takes a quarter, c three
    CHECK(near(a.bounds().y, 0) && near(a.bounds().height, 50));
    CHECK(near(b.bounds().y, 60) && near(b.bounds().height, 50 + 32.5f));
    CHECK(near(c.bounds().y, 60 + 82.5f + 10) && near(c.bounds().height, 50 + 97.5f));
    CHECK(near(a.bounds().width, 100));     // stretched across
}

TEST(horizontalStackIsMirroredRightToLeft) {
    UiRoot root(context());
    root.setSize({300, 40});
    auto content = stack(Orientation::Horizontal);
    std::vector<std::string> log;
    Widget& first = content->add(std::make_unique<Probe>("first", &log, Size{50, 20}));
    Widget& second = content->add(std::make_unique<Probe>("second", &log, Size{70, 20}));
    second.setMargin({10, 0, 0, 0});        // at its start
    Widget* row = content.get();
    root.setContent(std::move(content));
    root.layout();
    CHECK(near(first.bounds().x, 0));
    CHECK(near(second.bounds().x, 60));     // after its start margin, on the left
    root.setLayoutDirection(LayoutDirection::RightToLeft);
    root.layout();
    CHECK(row->rightToLeft());
    CHECK(near(first.bounds().right(), 300));
    CHECK(near(second.bounds().right(), 240));     // its start margin is on its right now
}

TEST(alignmentsInACell) {
    UiRoot root(context());
    root.setSize({200, 100});
    auto content = std::make_unique<Widget>();
    std::vector<std::string> log;
    Widget& w = content->add(std::make_unique<Probe>("w", &log, Size{40, 20}));
    w.setAlignment(Alignment::Start, Alignment::End);
    root.setContent(std::move(content));
    root.layout();
    CHECK(near(w.bounds().x, 0) && near(w.bounds().y, 80) && near(w.bounds().width, 40));
    root.setLayoutDirection(LayoutDirection::RightToLeft);
    root.layout();
    CHECK(near(w.bounds().x, 160));         // Start is the right
}

TEST(mouseEventsPreviewThenBubble) {
    UiRoot root(context());
    root.setSize({100, 100});
    std::vector<std::string> log;
    auto outer = std::make_unique<Probe>("outer", &log);
    Probe& inner = outer->add(std::make_unique<Probe>("inner", &log, Size{40, 40}));
    inner.setAlignment(Alignment::Start, Alignment::Start);
    Probe* outerPtr = outer.get();
    root.setContent(std::move(outer));
    root.layout();

    root.dispatch(press({10, 10}));
    CHECK((log == std::vector<std::string>{"preview outer", "preview inner", "press inner", "press outer"}));

    // handled by the outer one: it has the mouse until the button is released, even outside of itself
    log.clear();
    root.dispatch(release({10, 10}));
    outerPtr->handlePress = true;
    log.clear();
    root.dispatch(press({10, 10}));
    CHECK((log == std::vector<std::string>{"preview outer", "preview inner", "press inner", "press outer"}));
    log.clear();
    root.dispatch(platform::MouseMoved{1, {500, 500}});
    root.dispatch(release({500, 500}));
    CHECK((log == std::vector<std::string>{"move outer", "release outer"}));
}

TEST(hoverFollowsTheMouse) {
    UiRoot root(context());
    root.setSize({100, 100});
    std::vector<std::string> log;
    auto outer = std::make_unique<Probe>("outer", &log);
    Probe& inner = outer->add(std::make_unique<Probe>("inner", &log, Size{40, 40}));
    inner.setAlignment(Alignment::Start, Alignment::Start);
    Probe* outerPtr = outer.get();
    root.setContent(std::move(outer));
    root.layout();
    root.dispatch(platform::MouseMoved{1, {10, 10}});
    CHECK(inner.hoveredNow && outerPtr->hoveredNow);
    root.dispatch(platform::MouseMoved{1, {80, 80}});
    CHECK(!inner.hoveredNow && outerPtr->hoveredNow);
    root.dispatch(platform::MouseCrossed{1, false});
    CHECK(!outerPtr->hoveredNow);
}

TEST(focusByClickAndTab) {
    UiRoot root(context());
    root.setSize({400, 100});
    auto content = stack(Orientation::Horizontal, 4);
    Button& a = content->add(std::make_unique<Button>("a"));
    Button& b = content->add(std::make_unique<Button>("b"));
    Button& c = content->add(std::make_unique<Button>("c"));
    c.setEnabled(false);
    root.setContent(std::move(content));
    root.layout();

    root.dispatch(press(b.mapToRoot({2, 2})));
    root.dispatch(release(b.mapToRoot({2, 2})));
    CHECK(b.hasFocus());
    root.dispatch(platform::KeyChanged{1, platform::Key::Tab, platform::KeyModifiers::None, true, false});
    CHECK(a.hasFocus());        // c is disabled: around to a
    root.dispatch(platform::KeyChanged{1, platform::Key::Tab, platform::KeyModifiers::Shift, true, false});
    CHECK(b.hasFocus());

    // a removed widget leaves no focus behind
    std::unique_ptr<Widget> removed = b.parent()->removeChild(b);
    CHECK(root.focusWidget() == nullptr);
}

TEST(buttonClicks) {
    UiRoot root(context());
    root.setSize({200, 100});
    int clicks = 0;
    auto button = std::make_unique<Button>("OK", [&] { ++clicks; });
    Button* b = button.get();
    root.setContent(std::move(button));
    root.layout();
    const Point inside = b->mapToRoot({3, 3});
    root.dispatch(press(inside));
    root.dispatch(release(inside));
    CHECK(clicks == 1);
    root.dispatch(press(inside));
    root.dispatch(release({199, 99}));      // let go elsewhere: no click
    CHECK(clicks == 1);
    root.dispatch(platform::KeyChanged{1, platform::Key::Enter, platform::KeyModifiers::None, true, false});
    CHECK(clicks == 2);         // it has the focus since it was pressed
}

TEST(windowFrameHitAreas) {
    FakeHost host;
    UiRoot root(context(), &host);
    root.setSize({800, 600});
    auto frame = std::make_unique<WindowFrame>();
    WindowFrame* f = frame.get();
    root.setContent(std::move(frame));
    root.layout();
    CHECK(near(f->titleBar().bounds().height, context().theme.titleBar.height));
    CHECK(root.windowHitTest({400, 20}) == HitArea::Caption);
    CHECK(root.windowHitTest({400, 300}) == HitArea::Client);
    CHECK(root.windowHitTest({2, 2}) == HitArea::ResizeTopLeft);
    CHECK(root.windowHitTest({798, 300}) == HitArea::ResizeRight);
    CHECK(root.windowHitTest({400, 599}) == HitArea::ResizeBottom);
    // the buttons, at the end: close is the last one, on the right
    const bool atEnd = !context().theme.frame.buttonsAtStart;
    const Point close = atEnd ? Point{800 - 20, 20} : Point{20, 20};
    CHECK(root.windowHitTest(close) == HitArea::Client);
    root.dispatch(press(close));
    root.dispatch(release(close));
    CHECK(host.closed == 1);
    // maximized: no resize borders (the corner away from the buttons: the caption)
    host.state = platform::WindowState::Maximized;
    CHECK(root.windowHitTest(atEnd ? Point{2, 2} : Point{798, 2}) == HitArea::Caption);
    // right to left: the buttons go to the other side
    host.state = platform::WindowState::Normal;
    root.setLayoutDirection(LayoutDirection::RightToLeft);
    root.layout();
    const Point mirrored{800 - close.x, close.y};
    root.dispatch(press(mirrored));
    root.dispatch(release(mirrored));
    CHECK(host.closed == 2);
}

TEST(labelsMeasureTheirText) {
    UiRoot root(context());
    root.setSize({120, 400});
    auto content = stack(Orientation::Vertical);
    Label& line = content->add(std::make_unique<Label>("Hello"));
    Label& wrapped = content->add(std::make_unique<Label>("ط³ظ„ط§ظ… ط¯ظ†غŒط§طŒ ط§غŒظ† ظ…طھظ† ط¨ط§غŒط¯ ط¯ط± ع†ظ†ط¯ ط®ط· ط´ع©ط³طھظ‡ ط´ظˆط¯"));
    wrapped.setWrap(true);
    root.setContent(std::move(content));
    root.layout();
    CHECK(line.desiredSize().width > 10 && line.desiredSize().height > 10);
    CHECK(wrapped.desiredSize().height > 2 * line.desiredSize().height);   // several lines in 120 dips
    CHECK(wrapped.desiredSize().width <= 120.01f);
}

TEST(paintRecordsTheTree) {
    UiRoot root(context());
    root.setSize({300, 200});
    root.setContent(std::make_unique<WindowFrame>(std::make_unique<Button>("OK")));
    paint::DrawList list;
    root.paint(list);
    CHECK(!list.empty());
    CHECK(!root.needsPaint() && !root.needsLayout());
}

namespace {

// Counts its arrangements and its paintings; as large as it is told.
struct Counted final : Widget {
    Size wanted{100, 20};
    int arranged = 0;
    int painted = 0;

protected:
    Size measureContent(Size) override { return wanted; }
    void arrangeContent(const Rect&) override { ++arranged; }
    void paint(PaintContext&) override { ++painted; }
};

} // namespace

TEST(onlyWhatChangedIsLaidOutAndOnlyWhatShowsIsPainted) {
    UiRoot root(tests::testContext());
    root.setSize({400, 300});
    auto list = std::make_unique<Widget>();
    list->setLayout(std::make_unique<StackLayout>(Orientation::Vertical));
    std::vector<Counted*> items;
    for (int i = 0; i < 100; ++i)
        items.push_back(&list->add(std::make_unique<Counted>()));
    auto scroll = std::make_unique<ScrollArea>();
    ScrollArea& area = *scroll;
    scroll->setContent(std::move(list));
    root.setContent(std::move(scroll));
    root.layout();
    CHECK(items[0]->arranged == 1 && items[99]->arranged == 1);

    // the first one grows: it is arranged again; the others only move down
    items[0]->wanted = {100, 40};
    items[0]->invalidateLayout();
    root.layout();
    CHECK(items[0]->arranged == 2 && items[1]->arranged == 1 && items[99]->arranged == 1);
    CHECK(near(items[1]->bounds().y, 40));

    // scrolled: nothing in the content is arranged again
    area.setOffset({0, 500});
    root.layout();
    CHECK(items[30]->arranged == 1 && items[0]->arranged == 2);

    // painted: those that show (and a few around, for what paints out of its bounds), not the others
    paint::DrawList list2;
    root.paint(list2);
    CHECK(items[30]->painted == 1 && items[0]->painted == 0 && items[99]->painted == 0);
}
