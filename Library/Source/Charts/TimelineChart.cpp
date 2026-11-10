// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/TimelineChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;       // a step of the wheel in: the span this much of what it was
constexpr float laneGap = 6;            // between lanes

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

// The first span of a row that ends after `t` (its spans in order, not over one another: their ends in order too).
std::size_t firstEndingAfter(const std::deque<TimelineSpan>& row, double t) {
    return static_cast<std::size_t>(std::partition_point(row.begin(), row.end(), [&](const TimelineSpan& s) { return s.end <= t; }) - row.begin());
}

} // namespace

TimelineChart::TimelineChart() : TimelineChart(Props{}) {}

TimelineChart::TimelineChart(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), scrolling_([this](double offset) {
          scroll_ = std::clamp(offset, 0.0, lastScroll());
          invalidatePaint();
      }),
      bar_(Orientation::Vertical, [this](double offset, bool eased) { scrollTo(offset, eased); }, [this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setLegendVisible(!props_.categories.empty());
    for (LocalizedText& name : props_.lanes)
        lanes_.push_back({.name = std::move(name)});
    props_.lanes.clear();
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    progress_.set(1);
}

TimelineChart::~TimelineChart() = default;

std::size_t TimelineChart::addLane(LocalizedText name) {
    lanes_.push_back({.name = std::move(name)});
    invalidatePaint();
    return lanes_.size() - 1;
}

void TimelineChart::addSpan(std::size_t lane, TimelineSpan span) {
    if (std::isnan(span.start) || std::isnan(span.end))
        return;
    if (span.end < span.start)
        std::swap(span.start, span.end);
    while (lanes_.size() <= lane)
        lanes_.push_back({.name = LocalizedText(std::to_string(lanes_.size() + 1))});
    Lane& l = lanes_[lane];
    if (l.rows.size() <= span.depth)
        l.rows.resize(span.depth + 1);
    std::deque<TimelineSpan>& row = l.rows[span.depth];
    l.order.emplace_back(span.depth, span.start);
    // in the order of their starts (at the end, as they come)
    if (row.empty() || span.start >= row.back().start)
        row.push_back(std::move(span));
    else
        row.insert(std::upper_bound(row.begin(), row.end(), span.start, [](double t, const TimelineSpan& s) { return t < s.start; }), std::move(span));
    ++l.count;
    // the oldest dropped past its capacity
    while (props_.capacity > 0 && l.count > props_.capacity && !l.order.empty()) {
        const auto [depth, start] = l.order.front();
        l.order.pop_front();
        std::deque<TimelineSpan>& r = l.rows[depth];
        if (!r.empty() && r.front().start == start) {
            r.pop_front();
        } else {
            const auto it = std::find_if(r.begin(), r.end(), [&](const TimelineSpan& s) { return s.start == start; });
            if (it == r.end())
                continue;
            r.erase(it);
        }
        --l.count;
    }
    invalidatePaint();
}

void TimelineChart::clearLane(std::size_t lane) {
    if (lane >= lanes_.size())
        return;
    lanes_[lane].rows.clear();
    lanes_[lane].order.clear();
    lanes_[lane].count = 0;
    invalidatePaint();
}

std::size_t TimelineChart::spanCount(std::size_t lane) const {
    return lane < lanes_.size() ? lanes_[lane].count : 0;
}

std::size_t TimelineChart::rowCount(std::size_t lane) const {
    return lane < lanes_.size() ? lanes_[lane].rows.size() : 0;
}

void TimelineChart::setXFormatter(std::function<std::string(double, double)> format) {
    xFormat_ = std::move(format);
    axis_.format = xFormat_;
    invalidatePaint();
}

void TimelineChart::setLengthFormatter(std::function<std::string(double)> format) {
    lengthFormat_ = std::move(format);
    invalidatePaint();
}

Color TimelineChart::colorOf(const TimelineSpan& span) const {
    if (span.category)
        return seriesColor(*span.category);
    return seriesColor(std::hash<std::string>{}(span.name) % 10);
}

std::string TimelineChart::length(double seconds) const {
    if (lengthFormat_)
        return lengthFormat_(seconds);
    if (props_.xAxis != ChartAxisKind::Time)
        return formatNumber(seconds, axis_.decimals() + 1);
    const double a = std::abs(seconds);
    if (a >= 1)
        return formatNumber(seconds, a >= 100 ? 0 : 2) + " s";
    if (a >= 1e-3)
        return formatNumber(seconds * 1e3, a >= 0.1 ? 0 : 2) + " ms";
    return formatNumber(seconds * 1e6, 1) + " µs";
}

// ---- the view ----

ChartRange TimelineChart::fitX() const {
    double low = std::numeric_limits<double>::infinity(), high = -low;
    for (const Lane& l : lanes_) {
        for (const auto& row : l.rows) {
            if (row.empty())
                continue;
            low = std::min(low, row.front().start);
            high = std::max(high, row.back().end);
        }
    }
    if (!(high >= low))
        return {0, 1};
    switch (xMode_) {
    case XMode::Manual:
        return userX_;
    case XMode::Window:
        return {high - window_, high};
    case XMode::Fit:
        break;
    }
    if (!(high > low))
        return {low - 0.5, high + 0.5};
    const double pad = (high - low) * 0.01;
    return {low - pad, high + pad};
}

void TimelineChart::updateView() {
    const ChartRange target = fitX();
    const Duration duration = transition(quick_);
    if (!hasView_ || immediate_ || duration <= Duration::zero()) {
        view_ = {target, target};
        hasView_ = true;
        progress_.set(1);
        immediate_ = quick_ = false;
        return;
    }
    if (near(target, view_.to))
        return;
    view_ = {timeView(), target};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange TimelineChart::timeView() const {
    return hasView_ ? between(view_.from, view_.to, progress_.value()) : fitX();
}

void TimelineChart::setTimeView(ChartRange view) {
    xMode_ = XMode::Manual;
    userX_ = view;
    invalidatePaint();
}

void TimelineChart::resetView() {
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    invalidatePaint();
}

bool TimelineChart::following() const {
    return xMode_ == XMode::Window;
}

// ---- scrolling the lanes ----

float TimelineChart::laneHeight(const Lane& lane) const {
    return static_cast<float>(std::max<std::size_t>(1, lane.rows.size())) * props_.rowHeight + laneGap;
}

float TimelineChart::contentHeight() const {
    float h = 0;
    for (const Lane& l : lanes_)
        h += laneHeight(l);
    return h;
}

double TimelineChart::lastScroll() const {
    return std::max(0.0, static_cast<double>(contentHeight() - lanesArea_.height));
}

void TimelineChart::scrollTo(double offset, bool animated) {
    offset = std::clamp(offset, 0.0, lastScroll());
    const UiContext* ui = context();
    if (animated && ui && scheduler()) {
        scrolling_.scrollTo(scheduler(), scroll_, offset, ui->theme.motion.scroll);
    } else {
        scrolling_.stop();
        scroll_ = offset;
        invalidatePaint();
    }
}

void TimelineChart::placeBar() {
    const UiContext* ui = context();
    if (barTrack_.empty() || !ui) {
        bar_.hide();
        return;
    }
    bar_.place(barTrack_, contentHeight(), lanesArea_.height, scroll_, scrolling_.target(scroll_), rightToLeft(), ui->theme.scrollBar.minimumThumb);
}

// ---- painting ----

void TimelineChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    const float barWidth = context.ui.theme.scrollBar.width;

    // the lanes' names at the start side; the time's labels under them; the scroll bar at the end side
    labelWidth_ = 0;
    for (const Lane& l : lanes_)
        labelWidth_ = std::max(labelWidth_, textSize(translate(l.name)).width);
    labelWidth_ = std::min(labelWidth_ + 12, area.width * 0.3f);
    Rect plot = area;
    plot.height = std::max(0.0f, plot.height - line - 6);
    plot.width = std::max(0.0f, plot.width - labelWidth_ - barWidth - 4);
    plot.x += rtl ? barWidth + 4 : labelWidth_;
    plot_ = plot;
    lanesArea_ = plot;
    barTrack_ = {rtl ? area.x : plot.right() + 4, plot.y, barWidth, plot.height};
    scroll_ = std::clamp(scroll_, 0.0, lastScroll());
    placeBar();
    updateView();
    const ChartRange xv = timeView();
    if (plot.width < 8 || plot.height < 8 || !(xv.span() > 0))
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    const auto px = [&](double t) { return static_cast<float>(plot.x + (t - xv.low) / xv.span() * plot.width); };

    // the time's grid and labels
    axis_.orientation = Orientation::Horizontal;
    axis_.time = props_.xAxis == ChartAxisKind::Time;
    axis_.fit(*this, xv, plot.width);
    axis_.paintGrid(p, plot, s.grid);
    axis_.paintLabels(context, *this, plot, Axis::Side::Bottom, s.text, area);
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);

    // the lanes that show: their names, a line between them, their rows' spans (those under a pixel run together)
    const float hair = 1.0f / ppd;
    float y = plot.y - static_cast<float>(scroll_);
    for (const Lane& l : lanes_) {
        const float h = laneHeight(l);
        if (y + h < plot.y) {
            y += h;
            continue;
        }
        if (y > plot.bottom())
            break;
        p.pushClip({area.x, plot.y, area.width, plot.height});
        drawText(context, translate(l.name), {rtl ? plot.right() + barWidth + 4 + labelWidth_ - 6 : area.x, y + props_.rowHeight / 2}, s.text, 0,
                 {rtl ? 1.0f : 0.0f, 0.5f});
        p.fillRect({plot.x, snap(y + h - laneGap / 2, ppd), plot.width, hair}, s.grid);
        p.popClip();
        p.pushClip(plot);
        for (std::size_t r = 0; r < l.rows.size(); ++r) {
            const std::deque<TimelineSpan>& row = l.rows[r];
            const float top = y + static_cast<float>(r) * props_.rowHeight + 1;
            const float height = props_.rowHeight - 2;
            if (top > plot.bottom() || top + height < plot.y)
                continue;
            // a run of spans narrower than a pixel, drawn as one when the next is not with it
            std::optional<Rect> run;
            Color runColor;
            const auto flush = [&] {
                if (run)
                    p.fillRect(*run, runColor);
                run.reset();
            };
            for (std::size_t i = firstEndingAfter(row, xv.low); i < row.size() && row[i].start < xv.high; ++i) {
                const TimelineSpan& span = row[i];
                const float x0 = px(span.start), x1 = std::max(px(span.end), x0 + hair);
                const Color color = colorOf(span);
                if (x1 - x0 < 2 * hair) {
                    if (run && x0 <= run->right() + hair) {
                        run->width = std::max(run->width, x1 - run->x);
                        continue;
                    }
                    flush();
                    run = Rect{x0, top, x1 - x0, height};
                    runColor = withAlpha(color, color.a * 0.85f);
                    continue;
                }
                flush();
                const float gap = x1 - x0 >= 4 ? hair : 0.0f;
                const Rect rect{x0, top, x1 - x0 - gap, height};
                if (rect.width >= 4)
                    p.fillRoundedRect(rect, std::min(s.barRadius, rect.height / 4), color);
                else
                    p.fillRect(rect, color);
                // its name in it, as much of it as fits
                if (rect.width >= 24 && !span.name.empty()) {
                    const Rect visible = rect.intersected(plot);
                    p.pushClip({visible.x + 3, visible.y, std::max(0.0f, visible.width - 6), visible.height});
                    drawText(context, span.name, {visible.x + 4, rect.y + rect.height / 2}, textOn(color), 0, {0, 0.5f});
                    p.popClip();
                }
            }
            flush();
        }
        p.popClip();
        y += h;
    }
    bar_.paint(context, context.ui.theme.scrollBar);

    // the span under the mouse: outlined, its name, start, length and lane by it
    if (mouse_ && !dragging_) {
        if (const auto hit = spanAt(*mouse_)) {
            const Lane& l = lanes_[hit->lane];
            const TimelineSpan& span = l.rows[hit->row][hit->index];
            float laneTop = plot.y - static_cast<float>(scroll_);
            for (std::size_t k = 0; k < hit->lane; ++k)
                laneTop += laneHeight(lanes_[k]);
            const Rect rect{px(span.start), laneTop + static_cast<float>(hit->row) * props_.rowHeight + 1, std::max(px(span.end) - px(span.start), hair),
                            props_.rowHeight - 2};
            p.pushClip(plot);
            p.strokeRoundedRect(rect.inflated(1), std::min(s.barRadius, rect.height / 4) + 1, 1.5f, s.title);
            p.popClip();
            const std::vector<ToolTipRow> rows = {
                {std::nullopt, translate(LocalizedText("Start")), axis_.precise(span.start)},
                {std::nullopt, translate(LocalizedText("Length")), length(span.end - span.start)},
                {std::nullopt, translate(LocalizedText("Lane")), translate(l.name)},
            };
            paintToolTip(context, *mouse_, span.name, rows);
        }
    }
}

