// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/LineChart.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;       // a step of the wheel in: the span this much of what it was
constexpr float dragThreshold = 6;      // a rectangle dragged narrower than this zooms nothing that way

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

// A range of no width made one: around its value.
ChartRange opened(ChartRange r, bool time) {
    if (r.span() > 0)
        return r;
    const double d = time ? 30 : std::max(std::abs(r.low) * 0.1, 1.0);
    return {r.low - d, r.high + d};
}

// The smallest span the view zooms to near `at`: what doubles still tell apart there.
double finest(double at) {
    return std::max(1e-9, std::abs(at) * 1e-12);
}

} // namespace

// ---- a series ----

LineSeries::LineSeries(LineChart& chart, Props props)
    : chart_(chart), props_(std::move(props)), data_(props_.capacity), shown_([this] { chart_.invalidatePaint(); }) {
    shown_.set(1);
}

LineSeries::~LineSeries() = default;

void LineSeries::append(double x, double y) {
    data_.append(x, y);
    changed();
}

void LineSeries::append(std::span<const ChartPoint> points) {
    for (const ChartPoint& p : points)
        data_.append(p.x, p.y);
    changed();
}

void LineSeries::clear() {
    data_.clear();
    changed();
}

void LineSeries::changed() {
    chart_.dataChanged();
}

void LineSeries::setName(LocalizedText name) {
    props_.name = std::move(name);
    chart_.invalidatePaint();
}

void LineSeries::setColor(std::optional<Color> color) {
    props_.color = color;
    chart_.invalidatePaint();
}

Color LineSeries::color() const {
    if (props_.color)
        return *props_.color;
    std::size_t index = 0;
    for (; index < chart_.series_.size() && chart_.series_[index].get() != this; ++index) {
    }
    return chart_.seriesColor(index);
}

void LineSeries::setArea(bool area) {
    props_.area = area;
    chart_.invalidatePaint();
}

void LineSeries::setSteps(bool steps) {
    props_.steps = steps;
    chart_.invalidatePaint();
}

void LineSeries::setWidth(float width) {
    props_.width = width;
    chart_.invalidatePaint();
}

void LineSeries::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    shown_.animateTo(chart_.scheduler(), visible ? 1.0f : 0.0f, chart_.transition());
    chart_.dataChanged();
}

// ---- an area ----

AreaSeries::AreaSeries(LineChart& chart, Props props) : chart_(chart), props_(std::move(props)), shown_([this] { chart_.invalidatePaint(); }) {
    shown_.set(1);
}

AreaSeries::~AreaSeries() = default;

void AreaSeries::setColor(std::optional<Color> color) {
    props_.color = color;
    chart_.invalidatePaint();
}

void AreaSeries::setOpacity(float opacity) {
    props_.opacity = opacity;
    chart_.invalidatePaint();
}

void AreaSeries::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    shown_.animateTo(chart_.scheduler(), visible ? 1.0f : 0.0f, chart_.transition());
    chart_.invalidatePaint();
}

// ---- the chart ----

LineChart::LineChart() : LineChart(Props{}) {}

LineChart::LineChart(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    userY_ = props_.yRange;
    progress_.set(1);
}

LineChart::~LineChart() = default;

LineSeries& LineChart::addSeries(LineSeries::Props props) {
    series_.push_back(std::unique_ptr<LineSeries>(new LineSeries(*this, std::move(props))));
    invalidatePaint();
    return *series_.back();
}

void LineChart::removeSeries(LineSeries& series) {
    std::erase_if(areas_, [&](const std::unique_ptr<AreaSeries>& a) { return a->props_.upper == &series || a->props_.lower == &series; });
    std::erase_if(series_, [&](const std::unique_ptr<LineSeries>& s) { return s.get() == &series; });
    invalidatePaint();
}

AreaSeries& LineChart::addArea(AreaSeries::Props props) {
    areas_.push_back(std::unique_ptr<AreaSeries>(new AreaSeries(*this, std::move(props))));
    invalidatePaint();
    return *areas_.back();
}

void LineChart::removeArea(AreaSeries& area) {
    std::erase_if(areas_, [&](const std::unique_ptr<AreaSeries>& a) { return a.get() == &area; });
    invalidatePaint();
}

Color LineChart::colorOf(const LineSeries& series) const {
    if (series.props_.color)
        return *series.props_.color;
    for (std::size_t i = 0; i < series_.size(); ++i)
        if (series_[i].get() == &series)
            return seriesColor(i);
    return seriesColor(0);
}

void LineChart::setXAxisKind(ChartAxisKind kind) {
    props_.xAxis = kind;
    invalidatePaint();
}

void LineChart::setWindow(double window) {
    props_.window = window_ = window;
    xMode_ = window > 0 ? XMode::Window : XMode::Fit;
    invalidatePaint();
}

void LineChart::setYRange(std::optional<ChartRange> range) {
    props_.yRange = userY_ = range;
    invalidatePaint();
}

void LineChart::setLogY(bool log) {
    if (props_.logY == log)
        return;
    props_.logY = log;
    hasView_ = false;           // (its view in the other scale's terms)
    invalidatePaint();
}

bool LineChart::logY() const {
    return props_.logY && std::none_of(series_.begin(), series_.end(), [](const auto& s) { return s->props_.stacked; });
}

