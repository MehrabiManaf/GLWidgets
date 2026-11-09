// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/CandleChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;       // a step of the wheel in: the span this much of what it was

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

// `candle` taken into `into` (its high, low, close and volume; its open and time stay).
void mergeInto(Candle& into, const Candle& candle) {
    into.high = std::max(into.high, candle.high);
    into.low = std::min(into.low, candle.low);
    into.close = candle.close;
    into.volume += candle.volume;
}

std::tm localTime(double seconds) {
    const auto t = static_cast<std::time_t>(std::floor(seconds));
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

// A local time's seconds since 1970 (its fields out of their ranges carried over: the 32nd of January is the 1st of
// February), its daylight saving time found.
double fromLocal(std::tm t) {
    t.tm_isdst = -1;
    return static_cast<double>(std::mktime(&t));
}

// Days from 1970-01-01 to a date (Howard Hinnant's days_from_civil), so that runs of days and weeks line up anywhere.
long long daysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

long long floorDiv(long long a, long long b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

double unitSeconds(CandlePeriod::Unit unit) {
    switch (unit) {
    case CandlePeriod::Unit::Second:
        return 1;
    case CandlePeriod::Unit::Minute:
        return 60;
    case CandlePeriod::Unit::Hour:
        return 3600;
    case CandlePeriod::Unit::Day:
        return 86400;
    case CandlePeriod::Unit::Week:
        return 604800;
    case CandlePeriod::Unit::Month:
        return 2629746;         // a mean Gregorian month's
    case CandlePeriod::Unit::Year:
        return 31556952;
    }
    return 1;
}

bool clockUnit(CandlePeriod::Unit unit) {
    return unit == CandlePeriod::Unit::Second || unit == CandlePeriod::Unit::Minute || unit == CandlePeriod::Unit::Hour;
}

} // namespace

// ---- a candle's period ----

CandlePeriod::CandlePeriod(double seconds) {
    const auto s = std::max<long long>(1, std::llround(seconds));
    const auto in = [&](long long unit) { return s % unit == 0; };
    if (in(604800)) {
        unit = Unit::Week;
        count = static_cast<int>(s / 604800);
    } else if (in(86400)) {
        unit = Unit::Day;
        count = static_cast<int>(s / 86400);
    } else if (in(3600)) {
        unit = Unit::Hour;
        count = static_cast<int>(s / 3600);
    } else if (in(60)) {
        unit = Unit::Minute;
        count = static_cast<int>(s / 60);
    } else {
        unit = Unit::Second;
        count = static_cast<int>(s);
    }
}

double CandlePeriod::length() const {
    return unitSeconds(unit) * count;
}

double CandlePeriod::start(double time, int weekStart) const {
    std::tm t = localTime(time);
    if (clockUnit(unit)) {
        // seconds, minutes, hours: from local midnight (a run longer than a day: from 1970)
        const auto length = static_cast<long long>(this->length());
        if (length >= 86400)
            return static_cast<double>(floorDiv(static_cast<long long>(std::floor(time)), length) * length);
        const long long ofDay = t.tm_hour * 3600LL + t.tm_min * 60LL + t.tm_sec;
        const long long at = ofDay / length * length;
        t.tm_hour = static_cast<int>(at / 3600);
        t.tm_min = static_cast<int>(at % 3600 / 60);
        t.tm_sec = static_cast<int>(at % 60);
        return fromLocal(t);
    }
    // the calendar's: from a local midnight
    t.tm_hour = t.tm_min = t.tm_sec = 0;
    const long long day = daysFromCivil(t.tm_year + 1900LL, static_cast<unsigned>(t.tm_mon + 1), static_cast<unsigned>(t.tm_mday));
    switch (unit) {
    case Unit::Day:
        t.tm_mday -= static_cast<int>(day - floorDiv(day, count) * count);
        break;
    case Unit::Week: {
        // back to the day weeks begin on (1970-01-01 was a Thursday: day 0 is weekday 4), then to a run of `count`
        const long long back = ((day + 4 - weekStart) % 7 + 7) % 7;
        const long long week = floorDiv(day - back + 3, 7);         // weeks since one that began near 1970
        t.tm_mday -= static_cast<int>(back + (week - floorDiv(week, count) * count) * 7);
        break;
    }
    case Unit::Month: {
        const long long month = (t.tm_year + 1900LL) * 12 + t.tm_mon;
        const long long first = floorDiv(month, count) * count;
        t.tm_year = static_cast<int>(floorDiv(first, 12) - 1900);
        t.tm_mon = static_cast<int>(first - floorDiv(first, 12) * 12);
        t.tm_mday = 1;
        break;
    }
    case Unit::Year:
        t.tm_year = static_cast<int>(floorDiv(t.tm_year + 1900LL, count) * count - 1900);
        t.tm_mon = 0;
        t.tm_mday = 1;
        break;
    default:
        break;
    }
    return fromLocal(t);
}

double CandlePeriod::next(double start) const {
    std::tm t = localTime(start);
    switch (unit) {
    case Unit::Second:
    case Unit::Minute:
    case Unit::Hour:
        if (length() >= 86400)
            return start + length();
        t.tm_sec += static_cast<int>(length());
        break;
    case Unit::Day:
        t.tm_mday += count;
        break;
    case Unit::Week:
        t.tm_mday += 7 * count;
        break;
    case Unit::Month:
        t.tm_mon += count;
        break;
    case Unit::Year:
        t.tm_year += count;
        break;
    }
    return fromLocal(t);
}

bool CandlePeriod::madeOf(const CandlePeriod& base) const {
    if (*this == base)
        return true;
    if (clockUnit(base.unit)) {
        const auto b = static_cast<long long>(base.length());
        if (clockUnit(unit))
            return static_cast<long long>(length()) % b == 0;
        return 86400 % b == 0;          // a day made of whole ones, so the calendar's are
    }
    switch (base.unit) {
    case Unit::Day:
        return (unit == Unit::Day && count % base.count == 0) || (base.count == 1 && (unit == Unit::Week || unit == Unit::Month || unit == Unit::Year));
    case Unit::Week:
        return unit == Unit::Week && count % base.count == 0;
    case Unit::Month:
        return (unit == Unit::Month && count % base.count == 0) || (unit == Unit::Year && 12 % base.count == 0);
    case Unit::Year:
        return unit == Unit::Year && count % base.count == 0;
    default:
        return false;
    }
}

std::string CandlePeriod::name() const {
    static constexpr const char* suffixes[] = {"s", "m", "h", "D", "W", "M", "Y"};
    return std::to_string(count) + suffixes[static_cast<int>(unit)];
}

// ---- a line over it ----

CandleChart::Line::Line(CandleChart& chart, LocalizedText name, std::optional<Color> color, std::size_t capacity, std::size_t count)
    : chart_(chart), name_(std::move(name)), color_(color), data_(capacity), count_(count), shown_([this] { chart_.invalidatePaint(); }) {
    shown_.set(1);
}

void CandleChart::Line::append(double x, double y) {
    data_.append(x, y);
    chart_.invalidatePaint();
}

void CandleChart::Line::clear() {
    data_.clear();
    chart_.invalidatePaint();
}

void CandleChart::Line::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    shown_.animateTo(chart_.scheduler(), visible ? 1.0f : 0.0f, chart_.transition());
    chart_.invalidatePaint();
}