std::optional<TimelineChart::Hit> TimelineChart::spanAt(Point at) const {
    if (!plot_.contains(at) || bar_.hovered())
        return std::nullopt;
    const ChartRange xv = timeView();
    const double t = xv.low + (at.x - plot_.x) / plot_.width * xv.span();
    const double tolerance = xv.span() / plot_.width * 2;      // two dips
    float y = plot_.y - static_cast<float>(scroll_);
    for (std::size_t li = 0; li < lanes_.size(); ++li) {
        const Lane& l = lanes_[li];
        const float h = laneHeight(l);
        if (at.y >= y && at.y < y + h - laneGap) {
            const auto r = static_cast<std::size_t>((at.y - y) / props_.rowHeight);
            if (r >= l.rows.size())
                return std::nullopt;
            const std::deque<TimelineSpan>& row = l.rows[r];
            const std::size_t i = firstEndingAfter(row, t - tolerance);
            if (i < row.size() && row[i].start <= t + tolerance)
                return Hit{li, r, i};
            return std::nullopt;
        }
        y += h;
    }
    return std::nullopt;
}

std::vector<Chart::LegendItem> TimelineChart::legendItems() const {
    std::vector<LegendItem> items;
    for (std::size_t i = 0; i < props_.categories.size(); ++i)
        items.push_back({translate(props_.categories[i]), seriesColor(i), true});
    return items;
}

