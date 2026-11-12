// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Time in the interface: timers (a tooltip's delay, a caret's blink) and animations (a color easing to another), run by one
// Scheduler that every root of an application shares (UiContext::scheduler).
//
// Nothing is drawn for time alone. The application sleeps until the next timer is due (nextDeadline), or until an event
// comes; only while an animation runs does it draw at each refresh of the display, and then only the windows whose widgets
// an animation changed (their onStep invalidates their paint). When the last animation ends, the frames stop. An engine
// driving its own roots calls tick() before it paints, and waits as nextDeadline says.
//
// A widget does not say when its state changes, only what it shows now: ColorTransition::to(color) in paint eases from
// what it showed to that color (at once the first time), and asks for the frames that takes.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Export.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace glw {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

class Timer;
class Animation;

// One of `ways` (0 to ways - 1) at random: which way a motion that varies goes this time (MotionStyle::varied).
GLW_EXPORT int randomWay(int ways);

class GLW_EXPORT Scheduler {
public:
    // `clock`: where the time comes from (a test's own); the steady clock by default.
    explicit Scheduler(std::function<TimePoint()> clock = {});
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    TimePoint now() const;
    // Runs what was posted, fires the timers that are due and steps the animations that run, to now().
    void tick();
    // From any thread: `fn` runs on the thread of the user interface, at the start of the next tick, in the order posted (what
    // a worker found, shown). It wakes the loop that sleeps in its events (setWakeUp). A function must not be posted to a
    // scheduler that is gone: what posts from a thread stops before the application ends.
    void post(std::function<void()> fn);
    // What post calls so that the loop waiting for events wakes up: the Application's (the platform's wakeUp); an engine
    // driving its roots, its own.
    void setWakeUp(std::function<void()> wakeUp) { wakeUp_ = std::move(wakeUp); }
    // When tick has something to do next: the earliest timer, or now while an animation runs or something posted waits; none
    // when nothing waits.
    std::optional<TimePoint> nextDeadline() const;
    bool animating() const { return !animations_.empty(); }

    // Off: animations end where they go at once (reduced motion; tests). Timers still fire.
    void setAnimationsEnabled(bool enabled) { animationsEnabled_ = enabled; }
    bool animationsEnabled() const { return animationsEnabled_; }

    // The theme changed (UiRoot::styleChanged): the colors of states go to the new ones at once, not eased (a theme being
    // eased to changes at every step itself).
    void styleChanged() { ++styleGeneration_; }
    std::uint64_t styleGeneration() const { return styleGeneration_; }

private:
    friend class Timer;
    friend class Animation;

    std::function<TimePoint()> clock_;
    std::vector<Timer*> timers_;            // those started
    std::vector<Animation*> animations_;    // those running
    bool animationsEnabled_ = true;
    std::uint64_t styleGeneration_ = 0;
    std::mutex postMu_;
    std::vector<std::function<void()>> posted_;
    std::atomic<bool> hasPosted_{false};
    std::function<void()> wakeUp_;
};

// Calls a function once a delay has passed, or every time it has, until stopped. Stopped when destroyed: a widget keeps
// its timers as members.
class GLW_EXPORT Timer {
public:
    Timer() = default;
    ~Timer();
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // `fire` after `delay` (and every `delay` after that, when `repeat`); started again from now when it runs.
    void start(Scheduler& scheduler, Duration delay, std::function<void()> fire, bool repeat = false);
    void stop();
    bool active() const { return scheduler_ != nullptr; }

private:
    friend class Scheduler;
    Scheduler* scheduler_ = nullptr;
    TimePoint due_;
    Duration delay_{};
    bool repeat_ = false;
    std::function<void()> fire_;
};

enum class Easing : std::uint8_t {
    Linear,
    OutCubic,               // fast, then slowing down: what responds to the user (Fluent's "decelerate")
    InOutCubic,             // slow, fast, slow: what moves from one place to another
    OutBack,                // fast, a little past the target, back to it: a key springing back
};

GLW_EXPORT float ease(Easing easing, float t);
GLW_EXPORT Color lerp(Color a, Color b, float t);

// A number going to a target over a time, eased. While it moves, `onStep` is called at each frame (after the value
// changed): a widget invalidates its paint there.
class GLW_EXPORT Animation {
public:
    explicit Animation(std::function<void()> onStep = {});
    ~Animation();
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;

    void setOnStep(std::function<void()> onStep) { onStep_ = std::move(onStep); }
    float value() const { return value_; }
    float target() const { return to_; }
    bool running() const { return scheduler_ != nullptr; }

