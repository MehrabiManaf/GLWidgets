// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/BarChart.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;
constexpr double band = 0.72;           // of a category's slot: its bars

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

double number(double v) {
    return std::isfinite(v) ? v : 0.0;
}

} // namespace

BarChart::BarChart(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), viewProgress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    window_ = static_cast<double>(props_.window);
    mode_ = window_ > 0 ? Mode::Window : Mode::Fit;
    progress_.set(1);
    viewProgress_.set(1);
}

BarChart::~BarChart() = default;

// ---- data ----

std::size_t BarChart::addSeries(LocalizedText name, std::optional<Color> color) {
    Series s;
    s.name = std::move(name);
    s.color = color;
    s.values.assign(categories_.size(), 0.0);
    s.from.assign(categories_.size(), 0.0);
    s.to.assign(categories_.size(), 0.0);
    s.shown = std::make_unique<Animation>([this] { invalidatePaint(); });
    s.shown->set(1);
    series_.push_back(std::move(s));
    changed();
    return series_.size() - 1;
}

void BarChart::setSeriesVisible(std::size_t series, bool visible) {
    if (series >= series_.size() || series_[series].visible == visible)
        return;
    series_[series].visible = visible;
    series_[series].shown->animateTo(scheduler(), visible ? 1.0f : 0.0f, transition());
    changed();
}

Color BarChart::seriesColorOf(std::size_t series) const {
    return series_[series].color ? *series_[series].color : seriesColor(series);
}

void BarChart::setCategories(std::vector<LocalizedText> categories) {
    categories_.assign(std::make_move_iterator(categories.begin()), std::make_move_iterator(categories.end()));
    dropped_ = 0;
    for (Series& s : series_) {
        s.values.resize(categories_.size(), 0.0);
        s.from.resize(categories_.size(), 0.0);
        s.to.resize(categories_.size(), 0.0);
    }
    changed();
}

void BarChart::appendCategory(LocalizedText category, std::span<const double> values) {
    categories_.push_back(std::move(category));
    for (std::size_t i = 0; i < series_.size(); ++i) {
        Series& s = series_[i];
        s.values.push_back(i < values.size() ? values[i] : 0.0);
        s.from.push_back(0);        // a new bar grows out of the axis
        s.to.push_back(0);
    }
    if (props_.capacity > 0 && categories_.size() > props_.capacity) {
        categories_.pop_front();
        for (Series& s : series_) {
            s.values.pop_front();
            s.from.pop_front();
            s.to.pop_front();
        }
        ++dropped_;
    }
    changed();
}

void BarChart::setValue(std::size_t series, std::size_t category, double value) {
    if (series < series_.size() && category < categories_.size()) {
        series_[series].values[category] = value;
        changed();
    }
}

void BarChart::setValues(std::size_t series, std::span<const double> values) {
    if (series >= series_.size())
        return;
    for (std::size_t c = 0; c < std::min(values.size(), categories_.size()); ++c)
        series_[series].values[c] = values[c];
    changed();
}

double BarChart::value(std::size_t series, std::size_t category) const {
    return series < series_.size() && category < categories_.size() ? series_[series].values[category] : 0.0;
}

void BarChart::setLayout(BarLayout layout) {
    props_.layout = layout;
    changed();
}

void BarChart::setOrientation(Orientation orientation) {
    props_.orientation = orientation;
    invalidatePaint();
}

void BarChart::setWindow(std::size_t window) {
    props_.window = window;
    window_ = static_cast<double>(window);
    mode_ = window > 0 ? Mode::Window : Mode::Fit;
    invalidatePaint();
}

