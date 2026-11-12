// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/HistogramChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace glw {

namespace {

const double none = std::numeric_limits<double>::quiet_NaN();

// The decimals that tell values `step` apart.
int decimalsFor(double step) {
    return step > 0 ? std::clamp(static_cast<int>(std::ceil(-std::log10(step) - 1e-9)), 0, 8) : 0;
}

} // namespace

// ---- a series ----

HistogramSeries::HistogramSeries(HistogramChart& chart, Props props)
    : chart_(chart), props_(std::move(props)), shown_([this] { chart_.invalidatePaint(); }) {
    shown_.set(1);
}

HistogramSeries::~HistogramSeries() = default;

void HistogramSeries::count(double value, int by) {
    const HistogramChart& c = chart_;
    if (c.rebin_ || c.bins_ == 0)
        return;         // (all of them counted again when the bins are found)
    double at = std::floor((value - c.start_) / c.width_);
    const bool inside = at >= 0 && at < static_cast<double>(c.bins_);
    if (!inside) {
        // past its bins: in the first or last when they hold those (found again when many are), else found again now
        if (by > 0)
            ++outside_;
        else if (outside_ > 0)
            --outside_;
        if (!c.clipped_) {
            if (by > 0 && !c.props_.range)
                c.rebin_ = true;
            return;
        }
        at = at < 0 ? 0 : static_cast<double>(c.bins_ - 1);
    }
    std::uint32_t& n = counts_[static_cast<std::size_t>(at)];
    n = by > 0 ? n + 1 : (n > 0 ? n - 1 : 0);
}

void HistogramSeries::add(double value) {
    if (std::isnan(value))
        return;
    values_.push_back(value);
    sum_ += value;
    count(value, 1);
    while (props_.capacity > 0 && values_.size() > props_.capacity) {
        sum_ -= values_.front();
        count(values_.front(), -1);
        values_.pop_front();
    }
    chart_.changed();
}

void HistogramSeries::add(std::span<const double> values) {
    for (double v : values) {
        if (std::isnan(v))
            continue;
        values_.push_back(v);
        sum_ += v;
        count(v, 1);
    }
    while (props_.capacity > 0 && values_.size() > props_.capacity) {
        sum_ -= values_.front();
        count(values_.front(), -1);
        values_.pop_front();
    }
    chart_.changed();
}

void HistogramSeries::clear() {
    values_.clear();
    sum_ = 0;
    std::fill(counts_.begin(), counts_.end(), 0u);
    outside_ = 0;
    chart_.rebin_ = true;
    chart_.changed();
}

double HistogramSeries::mean() const {
    return values_.empty() ? none : sum_ / static_cast<double>(values_.size());
}

double HistogramSeries::quantile(double q) const {
    if (values_.empty())
        return none;
    if (chart_.stale())
        chart_.rebin();
    std::size_t total = 0;
    for (std::uint32_t n : counts_)
        total += n;
    if (total == 0)
        return none;
    const double wanted = std::clamp(q, 0.0, 1.0) * static_cast<double>(total);
    double below = 0;
    for (std::size_t b = 0; b < counts_.size(); ++b) {
        const double n = counts_[b];
        if (n > 0 && below + n >= wanted)
            return chart_.start_ + (static_cast<double>(b) + (wanted - below) / n) * chart_.width_;
        below += n;
    }
    return chart_.start_ + static_cast<double>(counts_.size()) * chart_.width_;
}

void HistogramSeries::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    shown_.animateTo(chart_.scheduler(), visible ? 1.0f : 0.0f, chart_.transition());
    chart_.changed();
}

// ---- the chart ----

HistogramChart::HistogramChart() : HistogramChart(Props{}) {}

HistogramChart::HistogramChart(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    progress_.set(1);
}

HistogramChart::~HistogramChart() = default;

HistogramSeries& HistogramChart::addSeries(HistogramSeries::Props props) {
    series_.push_back(std::unique_ptr<HistogramSeries>(new HistogramSeries(*this, std::move(props))));
    rebin_ = true;
    changed();
    return *series_.back();
}

void HistogramChart::changed() {
    ++version_;
    invalidatePaint();
}

void HistogramChart::setScale(Scale scale) {
    props_.scale = scale;
    changed();
}

void HistogramChart::setBins(int bins) {
    props_.bins = std::max(0, bins);
    rebin_ = true;
    changed();
}

void HistogramChart::setRange(std::optional<ChartRange> range) {
    props_.range = range;
    rebin_ = true;
    changed();
}

