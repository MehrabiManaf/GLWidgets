// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Time: timers due and repeated, animations eased to their targets and then nothing more to do; what widgets do with them
// (a button's colors easing, a caret blinking then resting, a tooltip after a delay). Nothing asks for a frame once
// nothing moves.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <thread>
#include <vector>
#include <memory>
#include <optional>
#include <string>

using namespace glw;
using namespace std::chrono_literals;

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 0.001f;
}

// A scheduler on a clock of its own.
struct Timed {
    TimePoint now{};
    Scheduler scheduler{[this] { return now; }};
    void advance(Duration by) {
        now += by;
        scheduler.tick();
    }
};

// Animations on in the tests' scheduler while it lives.
struct Animated {
    Animated() { tests::testScheduler().setAnimationsEnabled(true); }
    ~Animated() { tests::testScheduler().setAnimationsEnabled(false); }
};

} // namespace

TEST(timersFireWhenDueAndRepeat) {
    Timed t;
    int once = 0, every = 0;
    Timer a, b;
    a.start(t.scheduler, 100ms, [&] { ++once; });
    CHECK(t.scheduler.nextDeadline() == t.now + 100ms);
    t.advance(99ms);
    CHECK(once == 0 && a.active());
    t.advance(1ms);
    CHECK(once == 1 && !a.active() && !t.scheduler.nextDeadline());
    // repeating: every 50 ms; late, it goes on from then, without catching up
    b.start(t.scheduler, 50ms, [&] { ++every; }, true);
    t.advance(50ms);
    t.advance(50ms);
    CHECK(every == 2 && t.scheduler.nextDeadline() == t.now + 50ms);
    t.advance(500ms);
    CHECK(every == 3 && t.scheduler.nextDeadline() == t.now + 50ms);
    b.stop();
    CHECK(!t.scheduler.nextDeadline());
    // destroyed while started: gone from the scheduler
    {
        Timer c;
        c.start(t.scheduler, 10ms, [&] { ++once; });
    }
    t.advance(20ms);
    CHECK(once == 1 && !t.scheduler.nextDeadline());
}

TEST(animationsEaseToTheirTargetsThenNothingIsLeftToDo) {
    Timed t;
    int steps = 0;
    Animation a([&] { ++steps; });
    a.animateTo(&t.scheduler, 1, 100ms, Easing::Linear);
    CHECK(a.running() && t.scheduler.animating() && t.scheduler.nextDeadline() == t.now);
    t.advance(50ms);
    CHECK(near(a.value(), 0.5f) && steps == 1);
    t.advance(50ms);
    CHECK(near(a.value(), 1) && !a.running() && steps == 2);
    CHECK(!t.scheduler.animating() && !t.scheduler.nextDeadline());
    // eased: fast first
    a.animateTo(&t.scheduler, 0, 100ms, Easing::OutCubic);
    t.advance(50ms);
    CHECK(a.value() < 0.5f && a.value() > 0);
    // retargeted halfway: from where it is
    const float at = a.value();
    a.animateTo(&t.scheduler, 1, 100ms, Easing::Linear);
    CHECK(near(a.value(), at));
    t.advance(100ms);
    CHECK(near(a.value(), 1) && !a.running());
    // without a scheduler, with animations off or no time: at once
    a.animateTo(nullptr, 0, 100ms);
    CHECK(a.value() == 0 && !a.running());
    t.scheduler.setAnimationsEnabled(false);
    a.animateTo(&t.scheduler, 1, 100ms);
    CHECK(a.value() == 1 && !t.scheduler.animating());
}

TEST(colorTransitionsStartWhereTheyAreAndEaseFromThere) {
    Timed t;
    ColorTransition c;
    const Color red{1, 0, 0, 1}, blue{0, 0, 1, 1};
    c.to(&t.scheduler, red, 100ms);
    CHECK(c.value() == red && !c.running());                // the first: at once
    c.to(&t.scheduler, blue, 100ms);
    CHECK(c.value() == red && c.running());
    t.advance(100ms);
    CHECK(c.value() == blue && !c.running());
    c.to(&t.scheduler, blue, 100ms);                        // where it is: nothing to do
    CHECK(!t.scheduler.animating());
}