double LineChart::toView(double y) const {
    if (!logY())
        return y;
    return y > 0 ? std::log10(y) : -1e3;        // at or below 0: far under the bottom
}

double LineChart::fromView(double v) const {
    return logY() ? std::pow(10.0, v) : v;
}

ChartRange LineChart::toView(ChartRange y) const {
    return {toView(y.low), toView(y.high)};
}

ChartRange LineChart::fromView(ChartRange v) const {
    return {fromView(v.low), fromView(v.high)};
}

void LineChart::setIncludeZero(bool include) {
    props_.includeZero = include;
    invalidatePaint();
}

void LineChart::setPercent(bool percent) {
    props_.percent = percent;
    invalidatePaint();
}

void LineChart::setXFormatter(std::function<std::string(double, double)> format) {
    xAxis_.format = std::move(format);
    invalidatePaint();
}

void LineChart::setYFormatter(std::function<std::string(double, double)> format) {
    yAxis_.format = std::move(format);
    invalidatePaint();
}

void LineChart::setY2Formatter(std::function<std::string(double, double)> format) {
    y2Axis_.format = std::move(format);
    invalidatePaint();
}

void LineChart::setY2Range(std::optional<ChartRange> range) {
    props_.y2Range = range;
    invalidatePaint();
}

bool LineChart::hasSecond() const {
    return std::any_of(series_.begin(), series_.end(), [this](const auto& s) { return onSecond(*s); });
}

void LineChart::dataChanged() {
    invalidatePaint();
}

// ---- the view ----

void LineChart::setXView(ChartRange x) {
    xMode_ = XMode::Manual;
    userX_ = x;
    invalidatePaint();
}

void LineChart::resetView() {
    window_ = props_.window;
    xMode_ = window_ > 0 ? XMode::Window : XMode::Fit;
    userY_ = props_.yRange;
    invalidatePaint();
}

bool LineChart::following() const {
    return xMode_ == XMode::Window;
}

double LineChart::newestX() const {
    double newest = std::numeric_limits<double>::quiet_NaN();
    for (const auto& s : series_) {
        const SeriesBuffer& d = s->data_;
        if (d.empty())
            continue;
        const double x = d.sorted() ? d.x(d.size() - 1) : d.xExtent()->high;
        newest = std::isnan(newest) ? x : std::max(newest, x);
    }
    return newest;
}

ChartRange LineChart::fitX() const {
    const bool time = props_.xAxis == ChartAxisKind::Time;
    switch (xMode_) {
    case XMode::Manual:
        return userX_;
    case XMode::Window: {
        double newest = newestX();
        if (std::isnan(newest))
            newest = time ? chartNow() : window_;
        return {newest - window_, newest};
    }
    case XMode::Fit:
        break;
    }
    std::optional<ChartExtent> all;
    for (const auto& s : series_) {
        if (!s->visible_)
            continue;
        if (const auto e = s->data_.xExtent())
            all = all ? ChartExtent{std::min(all->low, e->low), std::max(all->high, e->high)} : *e;
    }
    if (!all)
        return time ? ChartRange{chartNow() - 60, chartNow()} : ChartRange{0, 1};
    return opened({all->low, all->high}, time);
}

ChartRange LineChart::fitY(ChartRange x, bool second) const {
    if (!second && userY_)
        return toView(*userY_);
    if (second && props_.y2Range)
        return *props_.y2Range;
    const bool log = !second && logY();
    std::optional<ChartExtent> all;
    // the stack: from 0 to its highest sum (its shares: 0 to 100)
    if (!second && !stackMembers().empty()) {
        if (props_.percent)
            return {0, 100};
        sampleStack(x, std::max(64.0, static_cast<double>(plot_.width) * 2));
        const std::size_t n = stackX_.size(), last = stacked_.size() - 1;
        double high = 0, low = 0;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t k = 0; k <= last; ++k) {
                high = std::max(high, stackTops_[k * n + i]);
                low = std::min(low, stackTops_[k * n + i]);
            }
        }
        all = ChartExtent{low, high};
    }
    for (const auto& s : series_) {
        if (!s->visible_ || s->props_.stacked || onSecond(*s) != second)
            continue;
        const SeriesBuffer& d = s->data_;
        std::optional<ChartExtent> e;
        if (d.sorted()) {
            // what shows, and a point on each side (its line goes to them)
            std::size_t first = d.lowerBound(x.low);
            first = first > 0 ? first - 1 : 0;
            e = d.yExtent(first, std::min(d.size(), d.lowerBound(x.high) + 1));
        } else {
            e = d.yExtent();
        }
        if (e)
            all = all ? ChartExtent{std::min(all->low, e->low), std::max(all->high, e->high)} : *e;
    }
    if (!all)
        return {0, 1};
    if (log) {
        // logarithmic: from its lowest above 0 (none: three decades under its highest) to its highest, a little room
        // around in decades, out to 1, 2 or 5 times a power of ten
        const double high = all->high > 0 ? all->high : 1;
        const double low = all->low > 0 ? all->low : high / 1000;
        ChartRange r{std::log10(low), std::log10(high)};
        if (!(r.span() > 0))
            r = {r.low - 0.5, r.high + 0.5};
        const double pad = r.span() * 0.04;
        const ChartRange nice = niceLogRange({std::pow(10.0, r.low - pad), std::pow(10.0, r.high + pad)});
        return {std::log10(nice.low), std::log10(nice.high)};
    }
    ChartRange r{all->low, all->high};
    if (props_.includeZero || (!second && !stacked_.empty())) {
        r.low = std::min(r.low, 0.0);
        r.high = std::max(r.high, 0.0);
    }
    r = opened(r, false);
    // a little room above and below its data (not below 0 when it starts there), then out to round values
    const double pad = r.span() * 0.04;
    const bool zero = props_.includeZero || (!second && !stacked_.empty());
    if (!(zero && r.low == 0))
        r.low -= pad;
    if (!(zero && r.high == 0))
        r.high += pad;
    return niceRange(r, yTickCount_);
}

