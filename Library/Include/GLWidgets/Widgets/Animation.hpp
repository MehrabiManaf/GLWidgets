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

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

namespace glw {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

class Timer;
class Animation;

class GLW_EXPORT Scheduler {
public:
    // `clock`: where the time comes from (a test's own); the steady clock by default.
    explicit Scheduler(std::function<TimePoint()> clock = {});
    ~Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    TimePoint now() const;
    // Fires the timers that are due and steps the animations that run, to now().
    void tick();
    // When tick has something to do next: the earliest timer, or now while an animation runs; none when nothing waits.
    std::optional<TimePoint> nextDeadline() const;
    bool animating() const { return !animations_.empty(); }

    // Off: animations end where they go at once (reduced motion; tests). Timers still fire.
    void setAnimationsEnabled(bool enabled) { animationsEnabled_ = enabled; }
    bool animationsEnabled() const { return animationsEnabled_; }

private:
    friend class Timer;
    friend class Animation;

    std::function<TimePoint()> clock_;
    std::vector<Timer*> timers_;            // those started
    std::vector<Animation*> animations_;    // those running
    bool animationsEnabled_ = true;
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

// A color easing to the one asked for.
class GLW_EXPORT ColorTransition {
public:
    explicit ColorTransition(std::function<void()> onStep = {}) : progress_(std::move(onStep)) {}

    void setOnStep(std::function<void()> onStep) { progress_.setOnStep(std::move(onStep)); }
    // Toward `color` from what it shows now (nothing when it goes there already); the first time, at once.
    void to(Scheduler* scheduler, Color color, Duration duration);
    Color value() const;
    bool running() const { return progress_.running(); }

private:
    Animation progress_;                    // from 0 (from_) to 1 (to_)
    Color from_;
    Color to_;
    bool set_ = false;
};

} // namespace glw
