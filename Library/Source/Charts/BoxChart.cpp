// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/BoxChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace glw {

namespace {

std::array<double, 6> numbers(const BoxSummary& s) {
    return {s.low, s.q1, s.median, s.q3, s.high, s.mean};
}

} // namespace

BoxChart::BoxChart() : BoxChart(Props{}) {}

BoxChart::BoxChart(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setLegendVisible(false);
    setCategories(props_.categories);
    progress_.set(1);
}

BoxChart::~BoxChart() = default;

void BoxChart::setCategories(std::vector<LocalizedText> categories) {
    categories_.resize(categories.size());
    for (std::size_t i = 0; i < categories.size(); ++i)
        categories_[i].name = std::move(categories[i]);
    invalidatePaint();
}

void BoxChart::changed(Category& category) {
    category.own = false;
    category.dirty = true;
    while (props_.capacity > 0 && category.values.size() > props_.capacity)
        category.values.pop_front();
    invalidatePaint();
}

void BoxChart::setValues(std::size_t category, std::span<const double> values) {
    if (category >= categories_.size())
        categories_.resize(category + 1);
    Category& c = categories_[category];
    c.values.clear();
    for (double v : values)
        if (!std::isnan(v))
            c.values.push_back(v);
    changed(c);
}

void BoxChart::addValues(std::size_t category, std::span<const double> values) {
    if (category >= categories_.size())
        categories_.resize(category + 1);
    Category& c = categories_[category];
    for (double v : values)
        if (!std::isnan(v))
            c.values.push_back(v);
    changed(c);
}

void BoxChart::setSummary(std::size_t category, BoxSummary summary) {
    if (category >= categories_.size())
        categories_.resize(category + 1);
    Category& c = categories_[category];
    c.values.clear();
    c.own = true;
    c.dirty = false;
    c.summary = std::move(summary);
    invalidatePaint();
}

void BoxChart::setWhiskers(Whiskers whiskers) {
    props_.whiskers = whiskers;
    for (Category& c : categories_)
        if (!c.own)
            c.dirty = true;
    invalidatePaint();
}

void BoxChart::setValueFormatter(std::function<std::string(double, double)> format) {
    format_ = std::move(format);
    axis_.format = format_;
    invalidatePaint();
}

BoxSummary BoxChart::summarize(std::span<double> values, Whiskers whiskers) {
    BoxSummary s;
    s.count = values.size();
    if (values.empty()) {
        s.low = s.q1 = s.median = s.q3 = s.high = s.mean = std::numeric_limits<double>::quiet_NaN();
        return s;
    }
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    // a quantile: linearly between the values around it
    const auto at = [&](double p) {
        const double i = p * static_cast<double>(n - 1);
        const auto k = static_cast<std::size_t>(i);
        return k + 1 < n ? values[k] + (values[k + 1] - values[k]) * (i - static_cast<double>(k)) : values[n - 1];
    };
    s.q1 = at(0.25);
    s.median = at(0.5);
    s.q3 = at(0.75);
    double sum = 0;
    for (double v : values)
        sum += v;
    s.mean = sum / static_cast<double>(n);
    if (whiskers == Whiskers::Range) {
        s.low = values.front();
        s.high = values.back();
        return s;
    }
    // Tukey's: to the last values within 1.5 times the box's height of it, those past them outliers
    const double reach = 1.5 * (s.q3 - s.q1);
    const double lowFence = s.q1 - reach, highFence = s.q3 + reach;
    s.low = *std::lower_bound(values.begin(), values.end(), lowFence);
    s.high = *(std::upper_bound(values.begin(), values.end(), highFence) - 1);
    for (double v : values)
        if (v < lowFence || v > highFence)
            s.outliers.push_back(v);
    return s;
}

const BoxSummary& BoxChart::summary(std::size_t category) const {
    const Category& c = categories_[category];
    if (c.dirty) {
        scratch_.assign(c.values.begin(), c.values.end());
        c.summary = summarize(scratch_, props_.whiskers);
        c.dirty = false;
    }
    return c.summary;
}

Color BoxChart::colorOf(std::size_t index) const {
    return props_.color ? *props_.color : seriesColor(index);
}

// ---- painting ----

void BoxChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    const std::size_t n = categories_.size();
    boxes_.assign(n, Rect{});
    if (n == 0)
        return;

    // what each shows: eased from what it showed when any changed; the axis fitted to its whiskers and outliers
    bool moved = false;
    double low = std::numeric_limits<double>::infinity(), high = -low;
    std::vector<std::array<double, 6>> targets(n);
    for (std::size_t i = 0; i < n; ++i) {
        const BoxSummary& b = summary(i);
        targets[i] = numbers(b);
        if (b.count == 0 && !categories_[i].own)
            continue;
        low = std::min(low, b.low);
        high = std::max(high, b.high);
        if (props_.outliers)
            for (double v : b.outliers) {
                low = std::min(low, v);
                high = std::max(high, v);
            }
        moved = moved || !categories_[i].shown || targets[i] != categories_[i].to;
    }
    if (!(high >= low)) {
        low = 0;
        high = 1;
    }
    if (props_.includeZero) {
        low = std::min(low, 0.0);
        high = std::max(high, 0.0);
    }
    if (!(high > low)) {
        low -= 1;
        high += 1;
    }
    Rect plot = area;
    plot.y += line / 2;
    plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
    const ChartRange range = niceRange({low, high}, std::max(2, static_cast<int>(plot.height / 34)));
    const float t0 = progress_.value();
    if (moved || !hasRange_ || range != toRange_) {
        const bool ease = hasRange_ && transition() > Duration::zero();
        for (std::size_t i = 0; i < n; ++i) {
            Category& c = categories_[i];
            if (ease && c.shown)
                for (std::size_t k = 0; k < 6; ++k)
                    c.from[k] = c.from[k] + (c.to[k] - c.from[k]) * t0;
            else
                c.from = targets[i];
            c.to = targets[i];
            c.shown = true;
        }
        fromRange_ = ease ? ChartRange{fromRange_.low + (toRange_.low - fromRange_.low) * t0, fromRange_.high + (toRange_.high - fromRange_.high) * t0}
                          : range;
        toRange_ = range;
        hasRange_ = true;
        if (ease) {
            progress_.set(0);
            progress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
        } else {
            progress_.set(1);
        }
    }
    const float t = progress_.value();
    const ChartRange yv{fromRange_.low + (toRange_.low - fromRange_.low) * t, fromRange_.high + (toRange_.high - fromRange_.high) * t};

    // the value axis at the start side; the categories' names under their boxes, as many as fit
    axis_.orientation = Orientation::Vertical;
    axis_.fit(*this, yv, plot.height);
    const float axisWidth = axis_.labelWidth() + 8;
    plot.width = std::max(0.0f, plot.width - axisWidth - 6);
    if (!rtl)
        plot.x += axisWidth;
    plot_ = plot;
    if (plot.width < 8 || plot.height < 8 || !(yv.span() > 0))
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    axis_.paintGrid(p, plot, s.grid);
    axis_.paintLabels(context, *this, plot, rtl ? Axis::Side::Right : Axis::Side::Left, s.text, area);
    p.fillRect({plot.x, snap(plot.bottom(), ppd), plot.width, 1.0f / ppd}, s.axis);
    const auto py = [&](double v) { return static_cast<float>(plot.bottom() - (v - yv.low) / yv.span() * plot.height); };
    const float slot = plot.width / static_cast<float>(n);
    float widest = 0;
    for (const Category& c : categories_)
        widest = std::max(widest, textSize(translate(c.name)).width);
    const auto every = static_cast<std::size_t>(std::max(1.0f, std::ceil((widest + 10) / slot)));
    // a category's place: in reading order (from the right in right to left)
    const auto centerOf = [&](std::size_t i) {
        const float k = rtl ? static_cast<float>(n - 1 - i) : static_cast<float>(i);
        return plot.x + (k + 0.5f) * slot;
    };

    std::optional<std::size_t> hovered;
    if (mouse_ && plot.contains(*mouse_)) {
        const auto k = static_cast<std::size_t>(std::clamp((mouse_->x - plot.x) / slot, 0.0f, static_cast<float>(n) - 0.001f));
        hovered = rtl ? n - 1 - k : k;
    }
    const float hair = std::max(1.0f / ppd, 1.2f);
    for (std::size_t i = 0; i < n; ++i) {
        const Category& c = categories_[i];
        const float cx = centerOf(i);
        if (i % every == 0)
            drawText(context, translate(c.name), {cx, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
        if (summary(i).count == 0 && !c.own)
            continue;
        std::array<double, 6> v;
        for (std::size_t k = 0; k < 6; ++k)
            v[k] = c.from[k] + (c.to[k] - c.from[k]) * t;
        const auto [whiskerLow, q1, median, q3, whiskerHigh, mean] = v;
        const Color base = colorOf(i);
        const Color color = hovered && *hovered == i ? mix(base, Color{1, 1, 1, base.a}, 0.18f) : base;
        const float width = std::clamp(slot * 0.5f, 6.0f, 64.0f);
        // the whiskers: a line out of the box each way, a cap at its end
        const Color whisker = withAlpha(s.title, s.title.a * 0.7f);
        p.fillRect({cx - hair / 2, py(whiskerHigh), hair, py(q3) - py(whiskerHigh)}, whisker);
        p.fillRect({cx - hair / 2, py(q1), hair, py(whiskerLow) - py(q1)}, whisker);
        p.fillRect({cx - width / 4, py(whiskerHigh) - hair / 2, width / 2, hair}, whisker);
        p.fillRect({cx - width / 4, py(whiskerLow) - hair / 2, width / 2, hair}, whisker);
        // the box, raised as the style's bars are; its median a line across it; its mean a mark
        const Rect box{cx - width / 2, py(q3), width, std::max(hair, py(q1) - py(q3))};
        boxes_[i] = box;
        if (box.height >= 3)
            paintRaised(p, box, std::min(s.barRadius, width / 4), color, {0, 0, 0, 0}, s.barBevel);
        else
            p.fillRect(box, color);
        const Color across = textOn(color);
        p.fillRect({box.x + 1, py(median) - 1, box.width - 2, 2}, withAlpha(across, 0.9f));
        if (props_.mean && !std::isnan(mean)) {
            p.fillCircle({cx, py(mean)}, 3.4f, withAlpha(across, 0.9f));
            p.fillCircle({cx, py(mean)}, 2.0f, color);
        }
        // the outliers: dots where they are
        if (props_.outliers) {
            const Color dot = withAlpha(base, base.a * 0.75f);
            for (double o : summary(i).outliers) {
                const float y = py(o);
                if (y >= plot.y - 3 && y <= plot.bottom() + 3)
                    p.fillCircle({cx, y}, 2.4f, dot);
            }
        }
    }

    // the box under the mouse: its numbers
    if (hovered && (summary(*hovered).count > 0 || categories_[*hovered].own)) {
        const BoxSummary& b = summary(*hovered);
        const int decimals = axis_.decimals() + 1;
        const auto value = [&](double v) { return format_ ? format_(v, axis_.step()) : formatNumber(v, decimals); };
        std::vector<ToolTipRow> rows = {
            {std::nullopt, translate(LocalizedText("High")), value(b.high)},
            {std::nullopt, translate(LocalizedText("Q3")), value(b.q3)},
            {colorOf(*hovered), translate(LocalizedText("Median")), value(b.median)},
            {std::nullopt, translate(LocalizedText("Q1")), value(b.q1)},
            {std::nullopt, translate(LocalizedText("Low")), value(b.low)},
        };
        if (props_.mean && !std::isnan(b.mean))
            rows.push_back({std::nullopt, translate(LocalizedText("Mean")), value(b.mean)});
        if (b.count > 0)
            rows.push_back({std::nullopt, translate(LocalizedText("Count")), formatNumber(static_cast<double>(b.count), 0)});
        if (props_.outliers && !b.outliers.empty())
            rows.push_back({std::nullopt, translate(LocalizedText("Outliers")), formatNumber(static_cast<double>(b.outliers.size()), 0)});
        paintToolTip(context, *mouse_, translate(categories_[*hovered].name), rows);
    }
}

void BoxChart::onMouse(MouseEvent& event) {
    if (event.type == MouseEventType::Move) {
        mouse_ = event.position;
        invalidatePaint();
    }
}

void BoxChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

} // namespace glw