void LineChart::updateView() {
    const ChartRange tx = fitX(), ty = fitY(tx), ty2 = fitY(tx, true);
    const Duration duration = transition(quick_);
    if (!hasView_ || immediate_ || duration <= Duration::zero()) {
        view_ = {tx, tx, ty, ty, ty2, ty2};
        hasView_ = true;
        progress_.set(1);
        immediate_ = quick_ = false;
        return;
    }
    if (near(tx, view_.toX) && near(ty, view_.toY) && near(ty2, view_.toY2))
        return;
    // from where it shows now, on the way
    view_ = {xView(), tx, shownY(), ty, shownY2(), ty2};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange LineChart::xView() const {
    return hasView_ ? between(view_.fromX, view_.toX, progress_.value()) : fitX();
}

ChartRange LineChart::shownY() const {
    return hasView_ ? between(view_.fromY, view_.toY, progress_.value()) : fitY(xView());
}

ChartRange LineChart::targetXView() const {
    return hasView_ ? view_.toX : fitX();
}

ChartRange LineChart::shownY2() const {
    return hasView_ ? between(view_.fromY2, view_.toY2, progress_.value()) : fitY(xView(), true);
}

ChartRange LineChart::y2View() const {
    return shownY2();
}

Point LineChart::pointOn(ChartPoint value, bool second) const {
    if (!second)
        return pointOf(value);
    const ChartRange x = xView(), y = shownY2();
    return {static_cast<float>(plot_.x + (value.x - x.low) / x.span() * plot_.width),
            static_cast<float>(plot_.bottom() - (value.y - y.low) / y.span() * plot_.height)};
}

ChartRange LineChart::targetShownY() const {
    return hasView_ ? view_.toY : fitY(targetXView());
}

ChartRange LineChart::yView() const {
    return fromView(shownY());
}

ChartRange LineChart::targetYView() const {
    return fromView(targetShownY());
}

ChartPoint LineChart::valueAt(Point at) const {
    const ChartRange x = xView(), y = shownY();
    const double fx = plot_.width > 0 ? (at.x - plot_.x) / plot_.width : 0;
    const double fy = plot_.height > 0 ? (plot_.bottom() - at.y) / plot_.height : 0;
    return {x.low + fx * x.span(), fromView(y.low + fy * y.span())};
}

Point LineChart::pointOf(ChartPoint value) const {
    const ChartRange x = xView(), y = shownY();
    return {static_cast<float>(plot_.x + (value.x - x.low) / x.span() * plot_.width),
            static_cast<float>(plot_.bottom() - (toView(value.y) - y.low) / y.span() * plot_.height)};
}

// ---- painting ----

void LineChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const bool time = props_.xAxis == ChartAxisKind::Time;
    const float line = textSize("0").height;

    // the plot: under it the x axis's labels, above it room for the top label's half
    Rect plot = area;
    plot.y += line / 2;
    plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
    yTickCount_ = std::max(2, static_cast<int>(plot.height / 34));
    updateView();
    const ChartRange xv = xView(), yv = shownY();

    // the y axis as wide as its labels (a stack in shares: in percent)
    yAxis_.orientation = Orientation::Vertical;
    yAxis_.suffix = props_.percent && !stackMembers().empty() ? "%" : "";
    yAxis_.log = logY();
    yAxis_.fit(*this, fromView(yv), plot.height);
    const float axisWidth = yAxis_.labelWidth() + 8;
    const bool second = hasSecond();
    float secondWidth = 6;
    if (second) {
        y2Axis_.orientation = Orientation::Vertical;
        y2Axis_.fit(*this, shownY2(), plot.height);
        secondWidth = y2Axis_.labelWidth() + 8;
    }
    plot.width = std::max(0.0f, plot.width - axisWidth - secondWidth);
    if (!rtl)
        plot.x += axisWidth;
    else
        plot.x += secondWidth;
    plot_ = plot;
    if (plot.width < 4 || plot.height < 4 || !(xv.span() > 0) || !(yv.span() > 0))
        return;
    // the x axis's ticks: as many as their labels fit
    xAxis_.orientation = Orientation::Horizontal;
    xAxis_.time = time;
    xAxis_.fit(*this, xv, plot.width);

    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    // the grid, sharp on the pixels; the axis along the bottom; the labels beside them
    yAxis_.paintGrid(p, plot, s.grid);
    xAxis_.paintGrid(p, plot, s.grid);
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);
    // with two axes, each one's labels of its series's color when it has one series
    const auto colorOfAxis = [&](bool onTheSecond) {
        const LineSeries* only = nullptr;
        for (const auto& series : series_) {
            if (onSecond(*series) != onTheSecond || series->props_.stacked)
                continue;
            if (only)
                return s.text;
            only = series.get();
        }
        return second && only ? colorOf(*only) : s.text;
    };
    yAxis_.paintLabels(context, *this, plot, rtl ? Axis::Side::Right : Axis::Side::Left, colorOfAxis(false), area);
    if (second)
        y2Axis_.paintLabels(context, *this, plot, rtl ? Axis::Side::Left : Axis::Side::Right, colorOfAxis(true), area);
    xAxis_.paintLabels(context, *this, plot, Axis::Side::Bottom, s.text, area);

    // the series, cut to the plot (a little above and below it: a line along its edge)
    paintAnnotations(context, plot, xAxis_, yAxis_, false, second ? &y2Axis_ : nullptr);
    p.pushClip({plot.x, plot.y - 2, plot.width, plot.height + 4});
    paintStack(context);
    for (const auto& a : areas_)
        paintArea(context, *a);
    for (std::size_t i = 0; i < series_.size(); ++i)
        paintSeries(context, *series_[i], i);
    p.popClip();
    paintAnnotations(context, plot, xAxis_, yAxis_, true, second ? &y2Axis_ : nullptr);

    if (drag_ == Drag::Box) {
        const Rect box = Rect{std::min(dragFrom_.x, dragTo_.x), std::min(dragFrom_.y, dragTo_.y), std::abs(dragTo_.x - dragFrom_.x),
                              std::abs(dragTo_.y - dragFrom_.y)}
                             .intersected(plot);
        p.fillRect(box, s.selection);
        p.strokeRoundedRect(box, 0, 1.0f / ppd, s.crosshair);
    } else if (mouse_ && drag_ == Drag::None && plot.contains(*mouse_)) {
        paintHover(context);
    } else if (const std::optional<double> shared = sharedX(); shared && drag_ == Drag::None) {
        paintHover(context, shared);
    }
}

