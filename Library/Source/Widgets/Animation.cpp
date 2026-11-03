// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Animation.hpp>

#include <algorithm>

namespace glw {

// ---- scheduler ----

Scheduler::Scheduler(std::function<TimePoint()> clock) : clock_(std::move(clock)) {}

Scheduler::~Scheduler() {
    // what still runs outlives it: let go of it
    for (Timer* t : timers_)
        t->scheduler_ = nullptr;
    for (Animation* a : animations_)
        a->scheduler_ = nullptr;
}

TimePoint Scheduler::now() const {
    return clock_ ? clock_() : Clock::now();
}

void Scheduler::tick() {
    const TimePoint at = now();
    // timers first (one may start an animation), each that is due once; a handler may start or stop others
    for (Timer* t : std::vector(timers_)) {
        if (std::find(timers_.begin(), timers_.end(), t) == timers_.end() || t->due_ > at)
            continue;
        if (t->repeat_) {
            t->due_ += t->delay_;
            if (t->due_ <= at)          // late (the application was busy): from now, not to catch up
                t->due_ = at + t->delay_;
        } else {
            t->stop();
        }
        if (const auto fire = t->fire_)
            fire();
    }
    for (Animation* a : std::vector(animations_)) {
        if (std::find(animations_.begin(), animations_.end(), a) != animations_.end())
            a->step(at);
    }
}

std::optional<TimePoint> Scheduler::nextDeadline() const {
    if (!animations_.empty())
        return now();
    std::optional<TimePoint> next;
    for (const Timer* t : timers_)
        next = next ? std::min(*next, t->due_) : t->due_;
    return next;
}

// ---- timers ----

Timer::~Timer() {
    stop();
}

void Timer::start(Scheduler& scheduler, Duration delay, std::function<void()> fire, bool repeat) {
    stop();
    scheduler_ = &scheduler;
    delay_ = std::max(delay, Duration{1});
    due_ = scheduler.now() + delay_;
    repeat_ = repeat;
    fire_ = std::move(fire);
    scheduler.timers_.push_back(this);
}

void Timer::stop() {
    if (!scheduler_)
        return;
    std::erase(scheduler_->timers_, this);
    scheduler_ = nullptr;
}

// ---- easing ----

float ease(Easing easing, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing) {
    case Easing::Linear: return t;
    case Easing::OutCubic: {
        const float u = 1 - t;
        return 1 - u * u * u;
    }
    case Easing::InOutCubic: {
        if (t < 0.5f)
            return 4 * t * t * t;
        const float u = -2 * t + 2;
        return 1 - u * u * u / 2;
    }
    }
    return t;
}

Color lerp(Color a, Color b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

// ---- animations ----

Animation::Animation(std::function<void()> onStep) : onStep_(std::move(onStep)) {}

Animation::~Animation() {
    stopRunning();
}

void Animation::animateTo(Scheduler* scheduler, float target, Duration duration, Easing easing) {
    if (target == to_ && (running() || value_ == target))
        return;
    if (!scheduler || !scheduler->animationsEnabled() || duration <= Duration::zero()) {
        set(target);
        return;
    }
    from_ = value_;
    to_ = target;
    start_ = scheduler->now();
    duration_ = duration;
    easing_ = easing;
    if (scheduler_ != scheduler) {
        stopRunning();
        scheduler_ = scheduler;
        scheduler->animations_.push_back(this);
    }
}

void Animation::set(float value) {
    stopRunning();
    value_ = from_ = to_ = value;
}

void Animation::step(TimePoint now) {
    const float t = std::chrono::duration<float>(now - start_) / std::chrono::duration<float>(duration_);
    if (t >= 1) {
        value_ = to_;
        stopRunning();
    } else {
        value_ = from_ + (to_ - from_) * ease(easing_, t);
    }
    if (onStep_)
        onStep_();
}

void Animation::stopRunning() {
    if (!scheduler_)
        return;
    std::erase(scheduler_->animations_, this);
    scheduler_ = nullptr;
}

// ---- colors ----

void ColorTransition::to(Scheduler* scheduler, Color color, Duration duration) {
    if (!set_) {
        set_ = true;
        from_ = to_ = color;
        progress_.set(1);
        return;
    }
    if (color == to_)
        return;
    from_ = value();
    to_ = color;
    progress_.set(0);
    progress_.animateTo(scheduler, 1, duration);
}

Color ColorTransition::value() const {
    return lerp(from_, to_, progress_.value());
}

} // namespace glw