// ---- the chart ----

CandleChart::CandleChart() : CandleChart(Props{}) {}

CandleChart::CandleChart(Props props)
    : props_(std::move(props)), volumeShown_([this] { invalidatePaint(); }), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    period_ = props_.period;
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    volumeShown_.set(props_.volume ? 1.0f : 0.0f);
    progress_.set(1);
}

CandleChart::~CandleChart() = default;

// ---- data ----

void CandleChart::append(const Candle& candle) {
    if (!given_.empty() && candle.time <= given_.back().time) {
        if (candle.time == given_.back().time)
            setLast(candle);
        return;
    }
    given_.push_back(candle);
    while (props_.capacity > 0 && given_.size() > props_.capacity)
        dropFront();
    refreshLast();
}

void CandleChart::setLast(const Candle& candle) {
    if (given_.empty()) {
        append(candle);
        return;
    }
    given_.back() = candle;
    refreshLast();
}

void CandleChart::trade(double time, double price, double volume) {
    const double start = props_.period.start(time, props_.weekStart);
    if (given_.empty() || start > given_.back().time) {
        append({start, price, price, price, price, volume});
        return;
    }
    if (start < given_.back().time)
        return;         // (of a period gone)
    mergeInto(given_.back(), {start, price, price, price, price, volume});
    refreshLast();
}

void CandleChart::clear() {
    given_.clear();
    candles_.clear();
    first_ = 0;
    invalidatePaint();
}

