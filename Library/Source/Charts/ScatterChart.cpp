// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/ScatterChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;       // a step of the wheel in: the span this much of what it was
constexpr float dragThreshold = 6;      // a rectangle dragged narrower than this zooms nothing that way
constexpr float reach = 10;             // the mouse finds a point this near it (dips)

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

std::uint64_t mixed(std::uint64_t h, double v) {
    return h ^ (std::bit_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

} // namespace

// ---- a series ----

ScatterSeries::ScatterSeries(ScatterChart& chart, Props props)
    : chart_(chart), props_(std::move(props)), shown_([this] { chart_.invalidatePaint(); }) {
    shown_.set(1);
}

ScatterSeries::~ScatterSeries() = default;

void ScatterSeries::take(double x, double y, double size, int by) {
    (void)size;
    const double dx = x - ox_, dy = y - oy_, k = by;
    sx_ += k * dx;
    sy_ += k * dy;
    sxx_ += k * dx * dx;
    sxy_ += k * dx * dy;
    syy_ += k * dy * dy;
}

void ScatterSeries::append(double x, double y, double size) {
    if (std::isnan(x) || std::isnan(y))
        return;
    if (!origin_) {
        ox_ = x;
        oy_ = y;
        origin_ = true;
    }
    if (size > 0 && sizes_.empty())
        sizes_.assign(xs_.size(), 0);
    xs_.push_back(x);
    ys_.push_back(y);
    if (!sizes_.empty())
        sizes_.push_back(std::max(0.0, size));
    if (extentKnown_) {
        if (xs_.size() == 1) {
            lowX_ = highX_ = x;
            lowY_ = highY_ = y;
        } else {
            lowX_ = std::min(lowX_, x);
            highX_ = std::max(highX_, x);
            lowY_ = std::min(lowY_, y);
            highY_ = std::max(highY_, y);
        }
        largest_ = std::max(largest_, size);
    }
    take(x, y, size, 1);
    while (props_.capacity > 0 && xs_.size() > props_.capacity)
        dropOldest();
    chart_.changed();
}

void ScatterSeries::append(std::span<const ChartPoint> points) {
    for (const ChartPoint& p : points)
        append(p.x, p.y);
}

void ScatterSeries::dropOldest() {
    const double x = xs_.front(), y = ys_.front(), size = sizes_.empty() ? 0 : sizes_.front();
    take(x, y, size, -1);
    if (x == lowX_ || x == highX_ || y == lowY_ || y == highY_ || size == largest_)
        extentKnown_ = false;
    xs_.pop_front();
    ys_.pop_front();
    if (!sizes_.empty())
        sizes_.pop_front();
}

void ScatterSeries::clear() {
    xs_.clear();
    ys_.clear();
    sizes_.clear();
    origin_ = false;
    sx_ = sy_ = sxx_ = sxy_ = syy_ = 0;
    extentKnown_ = true;
    largest_ = 0;
    chart_.changed();
}

std::optional<ScatterSeries::Fit> ScatterSeries::fit() const {
    const auto n = static_cast<double>(xs_.size());
    if (n < 2)
        return std::nullopt;
    const double sxx = sxx_ - sx_ * sx_ / n, sxy = sxy_ - sx_ * sy_ / n, syy = syy_ - sy_ * sy_ / n;
    if (!(sxx > 0))
        return std::nullopt;
    Fit f;
    f.slope = sxy / sxx;
    f.intercept = oy_ + (sy_ - f.slope * sx_) / n - f.slope * ox_;
    f.r2 = syy > 0 ? std::clamp(sxy * sxy / (sxx * syy), 0.0, 1.0) : 1.0;
    return f;
}

void ScatterSeries::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    shown_.animateTo(chart_.scheduler(), visible ? 1.0f : 0.0f, chart_.transition());
    chart_.changed();
}

// ---- the chart ----

ScatterChart::ScatterChart() : ScatterChart(Props{}) {}