void BarChart::setFormatter(std::function<std::string(double, double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string BarChart::format(double value, double step) const {
    if (format_)
        return format_(value, step);
    if (step > 0)
        return formatNumber(value, ticks_.decimals, step) + (props_.layout == BarLayout::Percent ? "%" : "");
    return formatNumber(value, value == std::floor(value) ? 0 : 2);
}

void BarChart::changed() {
    dirty_ = true;
    invalidatePaint();
}

double BarChart::target(const Series& s, std::size_t category) const {
    return s.visible ? number(s.values[category]) : 0.0;
}

double BarChart::shownValue(const Series& s, std::size_t category) const {
    return s.from[category] + (s.to[category] - s.from[category]) * progress_.value();
}

void BarChart::settle() {
    if (!dirty_)
        return;
    dirty_ = false;
    for (Series& s : series_) {
        for (std::size_t c = 0; c < categories_.size(); ++c) {
            const double now = shownValue(s, c);
            s.from[c] = now;
            s.to[c] = target(s, c);
        }
    }
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, transition());
}

// ---- the view ----

ChartRange BarChart::fitCategories() const {
    const double begin = static_cast<double>(dropped_), end = begin + static_cast<double>(categories_.size());
    switch (mode_) {
    case Mode::Manual:
        return user_;
    case Mode::Window:
        return {end - window_, end};
    case Mode::Fit:
        break;
    }
    return categories_.empty() ? ChartRange{0, 1} : ChartRange{begin, end};
}

ChartRange BarChart::fitValues(ChartRange categories) const {
    const auto n = static_cast<long long>(categories_.size());
    const long long first = std::clamp(static_cast<long long>(std::floor(categories.low)) - static_cast<long long>(dropped_), 0LL, n);
    const long long last = std::clamp(static_cast<long long>(std::ceil(categories.high)) - static_cast<long long>(dropped_), 0LL, n);
    double low = 0, high = 0;
    bool negative = false;
    for (long long c = first; c < last; ++c) {
        const auto i = static_cast<std::size_t>(c);
        double positive = 0, below = 0;
        for (const Series& s : series_) {
            const double v = target(s, i);
            if (props_.layout == BarLayout::Grouped) {
                high = std::max(high, v);
                low = std::min(low, v);
            } else if (v >= 0) {
                positive += v;
            } else {
                below += v;
            }
        }
        negative = negative || below < 0;
        if (props_.layout == BarLayout::Stacked) {
            high = std::max(high, positive);
            low = std::min(low, below);
        }
    }
    if (props_.layout == BarLayout::Percent)
        return {negative ? -100.0 : 0.0, 100.0};
    if (high <= low)
        high = low + 1;
    const double pad = (high - low) * 0.04;
    if (high > 0)
        high += pad;
    if (low < 0)
        low -= pad;
    return niceRange({low, high}, valueTickCount_);
}

void BarChart::updateView() {
    const ChartRange tc = fitCategories(), tv = fitValues(tc);
    const Duration duration = transition(quick_);
    if (!hasView_ || immediate_ || duration <= Duration::zero()) {
        view_ = {tc, tc, tv, tv};
        hasView_ = true;
        viewProgress_.set(1);
        immediate_ = quick_ = false;
        return;
    }
    if (near(tc, view_.toC) && near(tv, view_.toV))
        return;
    view_ = {absoluteView(), tc, valueView(), tv};
    viewProgress_.set(0);
    viewProgress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange BarChart::absoluteView() const {
    return hasView_ ? between(view_.fromC, view_.toC, viewProgress_.value()) : fitCategories();
}

ChartRange BarChart::categoryView() const {
    const ChartRange a = absoluteView();
    const auto d = static_cast<double>(dropped_);
    return {a.low - d, a.high - d};
}

ChartRange BarChart::valueView() const {
    return hasView_ ? between(view_.fromV, view_.toV, viewProgress_.value()) : fitValues(fitCategories());
}

void BarChart::setCategoryView(double first, double count) {
    mode_ = Mode::Manual;
    user_ = {static_cast<double>(dropped_) + first, static_cast<double>(dropped_) + first + std::max(count, 1e-3)};
    invalidatePaint();
}

void BarChart::resetView() {
    window_ = static_cast<double>(props_.window);
    mode_ = window_ > 0 ? Mode::Window : Mode::Fit;
    invalidatePaint();
}

std::optional<std::pair<std::size_t, std::size_t>> BarChart::barAt(Point at) const {
    for (const Bar& bar : bars_)
        if (bar.rect.inflated(0.5f).contains(at))
            return std::pair{bar.series, bar.category};
    return std::nullopt;
}

// ---- painting ----

void BarChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    settle();
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const float hair = 1.0f / ppd;
    const bool rtl = rightToLeft();
    const bool standing = props_.orientation == Orientation::Vertical;
    const float line = textSize("0").height;

    Rect plot = area;
    if (standing) {
        plot.y += line / 2;
        plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
        valueTickCount_ = std::max(2, static_cast<int>(plot.height / 34));
    } else {
        plot.height = std::max(0.0f, plot.height - line - 6);
        valueTickCount_ = std::max(2, static_cast<int>(plot.width / 100));
    }
    updateView();
    const ChartRange cv = absoluteView(), vv = valueView();
    if (!(cv.span() > 0) || !(vv.span() > 0))
        return;
    ticks_ = niceTicks(vv, valueTickCount_);
    std::vector<std::string> valueLabels;
    float valueWidth = 0;
    for (double v : ticks_.values) {
        valueLabels.push_back(format(v, ticks_.step));
        valueWidth = std::max(valueWidth, textSize(valueLabels.back()).width);
    }
    // the categories that show (a part of one too)
    const auto begin = static_cast<long long>(dropped_);
    const auto end = begin + static_cast<long long>(categories_.size());
    const long long first = std::clamp(static_cast<long long>(std::floor(cv.low)), begin, end);
    const long long last = std::clamp(static_cast<long long>(std::ceil(cv.high)), begin, end);
    std::vector<std::string> names;
    float widestName = 0;
    for (long long c = first; c < last; ++c) {
        names.push_back(translate(categories_[static_cast<std::size_t>(c - begin)]));
        widestName = std::max(widestName, textSize(names.back()).width);
    }
    if (standing) {
        const float axis = valueWidth + 8;
        plot.width = std::max(0.0f, plot.width - axis - 6);
        plot.x += rtl ? 6 : axis;
    } else {
        // the categories' names at the start side, the values under
        const float side = std::min(widestName + 10, area.width * 0.35f);
        plot.width = std::max(0.0f, plot.width - side - valueWidth / 2 - 4);
        plot.x += rtl ? valueWidth / 2 + 4 : side;
    }
    plot_ = plot;
    if (plot.width < 4 || plot.height < 4)
        return;

    // categories along, values across
    const auto along = [&](double c) {
        const auto f = static_cast<float>((c - cv.low) / cv.span());
        return standing ? (rtl ? plot.right() - f * plot.width : plot.x + f * plot.width) : plot.y + f * plot.height;
    };
    const auto across = [&](double v) {
        const auto g = static_cast<float>((v - vv.low) / vv.span());
        return standing ? plot.bottom() - g * plot.height : (rtl ? plot.right() - g * plot.width : plot.x + g * plot.width);
    };
    const auto rectOf = [&](double c0, double c1, double v0, double v1) {
        const float a0 = along(c0), a1 = along(c1), b0 = across(v0), b1 = across(v1);
        return standing ? Rect{std::min(a0, a1), std::min(b0, b1), std::abs(a1 - a0), std::abs(b1 - b0)}
                        : Rect{std::min(b0, b1), std::min(a0, a1), std::abs(b1 - b0), std::abs(a1 - a0)};
    };

    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);
    // the grid at the values' ticks, the axis at 0
    for (std::size_t i = 0; i < ticks_.values.size(); ++i) {
        const float at = snap(across(ticks_.values[i]), ppd);
        if (standing) {
            if (at < plot.y - 0.5f || at > plot.bottom() + 0.5f)
                continue;
            p.fillRect({plot.x, at, plot.width, hair}, s.grid);
            drawText(context, valueLabels[i], {rtl ? plot.right() + 8 : plot.x - 8, at}, s.text, 0, {rtl ? 0.0f : 1.0f, 0.5f});
        } else {
            if (at < plot.x - 0.5f || at > plot.right() + 0.5f)
                continue;
            p.fillRect({at, plot.y, hair, plot.height}, s.grid);
            drawText(context, valueLabels[i], {at, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
        }
    }
    const float zero = snap(across(vv.contains(0) ? 0 : vv.low), ppd);
    if (standing)
        p.fillRect({plot.x, zero, plot.width, hair}, s.axis);
    else
        p.fillRect({zero, plot.y, hair, plot.height}, s.axis);

    // the bars
    const float slot = (standing ? plot.width : plot.height) / static_cast<float>(cv.span());
    const double inset = slot * band >= 3 ? (1 - band) / 2 : 0.0;
    double weights = 0;
    for (const Series& series : series_)
        weights += series.shown->value();
    if (hovered_) {
        const Rect strip = rectOf(static_cast<double>(begin + static_cast<long long>(hovered_->second)),
                                  static_cast<double>(begin + static_cast<long long>(hovered_->second)) + 1, vv.low, vv.high);
        p.fillRect(strip.intersected(plot), s.grid);
    }
    bars_.clear();
    p.pushClip(plot);
    const auto bar = [&](Rect r, std::size_t si, std::size_t ci, float thickness) {
        if (r.width <= 0.2f || r.height <= 0.2f)
            return;
        Color color = seriesColorOf(si);
        if (hovered_ && hovered_->first == si && hovered_->second == ci)
            color = mix(color, Color{1, 1, 1, color.a}, 0.16f);
        const float radius = std::min({s.barRadius, thickness / 2, std::min(r.width, r.height) / 2});
        if (r.width >= 3 && r.height >= 3)
            paintRaised(p, r, radius, color, {0, 0, 0, 0}, s.barBevel);
        else
            p.fillRect(r, color);
        bars_.push_back({r, si, ci});
    };
    const float gap = s.gap;
    for (long long c = first; c < last; ++c) {
        const auto ci = static_cast<std::size_t>(c - begin);
        const auto x = static_cast<double>(c);
        const float thickness = slot * static_cast<float>(1 - 2 * inset);
        if (props_.layout == BarLayout::Grouped) {
            double at = x + inset;
            for (std::size_t si = 0; si < series_.size(); ++si) {
                const double w = weights > 0 ? series_[si].shown->value() / weights * (1 - 2 * inset) : 0.0;
                if (w <= 0)
                    continue;
                Rect r = rectOf(at, at + w, 0, shownValue(series_[si], ci));
                // a hairline between neighbours
                if (standing && r.width > 3) {
                    r.x += 0.5f;
                    r.width -= 1;
                } else if (!standing && r.height > 3) {
                    r.y += 0.5f;
                    r.height -= 1;
                }
                bar(r, si, ci, slot * static_cast<float>(w));
                at += w;
            }
            continue;
        }
        double whole = 0;
        if (props_.layout == BarLayout::Percent)
            for (const Series& series : series_)
                whole += std::abs(shownValue(series, ci));
        double positive = 0, negative = 0;
        for (std::size_t si = 0; si < series_.size(); ++si) {
            double v = shownValue(series_[si], ci);
            if (props_.layout == BarLayout::Percent)
                v = whole > 0 ? v / whole * 100 : 0.0;
            if (v == 0)
                continue;
            const double v0 = v > 0 ? positive : negative + v, v1 = v > 0 ? positive + v : negative;
            (v > 0 ? positive : negative) += v;
            Rect r = rectOf(x + inset, x + 1 - inset, v0, v1);
            // a gap between the parts of a stack: their ends away from 0 in
            const float length = standing ? r.height : r.width;
            const float cut = std::min(gap, length / 3);
            if (standing) {
                if (v > 0)
                    r.y += cut;
                r.height -= cut;
            } else {
                if ((v > 0) == rtl)
                    r.x += cut;
                r.width -= cut;
            }
            bar(r, si, ci, thickness);
        }
    }
    p.popClip();

    // the categories' names: every one, or every second, fifth... as they fit (steady as they scroll)
    const float need = standing ? widestName + 10 : line + 2;
    const long long every = std::max(1LL, static_cast<long long>(std::ceil(need / std::max(slot, 0.01f))));
    for (long long c = first; c < last; ++c) {
        if (c % every != 0)
            continue;
        const std::string& name = names[static_cast<std::size_t>(c - first)];
        const float at = along(static_cast<double>(c) + 0.5);
        if (standing) {
            const float half = textSize(name).width / 2;
            if (at < plot.x || at > plot.right() || at - half < area.x || at + half > area.right())
                continue;
            drawText(context, name, {at, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
        } else {
            if (at < plot.y || at > plot.bottom())
                continue;
            drawText(context, name, {rtl ? plot.right() + 8 : plot.x - 8, at}, s.text, 0, {rtl ? 0.0f : 1.0f, 0.5f});
        }
    }

    // the category under the mouse: its values and its total by it
    if (hovered_ && mouse_ && hovered_->second < categories_.size() && !dragging_) {
        const std::size_t ci = hovered_->second;
        std::vector<ToolTipRow> rows;
        double total = 0;
        for (std::size_t si = 0; si < series_.size(); ++si) {
            if (!series_[si].visible)
                continue;
            const double v = number(series_[si].values[ci]);
            total += v;
            rows.push_back({seriesColorOf(si), translate(series_[si].name), format(v, 0)});
        }
        if (rows.size() > 1 && props_.layout != BarLayout::Grouped)
            rows.push_back({std::nullopt, "\xCE\xA3", format(total, 0)});      // Σ
        paintToolTip(context, *mouse_, translate(categories_[ci]), rows);
    }
}

// ---- the mouse ----

void BarChart::zoom(double factor, double anchor) {
    const ChartRange t = hasView_ ? view_.toC : fitCategories();
    const double count = static_cast<double>(std::max<std::size_t>(categories_.size(), 1));
    if (mode_ == Mode::Window) {
        window_ = std::clamp(window_ * factor, 1.0, std::max(count, window_));
    } else {
        ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
        if (r.span() < 1)
            return;
        if (r.span() >= count) {
            mode_ = Mode::Fit;
        } else {
            // within its categories
            const double begin = static_cast<double>(dropped_), end = begin + count;
            const double shift = r.low < begin ? begin - r.low : r.high > end ? end - r.high : 0.0;
            user_ = {r.low + shift, r.high + shift};
            mode_ = Mode::Manual;
        }
    }
    quick_ = true;
    invalidatePaint();
}

void BarChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    const bool standing = props_.orientation == Orientation::Vertical;
    const bool rtl = rightToLeft();
    // the category (absolute, a fraction) at a point
    const auto categoryAt = [&](Point point) {
        const ChartRange cv = absoluteView();
        const float f = standing ? (rtl ? plot_.right() - point.x : point.x - plot_.x) / std::max(plot_.width, 1.0f)
                                 : (point.y - plot_.y) / std::max(plot_.height, 1.0f);
        return cv.low + f * cv.span();
    };
    switch (event.type) {
    case MouseEventType::Wheel:
        if (!plot_.contains(at))
            return;
        zoom(std::pow(zoomStep, event.wheelDelta.y), categoryAt(at));
        event.handled = true;
        return;
    case MouseEventType::Press:
        if (!plot_.contains(at) || event.button != platform::MouseButton::Left)
            return;
        if (event.clicks == 2) {
            resetView();
        } else {
            dragging_ = true;
            dragFrom_ = at;
            dragView_ = absoluteView();
            dragFollowing_ = mode_ == Mode::Window;
        }
        event.handled = true;
        return;
    case MouseEventType::Move:
        mouse_ = at;
        if (dragging_) {
            const float length = standing ? plot_.width : plot_.height;
            const float moved = standing ? (rtl ? dragFrom_.x - at.x : at.x - dragFrom_.x) : at.y - dragFrom_.y;
            const double by = moved / std::max(length, 1.0f) * dragView_.span();
            if (by != 0) {
                mode_ = Mode::Manual;
                user_ = {dragView_.low - by, dragView_.high - by};
                immediate_ = true;
            }
        } else {
            hovered_ = barAt(at);
        }
        invalidatePaint();
        return;
    case MouseEventType::Release:
        if (dragging_) {
            dragging_ = false;
            const double end = static_cast<double>(dropped_ + categories_.size());
            if ((dragFollowing_ || props_.window > 0) && mode_ == Mode::Manual && user_.high >= end - 0.5) {
                window_ = user_.span();
                mode_ = Mode::Window;
            }
            invalidatePaint();
            event.handled = true;
        }
        return;
    }
}

void BarChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        hovered_.reset();
        mouse_.reset();
        invalidatePaint();
    }
}

std::optional<platform::Cursor> BarChart::cursorAt(Point point) const {
    if (dragging_)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

std::vector<Chart::LegendItem> BarChart::legendItems() const {
    std::vector<LegendItem> items;
    if (series_.size() < 2)
        return items;
    for (std::size_t i = 0; i < series_.size(); ++i)
        items.push_back({translate(series_[i].name), seriesColorOf(i), series_[i].visible});
    return items;
}

void BarChart::legendItemClicked(std::size_t index) {
    setSeriesVisible(index, !seriesVisible(index));
}

} // namespace glw