TEST(aButtonsColorsEaseAndThenNothingIsDrawn) {
    Animated on;
    UiRoot root(tests::testContext());
    auto made = std::make_unique<Button>("OK");
    made->setAlignment(Alignment::Start, Alignment::Start);
    root.setContent(std::move(made));
    root.setSize({200, 100});
    paint::DrawList list;
    root.paint(list);
    CHECK(!tests::testScheduler().animating());
    // the mouse over it: its background eases for the theme's fast duration, a frame each step, then none
    root.dispatch(platform::MouseMoved{1, {10, 10}});
    root.paint(list);
    CHECK(tests::testScheduler().animating() && !root.needsPaint());
    tests::advance(40ms);
    CHECK(root.needsPaint());
    root.paint(list);
    tests::advance(60ms);
    CHECK(root.needsPaint() && !tests::testScheduler().animating());
    root.paint(list);
    CHECK(!root.needsPaint() && !tests::testScheduler().nextDeadline());
}

TEST(aCaretBlinksThenRests) {
    UiRoot root(tests::testContext());
    auto made = std::make_unique<LineEdit>();
    LineEdit& field = *made;
    root.setContent(std::move(made));
    root.setSize({300, 40});
    root.layout();
    paint::DrawList list;
    root.paint(list);
    field.focus();
    root.paint(list);
    const TimePoint start = tests::testClock();
    CHECK(tests::testScheduler().nextDeadline() == start + 530ms);
    // hidden, shown... each a frame
    tests::advance(530ms);
    CHECK(root.needsPaint());
    root.paint(list);
    // typing shows it again and starts again
    root.dispatch(platform::TextInput{1, "a"});
    root.layout();
    CHECK(tests::testScheduler().nextDeadline() == tests::testClock() + 530ms);
    // twenty blinks later: resting, shown, nothing more to draw
    for (int i = 0; i < 20; ++i)
        tests::advance(530ms);
    CHECK(!tests::testScheduler().nextDeadline());
    // the focus gone: no blinking
    field.focus();
    root.setFocus(nullptr);
    CHECK(!tests::testScheduler().nextDeadline());
}

TEST(aTooltipShowsAfterADelayAndGoesWhenTheMouseDoesSomething) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    auto content = std::make_unique<Widget>();
    auto& a = content->add(std::make_unique<Widget>());
    a.setMinimumSize({100, 40});
    a.setAlignment(Alignment::Start, Alignment::Start);
    a.setToolTip("Saves the script");
    root.setContent(std::move(content));
    root.setSize({300, 200});
    root.layout();
    // over it: not yet; after the delay, under the mouse
    root.dispatch(platform::MouseMoved{1, {20, 10}});
    tests::advance(499ms);
    CHECK(host.open().empty());
    tests::advance(1ms);
    CHECK(host.open().size() == 1 && host.open()[0]->tooltip);
    CHECK(host.open()[0]->anchor.x == 20 && host.open()[0]->anchor.y == 10);
    // a press: gone, and not again while the mouse stays
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, {20, 10}});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, {20, 10}});
    CHECK(host.open().empty());
    tests::advance(300ms);
    CHECK(host.open().empty() && !tests::testScheduler().nextDeadline());
    // out and in again soon after (it closed less than 600 ms ago): quickly
    root.dispatch(platform::MouseMoved{1, {200, 150}});
    root.dispatch(platform::MouseMoved{1, {30, 20}});
    tests::advance(100ms);
    CHECK(host.open().size() == 1);
    // out: gone
    root.dispatch(platform::MouseMoved{1, {200, 150}});
    CHECK(host.open().empty());
}

TEST(aSmoothScrollKeepsItsSpeedWhenSentOn) {
    Timed t;
    double shown = 0;
    SmoothScroll s([&](double at) { shown = at; });
    // from rest: at once, as fast as it goes (three times the distance over the time)
    s.scrollTo(&t.scheduler, 0, 100, 180ms);
    CHECK(std::abs(s.velocity() - 3 * 100 / 0.18) < 1);
    t.advance(60ms);
    const double before = s.velocity();
    CHECK(shown > 50 && shown < 100 && before > 0);
    // sent on: from the speed it has, no jump
    const double target = shown + 60;
    s.scrollTo(&t.scheduler, shown, target, 180ms);
    CHECK(std::abs(s.velocity() - before) < before * 0.01);
    // never back, never past it: it settles there
    double last = shown;
    bool forward = true;
    for (int i = 0; i < 20; ++i) {
        t.advance(10ms);
        forward = forward && shown >= last - 1e-9 && shown <= target + 1e-9;
        last = shown;
    }
    CHECK(forward && std::abs(shown - target) < 1e-6 && !s.running());
    // the other way: from rest again, toward it
    s.scrollTo(&t.scheduler, shown, 0, 180ms);
    CHECK(s.velocity() < 0);
}