// ---- the mouse ----

void TimelineChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    placeBar();
    if (event.type != MouseEventType::Wheel && bar_.mouse(event)) {
        event.handled = true;
        invalidatePaint();
        return;
    }
    const Point at = event.position;
    const auto newest = [&] {
        double high = -std::numeric_limits<double>::infinity();
        for (const Lane& l : lanes_)
            for (const auto& row : l.rows)
                if (!row.empty())
                    high = std::max(high, row.back().end);
        return high;
    };
    switch (event.type) {
    case MouseEventType::Wheel: {
        if (!plot_.contains(at))
            return;
        const Point delta = event.wheelDelta;
        if (platform::hasAny(event.modifiers, platform::KeyModifiers::Control)) {
            // the lanes: as every scrolling widget
            const WheelScroll w = wheelScroll(scrolling_, scheduler(), scroll_, -delta.y * props_.rowHeight * 3, event.wheelPrecise, lastScroll());
            const UiContext* ui = context();
            if (w.moved)
                scrolling_.scrollTo(scheduler(), scroll_, w.target, ui ? Duration(ui->theme.motion.scroll) : Duration::zero(), w.aimed);
            event.handled = w.moved;
            return;
        }
        const ChartRange t = view_.to;
        if (platform::hasAny(event.modifiers, platform::KeyModifiers::Shift) || std::abs(delta.x) > std::abs(delta.y)) {
            const double steps = std::abs(delta.x) > std::abs(delta.y) ? delta.x : delta.y;
            const double by = -steps * t.span() * 0.1;
            userX_ = {t.low + by, t.high + by};
            xMode_ = XMode::Manual;
            if (props_.window > 0 && userX_.high >= newest()) {
                window_ = userX_.span();
                xMode_ = XMode::Window;
            }
        } else {
            const double factor = std::pow(zoomStep, delta.y);
            if (xMode_ == XMode::Window) {
                window_ = std::max(1e-9, window_ * factor);
            } else {
                const double anchor = t.low + (at.x - plot_.x) / plot_.width * t.span();
                userX_ = {anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
                xMode_ = XMode::Manual;
            }
        }
        quick_ = true;
        invalidatePaint();
        event.handled = true;
        return;
    }
    case MouseEventType::Press:
        if (!plot_.contains(at) || event.button != platform::MouseButton::Left)
            return;
        if (event.clicks == 2) {
            // a span: zoomed to it (a little around it); elsewhere: all again
            if (const auto hit = spanAt(at)) {
                const TimelineSpan& span = lanes_[hit->lane].rows[hit->row][hit->index];
                const double pad = std::max((span.end - span.start) * 0.05, 1e-9);
                setTimeView({span.start - pad, span.end + pad});
            } else {
                resetView();
            }
        } else {
            dragging_ = true;
            dragFrom_ = at;
            dragX_ = timeView();
            dragScroll_ = scroll_;
            scrolling_.stop();
        }
        event.handled = true;
        return;
    case MouseEventType::Move:
        mouse_ = at;
        if (dragging_ && plot_.width > 0) {
            const double dx = (at.x - dragFrom_.x) / plot_.width * dragX_.span();
            if (dx != 0) {
                xMode_ = XMode::Manual;
                userX_ = {dragX_.low - dx, dragX_.high - dx};
                immediate_ = true;
            }
            scroll_ = std::clamp(dragScroll_ - (at.y - dragFrom_.y), 0.0, lastScroll());
        }
        invalidatePaint();
        return;
    case MouseEventType::Release:
        if (dragging_) {
            dragging_ = false;
            if (props_.window > 0 && xMode_ == XMode::Manual && userX_.high >= newest() - userX_.span() * 0.02) {
                window_ = userX_.span();
                xMode_ = XMode::Window;
                immediate_ = true;
            }
            invalidatePaint();
            event.handled = true;
        }
        return;
    }
}

void TimelineChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        bar_.leave();
        invalidatePaint();
    }
}

std::optional<platform::Cursor> TimelineChart::cursorAt(Point point) const {
    if (dragging_)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

} // namespace glw