void LineChart::paintSeries(PaintContext& context, const LineSeries& series, std::size_t index) {
    const float alpha = series.shown_.value();
    const SeriesBuffer& d = series.data_;
    const std::size_t n = d.size();
    if (alpha <= 0.002f || n == 0 || series.props_.stacked)       // (a stacked one: the stack's)
        return;
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    Color color = series.props_.color ? *series.props_.color : seriesColor(index);
    color.a *= alpha;
    const float width = series.props_.width > 0 ? series.props_.width : s.lineWidth;
    const bool second = onSecond(series);
    const ChartRange xv = xView(), yv = second ? shownY2() : shownY();
    const Rect plot = plot_;
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) {
        return static_cast<float>(plot.bottom() - ((second ? y : toView(y)) - yv.low) / yv.span() * plot.height);
    };

    // what shows, and a point on each side
    std::size_t first = 0, last = n;
    if (d.sorted()) {
        first = d.lowerBound(xv.low);
        first = first > 0 ? first - 1 : 0;
        last = std::min(n, d.lowerBound(xv.high) + 1);
    }
    if (last <= first)
        return;
    // the area down to 0, or to the axis the data is all above (below); along a logarithmic one, to its bottom
    const double base = yv.contains(0) ? 0 : yv.low > 0 ? yv.low : yv.high;
    const float baseline = logY() && !second ? plot.bottom() : py(base);
    const bool area = series.props_.area;
    const Color areaTop = withAlpha(color, color.a * s.areaOpacity), areaBottom = withAlpha(color, 0);
    points_.clear();
    const auto flush = [&] {
        if (points_.size() >= 2) {
            if (area)
                p.fillArea(points_, baseline, areaTop, areaBottom);
            if (series.props_.line)
                p.drawPolyline(points_, width, color);
        } else if (points_.size() == 1 && series.props_.line) {
            p.fillCircle(points_[0], width, color);
        }
        points_.clear();
    };

    const double columns = std::max(1.0, static_cast<double>(plot.width) * p.pixelsPerDip());
    if (d.sorted() && static_cast<double>(last - first) > 2 * columns) {
        // more points than columns of pixels: each column what it shows, a stroke from its lowest to its highest and a
        // line on through its first and last points; always forward, so that it never turns back on itself (a turn back
        // pinches a line to nothing)
        const double dx = xv.span() / columns;
        const float pixel = 1.0f / p.pixelsPerDip();
        spans_.clear();
        std::size_t i = first;
        for (double c = 0; c < columns && i < last; ++c) {
            const std::size_t j = std::min(last, d.lowerBound(xv.low + (c + 1) * dx, i));
            if (j <= i)
                continue;
            if (const auto e = d.yExtent(i, j)) {
                const float cx = px(xv.low + (c + 0.5) * dx);
                const double middle = (e->low + e->high) / 2;
                const double a = d.y(i), b = d.y(j - 1);
                const float top = py(e->high), bottom = py(e->low);
                if (bottom - top > pixel / 2)
                    spans_.push_back({cx - width / 2, top, width, bottom - top});
                points_.push_back({cx - pixel / 4, py(std::isnan(a) ? middle : a)});
                points_.push_back({cx + pixel / 4, py(std::isnan(b) ? middle : b)});
            }
            i = j;
        }
        for (; i < last; ++i)       // past the view
            if (!std::isnan(d.y(i)))
                points_.push_back({px(d.x(i)), py(d.y(i))});
        if (area && points_.size() >= 2)
            p.fillArea(points_, baseline, areaTop, areaBottom);
        if (series.props_.line) {
            for (const Rect& span : spans_)
                p.fillRect(span, color);
            if (points_.size() >= 2)
                p.drawPolyline(points_, width, color);
        }
        points_.clear();
        return;
    }
    for (std::size_t i = first; i < last; ++i) {
        const double y = d.y(i);
        if (std::isnan(y)) {
            flush();
            continue;
        }
        const Point at{px(d.x(i)), py(y)};
        if (series.props_.steps && !points_.empty())
            points_.push_back({at.x, points_.back().y});
        points_.push_back(at);
    }
    flush();
    // dots, when they are far enough apart
    if (series.props_.markers && plot.width / static_cast<float>(last - first) >= 8) {
        const float r = width + 1.5f;
        for (std::size_t i = first; i < last; ++i)
            if (!std::isnan(d.y(i)))
                p.fillCircle({px(d.x(i)), py(d.y(i))}, r, color);
    }
}