ScatterChart::ScatterChart(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    progress_.set(1);
}

ScatterChart::~ScatterChart() = default;

ScatterSeries& ScatterChart::addSeries(ScatterSeries::Props props) {
    series_.push_back(std::unique_ptr<ScatterSeries>(new ScatterSeries(*this, std::move(props))));
    changed();
    return *series_.back();
}

void ScatterChart::changed() {
    ++version_;
    invalidatePaint();
}

void ScatterChart::setXFormatter(std::function<std::string(double, double)> format) {
    xAxis_.format = std::move(format);
    invalidatePaint();
}

void ScatterChart::setYFormatter(std::function<std::string(double, double)> format) {
    yAxis_.format = std::move(format);
    invalidatePaint();
}

Color ScatterChart::colorOf(std::size_t index) const {
    const ScatterSeries& s = *series_[index];
    return s.props_.color ? *s.props_.color : seriesColor(index);
}

// ---- the view ----

double ScatterChart::toViewX(double x) const {
    return logX() ? (x > 0 ? std::log10(x) : -1e3) : x;
}

double ScatterChart::toViewY(double y) const {
    return props_.logY ? (y > 0 ? std::log10(y) : -1e3) : y;
}

ChartRange ScatterChart::fitted(ChartRange r, bool log, int ticks, double pad) const {
    if (log) {
        const double high = r.high > 0 ? r.high : 1;
        const double low = r.low > 0 ? r.low : high / 1000;
        ChartRange l{std::log10(low), std::log10(high)};
        if (!(l.span() > 0))
            l = {l.low - 0.5, l.high + 0.5};
        const double room = l.span() * pad;
        const ChartRange nice = niceLogRange({std::pow(10.0, l.low - room), std::pow(10.0, l.high + room)});
        return {std::log10(nice.low), std::log10(nice.high)};
    }
    if (!(r.span() > 0))
        r = {r.low - 1, r.high + 1};
    // a little room around them, not past 0 when they are all on one side of it
    const double room = r.span() * pad;
    const ChartRange padded{r.low >= 0 ? std::max(0.0, r.low - room) : r.low - room, r.high <= 0 ? std::min(0.0, r.high + room) : r.high + room};
    return niceRange(padded, ticks);
}

void ScatterChart::setLogX(bool log) {
    if (props_.logX == log)
        return;
    props_.logX = log;
    userX_.reset();
    hasView_ = false;           // (its view in the other scale's terms)
    invalidatePaint();
}

void ScatterChart::setLogY(bool log) {
    if (props_.logY == log)
        return;
    props_.logY = log;
    userY_.reset();
    hasView_ = false;
    invalidatePaint();
}

ChartRange ScatterChart::fitX() const {
    if (userX_)
        return *userX_;
    if (props_.xRange)
        return {toViewX(props_.xRange->low), toViewX(props_.xRange->high)};
    std::optional<ChartRange> all;
    for (const auto& s : series_) {
        if (!s->visible_ || s->xs_.empty())
            continue;
        if (!s->extentKnown_) {
            s->lowX_ = *std::min_element(s->xs_.begin(), s->xs_.end());
            s->highX_ = *std::max_element(s->xs_.begin(), s->xs_.end());
            s->lowY_ = *std::min_element(s->ys_.begin(), s->ys_.end());
            s->highY_ = *std::max_element(s->ys_.begin(), s->ys_.end());
            s->largest_ = s->sizes_.empty() ? 0 : *std::max_element(s->sizes_.begin(), s->sizes_.end());
            s->extentKnown_ = true;
        }
        all = all ? ChartRange{std::min(all->low, s->lowX_), std::max(all->high, s->highX_)} : ChartRange{s->lowX_, s->highX_};
    }
    if (!all)
        return {0, 1};
    return fitted(*all, logX(), std::max(2, static_cast<int>(plot_.width / 70)), 0.04);
}