void CandleChart::refreshLast() {
    if (given_.empty())
        return;
    // of the period given: the candle as it is given
    if (period_ == props_.period) {
        if (!candles_.empty() && candles_.back().time == given_.back().time)
            candles_.back() = given_.back();
        else if (candles_.empty() || given_.back().time > candles_.back().time)
            candles_.push_back(given_.back());
        invalidatePaint();
        return;
    }
    // else the last of the period that shows, made again of the given candles in it
    const double start = period_.start(given_.back().time, props_.weekStart);
    std::size_t i = given_.size();
    while (i > 0 && given_[i - 1].time >= start)
        --i;
    Candle made = given_[i];
    made.time = start;
    for (std::size_t k = i + 1; k < given_.size(); ++k)
        mergeInto(made, given_[k]);
    if (!candles_.empty() && candles_.back().time == start)
        candles_.back() = made;
    else if (candles_.empty() || start > candles_.back().time)
        candles_.push_back(made);
    invalidatePaint();
}

void CandleChart::dropFront() {
    given_.pop_front();
    if (candles_.empty())
        return;
    if (period_ == props_.period) {
        candles_.pop_front();
        ++first_;
        return;
    }
    // the first that shows: gone when none of its given ones are left, else made again of those that are
    const double start = candles_.front().time, end = period_.next(start);
    if (given_.empty() || given_.front().time >= end) {
        candles_.pop_front();
        ++first_;
        return;
    }
    Candle made = given_.front();
    made.time = start;
    for (std::size_t k = 1; k < given_.size() && given_[k].time < end; ++k)
        mergeInto(made, given_[k]);
    candles_.front() = made;
}

void CandleChart::rebuild() {
    candles_.clear();
    first_ = 0;
    if (period_ == props_.period) {
        candles_ = given_;
        return;
    }
    for (const Candle& c : given_) {
        if (!candles_.empty() && c.time < period_.next(candles_.back().time)) {
            mergeInto(candles_.back(), c);
        } else {
            Candle made = c;
            made.time = period_.start(c.time, props_.weekStart);
            candles_.push_back(made);
        }
    }
}

CandleChart::Line& CandleChart::addAverage(std::size_t count, std::optional<Color> color, LocalizedText name) {
    count = std::max<std::size_t>(count, 1);
    if (name.source.empty())
        name = LocalizedText("MA " + std::to_string(count));
    lines_.push_back(std::unique_ptr<Line>(new Line(*this, std::move(name), color, 0, count)));
    invalidatePaint();
    return *lines_.back();
}

CandleChart::Line& CandleChart::addLine(LocalizedText name, std::optional<Color> color, std::size_t capacity) {
    lines_.push_back(std::unique_ptr<Line>(new Line(*this, std::move(name), color, capacity, 0)));
    invalidatePaint();
    return *lines_.back();
}

// ---- its look ----

void CandleChart::setPeriod(CandlePeriod period) {
    if (period == period_ || !period.madeOf(props_.period))
        return;
    period_ = period;
    rebuild();
    // a new period: its candles at other places, shown at once from the newest
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    immediate_ = true;
    invalidatePaint();
}

void CandleChart::setKind(Kind kind) {
    props_.kind = kind;
    invalidatePaint();
}

void CandleChart::setVolumeVisible(bool visible) {
    props_.volume = visible;
    volumeShown_.animateTo(scheduler(), visible ? 1.0f : 0.0f, transition());
    invalidatePaint();
}

void CandleChart::setPriceFormatter(std::function<std::string(double, double)> format) {
    priceFormat_ = std::move(format);
    priceAxis_.format = priceFormat_;
    invalidatePaint();
}

void CandleChart::setVolumeFormatter(std::function<std::string(double)> format) {
    volumeFormat_ = std::move(format);
    invalidatePaint();
}

Color CandleChart::riseColor() const {
    return props_.rise ? *props_.rise : style().rise;
}

Color CandleChart::fallColor() const {
    return props_.fall ? *props_.fall : style().fall;
}

Color CandleChart::lineColor(std::size_t index) const {
    const Line& line = *lines_[index];
    if (line.color_)
        return *line.color_;
    // the style's series colors, but for those of rising and falling (its third and fourth: green, red)
    constexpr std::size_t order[] = {1, 4, 5, 6, 7, 8, 9, 0};
    return seriesColor(order[index % std::size(order)]);
}

std::string CandleChart::price(double value) const {
    if (priceFormat_)
        return priceFormat_(value, priceAxis_.step());
    return formatNumber(value, priceAxis_.decimals() + 1);
}