void HistogramChart::setQuantiles(std::vector<double> quantiles) {
    props_.quantiles = std::move(quantiles);
    invalidatePaint();
}

void HistogramChart::setCumulative(bool cumulative) {
    props_.cumulative = cumulative;
    invalidatePaint();
}

void HistogramChart::setValueFormatter(std::function<std::string(double, double)> format) {
    format_ = std::move(format);
    xAxis_.format = format_;
    invalidatePaint();
}

Color HistogramChart::colorOf(std::size_t index) const {
    const HistogramSeries& s = *series_[index];
    return s.props_.color ? *s.props_.color : seriesColor(index);
}

// ---- its bins ----

bool HistogramChart::stale() const {
    if (rebin_ || bins_ == 0)
        return true;
    if (props_.bins > 0 || props_.range)
        return false;
    // many more past them than were when they were found
    if (clipped_) {
        std::size_t past = 0, all = 0;
        for (const auto& s : series_) {
            past += s->outside_;
            all += s->values_.size();
        }
        if (static_cast<double>(past) > std::max(16.0, 6 * (1 - static_cast<double>(props_.clip)) * static_cast<double>(all)))
            return true;
    }
    // many more or fewer values than when they were found (bins of another width wanted), or its values in a third of them
    std::size_t n = 0;
    for (const auto& s : series_)
        n += s->values_.size();
    if (n > binnedAt_ * 2 + 16 || n * 2 + 16 < binnedAt_)
        return true;
    if (bins_ >= 9 && !series_.empty()) {
        std::size_t low = bins_, high = 0;
        for (const auto& s : series_) {
            for (std::size_t b = 0; b < s->counts_.size(); ++b) {
                if (s->counts_[b] > 0) {
                    low = std::min(low, b);
                    high = std::max(high, b);
                }
            }
        }
        if (low <= high && (high - low + 1) * 3 < bins_)
            return true;
    }
    return false;
}

void HistogramChart::rebin() const {
    double low = std::numeric_limits<double>::infinity(), high = -low;
    std::size_t n = 0;
    for (const auto& s : series_) {
        for (double v : s->values_) {
            low = std::min(low, v);
            high = std::max(high, v);
        }
        n += s->values_.size();
    }
    clipped_ = false;
    if (props_.range) {
        low = props_.range->low;
        high = props_.range->high;
    } else if (props_.clip < 1 && n >= 50) {
        // from a quantile of them to another (of a sample: a few tens of thousands of them), the rest in the end bins
        std::vector<double> sample;
        const std::size_t stride = std::max<std::size_t>(1, n / 65536);
        std::size_t i = 0;
        for (const auto& s : series_)
            for (double v : s->values_)
                if (i++ % stride == 0)
                    sample.push_back(v);
        const auto quantile = [&](double q) {
            const auto k = static_cast<std::size_t>(std::clamp(q, 0.0, 1.0) * static_cast<double>(sample.size() - 1));
            std::nth_element(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(k), sample.end());
            return sample[k];
        };
        const double clip = std::clamp(static_cast<double>(props_.clip), 0.5, 1.0);
        const double lowAt = quantile(1 - clip), highAt = quantile(clip);
        if (highAt > lowAt && (lowAt > low || highAt < high)) {
            low = lowAt;
            high = highAt;
            clipped_ = true;
        }
    }
    if (!(high >= low)) {
        low = 0;
        high = 1;
    } else if (!(high > low)) {
        low -= 0.5;
        high += 0.5;
    }
    // as many as its values want (twice the cube root of how many: Rice's), as its width has room for (six dips a bin)
    const int wanted = props_.bins > 0 ? props_.bins
                                       : std::clamp(static_cast<int>(std::lround(2 * std::cbrt(static_cast<double>(std::max<std::size_t>(n, 1))))),
                                                    5, std::clamp(static_cast<int>(plotWidth_ / 6), 5, 200));
    if (props_.range && props_.bins > 0) {
        // its own: exactly
        start_ = low;
        width_ = (high - low) / props_.bins;
        bins_ = static_cast<std::size_t>(props_.bins);
    } else {
        // a round width, its bins from a multiple of it
        const double step = niceTicks({low, high}, wanted).step;
        width_ = step > 0 ? step : 1;
        start_ = std::floor(low / width_) * width_;
        bins_ = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil((high - start_) / width_ - 1e-9)));
        if (start_ + static_cast<double>(bins_) * width_ <= high)
            ++bins_;
    }
    rebin_ = false;
    binnedAt_ = n;
    for (const auto& s : series_) {
        s->counts_.assign(bins_, 0);
        s->outside_ = 0;
        for (double v : s->values_)
            s->count(v, 1);
    }
}