ChartRange ScatterChart::fitY() const {
    if (userY_)
        return *userY_;
    if (props_.yRange)
        return {toViewY(props_.yRange->low), toViewY(props_.yRange->high)};
    fitX();         // (the extents found again)
    std::optional<ChartRange> all;
    for (const auto& s : series_)
        if (s->visible_ && !s->xs_.empty())
            all = all ? ChartRange{std::min(all->low, s->lowY_), std::max(all->high, s->highY_)} : ChartRange{s->lowY_, s->highY_};
    if (!all)
        return {0, 1};
    return fitted(*all, props_.logY, std::max(2, static_cast<int>(plot_.height / 34)), 0.05);
}

void ScatterChart::updateView() {
    const ChartRange tx = fitX(), ty = fitY();
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
    view_ = {shownX(), tx, shownY(), ty};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange ScatterChart::shownX() const {
    return hasView_ ? between(view_.fromX, view_.toX, progress_.value()) : fitX();
}

ChartRange ScatterChart::shownY() const {
    return hasView_ ? between(view_.fromY, view_.toY, progress_.value()) : fitY();
}

ChartRange ScatterChart::xView() const {
    const ChartRange v = shownX();
    return {fromViewX(v.low), fromViewX(v.high)};
}

ChartRange ScatterChart::yView() const {
    const ChartRange v = shownY();
    return {fromViewY(v.low), fromViewY(v.high)};
}

void ScatterChart::setView(ChartRange x, ChartRange y) {
    userX_ = ChartRange{toViewX(x.low), toViewX(x.high)};
    userY_ = ChartRange{toViewY(y.low), toViewY(y.high)};
    invalidatePaint();
}

void ScatterChart::resetView() {
    userX_.reset();
    userY_.reset();
    invalidatePaint();
}

// ---- painting ----

void ScatterChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    drawn_ = Drawn::Nothing;

    Rect plot = area;
    plot.y += line / 2;
    plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
    if (plot_.width <= 0)
        plot_ = plot;
    updateView();
    const ChartRange xv = shownX(), yv = shownY();
    yAxis_.orientation = Orientation::Vertical;
    yAxis_.log = props_.logY;
    yAxis_.fit(*this, {fromViewY(yv.low), fromViewY(yv.high)}, plot.height);
    const float axisWidth = yAxis_.labelWidth() + 8;
    plot.width = std::max(0.0f, plot.width - axisWidth - 6);
    plot.x += rtl ? 6 : axisWidth;
    plot_ = plot;
    if (plot.width < 8 || plot.height < 8 || !(xv.span() > 0) || !(yv.span() > 0))
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    xAxis_.orientation = Orientation::Horizontal;
    xAxis_.time = props_.xAxis == ChartAxisKind::Time;
    xAxis_.log = logX();
    xAxis_.fit(*this, {fromViewX(xv.low), fromViewX(xv.high)}, plot.width);
    yAxis_.paintGrid(p, plot, s.grid);
    xAxis_.paintGrid(p, plot, s.grid);
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);
    yAxis_.paintLabels(context, *this, plot, rtl ? Axis::Side::Right : Axis::Side::Left, s.text, area);
    xAxis_.paintLabels(context, *this, plot, Axis::Side::Bottom, s.text, area);
    const auto px = [&](double x) { return static_cast<float>(plot.x + (toViewX(x) - xv.low) / xv.span() * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (toViewY(y) - yv.low) / yv.span() * plot.height); };

    // how many show: dots for each, or (more than it draws as dots) an image of where they are
    std::size_t total = 0;
    for (const auto& series : series_)
        if (series->shown_.value() > 0.002f)
            total += series->xs_.size();
    std::size_t inView = total;
    if (total > props_.markers) {
        inView = 0;
        for (const auto& series : series_) {
            if (series->shown_.value() <= 0.002f)
                continue;
            for (std::size_t i = 0; i < series->xs_.size(); ++i)
                inView += xv.contains(toViewX(series->xs_[i])) && yv.contains(toViewY(series->ys_[i])) ? 1 : 0;
        }
    }
    paintAnnotations(context, plot, xAxis_, yAxis_, false);
    p.pushClip(plot);
    if (inView > props_.markers && context.ui.images) {
        paintDensity(context, xv, yv);
    } else {
        drawn_ = Drawn::Markers;
        // (without a store of images: every n-th of them)
        const std::size_t stride = std::max<std::size_t>(1, (inView + props_.markers - 1) / std::max<std::size_t>(1, props_.markers));
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const ScatterSeries& series = *series_[k];
            const float alpha = series.shown_.value();
            if (alpha <= 0.002f)
                continue;
            Color color = colorOf(k);
            color.a *= props_.opacity * alpha;
            const float r = series.props_.radius > 0 ? series.props_.radius : 3.5f;
            const bool bubbles = !series.sizes_.empty() && series.largest_ > 0;
            for (std::size_t i = 0; i < series.xs_.size(); i += stride) {
                const float x = px(series.xs_[i]), y = py(series.ys_[i]);
                const float radius = bubbles ? std::max(r * 0.6f, 4 * r * static_cast<float>(std::sqrt(series.sizes_[i] / series.largest_))) : r;
                if (x < plot.x - radius || x > plot.right() + radius || y < plot.y - radius || y > plot.bottom() + radius)
                    continue;
                p.fillCircle({x, y}, radius, color);
            }
        }
    }
    // the trend lines, their r² by their ends
    for (std::size_t k = 0; k < series_.size(); ++k) {
        const ScatterSeries& series = *series_[k];
        if (!series.props_.trend || series.shown_.value() <= 0.002f)
            continue;
        const auto f = series.fit();
        if (!f)
            continue;
        Color color = lit(colorOf(k), context.ui.theme.isDark() ? 0.25f : -0.25f);
        color.a *= series.shown_.value();
        // straight; on a logarithmic axis a curve, drawn as a line through points along it
        const auto along = [&](double t) {
            const double x = fromViewX(xv.low + t * xv.span());
            return Point{px(x), py(f->intercept + f->slope * x)};
        };
        const Point a = along(0), b = along(1);
        if (logX() || props_.logY) {
            std::vector<Point> curve;
            for (int i = 0; i <= 64; ++i)
                curve.push_back(along(i / 64.0));
            p.drawPolyline(curve, s.lineWidth + 0.5f, color);
        } else {
            p.drawLine(a, b, s.lineWidth + 0.5f, color);
        }
        const float y = std::clamp(b.y - 4, plot.y + line, plot.bottom() - 2);
        drawText(context, "r² " + formatNumber(f->r2, 2), {plot.right() - 6, y}, color, 0, {1, 1});
    }
    p.popClip();
    paintAnnotations(context, plot, xAxis_, yAxis_, true);

    if (drag_ == Drag::Box) {
        const Rect box = Rect{std::min(dragFrom_.x, dragTo_.x), std::min(dragFrom_.y, dragTo_.y), std::abs(dragTo_.x - dragFrom_.x),
                              std::abs(dragTo_.y - dragFrom_.y)}
                             .intersected(plot);
        p.fillRect(box, s.selection);
        p.strokeRoundedRect(box, 0, 1.0f / ppd, s.crosshair);
        return;
    }
    // the point nearest the mouse: ringed, its values by it
    if (mouse_ && drag_ == Drag::None && plot.contains(*mouse_)) {
        const ScatterSeries* nearest = nullptr;
        std::size_t index = 0, which = 0;
        float best = reach * reach;
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const ScatterSeries& series = *series_[k];
            if (!series.visible_)
                continue;
            for (std::size_t i = 0; i < series.xs_.size(); ++i) {
                const float dx = px(series.xs_[i]) - mouse_->x, dy = py(series.ys_[i]) - mouse_->y;
                if (const float d = dx * dx + dy * dy; d < best) {
                    best = d;
                    nearest = &series;
                    index = i;
                    which = k;
                }
            }
        }
        if (nearest) {
            const Point at{px(nearest->xs_[index]), py(nearest->ys_[index])};
            const Color color = colorOf(which);
            const Color ring = s.plot.a > 0.9f ? s.plot : context.ui.theme.palette.layer;
            const float r = nearest->props_.radius > 0 ? nearest->props_.radius : 3.5f;
            p.fillCircle(at, r + 2.5f, ring);
            p.fillCircle(at, r + 1, color);
            std::vector<ToolTipRow> rows = {{std::nullopt, "x", xAxis_.precise(nearest->xs_[index])},
                                            {std::nullopt, "y", yAxis_.precise(nearest->ys_[index])}};
            if (!nearest->sizes_.empty())
                rows.push_back({std::nullopt, translate(LocalizedText("Size")), formatNumber(nearest->sizes_[index], 2)});
            const std::string name = nearest->props_.name.source.empty() ? std::string() : translate(nearest->props_.name);
            paintToolTip(context, *mouse_, name, rows);
        }
    }
}