std::string CandleChart::volume(double value) const {
    if (volumeFormat_)
        return volumeFormat_(value);
    // large ones short: 12.3k, 4.5M
    const double a = std::abs(value);
    if (a >= 1e9)
        return formatNumber(value / 1e9, 2) + "B";
    if (a >= 1e6)
        return formatNumber(value / 1e6, 2) + "M";
    if (a >= 1e4)
        return formatNumber(value / 1e3, 1) + "k";
    return formatNumber(value, value == std::floor(value) ? 0 : 2);
}

// ---- the view ----

void CandleChart::setWindow(std::size_t candles) {
    props_.window = window_ = candles;
    xMode_ = candles > 0 ? XMode::Window : XMode::Fit;
    invalidatePaint();
}

ChartRange CandleChart::fitIndex() const {
    const auto end = static_cast<double>(first_ + candles_.size());
    switch (xMode_) {
    case XMode::Manual:
        return userX_;
    case XMode::Window:
        // its last candles, and one's room after them
        return {end - static_cast<double>(window_), end + 1};
    case XMode::Fit:
        break;
    }
    return {static_cast<double>(first_), std::max(end, static_cast<double>(first_) + 10) + 1};
}

ChartRange CandleChart::fitPrice(ChartRange index) const {
    double low = std::numeric_limits<double>::infinity(), high = -low;
    const auto from = static_cast<std::size_t>(std::clamp(std::floor(index.low), static_cast<double>(first_), static_cast<double>(first_ + candles_.size())));
    const auto to = static_cast<std::size_t>(std::clamp(std::ceil(index.high), static_cast<double>(first_), static_cast<double>(first_ + candles_.size())));
    for (std::size_t i = from; i < to; ++i) {
        low = std::min(low, candles_[i - first_].low);
        high = std::max(high, candles_[i - first_].high);
    }
    if (!(high >= low))
        return {0, 1};
    if (!(high > low)) {
        const double d = std::max(std::abs(low) * 0.01, 1e-9);
        low -= d;
        high += d;
    }
    // a little room above and below (the last price's tag, a wick's end)
    const double pad = (high - low) * 0.06;
    return {low - pad, high + pad};
}

