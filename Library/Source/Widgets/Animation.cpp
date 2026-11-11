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

void Scheduler::post(std::function<void()> fn) {
    {
        std::lock_guard lock(postMu_);
        posted_.push_back(std::move(fn));
        hasPosted_ = true;
    }
    if (wakeUp_)
        wakeUp_();
}

void Scheduler::tick() {
    // what was posted, first: what a worker found may start timers and animations
    if (hasPosted_) {
        std::vector<std::function<void()>> posted;
        {
            std::lock_guard lock(postMu_);
            posted.swap(posted_);
            hasPosted_ = false;
        }
        for (auto& fn : posted)
            fn();
    }
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
    if (!animations_.empty() || hasPosted_)
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
    const std::uint64_t generation = scheduler ? scheduler->styleGeneration() : 0;
    if (!set_ || generation != styleGeneration_) {
        set_ = true;
        styleGeneration_ = generation;
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

// ---- scrolling ----

SmoothScroll::SmoothScroll(std::function<void(double)> show)
    : progress_([this] { show_(from_ + (to_ - from_) * shape(progress_.value())); }), show_(std::move(show)) {
    progress_.set(1);
}

double SmoothScroll::shape(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    const double left = 1 - t;
    return 1 - left * left * left * (1 + bend_ * t);
}

double SmoothScroll::slope(double t) const {
    t = std::clamp(t, 0.0, 1.0);
    const double left = 1 - t;
    return left * left * (3 - bend_ + 4 * bend_ * t);
}

double SmoothScroll::velocity() const {
    if (!running() || seconds_ <= 0)
        return 0;
    return (to_ - from_) * slope(progress_.value()) / seconds_;
}

double SmoothScroll::aim(Scheduler* scheduler, double shown, double step, double low, double high) {
    const double from = running() && aiming_ ? aimed_ : target(shown);
    aimed_ = std::clamp(from + boosted(scheduler, step), low, high);
    return aimed_;
}

double SmoothScroll::boosted(Scheduler* scheduler, double step) {
    const bool sameWay = (step > 0) == (lastStep_ > 0) && lastStep_ != 0;
    const std::optional<TimePoint> now = scheduler ? std::optional<TimePoint>(scheduler->now()) : std::nullopt;
    const double gap = now && lastTurn_ && sameWay ? std::chrono::duration<double>(*now - *lastTurn_).count() : slowTurn;
    lastTurn_ = now;
    lastStep_ = step;
    if (gap >= slowTurn) {
        pace_ = slowTurn;       // a pause, or the other way: a step
        return step;
    }
    // the pace over the last few steps (one quick step among slow ones is not a flick), eased in: slow steps stay steps
    pace_ = (pace_ + gap) / 2;
    const double fast = std::clamp((slowTurn - pace_) / (slowTurn - fastTurn), 0.0, 1.0);
    return step * (1 + (maximumBoost - 1) * fast * fast);
}

void SmoothScroll::scrollTo(Scheduler* scheduler, double shown, double target, Duration duration, bool aimed) {
    const double seconds = std::chrono::duration<double>(duration).count();
    // from rest: at once, as fast as it goes; on the way, the speed it has (as much as it goes the same way, and some)
    double speed = startSpeed;
    if (running() && target != shown && seconds > 0)
        speed = std::clamp(velocity() * seconds / (target - shown), minimumSpeed, startSpeed);
    bend_ = startSpeed - speed;
    from_ = shown;
    to_ = target;
    seconds_ = seconds;
    aiming_ = aimed;
    progress_.set(0);
    progress_.animateTo(scheduler, 1, duration, Easing::Linear);
    if (!progress_.running())
        show_(to_);
}

} // namespace glw