TEST(whatIsPostedFromAThreadRunsOnTheNextTickInOrder) {
    Timed t;
    int wakes = 0;
    t.scheduler.setWakeUp([&] { ++wakes; });
    CHECK(!t.scheduler.nextDeadline());
    std::vector<int> ran;
    std::thread worker([&] {
        for (int i = 0; i < 3; ++i)
            t.scheduler.post([&ran, i] { ran.push_back(i); });
    });
    worker.join();
    CHECK(wakes == 3);                                  // the loop asleep in its events wakes up
    CHECK(ran.empty());                                 // not on the worker
    CHECK(t.scheduler.nextDeadline() == t.now);         // something waits: the loop does not go back to sleep
    t.scheduler.tick();
    CHECK((ran == std::vector<int>{0, 1, 2}));
    CHECK(!t.scheduler.nextDeadline());
    // posted while what was posted runs: the next tick's
    t.scheduler.post([&] { t.scheduler.post([&] { ran.push_back(9); }); });
    t.scheduler.tick();
    CHECK(ran.size() == 3);
    t.scheduler.tick();
    CHECK(ran.back() == 9);
}

namespace {
// Its left half says "Left", its right half "Right"; below 30, nothing (its parent's: none).
class Halves final : public Widget {
protected:
    std::optional<ToolTip> toolTipAt(Point p) const override {
        if (p.y >= 30)
            return std::nullopt;
        return ToolTip{.text = LocalizedText(p.x < size().width / 2 ? "Left" : "Right")};
    }
};
} // namespace

TEST(aWidgetsPartsShowTheirOwnTooltips) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    auto content = std::make_unique<Widget>();
    auto& h = content->add(std::make_unique<Halves>());
    h.setMinimumSize({100, 40});
    h.setAlignment(Alignment::Start, Alignment::Start);
    root.setContent(std::move(content));
    root.setSize({300, 200});
    root.layout();
    const auto shown = [&]() -> std::string {
        const auto open = host.open();
        if (open.empty())
            return {};
        const Widget& view = *open[0]->root.content();
        return tests::testContext().translator.resolve(static_cast<const Label&>(*view.children()[0]).text());
    };
    root.dispatch(platform::MouseMoved{1, {20, 10}});
    tests::advance(500ms);
    CHECK(shown() == "Left");
    root.dispatch(platform::MouseMoved{1, {30, 12}});      // the same part: it stays
    CHECK(shown() == "Left");
    root.dispatch(platform::MouseMoved{1, {80, 10}});      // another: gone, and its own after the delay (soon: just closed)
    CHECK(host.open().empty());
    tests::advance(500ms);
    CHECK(shown() == "Right");
    root.dispatch(platform::MouseMoved{1, {80, 35}});      // a part without one
    tests::advance(1000ms);
    CHECK(host.open().empty());
    root.dispatch(platform::MouseMoved{1, {200, 150}});
}

TEST(aTooltipHasATitleATextAndANote) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    auto content = std::make_unique<Widget>();
    auto& a = content->add(std::make_unique<Widget>());
    a.setMinimumSize({100, 40});
    a.setAlignment(Alignment::Start, Alignment::Start);
    a.setToolTip(ToolTip{.title = "Type: int", .text = "A whole number.", .note = "Click to change it."});
    auto& b = content->add(std::make_unique<Widget>());
    b.setMinimumSize({100, 40});
    b.setAlignment(Alignment::End, Alignment::End);
    b.setToolTip(ToolTip{.text = "Only a text"});
    root.setContent(std::move(content));
    root.setSize({300, 200});
    root.layout();
    const auto parts = [&] {
        std::vector<std::string> texts;
        for (const auto& child : host.open().at(0)->root.content()->children())
            texts.push_back(tests::testContext().translator.resolve(static_cast<const Label&>(*child).text()));
        return texts;
    };
    root.dispatch(platform::MouseMoved{1, {20, 10}});
    tests::advance(500ms);
    CHECK((parts() == std::vector<std::string>{"Type: int", "A whole number.", "Click to change it."}));
    root.dispatch(platform::MouseMoved{1, {250, 180}});
    tests::advance(500ms);
    CHECK((parts() == std::vector<std::string>{"Only a text"}));
    root.dispatch(platform::MouseMoved{1, {150, 100}});
    CHECK(host.open().empty());
    a.setToolTip(ToolTip{});                    // empty: none
    CHECK(!a.toolTip());
}
