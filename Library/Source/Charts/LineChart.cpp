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

void LineChart::setIncludeZero(bool include) {
    props_.includeZero = include;
    invalidatePaint();
}

void LineChart::setXFormatter(std::function<std::string(double, double)> format) {
    formatX_ = std::move(format);
    invalidatePaint();
}

void LineChart::setYFormatter(std::function<std::string(double, double)> format) {
    formatY_ = std::move(format);
    invalidatePaint();
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

ChartRange LineChart::fitY(ChartRange x) const {
    if (userY_)
        return *userY_;
    std::optional<ChartExtent> all;
    for (const auto& s : series_) {
        if (!s->visible_)
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
    ChartRange r{all->low, all->high};
    if (props_.includeZero) {
        r.low = std::min(r.low, 0.0);
        r.high = std::max(r.high, 0.0);
    }
    r = opened(r, false);
    // a little room above and below its data (not below 0 when it starts there), then out to round values
    const double pad = r.span() * 0.04;
    if (!(props_.includeZero && r.low == 0))
        r.low -= pad;
    if (!(props_.includeZero && r.high == 0))
        r.high += pad;
    return niceRange(r, yTickCount_);
}

void LineChart::updateView() {
    const ChartRange tx = fitX(), ty = fitY(tx);
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
    // from where it shows now, on the way
    view_ = {xView(), tx, yView(), ty};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange LineChart::xView() const {
    return hasView_ ? between(view_.fromX, view_.toX, progress_.value()) : fitX();
}

ChartRange LineChart::yView() const {
    return hasView_ ? between(view_.fromY, view_.toY, progress_.value()) : fitY(xView());
}

ChartRange LineChart::targetXView() const {
    return hasView_ ? view_.toX : fitX();
}

ChartRange LineChart::targetYView() const {
    return hasView_ ? view_.toY : fitY(targetXView());
}

ChartPoint LineChart::valueAt(Point at) const {
    const ChartRange x = xView(), y = yView();
    const double fx = plot_.width > 0 ? (at.x - plot_.x) / plot_.width : 0;
    const double fy = plot_.height > 0 ? (plot_.bottom() - at.y) / plot_.height : 0;
    return {x.low + fx * x.span(), y.low + fy * y.span()};
}

Point LineChart::pointOf(ChartPoint value) const {
    const ChartRange x = xView(), y = yView();
    return {static_cast<float>(plot_.x + (value.x - x.low) / x.span() * plot_.width),
            static_cast<float>(plot_.bottom() - (value.y - y.low) / y.span() * plot_.height)};
}

std::string LineChart::formatX(double v, double step) const {
    if (formatX_)
        return formatX_(v, step);
    if (props_.xAxis == ChartAxisKind::Time) {
        // to the second (to the millisecond when its ticks are), its day when more than a day shows
        const TimeUnit tick = timeTicks_.unit;
        const TimeUnit unit = tick == TimeUnit::Millisecond ? TimeUnit::Millisecond : tick <= TimeUnit::Hour ? TimeUnit::Second : tick;
        return formatTimeFully(v, unit, xView().span() > 86400);
    }
    return formatNumber(v, xTicks_.decimals + 1);
}

std::string LineChart::formatY(double v, double step) const {
    if (formatY_)
        return formatY_(v, step);
    return formatNumber(v, yTicks_.decimals + 1);
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
    const ChartRange xv = xView(), yv = yView();

    // the y axis as wide as its labels
    yTicks_ = niceTicks(yv, yTickCount_);
    std::vector<std::string> yLabels;
    float yWidth = 0;
    for (double v : yTicks_.values) {
        yLabels.push_back(formatY_ ? formatY_(v, yTicks_.step) : formatNumber(v, yTicks_.decimals, yTicks_.step));
        yWidth = std::max(yWidth, textSize(yLabels.back()).width);
    }
    const float axisWidth = yWidth + 8;
    plot.width = std::max(0.0f, plot.width - axisWidth - 6);
    if (!rtl)
        plot.x += axisWidth;
    else
        plot.x += 6;
    plot_ = plot;
    if (plot.width < 4 || plot.height < 4 || !(xv.span() > 0) || !(yv.span() > 0))
        return;

    // the x axis's ticks: as many as their labels fit
    std::vector<double> xValues;
    std::vector<std::string> xLabels;
    const auto ticksFor = [&](int count) {
        xValues.clear();
        xLabels.clear();
        if (time) {
            timeTicks_ = timeTicks(xv, count);
            xValues = timeTicks_.values;
            for (double v : xValues)
                xLabels.push_back(formatX_ ? formatX_(v, timeTicks_.step) : formatTime(v, timeTicks_));
        } else {
            xTicks_ = niceTicks(xv, count);
            xValues = xTicks_.values;
            for (double v : xValues)
                xLabels.push_back(formatX_ ? formatX_(v, xTicks_.step) : formatNumber(v, xTicks_.decimals, xTicks_.step));
        }
    };
    int count = std::max(2, static_cast<int>(plot.width / 90));
    ticksFor(count);
    float widest = 0;
    for (const std::string& label : xLabels)
        widest = std::max(widest, textSize(label).width);
    if ((widest + 20) * static_cast<float>(xLabels.size()) > plot.width) {
        count = std::max(2, static_cast<int>(plot.width / (widest + 20)));
        ticksFor(count);
    }

    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (y - yv.low) / yv.span() * plot.height); };
    const float hair = 1.0f / ppd;

    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    // the grid, sharp on the pixels
    for (double v : yTicks_.values) {
        const float y = snap(py(v), ppd);
        if (y >= plot.y - 0.5f && y <= plot.bottom() + 0.5f)
            p.fillRect({plot.x, y, plot.width, hair}, s.grid);
    }
    for (double v : xValues) {
        const float x = snap(px(v), ppd);
        if (x >= plot.x - 0.5f && x <= plot.right() + 0.5f)
            p.fillRect({x, plot.y, hair, plot.height}, s.grid);
    }
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, hair}, s.axis);
    // the labels
    for (std::size_t i = 0; i < yTicks_.values.size(); ++i) {
        const float y = py(yTicks_.values[i]);
        if (y < plot.y - 1 || y > plot.bottom() + 1)
            continue;
        drawText(context, yLabels[i], {rtl ? plot.right() + 8 : plot.x - 8, y}, s.text, 0, {rtl ? 0.0f : 1.0f, 0.5f});
    }
    for (std::size_t i = 0; i < xValues.size(); ++i) {
        const float x = px(xValues[i]);
        const float half = textSize(xLabels[i]).width / 2;
        if (x < plot.x - 1 || x > plot.right() + 1 || x - half < area.x || x + half > area.right())
            continue;
        drawText(context, xLabels[i], {x, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
    }

    // the series, cut to the plot (a little above and below it: a line along its edge)
    p.pushClip({plot.x, plot.y - 2, plot.width, plot.height + 4});
    for (const auto& a : areas_)
        paintArea(context, *a);
    for (std::size_t i = 0; i < series_.size(); ++i)
        paintSeries(context, *series_[i], i);
    p.popClip();

    if (drag_ == Drag::Box) {
        const Rect box = Rect{std::min(dragFrom_.x, dragTo_.x), std::min(dragFrom_.y, dragTo_.y), std::abs(dragTo_.x - dragFrom_.x),
                              std::abs(dragTo_.y - dragFrom_.y)}
                             .intersected(plot);
        p.fillRect(box, s.selection);
        p.strokeRoundedRect(box, 0, hair, s.crosshair);
    } else if (mouse_ && drag_ == Drag::None && plot.contains(*mouse_)) {
        paintHover(context);
    }
}

void LineChart::paintSeries(PaintContext& context, const LineSeries& series, std::size_t index) {
    const float alpha = series.shown_.value();
    const SeriesBuffer& d = series.data_;
    const std::size_t n = d.size();
    if (alpha <= 0.002f || n == 0)
        return;
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    Color color = series.props_.color ? *series.props_.color : seriesColor(index);
    color.a *= alpha;
    const float width = series.props_.width > 0 ? series.props_.width : s.lineWidth;
    const ChartRange xv = xView(), yv = yView();
    const Rect plot = plot_;
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (y - yv.low) / yv.span() * plot.height); };

    // what shows, and a point on each side
    std::size_t first = 0, last = n;
    if (d.sorted()) {
        first = d.lowerBound(xv.low);
        first = first > 0 ? first - 1 : 0;
        last = std::min(n, d.lowerBound(xv.high) + 1);
    }
    if (last <= first)
        return;
    // the area down to 0, or to the axis the data is all above (below)
    const double base = yv.contains(0) ? 0 : yv.low > 0 ? yv.low : yv.high;
    const float baseline = py(base);
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