void ScatterChart::paintDensity(PaintContext& context, ChartRange xv, ChartRange yv) {
    paint::Painter& p = context.painter;
    drawn_ = Drawn::Density;
    const Rect plot = plot_;
    const float ppd = p.pixelsPerDip();
    const int w = std::max(1, static_cast<int>(std::lround(plot.width * ppd))), h = std::max(1, static_cast<int>(std::lround(plot.height * ppd)));
    if (!image_)
        image_ = context.ui.images->make(rhi::Filter::Nearest);
    // made again when what it shows changed: its points, the view, its size, its series' colors and how they show
    std::uint64_t key = mixed(mixed(mixed(mixed(17, xv.low), xv.high), yv.low), yv.high);
    key = mixed(mixed(mixed(key, static_cast<double>(version_)), static_cast<double>(w)), static_cast<double>(h));
    for (std::size_t k = 0; k < series_.size(); ++k) {
        const Color c = colorOf(k);
        key = mixed(mixed(mixed(mixed(key, c.r), c.g), c.b), series_[k]->shown_.value());
    }
    if (key != imageKey_ || image_.size() != PixelSize{w, h}) {
        imageKey_ = key;
        image_.resize({w, h});
        const auto pixels = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
        composite_.assign(pixels * 4, 0);
        // each series's points counted into the pixels, then laid over those before it: as opaque as how many there are
        for (std::size_t k = 0; k < series_.size(); ++k) {
            const ScatterSeries& series = *series_[k];
            const float shown = series.shown_.value();
            if (shown <= 0.002f || series.xs_.empty())
                continue;
            counts_.assign(pixels, 0);
            std::uint32_t most = 0;
            const double sx = w / xv.span(), sy = h / yv.span();
            for (std::size_t i = 0; i < series.xs_.size(); ++i) {
                const double fx = (toViewX(series.xs_[i]) - xv.low) * sx, fy = (yv.high - toViewY(series.ys_[i])) * sy;
                if (fx < 0 || fy < 0 || fx >= w || fy >= h)
                    continue;
                std::uint32_t& n = counts_[static_cast<std::size_t>(fy) * static_cast<std::size_t>(w) + static_cast<std::size_t>(fx)];
                most = std::max(most, ++n);
            }
            if (most == 0)
                continue;
            const Color c = colorOf(k);
            const float scale = 1.0f / std::log1p(static_cast<float>(most));
            for (std::size_t i = 0; i < pixels; ++i) {
                if (counts_[i] == 0)
                    continue;
                const float a = (0.28f + 0.72f * std::log1p(static_cast<float>(counts_[i])) * scale) * shown * c.a;
                float* d = &composite_[i * 4];
                d[0] = c.r * a + d[0] * (1 - a);
                d[1] = c.g * a + d[1] * (1 - a);
                d[2] = c.b * a + d[2] * (1 - a);
                d[3] = a + d[3] * (1 - a);
            }
        }
        const auto byte = [](float v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
        for (int y = 0; y < h; ++y) {
            std::span<std::uint32_t> row = image_.row(y);
            for (int x = 0; x < w; ++x) {
                const float* d = &composite_[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4];
                row[static_cast<std::size_t>(x)] = byte(d[0]) | byte(d[1]) << 8 | byte(d[2]) << 16 | byte(d[3]) << 24;
            }
        }
        image_.changed();
    }
    p.drawImage(plot, image_.texture());
}

std::vector<Chart::LegendItem> ScatterChart::legendItems() const {
    std::vector<LegendItem> items;
    for (std::size_t i = 0; i < series_.size(); ++i)
        if (!series_[i]->props_.name.source.empty())
            items.push_back({translate(series_[i]->props_.name), colorOf(i), series_[i]->visible_});
    return items;
}

void ScatterChart::legendItemClicked(std::size_t index) {
    for (const auto& s : series_) {
        if (s->props_.name.source.empty())
            continue;
        if (index-- == 0) {
            s->setVisible(!s->visible_);
            return;
        }
    }
}

// ---- the mouse ----

void ScatterChart::zoom(double fx, double fy, Point at) {
    const ChartRange tx = view_.toX, ty = view_.toY;
    const double ax = tx.low + (at.x - plot_.x) / std::max(1.0f, plot_.width) * tx.span();
    const double ay = ty.low + (plot_.bottom() - at.y) / std::max(1.0f, plot_.height) * ty.span();
    if (fx != 1)
        userX_ = ChartRange{ax - (ax - tx.low) * fx, ax + (tx.high - ax) * fx};
    else if (!userX_)
        userX_ = tx;
    if (fy != 1)
        userY_ = ChartRange{ay - (ay - ty.low) * fy, ay + (ty.high - ay) * fy};
    else if (!userY_)
        userY_ = ty;
    quick_ = true;
    invalidatePaint();
}

void ScatterChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    switch (event.type) {
    case MouseEventType::Wheel: {
        if (!plot_.contains(at))
            return;
        const double f = std::pow(zoomStep, event.wheelDelta.y);
        const bool onlyY = platform::hasAny(event.modifiers, platform::KeyModifiers::Control);
        const bool onlyX = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
        zoom(onlyY ? 1 : f, onlyX ? 1 : f, at);
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
                dragX_ = shownX();
                dragY_ = shownY();
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
            const double dy = (at.y - dragFrom_.y) / plot_.height * dragY_.span();
            userX_ = ChartRange{dragX_.low - dx, dragX_.high - dx};
            userY_ = ChartRange{dragY_.low + dy, dragY_.high + dy};
            immediate_ = true;
        } else if (drag_ == Drag::Box) {
            dragTo_ = at;
        }
        invalidatePaint();
        return;
    case MouseEventType::Release:
        if (drag_ == Drag::Box) {
            const auto valueAt = [&](Point point) {
                const ChartRange xv = shownX(), yv = shownY();
                return ChartPoint{xv.low + (point.x - plot_.x) / plot_.width * xv.span(), yv.low + (plot_.bottom() - point.y) / plot_.height * yv.span()};
            };
            const ChartPoint a = valueAt(dragFrom_), b = valueAt(dragTo_);
            if (std::abs(dragTo_.x - dragFrom_.x) >= dragThreshold)
                userX_ = ChartRange{std::min(a.x, b.x), std::max(a.x, b.x)};
            if (std::abs(dragTo_.y - dragFrom_.y) >= dragThreshold)
                userY_ = ChartRange{std::min(a.y, b.y), std::max(a.y, b.y)};
        }
        if (drag_ != Drag::None) {
            drag_ = Drag::None;
            invalidatePaint();
            event.handled = true;
        }
        return;
    }
}

void ScatterChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

std::optional<platform::Cursor> ScatterChart::cursorAt(Point point) const {
    if (drag_ == Drag::Pan)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

} // namespace glw