// ---- the stack ----

const std::vector<const LineSeries*>& LineChart::stackMembers() const {
    stacked_.clear();
    for (const auto& s : series_)
        if (s->props_.stacked && s->shown_.value() > 0.002f)
            stacked_.push_back(s.get());
    return stacked_;
}

double LineChart::stackValue(const LineSeries& series, double x) const {
    const SeriesBuffer& d = series.data_;
    const std::size_t n = d.size();
    if (n == 0)
        return 0;
    const auto y = [&](std::size_t i) { return std::isnan(d.y(i)) ? 0.0 : d.y(i); };
    const double first = d.x(0), last = d.x(n - 1);
    // past its ends: its end's value a little way (its points' spacing: a feed a point behind the others), then none
    const double reach = n > 1 ? 2 * (last - first) / static_cast<double>(n - 1) : 0;
    if (x <= first)
        return first - x <= reach ? y(0) : 0;
    if (x >= last)
        return x - last <= reach ? y(n - 1) : 0;
    const std::size_t i = d.lowerBound(x);
    if (i == 0)
        return y(0);
    const double x0 = d.x(i - 1), x1 = d.x(i);
    const double t = x1 > x0 ? (x - x0) / (x1 - x0) : 1;
    return y(i - 1) + (y(i) - y(i - 1)) * t;
}

double LineChart::stackAt(double x, std::span<double> tops) const {
    double sum = 0;
    for (std::size_t k = 0; k < stacked_.size(); ++k) {
        sum += stacked_[k]->shown_.value() * stackValue(*stacked_[k], x);
        tops[k] = sum;
    }
    if (props_.percent)
        for (double& top : tops)
            top = sum != 0 ? top / sum * 100 : 0;
    return sum;
}

void LineChart::sampleStack(ChartRange x, double columns) const {
    stackX_.clear();
    stackTops_.clear();
    if (stackMembers().empty() || !(x.span() > 0))
        return;
    // where its series have points (and one on each side), when they are not many more than its columns of pixels; else
    // once a column
    std::size_t points = 0;
    for (const LineSeries* s : stacked_) {
        const SeriesBuffer& d = s->data_;
        if (!d.sorted())
            points += d.size();
        else if (!d.empty())
            points += std::min(d.size(), d.lowerBound(x.high) + 1) - std::min(d.size(), d.lowerBound(x.low) - (d.lowerBound(x.low) > 0 ? 1 : 0));
    }
    if (static_cast<double>(points) <= 2 * columns) {
        for (const LineSeries* s : stacked_) {
            const SeriesBuffer& d = s->data_;
            if (d.empty() || !d.sorted())
                continue;
            std::size_t first = d.lowerBound(x.low);
            first = first > 0 ? first - 1 : 0;
            const std::size_t last = std::min(d.size(), d.lowerBound(x.high) + 1);
            for (std::size_t i = first; i < last; ++i)
                stackX_.push_back(d.x(i));
        }
        std::sort(stackX_.begin(), stackX_.end());
        stackX_.erase(std::unique(stackX_.begin(), stackX_.end()), stackX_.end());
    } else {
        const double dx = x.span() / columns;
        for (double c = -1; c <= columns; ++c)
            stackX_.push_back(x.low + (c + 0.5) * dx);
    }
    const std::size_t n = stackX_.size(), members = stacked_.size();
    stackTops_.resize(n * members);
    std::vector<double> tops(members);
    for (std::size_t i = 0; i < n; ++i) {
        stackAt(stackX_[i], tops);
        for (std::size_t k = 0; k < members; ++k)
            stackTops_[k * n + i] = tops[k];
    }
}