double HistogramChart::binStart() const {
    if (stale())
        rebin();
    return start_;
}

double HistogramChart::binWidth() const {
    if (stale())
        rebin();
    return width_;
}

std::size_t HistogramChart::binCount() const {
    if (stale())
        rebin();
    return bins_;
}

std::optional<std::size_t> HistogramChart::binOf(double value) const {
    if (stale())
        rebin();
    const double at = std::floor((value - start_) / width_);
    if (!(at >= 0 && at < static_cast<double>(bins_)))
        return std::nullopt;
    return static_cast<std::size_t>(at);
}

double HistogramChart::height(const HistogramSeries& series, std::size_t bin) const {
    const double n = bin < series.counts_.size() ? series.counts_[bin] : 0;
    if (props_.scale == Scale::Percent)
        return series.values_.empty() ? 0 : n / static_cast<double>(series.values_.size()) * 100;
    return n;
}

// ---- painting ----

void HistogramChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    plotWidth_ = std::max(50.0f, area.width - 60);
    if (stale())
        rebin();

    // its heights now: eased from what they were to what they are, when its bins are the same
    const std::size_t count = series_.size();
    std::vector<double> target(count * bins_);
    double highest = 0;
    for (std::size_t k = 0; k < count; ++k) {
        for (std::size_t b = 0; b < bins_; ++b) {
            target[k * bins_ + b] = height(*series_[k], b);
            if (series_[k]->visible_)
                highest = std::max(highest, target[k * bins_ + b]);
        }
    }
    Rect plot = area;
    const bool marks = !props_.quantiles.empty() || props_.mean;
    // above it: room for its quantiles' labels; under it, the values'
    plot.y += marks ? line + 6 : line / 2;
    plot.height = std::max(0.0f, area.bottom() - plot.y - line - 6);
    const double top = niceRange({0, std::max(highest, props_.scale == Scale::Percent ? 1.0 : 1.0)}, std::max(2, static_cast<int>(plot.height / 34))).high;
    const bool sameBins = easedBins_ == bins_ && easedStart_ == start_ && easedWidth_ == width_ && from_.size() == target.size();
    if (version_ != easedVersion_ || !sameBins) {
        const float t = progress_.value();
        if (!sameBins || transition() <= Duration::zero()) {
            from_ = to_ = target;
            fromTop_ = toTop_ = top;
            progress_.set(1);
        } else {
            for (std::size_t i = 0; i < from_.size(); ++i)
                from_[i] = from_[i] + (to_[i] - from_[i]) * t;
            fromTop_ = fromTop_ + (toTop_ - fromTop_) * t;
            to_ = target;
            toTop_ = top;
            progress_.set(0);
            progress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
        }
        easedVersion_ = version_;
        easedBins_ = bins_;
        easedStart_ = start_;
        easedWidth_ = width_;
    }
    const float t = progress_.value();
    const double shownTop = fromTop_ + (toTop_ - fromTop_) * t;
    const auto heightAt = [&](std::size_t k, std::size_t b) {
        const std::size_t i = k * bins_ + b;
        return from_[i] + (to_[i] - from_[i]) * t;
    };

    // the count axis at the start side; the cumulative one (0 to 100 %) at the end side
    yAxis_.orientation = Orientation::Vertical;
    yAxis_.suffix = props_.scale == Scale::Percent ? "%" : "";
    yAxis_.fit(*this, {0, shownTop}, plot.height);
    const float axisWidth = yAxis_.labelWidth() + 8;
    const float shareWidth = props_.cumulative ? textSize("100%").width + 8 : 0;
    plot.width = std::max(0.0f, plot.width - axisWidth - 6 - shareWidth);
    plot.x += rtl ? shareWidth + 6 : axisWidth;
    plot_ = plot;
    if (plot.width < 8 || plot.height < 8 || bins_ == 0)
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    const ChartRange xr{start_, start_ + static_cast<double>(bins_) * width_};
    xAxis_.orientation = Orientation::Horizontal;
    xAxis_.fit(*this, xr, plot.width);
    yAxis_.paintGrid(p, plot, s.grid);
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);
    yAxis_.paintLabels(context, *this, plot, rtl ? Axis::Side::Right : Axis::Side::Left, s.text, area);
    xAxis_.paintLabels(context, *this, plot, Axis::Side::Bottom, s.text, area);
    const auto px = [&](double v) { return static_cast<float>(plot.x + (v - xr.low) / xr.span() * plot.width); };
    const auto py = [&](double v) { return static_cast<float>(plot.bottom() - v / shownTop * plot.height); };

    // the bar under the mouse; another chart's mouse: the bar at its value, a line there
    std::optional<std::size_t> hovered;
    const std::optional<double> shared = sharedX();
    if (mouse_ && plot.contains(*mouse_)) {
        hovered = binOf(xr.low + (mouse_->x - plot.x) / plot.width * xr.span());
    } else if (shared && xr.contains(*shared)) {
        hovered = binOf(*shared);
        p.fillRect({snap(px(*shared), ppd), plot.y, 1.0f / ppd, plot.height}, s.crosshair);
    }
    paintAnnotations(context, plot, xAxis_, yAxis_, false);
    // the bars: each series's (one: raised as the style's bars are; several: translucent, over one another)
    const float binWidth = plot.width / static_cast<float>(bins_);
    const float gap = binWidth >= 6 ? std::min(s.gap, binWidth * 0.15f) : 0.0f;
    const bool alone = std::count_if(series_.begin(), series_.end(), [](const auto& x) { return x->visible_; }) <= 1;
    // coming in (Chart::beginEntrance): its bars one after another out of the axis; the one under the mouse lifted
    if (bins_ > 0)
        beginEntrance();
    const bool coming = entering();
    liftPiece(hovered && alone ? std::optional(std::to_string(*hovered)) : std::nullopt);
    const bool lifts = lifting();
    // (solids on their way: not cut by the plot)
    const bool clipped = !((coming || lifts) && solidPieces());
    if (clipped)
        p.pushClip(plot);
    for (std::size_t k = 0; k < count; ++k) {
        const float alpha = series_[k]->shown_.value();
        if (alpha <= 0.002f)
            continue;
        const Color base = colorOf(k);
        for (std::size_t b = 0; b < bins_; ++b) {
            const double h = heightAt(k, b);
            if (h <= 0)
                continue;
            Color color = hovered && *hovered == b ? mix(base, Color{1, 1, 1, base.a}, 0.18f) : base;
            color.a *= alpha * (alone ? 1.0f : 0.55f);
            const float x0 = px(start_ + static_cast<double>(b) * width_) + gap / 2;
            const float y = py(h);
            const Rect r{x0, y, binWidth - gap, plot.bottom() - y};
            const paint::Painter::Mark mark = coming || lifts ? p.markHere() : paint::Painter::Mark{};
            if (alone && r.width >= 5 && r.height >= 3)
                paintRaised(p, r, std::min(s.barRadius, r.width / 4), color, {0, 0, 0, 0}, s.barBevel);
            else
                p.fillRect(r, color);
            if (lifts)
                placeLifted(p, mark, pieceLift(std::to_string(b)), r);
            if (coming)
                placeArriving(p, mark, arrived(bins_ > 1 ? static_cast<float>(b) / static_cast<float>(bins_ - 1) : 0.0f), r,
                              Point{r.center().x, plot.bottom()}, {0, -1});
        }
    }
    if (clipped)
        p.popClip();
    paintAnnotations(context, plot, xAxis_, yAxis_, true);

    // the first series that shows: its share at or under each value, a line; lines at its quantiles and its mean
    const HistogramSeries* first = nullptr;
    std::size_t firstIndex = 0;
    for (std::size_t k = 0; k < count && !first; ++k)
        if (series_[k]->visible_ && !series_[k]->values_.empty()) {
            first = series_[k].get();
            firstIndex = k;
        }
    if (first && props_.cumulative) {
        std::size_t total = 0;
        for (std::uint32_t n : first->counts_)
            total += n;
        if (total > 0) {
            points_.clear();
            points_.push_back({px(start_), plot.bottom()});
            double sum = 0;
            for (std::size_t b = 0; b < bins_; ++b) {
                sum += first->counts_[b];
                points_.push_back({px(start_ + static_cast<double>(b + 1) * width_),
                                   static_cast<float>(plot.bottom() - sum / static_cast<double>(total) * plot.height)});
            }
            const Color color = lit(colorOf(firstIndex), context.ui.theme.isDark() ? 0.35f : -0.35f);
            p.drawPolyline(points_, s.lineWidth, color);
            // its axis: 0 to 100 % at the end side
            const int every = plot.height >= line * 5.5f ? 25 : plot.height >= line * 3 ? 50 : 100;
            for (int pct = 0; pct <= 100; pct += every) {
                const float y = static_cast<float>(plot.bottom() - pct / 100.0 * plot.height);
                drawText(context, std::to_string(pct) + "%", {rtl ? plot.x - 6 : plot.right() + 6, y}, s.text, 0, {rtl ? 1.0f : 0.0f, 0.5f});
            }
        }
    }
    if (first && marks) {
        struct Mark {
            double value;
            std::string name;
        };
        std::vector<Mark> list;
        for (double q : props_.quantiles)
            list.push_back({first->quantile(q), "p" + formatNumber(q * 100, q * 100 == std::floor(q * 100) ? 0 : 1)});
        if (props_.mean)
            list.push_back({first->mean(), translate(LocalizedText("Mean"))});
        std::sort(list.begin(), list.end(), [](const Mark& a, const Mark& b) { return a.value < b.value; });
        const int decimals = decimalsFor(width_) + 1;
        float lastRight = -1e9f;
        for (const Mark& m : list) {
            if (std::isnan(m.value))
                continue;
            const float x = px(m.value);
            if (x < plot.x - 0.5f || x > plot.right() + 0.5f)
                continue;
            p.fillRect({snap(x, ppd), plot.y, 1.0f / ppd, plot.height}, withAlpha(s.title, s.title.a * 0.55f));
            // its name and value above the plot, when it does not run into the one before
            const std::string label = m.name + " " + (format_ ? format_(m.value, width_) : formatNumber(m.value, decimals));
            const float w = textSize(label).width;
            const float lx = std::clamp(x - w / 2, area.x, area.right() - w);
            if (lx < lastRight + 8)
                continue;
            drawText(context, label, {lx, plot.y - 4}, s.text, 0, {0, 1});
            lastRight = lx + w;
        }
    }

    // the bin under the mouse: its range, each series's count (and share)
    if (hovered && mouse_ && plot.contains(*mouse_)) {
        const double a = start_ + static_cast<double>(*hovered) * width_, b = a + width_;
        const int decimals = decimalsFor(width_);
        const auto value = [&](double v) { return format_ ? format_(v, width_) : formatNumber(v, decimals); };
        std::vector<ToolTipRow> rows;
        for (std::size_t k = 0; k < count; ++k) {
            const HistogramSeries& series = *series_[k];
            if (!series.visible_)
                continue;
            const std::uint32_t n = *hovered < series.counts_.size() ? series.counts_[*hovered] : 0;
            const double share = series.values_.empty() ? 0 : n * 100.0 / static_cast<double>(series.values_.size());
            rows.push_back({colorOf(k), series.props_.name.source.empty() ? std::string() : translate(series.props_.name),
                            formatNumber(n, 0) + "  " + formatNumber(share, share < 10 ? 1 : 0) + "%"});
        }
        // (its end bins: open, when they hold the values past them)
        std::string title = value(a) + " – " + value(b);
        if (clipped_ && *hovered + 1 == bins_)
            title = "≥ " + value(a);
        else if (clipped_ && *hovered == 0)
            title = "< " + value(b);
        paintToolTip(context, *mouse_, title, rows);
    }
}

std::vector<Chart::LegendItem> HistogramChart::legendItems() const {
    std::vector<LegendItem> items;
    for (std::size_t i = 0; i < series_.size(); ++i)
        if (!series_[i]->props_.name.source.empty())
            items.push_back({translate(series_[i]->props_.name), colorOf(i), series_[i]->visible_});
    return items;
}

void HistogramChart::legendItemClicked(std::size_t index) {
    // (its items: the named series, in order)
    for (const auto& s : series_) {
        if (s->props_.name.source.empty())
            continue;
        if (index-- == 0) {
            s->setVisible(!s->visible_);
            return;
        }
    }
}

void HistogramChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    if (event.type == MouseEventType::Move) {
        mouse_ = event.position;
        // the value under it, for the charts that share its crosshair
        if (bins_ > 0 && plot_.width > 0 && plot_.contains(event.position))
            shareCursor(start_ + (event.position.x - plot_.x) / plot_.width * static_cast<double>(bins_) * width_);
        else
            shareCursor(std::nullopt);
        invalidatePaint();
    }
}

void HistogramChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        shareCursor(std::nullopt);
        invalidatePaint();
    }
}

} // namespace glw