    // From its value now to `target` over `duration`; at once without a scheduler, with animations off or no duration.
    void animateTo(Scheduler* scheduler, float target, Duration duration, Easing easing = Easing::OutCubic);
    // At once, stopping it.
    void set(float value);

private:
    friend class Scheduler;
    void step(TimePoint now);               // the scheduler's
    void stopRunning();

    std::function<void()> onStep_;
    Scheduler* scheduler_ = nullptr;
    float value_ = 0;
    float from_ = 0;
    float to_ = 0;
    TimePoint start_;
    Duration duration_{};
    Easing easing_ = Easing::OutCubic;
};

// A scroll offset easing to where it is sent: a turn of the wheel, a page, an area brought into view. It starts as fast as
// it goes and slows down to a stop (as OutCubic); sent on while it moves (the wheel turned again), it goes from where it
// shows to the new place at the speed it has, speeding up or slowing down from it (no jump in its speed at each turn: a
// wheel turned on and on moves evenly). `show` sets the offset shown at each step, and at once with animations off; the
// widget clamps it. Offsets are doubles: a list of a hundred million rows is longer than a float counts dips exactly.
//
// The curve, in time t from 0 to 1: 1 - (1 - t)^3 (1 + c t), its speed at the start 3 - c (of the distance over the
// duration; c from 0 to 3), at the end none, nor any change in it: it settles without a bump.
class GLW_EXPORT SmoothScroll {
public:
    explicit SmoothScroll(std::function<void(double)> show);

    // From `shown` (where it is now) to `target`, over `duration`. `aimed`: the target is where the wheel aimed (aim),
    // snapped, so that its next turn adds to that aim.
    void scrollTo(Scheduler* scheduler, double shown, double target, Duration duration, bool aimed = false);
    // Where it is going; where it is (`shown`) when it does not move.
    double target(double shown) const { return running() ? to_ : shown; }
    bool running() const { return progress_.running(); }
    // Where it shows now, it stays (a drag of the thumb takes over).
    void stop() { progress_.set(1); }
    // How fast it moves now, in dips a second.
    double velocity() const;

    // Where a turn of the wheel aims: `step` (boosted) from where it aimed before while it goes there, not from where that
    // was snapped to (a row's top), so that the steps of a turning wheel add up evenly; between `low` and `high`.
    double aim(Scheduler* scheduler, double shown, double step, double low, double high);
    // A step of the wheel, larger as the wheel turns faster: by how soon it came after the last one the same way (the
    // time between them smoothed over the last few), a step at `slowTurn` apart or more, `maximumBoost` steps at
    // `fastTurn` or less, eased in between; a pause or the other way, a step again. Without a scheduler, a step.
    double boosted(Scheduler* scheduler, double step);
    static constexpr double maximumBoost = 4;
    static constexpr double slowTurn = 0.12;        // seconds between two steps of the wheel
    static constexpr double fastTurn = 0.02;
    // Its speed at the start, of the distance over the duration: from rest, and at least when sent on.
    static constexpr double startSpeed = 3;
    static constexpr double minimumSpeed = 1;

private:
    double shape(double t) const;           // the distance gone, from 0 to 1
    double slope(double t) const;           // its speed

    Animation progress_;                    // the time, from 0 (from_) to 1 (to_)
    double from_ = 0;
    double to_ = 0;
    double bend_ = 0;                       // c
    double seconds_ = 0;                    // how long it takes
    bool aiming_ = false;                   // the target is where the wheel aimed, snapped
    double aimed_ = 0;                      // ... that aim
    double lastStep_ = 0;
    std::optional<TimePoint> lastTurn_;     // when the wheel last turned
    double pace_ = slowTurn;                // the time between its steps, smoothed
    std::function<void(double)> show_;
};

// A color easing to the one asked for.
class GLW_EXPORT ColorTransition {
public:
    explicit ColorTransition(std::function<void()> onStep = {}) : progress_(std::move(onStep)) {}

    void setOnStep(std::function<void()> onStep) { progress_.setOnStep(std::move(onStep)); }
    // Toward `color` from what it shows now (nothing when it goes there already); the first time, and after the theme
    // changed, at once.
    void to(Scheduler* scheduler, Color color, Duration duration);
    Color value() const;
    bool running() const { return progress_.running(); }

private:
    Animation progress_;                    // from 0 (from_) to 1 (to_)
    Color from_;
    Color to_;
    bool set_ = false;
    std::uint64_t styleGeneration_ = 0;     // the scheduler's when it was last set
};

} // namespace glw