void LineChart::paintStack(PaintContext& context) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const ChartRange xv = xView(), yv = shownY();
    sampleStack(xv, std::max(1.0, static_cast<double>(plot_.width) * p.pixelsPerDip()));
    const std::size_t n = stackX_.size(), members = stacked_.size();
    if (n < 2 || members == 0)
        return;
    const Rect plot = plot_;
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (toView(y) - yv.low) / yv.span() * plot.height); };
    // each one's area from the one under it (the first's from 0) to its top, fuller at its top; then their lines over all
    // of them
    std::vector<std::vector<Point>> tops(members);
    for (std::size_t k = 0; k < members; ++k) {
        tops[k].resize(n);
        for (std::size_t i = 0; i < n; ++i)
            tops[k][i] = {px(stackX_[i]), py(stackTops_[k * n + i])};
    }
    lower_.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        lower_[i] = {px(stackX_[i]), py(0)};
    const float opacity = std::clamp(s.areaOpacity * 2.6f, 0.35f, 0.75f);
    for (std::size_t k = 0; k < members; ++k) {
        const LineSeries& series = *stacked_[k];
        const Color color = colorOf(series);
        const float alpha = series.shown_.value();
        p.fillBand(tops[k], k == 0 ? lower_ : tops[k - 1], withAlpha(color, color.a * opacity * alpha),
                   withAlpha(color, color.a * opacity * 0.7f * alpha));
    }
    for (std::size_t k = 0; k < members; ++k) {
        const LineSeries& series = *stacked_[k];
        if (!series.props_.line)
            continue;
        Color color = colorOf(series);
        color.a *= series.shown_.value();
        p.drawPolyline(tops[k], series.props_.width > 0 ? series.props_.width : s.lineWidth, color);
    }
}

void LineChart::project(const LineSeries& series, float pixelsPerDip, std::vector<Point>& out) const {
    out.clear();
    const SeriesBuffer& d = series.data_;
    const std::size_t n = d.size();
    if (n == 0)
        return;
    const bool second = onSecond(series);
    const ChartRange xv = xView(), yv = second ? shownY2() : shownY();
    const Rect plot = plot_;
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) {
        return static_cast<float>(plot.bottom() - ((second ? y : toView(y)) - yv.low) / yv.span() * plot.height);
    };
    std::size_t first = 0, last = n;
    if (d.sorted()) {
        first = d.lowerBound(xv.low);
        first = first > 0 ? first - 1 : 0;
        last = std::min(n, d.lowerBound(xv.high) + 1);
    }
    const double columns = std::max(1.0, static_cast<double>(plot.width) * pixelsPerDip);
    if (d.sorted() && static_cast<double>(last - first) > 2 * columns) {
        // a column of pixels at a time: its first and last points (its lowest and highest are its line's to show)
        const double dx = xv.span() / columns;
        const float pixel = 1.0f / pixelsPerDip;
        std::size_t i = first;
        for (double c = 0; c < columns && i < last; ++c) {
            const std::size_t j = std::min(last, d.lowerBound(xv.low + (c + 1) * dx, i));
            if (j <= i)
                continue;
            if (const auto e = d.yExtent(i, j)) {
                const float cx = px(xv.low + (c + 0.5) * dx);
                const double middle = (e->low + e->high) / 2, a = d.y(i), b = d.y(j - 1);
                out.push_back({cx - pixel / 4, py(std::isnan(a) ? middle : a)});
                out.push_back({cx + pixel / 4, py(std::isnan(b) ? middle : b)});
            }
            i = j;
        }
        for (; i < last; ++i)
            if (!std::isnan(d.y(i)))
                out.push_back({px(d.x(i)), py(d.y(i))});
        return;
    }
    for (std::size_t i = first; i < last; ++i) {
        const double y = d.y(i);
        if (std::isnan(y))
            continue;
        const Point at{px(d.x(i)), py(y)};
        if (series.props_.steps && !out.empty())
            out.push_back({at.x, out.back().y});
        out.push_back(at);
    }
}

void LineChart::paintArea(PaintContext& context, const AreaSeries& area) {
    const AreaSeries::Props& a = area.props_;
    if (!a.upper)
        return;
    // it fades with its series too
    const float alpha = area.shown_.value() * a.upper->shown_.value() * (a.lower ? a.lower->shown_.value() : 1.0f);
    if (alpha <= 0.002f)
        return;
    paint::Painter& p = context.painter;
    const Color color = a.color ? *a.color : colorOf(*a.upper);
    const Color fill = withAlpha(color, color.a * a.opacity * alpha);
    const Color fading = a.gradient ? withAlpha(color, 0) : fill;
    project(*a.upper, p.pixelsPerDip(), points_);
    if (points_.size() < 2)
        return;
    if (a.lower) {
        project(*a.lower, p.pixelsPerDip(), lower_);
        p.fillBand(points_, lower_, fill, a.gradient ? withAlpha(color, color.a * a.opacity * alpha * 0.25f) : fill);
    } else {
        // down to 0, or to the side of the plot its data is all past
        const bool second = onSecond(*a.upper);
        const ChartRange yv = second ? shownY2() : shownY();
        const double base = yv.contains(0) ? 0 : yv.low > 0 ? yv.low : yv.high;
        const float baseline =
            logY() && !second ? plot_.bottom() : static_cast<float>(plot_.bottom() - (base - yv.low) / yv.span() * plot_.height);
        p.fillArea(points_, baseline, fill, fading);
    }
    points_.clear();
}