void CandleChart::updateView() {
    const ChartRange tx = fitIndex();
    const ChartRange ty = fitPrice(tx);
    const Duration duration = transition(quick_);
    if (!hasView_ || immediate_ || duration <= Duration::zero()) {
        view_ = {tx, tx, ty, ty};
        hasView_ = true;
        progress_.set(1);
        immediate_ = quick_ = false;
        return;
    }
    if (near(tx, view_.toX) && near(ty, view_.toY))
        return;
    view_ = {indexView(), tx, priceView(), ty};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange CandleChart::indexView() const {
    return hasView_ ? between(view_.fromX, view_.toX, progress_.value()) : fitIndex();
}

ChartRange CandleChart::priceView() const {
    return hasView_ ? between(view_.fromY, view_.toY, progress_.value()) : fitPrice(indexView());
}

void CandleChart::setIndexView(ChartRange view) {
    xMode_ = XMode::Manual;
    userX_ = view;
    invalidatePaint();
}

void CandleChart::resetView() {
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    invalidatePaint();
}

bool CandleChart::following() const {
    return xMode_ == XMode::Window;
}

// ---- painting ----

void CandleChart::gather(ChartRange view, double columns) {
    shown_.clear();
    if (candles_.empty())
        return;
    const auto begin = static_cast<double>(first_), end = static_cast<double>(first_ + candles_.size());
    const auto from = static_cast<std::size_t>(std::clamp(std::floor(view.low), begin, end));
    const auto to = static_cast<std::size_t>(std::clamp(std::ceil(view.high), begin, end));
    if (to <= from)
        return;
    // two pixels a candle at least: each one; else a column of pixels a candle, of those in it
    if (static_cast<double>(to - from) * 2 <= columns) {
        for (std::size_t i = from; i < to; ++i)
            shown_.push_back({static_cast<double>(i) + 0.5, 1, candles_[i - first_]});
        return;
    }
    const double dx = view.span() / columns;
    std::size_t i = from;
    for (double c = 0; c < columns && i < to; ++c) {
        const double edge = view.low + (c + 1) * dx;
        const auto j = static_cast<std::size_t>(std::clamp(std::ceil(edge), static_cast<double>(i + 1), static_cast<double>(to)));
        Candle merged = candles_[i - first_];
        for (std::size_t k = i + 1; k < j; ++k)
            mergeInto(merged, candles_[k - first_]);
        shown_.push_back({(static_cast<double>(i) + static_cast<double>(j)) / 2, static_cast<double>(j - i), merged});
        i = j;
    }
}

void CandleChart::paintPeriods(PaintContext& context) {
    buttons_.clear();
    if (props_.periods.size() < 2)
        return;
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const Theme& theme = context.ui.theme;
    const bool rtl = rightToLeft();
    // a row of them at the end of its title's row, the one that shows raised as a button
    const Rect content = contentRect();
    const float height = textSize("0").height + 6;
    float width = 4;
    for (const CandlePeriod& period : props_.periods)
        width += textSize(period.name()).width + 14;
    float x = rtl ? content.x : content.right() - width;
    const Rect row{x, content.y, width, height};
    const float radius = std::min(theme.button.cornerRadius, height / 2);
    p.fillRoundedRect(row, radius, withAlpha(s.text, s.text.a * 0.07f));
    x += 2;
    for (const CandlePeriod& period : props_.periods) {
        const std::string name = period.name();
        const float w = textSize(name).width + 14;
        const Rect r{x, row.y + 2, w, height - 4};
        const bool chosen = period == period_;
        const bool hovered = mouse_ && r.contains(*mouse_);
        if (chosen)
            paintRaised(p, r, std::min(radius, r.height / 2), theme.button.background.normal, {0, 0, 0, 0}, theme.button.bevel);
        else if (hovered)
            p.fillRoundedRect(r, std::min(radius, r.height / 2), theme.palette.hover);
        drawText(context, name, {r.x + r.width / 2, r.y + r.height / 2}, chosen ? s.title : s.text, 0, {0.5f, 0.5f});
        buttons_.push_back({r, period});
        x += w;
    }
}

void CandleChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paintPeriods(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const bool time = props_.xAxis == ChartAxisKind::Time;
    const float line = textSize("0").height;

    // the plot: the time's labels under it, the price axis at the end side as wide as its labels
    Rect plot = area;
    plot.y += line / 2;
    plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
    updateView();
    const ChartRange xv = indexView(), yv = priceView();
    priceAxis_.orientation = Orientation::Vertical;
    priceAxis_.fit(*this, yv, plot.height * 0.8f);
    const float axisWidth = std::max(priceAxis_.labelWidth(), textSize(price(yv.high)).width) + 14;
    plot.width = std::max(0.0f, plot.width - axisWidth - 6);
    if (rtl)
        plot.x += axisWidth;
    // the volumes under the prices (growing out of the bottom as they show)
    Rect prices = plot;
    volumes_ = {};
    if (const float shown = volumeShown_.value(); shown > 0.002f) {
        const float h = std::floor(plot.height * 0.18f * shown);
        volumes_ = {plot.x, plot.bottom() - h, plot.width, h};
        prices.height = std::max(0.0f, plot.height - h - 6 * shown);
    }
    plot_ = prices;
    if (prices.width < 8 || prices.height < 8 || !(xv.span() > 0) || !(yv.span() > 0))
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    priceAxis_.fit(*this, yv, prices.height);

    const auto px = [&](double index) { return static_cast<float>(plot.x + (index - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double v) { return static_cast<float>(prices.bottom() - (v - yv.low) / yv.span() * prices.height); };
    // where a time is among the candles (between them, along their times; past them, a period a candle), and back
    const auto indexOf = [&](double t) {
        if (candles_.empty())
            return 0.0;
        const auto it = std::lower_bound(candles_.begin(), candles_.end(), t, [](const Candle& c, double v) { return c.time < v; });
        const auto k = static_cast<std::size_t>(it - candles_.begin());
        if (k == 0)
            return static_cast<double>(first_) + (t - candles_.front().time) / period_.length();
        if (k == candles_.size())
            return static_cast<double>(first_ + k - 1) + (t - candles_.back().time) / period_.length();
        const double t0 = candles_[k - 1].time, t1 = candles_[k].time;
        return static_cast<double>(first_ + k - 1) + (t1 > t0 ? (t - t0) / (t1 - t0) : 0);
    };
    const auto timeOf = [&](double index) {
        if (candles_.empty())
            return index * period_.length();
        const double local = index - static_cast<double>(first_);
        if (local <= 0)
            return candles_.front().time + local * period_.length();
        const auto k = static_cast<std::size_t>(local);
        if (k + 1 >= candles_.size())
            return candles_.back().time + (local - static_cast<double>(candles_.size() - 1)) * period_.length();
        return candles_[k].time + (local - static_cast<double>(k)) * (candles_[k + 1].time - candles_[k].time);
    };

    // the price's grid and labels; the time's (at round times, where they are among the candles), as many as fit
    priceAxis_.paintGrid(p, prices, s.grid);
    priceAxis_.paintLabels(context, *this, prices, rtl ? Axis::Side::Left : Axis::Side::Right, s.text, area);
    timeAxis_.orientation = Orientation::Horizontal;
    timeAxis_.time = time;
    timeAxis_.fit(*this, {timeOf(xv.low), timeOf(xv.high)}, plot.width);
    float lastRight = -1e9f;
    for (std::size_t i = 0; i < timeAxis_.values().size(); ++i) {
        const float x = px(indexOf(timeAxis_.values()[i]));
        if (x < plot.x || x > plot.right())
            continue;
        const std::string& label = timeAxis_.labels()[i];
        const float w = textSize(label).width;
        if (x - w / 2 < lastRight + 12 || x + w / 2 > area.right() || x - w / 2 < area.x)
            continue;
        lastRight = x + w / 2;
        p.fillRect({snap(x, ppd), plot.y, 1.0f / ppd, plot.height}, s.grid);
        drawText(context, label, {x, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
    }
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);

    const double columns = std::max(1.0, static_cast<double>(plot.width) * ppd);
    gather(xv, columns);
    const Color rise = riseColor(), fall = fallColor();
    const float slot = static_cast<float>(plot.width / xv.span());
    p.pushClip(plot);
    // the volumes: bars along the bottom, faint, as high as the highest that shows
    if (volumes_.height > 1) {
        double most = 0;
        for (const Shown& c : shown_)
            most = std::max(most, c.candle.volume);
        if (most > 0) {
            const float alpha = 0.4f * volumeShown_.value();
            for (const Shown& c : shown_) {
                const float w = std::max(1.0f / ppd, slot * static_cast<float>(c.width) * (c.width > 1 ? 1.0f : 0.7f));
                const float h = static_cast<float>(c.candle.volume / most) * volumes_.height;
                const Color color = c.candle.close >= c.candle.open ? rise : fall;
                p.fillRect({px(c.index) - w / 2, volumes_.bottom() - h, w, h}, withAlpha(color, color.a * alpha));
            }
        }
    }
    // the candles: a wick from the low to the high, a body from the open to the close (raised as the style's bars, when
    // wide enough); or bars: a stroke from the low to the high, a tick to the left at the open, one to the right at the close
    const float hair = 1.0f / ppd;
    for (const Shown& c : shown_) {
        const Candle& k = c.candle;
        const Color color = k.close >= k.open ? rise : fall;
        const float x = px(c.index);
        const float room = slot * static_cast<float>(c.width);
        const float top = py(k.high), bottom = py(k.low);
        if (props_.kind == Kind::Bars) {
            const float stroke = std::clamp(room * 0.12f, hair, 2.0f);
            const float tick = std::max(hair, room * 0.35f);
            p.fillRect({x - stroke / 2, top, stroke, std::max(hair, bottom - top)}, color);
            p.fillRect({x - tick, py(k.open) - stroke / 2, tick, stroke}, color);
            p.fillRect({x, py(k.close) - stroke / 2, tick, stroke}, color);
            continue;
        }
        const float body = c.width > 1 ? std::max(hair, room) : std::max(hair, room * 0.7f);
        const float wick = std::clamp(body * 0.14f, hair, 1.5f);
        p.fillRect({x - wick / 2, top, wick, std::max(hair, bottom - top)}, color);
        const float y0 = py(std::max(k.open, k.close)), y1 = py(std::min(k.open, k.close));
        const Rect r{x - body / 2, y0, body, std::max(hair, y1 - y0)};
        if (r.width >= 5 && r.height >= 3)
            paintRaised(p, r, std::min(s.barRadius, r.width / 4), color, {0, 0, 0, 0}, s.barBevel);
        else
            p.fillRect(r, color);
    }
    // the lines over it: averages of the candles' closes (at their middles), lines of points where their times are
    const auto from = static_cast<std::size_t>(std::clamp(std::floor(xv.low), static_cast<double>(first_), static_cast<double>(first_ + candles_.size())));
    const auto to = static_cast<std::size_t>(std::clamp(std::ceil(xv.high) + 1, static_cast<double>(first_), static_cast<double>(first_ + candles_.size())));
    const std::size_t stride = std::max<std::size_t>(1, static_cast<std::size_t>((to - from) / columns));
    for (std::size_t li = 0; li < lines_.size(); ++li) {
        const Line& l = *lines_[li];
        const float alpha = l.shown_.value();
        if (alpha <= 0.002f)
            continue;
        Color color = lineColor(li);
        color.a *= alpha;
        points_.clear();
        if (l.count_ > 0) {
            // a running sum of the closes from `count` before the first that shows
            const std::size_t n = l.count_;
            const std::size_t start = from - first_ >= n - 1 ? from - first_ - (n - 1) : 0;
            double sum = 0;
            for (std::size_t i = start; i < to - first_; ++i) {
                sum += candles_[i].close;
                if (i >= start + n)
                    sum -= candles_[i - n].close;
                if (i + 1 < n || i + first_ < from || (i + first_ - from) % stride != 0)
                    continue;
                points_.push_back({px(static_cast<double>(i + first_) + 0.5), py(sum / static_cast<double>(n))});
            }
        } else {
            const SeriesBuffer& d = l.data_;
            if (d.empty())
                continue;
            const double t0 = timeOf(xv.low - 1), t1 = timeOf(xv.high + 1);
            std::size_t a = d.sorted() ? d.lowerBound(t0) : 0;
            a = a > 0 ? a - 1 : 0;
            const std::size_t b = d.sorted() ? std::min(d.size(), d.lowerBound(t1) + 1) : d.size();
            const std::size_t step = std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<double>(b - a) / columns));
            for (std::size_t i = a; i < b; i += step)
                if (!std::isnan(d.y(i)))
                    points_.push_back({px(indexOf(d.x(i)) + 0.5), py(d.y(i))});
        }
        if (points_.size() >= 2)
            p.drawPolyline(points_, s.lineWidth, color);
    }
    p.popClip();

    // the last close: a line across, its value on the axis in its candle's color
    const auto tag = [&](double value, Color background, Color text) {
        const float y = py(value);
        if (y < prices.y - 1 || y > prices.bottom() + 1)
            return;
        const std::string label = price(value);
        const Size size = textSize(label);
        const Rect r{rtl ? plot.x - axisWidth + 2 : plot.right() + 2, y - size.height / 2 - 2, axisWidth - 4, size.height + 4};
        p.fillRoundedRect(r, std::min(3.0f, r.height / 2), background);
        drawText(context, label, {r.x + 6, y}, text, 0, {0, 0.5f});
    };
    if (const Candle* lastCandle = last()) {
        const Color color = lastCandle->close >= lastCandle->open ? rise : fall;
        const float y = py(lastCandle->close);
        if (y >= prices.y && y <= prices.bottom())
            p.fillRect({plot.x, snap(y, ppd), plot.width, hair}, withAlpha(color, color.a * 0.75f));
        tag(lastCandle->close, color, textOn(color));
    }

    // the mouse: a crosshair, the price at it on the axis, the candle under it in a tooltip
    if (mouse_ && !dragging_ && plot.contains(*mouse_)) {
        const double at = xv.low + (mouse_->x - plot.x) / plot.width * xv.span();
        const Shown* under = nullptr;
        for (const Shown& c : shown_)
            if (std::abs(c.index - at) <= c.width / 2 + 1e-9)
                under = &c;
        const float cx = under ? px(under->index) : mouse_->x;
        p.fillRect({snap(cx, ppd), plot.y, hair, plot.height}, s.crosshair);
        if (prices.contains(*mouse_)) {
            p.fillRect({plot.x, snap(mouse_->y, ppd), plot.width, hair}, s.crosshair);
            const Palette& palette = context.ui.theme.palette;
            tag(yv.low + (prices.bottom() - mouse_->y) / prices.height * yv.span(), palette.surface, palette.text);
        }
        if (under) {
            const Candle& k = under->candle;
            const double change = k.open != 0 ? (k.close - k.open) / k.open * 100 : 0;
            const Color color = k.close >= k.open ? rise : fall;
            std::vector<ToolTipRow> rows = {
                {std::nullopt, translate(LocalizedText("Open")), price(k.open)},
                {std::nullopt, translate(LocalizedText("High")), price(k.high)},
                {std::nullopt, translate(LocalizedText("Low")), price(k.low)},
                {color, translate(LocalizedText("Close")), price(k.close)},
                {std::nullopt, translate(LocalizedText("Change")), (change >= 0 ? "+" : "") + formatNumber(change, 2) + "%"},
            };
            if (k.volume > 0)
                rows.push_back({std::nullopt, translate(LocalizedText("Volume")), volume(k.volume)});
            const TimeUnit unit = period_.unit == CandlePeriod::Unit::Second ? TimeUnit::Second
                                  : clockUnit(period_.unit)                    ? TimeUnit::Minute
                                                                               : TimeUnit::Day;
            const std::string title = time ? formatTimeFully(k.time, unit, true) : formatNumber(k.time, 0);
            paintToolTip(context, *mouse_, title, rows);
        }
    }
}

std::vector<Chart::LegendItem> CandleChart::legendItems() const {
    // its volume (when it has one), then its lines
    std::vector<LegendItem> items;
    const bool volumes = std::any_of(given_.begin(), given_.end(), [](const Candle& c) { return c.volume > 0; });
    if (volumes) {
        const Color rise = riseColor();
        items.push_back({translate(LocalizedText("Volume")), withAlpha(rise, rise.a * 0.55f), props_.volume});
    }
    for (std::size_t i = 0; i < lines_.size(); ++i)
        items.push_back({translate(lines_[i]->name_), lineColor(i), lines_[i]->visible_});
    return items;
}

void CandleChart::legendItemClicked(std::size_t index) {
    const bool volumes = std::any_of(given_.begin(), given_.end(), [](const Candle& c) { return c.volume > 0; });
    if (volumes) {
        if (index == 0) {
            setVolumeVisible(!props_.volume);
            return;
        }
        --index;
    }
    if (index < lines_.size())
        lines_[index]->setVisible(!lines_[index]->visible_);
}

// ---- the mouse ----

void CandleChart::zoom(double factor, double anchor) {
    const ChartRange t = view_.toX;
    if (xMode_ == XMode::Window) {
        window_ = static_cast<std::size_t>(std::clamp(static_cast<double>(window_) * factor, 5.0, 1e9));
    } else {
        const ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
        if (r.span() < 5)
            return;
        xMode_ = XMode::Manual;
        userX_ = r;
    }
    quick_ = true;
    invalidatePaint();
}

void CandleChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    // a period's button
    if (event.type == MouseEventType::Press && event.button == platform::MouseButton::Left) {
        for (const Button& b : buttons_) {
            if (b.rect.contains(at)) {
                setPeriod(b.period);
                event.handled = true;
                return;
            }
        }
    }
    const Rect plot = plot_.united(volumes_);
    const auto newest = [&] { return static_cast<double>(first_ + candles_.size()); };
    switch (event.type) {
    case MouseEventType::Wheel: {
        if (!plot.contains(at))
            return;
        const Point delta = event.wheelDelta;
        const ChartRange xv = indexView();
        if (platform::hasAny(event.modifiers, platform::KeyModifiers::Shift) || std::abs(delta.x) > std::abs(delta.y)) {
            const double steps = std::abs(delta.x) > std::abs(delta.y) ? delta.x : delta.y;
            const ChartRange t = view_.toX;
            const double by = -steps * t.span() * 0.1;
            userX_ = {t.low + by, t.high + by};
            xMode_ = XMode::Manual;
            if (props_.window > 0 && userX_.high >= newest()) {
                window_ = static_cast<std::size_t>(std::max(5.0, userX_.span() - 1));
                xMode_ = XMode::Window;
            }
            quick_ = true;
            invalidatePaint();
        } else {
            zoom(std::pow(zoomStep, delta.y), xv.low + (at.x - plot.x) / std::max(1.0f, plot.width) * xv.span());
        }
        event.handled = true;
        return;
    }
    case MouseEventType::Press:
        if (!plot.contains(at) || event.button != platform::MouseButton::Left)
            return;
        if (event.clicks == 2) {
            resetView();
        } else {
            dragging_ = true;
            dragFrom_ = at;
            dragX_ = indexView();
        }
        event.handled = true;
        return;
    case MouseEventType::Move:
        mouse_ = at;
        if (dragging_ && plot.width > 0) {
            const double dx = (at.x - dragFrom_.x) / plot.width * dragX_.span();
            xMode_ = XMode::Manual;
            userX_ = {dragX_.low - dx, dragX_.high - dx};
            immediate_ = true;
        }
        invalidatePaint();
        return;
    case MouseEventType::Release:
        if (dragging_) {
            dragging_ = false;
            // a live one dragged back to its newest follows it again
            if (props_.window > 0 && xMode_ == XMode::Manual && userX_.high >= newest() - userX_.span() * 0.02) {
                window_ = static_cast<std::size_t>(std::max(5.0, userX_.span() - 1));
                xMode_ = XMode::Window;
                immediate_ = true;
            }
            invalidatePaint();
            event.handled = true;
        }
        return;
    }
}

void CandleChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

std::optional<platform::Cursor> CandleChart::cursorAt(Point point) const {
    if (dragging_)
        return platform::Cursor::Move;
    for (const Button& b : buttons_)
        if (b.rect.contains(point))
            return platform::Cursor::Pointer;
    return Widget::cursorAt(point);
}

} // namespace glw