void LineChart::project(const LineSeries& series, float pixelsPerDip, std::vector<Point>& out) const {
    out.clear();
    const SeriesBuffer& d = series.data_;
    const std::size_t n = d.size();
    if (n == 0)
        return;
    const ChartRange xv = xView(), yv = yView();
    const Rect plot = plot_;
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (y - yv.low) / yv.span() * plot.height); };
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
        const ChartRange yv = yView();
        const double base = yv.contains(0) ? 0 : yv.low > 0 ? yv.low : yv.high;
        const float baseline = static_cast<float>(plot_.bottom() - (base - yv.low) / yv.span() * plot_.height);
        p.fillArea(points_, baseline, fill, fading);
    }
    points_.clear();
}

void LineChart::paintHover(PaintContext& context) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const Point mouse = *mouse_;
    const ChartRange xv = xView();
    const double at = valueAt(mouse).x;
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
    for (std::size_t index = 0; index < series_.size(); ++index) {
        const LineSeries& series = *series_[index];
        if (!series.visible_ || series.shown_.value() < 0.5f || series.props_.name.source.empty())
            continue;
        const auto i = nearest(series.data_, *snapped);
        if (!i || std::abs(series.data_.x(*i) - *snapped) > tolerance)
            continue;
        const double y = series.data_.y(*i);
        const Color color = series.props_.color ? *series.props_.color : seriesColor(index);
        const Point dot = pointOf({series.data_.x(*i), y});
        if (plot_.inflated(2).contains(dot)) {
            p.fillCircle(dot, 4.5f, ring);
            p.fillCircle(dot, 3.2f, color);
        }
        rows.push_back({color, translate(series.props_.name), formatY(y, yTicks_.step)});
    }
    if (!rows.empty())
        paintToolTip(context, {cross.x, mouse.y}, formatX(*snapped, xTicks_.step), rows);
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
    const ChartRange t = targetYView();
    const ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
    if (r.span() < finest(anchor))
        return;
    userY_ = r;
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
                dragY_ = yView();
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
        if (drag_ == Drag::Pan && plot_.width > 0 && plot_.height > 0) {
            const double dx = (at.x - dragFrom_.x) / plot_.width * dragX_.span();
            if (dx != 0 || xMode_ == XMode::Manual) {
                xMode_ = XMode::Manual;
                userX_ = {dragX_.low - dx, dragX_.high - dx};
            }
            if (userY_ && !props_.yRange) {
                // y zoomed by the user: it pans too
                const double dy = (at.y - dragFrom_.y) / plot_.height * dragY_.span();
                userY_ = ChartRange{dragY_.low + dy, dragY_.high + dy};
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
