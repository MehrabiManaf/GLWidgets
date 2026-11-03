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
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>

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