void LineChart::paintHover(PaintContext& context, std::optional<double> shared) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const Point mouse = shared ? Point{pointOf({*shared, 0}).x, plot_.y} : *mouse_;
    const ChartRange xv = xView();
    const double at = shared ? *shared : valueAt(mouse).x;
    // a series's point nearest to an x (one that is a number), if its x is sorted
    const auto nearest = [](const SeriesBuffer& d, double x) -> std::optional<std::size_t> {
        if (d.empty() || !d.sorted())
            return std::nullopt;
        const std::size_t k = d.lowerBound(x);
        std::optional<std::size_t> best;
        double distance = std::numeric_limits<double>::infinity();
        for (std::size_t i = k > 2 ? k - 3 : 0; i < std::min(d.size(), k + 3); ++i) {
            if (std::isnan(d.y(i)))
                continue;
            if (const double dd = std::abs(d.x(i) - x); dd < distance) {
                distance = dd;
                best = i;
            }
        }
        return best;
    };
    // the nearest x of all the series that show
    std::optional<double> snapped;
    for (const auto& series : series_) {
        if (!series->visible_ || series->shown_.value() < 0.5f)
            continue;
        if (const auto i = nearest(series->data_, at)) {
            const double x = series->data_.x(*i);
            if (!snapped || std::abs(x - at) < std::abs(*snapped - at))
                snapped = x;
        }
    }
    if (!snapped || *snapped < xv.low || *snapped > xv.high)
        return;
    const Point cross = pointOf({*snapped, 0});
    const float ppd = p.pixelsPerDip();
    p.fillRect({snap(cross.x, ppd), plot_.y, 1.0f / ppd, plot_.height}, s.crosshair);
    // each series's point there: a dot, its value in the tooltip
    const Palette& palette = context.ui.theme.palette;
    const Color ring = s.plot.a > 0.9f ? s.plot : palette.layer;
    std::vector<ToolTipRow> rows;
    const double tolerance = xv.span() / plot_.width * 6;       // six dips
    // the stack: each one's dot on its top, its own value (and share) by it, the highest first; their sum after them
    std::vector<double> tops(stackMembers().size());
    const double sum = stacked_.empty() ? 0 : stackAt(*snapped, tops);
    std::vector<ToolTipRow> stackRows;
    for (std::size_t index = 0; index < series_.size(); ++index) {
        const LineSeries& series = *series_[index];
        if (!series.visible_ || series.shown_.value() < 0.5f || series.props_.name.source.empty())
            continue;
        if (series.props_.stacked) {
            const auto k = static_cast<std::size_t>(std::find(stacked_.begin(), stacked_.end(), &series) - stacked_.begin());
            if (k >= stacked_.size())
                continue;
            const double own = stackValue(series, *snapped);
            const Color color = colorOf(series);
            const Point dot = pointOf({*snapped, tops[k]});
            if (plot_.inflated(2).contains(dot)) {
                p.fillCircle(dot, 4.5f, ring);
                p.fillCircle(dot, 3.2f, color);
            }
            const int decimals = std::abs(own) >= 100 ? 0 : std::abs(own) >= 10 ? 1 : 2;
            std::string text = props_.percent ? formatNumber(own, decimals) : yAxis_.precise(own);
            if (props_.percent)
                text += "  " + formatNumber(sum != 0 ? own / sum * 100 : 0, 1) + "%";
            stackRows.insert(stackRows.begin(), {color, translate(series.props_.name), std::move(text)});
            continue;
        }
        const auto i = nearest(series.data_, *snapped);
        if (!i || std::abs(series.data_.x(*i) - *snapped) > tolerance)
            continue;
        const double y = series.data_.y(*i);
        const Color color = series.props_.color ? *series.props_.color : seriesColor(index);
        const Point dot = pointOn({series.data_.x(*i), y}, onSecond(series));
        if (plot_.inflated(2).contains(dot)) {
            p.fillCircle(dot, 4.5f, ring);
            p.fillCircle(dot, 3.2f, color);
        }
        rows.push_back({color, translate(series.props_.name), (onSecond(series) ? y2Axis_ : yAxis_).precise(y)});
    }
    if (!stackRows.empty()) {
        if (stackRows.size() > 1) {
            const int decimals = std::abs(sum) >= 100 ? 0 : std::abs(sum) >= 10 ? 1 : 2;
            stackRows.push_back({std::nullopt, translate(LocalizedText("Total")), props_.percent ? formatNumber(sum, decimals) : yAxis_.precise(sum)});
        }
        rows.insert(rows.begin(), stackRows.begin(), stackRows.end());
    }
    if (!rows.empty() && !shared)
        paintToolTip(context, {cross.x, mouse.y}, xAxis_.precise(*snapped), rows);
}

// ---- the mouse ----

void LineChart::zoomX(double factor, double anchor) {
    const ChartRange t = targetXView();
    if (xMode_ == XMode::Window) {
        // a live chart: its window, still following
        window_ = std::max(finest(t.high), window_ * factor);
    } else {
        ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
        if (r.span() < finest(anchor))
            return;
        xMode_ = XMode::Manual;
        userX_ = r;
    }
    quick_ = true;
    invalidatePaint();
}

void LineChart::zoomY(double factor, double anchor) {
    // in the view's terms (decades along a logarithmic axis)
    const ChartRange t = targetShownY();
    anchor = toView(anchor);
    const ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
    if (r.span() < finest(anchor))
        return;
    userY_ = fromView(r);
    quick_ = true;
    invalidatePaint();
}

void LineChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    switch (event.type) {
    case MouseEventType::Wheel: {
        if (!plot_.contains(at))
            return;
        const Point delta = event.wheelDelta;
        const bool sideways = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift) || std::abs(delta.x) > std::abs(delta.y);
        if (sideways) {
            // along x: a tenth of what shows a step
            const double steps = std::abs(delta.x) > std::abs(delta.y) ? delta.x : delta.y;
            const ChartRange t = targetXView();
            const double by = -steps * t.span() * 0.1;
            userX_ = {t.low + by, t.high + by};
            xMode_ = XMode::Manual;
            if (props_.window > 0 && userX_.high >= newestX()) {
                window_ = userX_.span();
                xMode_ = XMode::Window;
            }
            quick_ = true;
            invalidatePaint();
        } else if (platform::hasAny(event.modifiers, platform::KeyModifiers::Control)) {
            zoomY(std::pow(zoomStep, delta.y), valueAt(at).y);
        } else {
            zoomX(std::pow(zoomStep, delta.y), valueAt(at).x);
        }
        event.handled = true;
        return;
    }
    case MouseEventType::Press:
        if (!plot_.contains(at))
            return;
        if (event.button == platform::MouseButton::Left) {
            if (event.clicks == 2) {
                resetView();
            } else {
                drag_ = Drag::Pan;
                dragFrom_ = at;
                dragX_ = xView();
                dragY_ = shownY();
                dragFollowing_ = xMode_ == XMode::Window;
            }
            event.handled = true;
        } else if (event.button == platform::MouseButton::Right) {
            drag_ = Drag::Box;
            dragFrom_ = dragTo_ = at;
            event.handled = true;
        }
        return;
    case MouseEventType::Move:
        mouse_ = at;
        // where it is along x, for the charts that share its crosshair
        shareCursor(drag_ == Drag::None && plot_.contains(at) ? std::optional(valueAt(at).x) : std::nullopt);
        if (drag_ == Drag::Pan && plot_.width > 0 && plot_.height > 0) {
            const double dx = (at.x - dragFrom_.x) / plot_.width * dragX_.span();
            if (dx != 0 || xMode_ == XMode::Manual) {
                xMode_ = XMode::Manual;
                userX_ = {dragX_.low - dx, dragX_.high - dx};
            }
            if (userY_ && !props_.yRange) {
                // y zoomed by the user: it pans too
                const double dy = (at.y - dragFrom_.y) / plot_.height * dragY_.span();
                userY_ = fromView(ChartRange{dragY_.low + dy, dragY_.high + dy});
            }
            immediate_ = true;
        } else if (drag_ == Drag::Box) {
            dragTo_ = at;
        }
        invalidatePaint();
        return;
    case MouseEventType::Release:
        if (drag_ == Drag::Pan) {
            // a live chart dragged back to its newest data follows it again
            if ((dragFollowing_ || props_.window > 0) && xMode_ == XMode::Manual && userX_.high >= newestX() - userX_.span() * 0.02) {
                window_ = userX_.span();
                xMode_ = XMode::Window;
                immediate_ = true;
            }
        } else if (drag_ == Drag::Box) {
            const ChartPoint a = valueAt(dragFrom_), b = valueAt(dragTo_);
            if (std::abs(dragTo_.x - dragFrom_.x) >= dragThreshold) {
                xMode_ = XMode::Manual;
                userX_ = {std::min(a.x, b.x), std::max(a.x, b.x)};
            }
            if (std::abs(dragTo_.y - dragFrom_.y) >= dragThreshold)
                userY_ = ChartRange{std::min(a.y, b.y), std::max(a.y, b.y)};
        }
        drag_ = Drag::None;
        invalidatePaint();
        event.handled = true;
        return;
    }
}

void LineChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        shareCursor(std::nullopt);
        invalidatePaint();
    }
}

std::optional<platform::Cursor> LineChart::cursorAt(Point point) const {
    if (drag_ == Drag::Pan)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

std::vector<Chart::LegendItem> LineChart::legendItems() const {
    // its series, then its areas: those with names
    std::vector<LegendItem> items;
    legendTargets_.clear();
    for (std::size_t i = 0; i < series_.size(); ++i) {
        const LineSeries& s = *series_[i];
        if (s.props_.name.source.empty())
            continue;
        items.push_back({translate(s.props_.name), s.props_.color ? *s.props_.color : seriesColor(i), s.visible_});
        legendTargets_.emplace_back(false, i);
    }
    for (std::size_t i = 0; i < areas_.size(); ++i) {
        const AreaSeries& a = *areas_[i];
        if (a.props_.name.source.empty() || !a.props_.upper)
            continue;
        const Color color = a.props_.color ? *a.props_.color : colorOf(*a.props_.upper);
        items.push_back({translate(a.props_.name), withAlpha(color, color.a * std::max(0.45f, a.props_.opacity)), a.visible_});
        legendTargets_.emplace_back(true, i);
    }
    if (items.size() < 2) {
        items.clear();
        legendTargets_.clear();
    }
    return items;
}

void LineChart::legendItemClicked(std::size_t index) {
    if (index >= legendTargets_.size())
        return;
    const auto [area, i] = legendTargets_[index];
    if (area && i < areas_.size())
        areas_[i]->setVisible(!areas_[i]->visible_);
    else if (!area && i < series_.size())
        series_[i]->setVisible(!series_[i]->visible_);
}

} // namespace glw
