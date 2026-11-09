// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/Sparkline.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace glw {

namespace {

constexpr float dot = 2.5f;             // the radius of its dots: room kept for them around it

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

} // namespace

Sparkline::Sparkline() : Sparkline(Props{}) {}

Sparkline::Sparkline(Props props) : props_(std::move(props)), data_(props_.capacity), progress_([this] { invalidatePaint(); }) {
    setLegendVisible(false);
    progress_.set(1);
}

Sparkline::~Sparkline() = default;

void Sparkline::append(double value) {
    data_.append(next_++, value);
    invalidatePaint();
}

void Sparkline::setValues(std::span<const double> values) {
    data_.clear();
    next_ = 0;
    for (double v : values)
        data_.append(next_++, v);
    invalidatePaint();
}

void Sparkline::clear() {
    data_.clear();
    invalidatePaint();
}

double Sparkline::last() const {
    return data_.empty() ? std::numeric_limits<double>::quiet_NaN() : data_.y(data_.size() - 1);
}

void Sparkline::setColor(std::optional<Color> color) {
    props_.color = color;
    invalidatePaint();
}

void Sparkline::setLevel(std::optional<double> level) {
    props_.level = level;
    invalidatePaint();
}

void Sparkline::setFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string Sparkline::format(double value) const {
    if (format_)
        return format_(value);
    return formatNumber(value, std::abs(value) >= 100 ? 0 : std::abs(value) >= 10 ? 1 : 2);
}

Size Sparkline::measureContent(Size available) {
    // small: a word's height, a few words wide
    return {std::min(available.width, 120.0f), std::min(available.height, 28.0f)};
}

ChartRange Sparkline::fit() const {
    if (props_.range)
        return *props_.range;
    const auto e = data_.yExtent();
    ChartRange r = e ? ChartRange{e->low, e->high} : ChartRange{0, 1};
    if (props_.kind == Kind::Columns) {
        r.low = std::min(r.low, 0.0);
        r.high = std::max(r.high, 0.0);
    }
    if (props_.level) {
        r.low = std::min(r.low, *props_.level);
        r.high = std::max(r.high, *props_.level);
    }
    if (!(r.span() > 0))
        r = {r.low - 1, r.high + 1};
    return r;
}

ChartRange Sparkline::shownRange() const {
    return fitted_ ? between(from_, to_, progress_.value()) : fit();
}

void Sparkline::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    // its range: fitted to its values, eased there
    const ChartRange target = fit();
    if (!fitted_ || transition() <= Duration::zero()) {
        from_ = to_ = target;
        fitted_ = true;
    } else if (std::abs(target.low - to_.low) > 1e-9 * target.span() || std::abs(target.high - to_.high) > 1e-9 * target.span()) {
        from_ = shownRange();
        to_ = target;
        progress_.set(0);
        progress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
    }
    const ChartRange yv = shownRange();
    const Rect plot = {area.x + dot, area.y + dot, std::max(0.0f, area.width - 2 * dot), std::max(0.0f, area.height - 2 * dot)};
    plot_ = plot;
    const std::size_t n = data_.size();
    if (n == 0 || plot.width < 2 || plot.height < 2)
        return;
    const Color color = props_.color ? *props_.color : seriesColor(props_.series);
    // as wide as its capacity (until it has as many values: they fill it from the left)
    const double first = data_.x(0);
    const double span = std::max({1.0, data_.x(n - 1) - first, static_cast<double>(props_.capacity) - 1});
    const auto px = [&](double x) { return static_cast<float>(plot.x + (x - first) / span * plot.width); };
    const auto py = [&](double y) { return static_cast<float>(plot.bottom() - (y - yv.low) / yv.span() * plot.height); };
    const float ppd = p.pixelsPerDip();
    const double columns = std::max(1.0, static_cast<double>(plot.width) * ppd);

    // a level across it
    if (props_.level)
        p.fillRect({plot.x, snap(py(*props_.level), ppd), plot.width, 1.0f / ppd}, s.axis);

    if (props_.kind == Kind::Columns) {
        // a column for each value (or each column of pixels: its lowest and highest), from 0
        const float zero = py(std::clamp(0.0, yv.low, yv.high));
        const double slots = std::min(std::max(static_cast<double>(n), static_cast<double>(props_.capacity)), columns);
        const double count = std::max(static_cast<double>(n), static_cast<double>(props_.capacity));
        const float slot = plot.width / static_cast<float>(slots);
        const float width = std::max(1.0f / ppd, slot * (slots < columns / 2 ? 0.7f : 1.0f));
        std::size_t i = 0;
        for (double c = 0; c < slots && i < n; ++c) {
            const std::size_t j = std::min(n, static_cast<std::size_t>(std::ceil((c + 1) / slots * count)));
            if (j <= i)
                continue;
            const auto e = data_.yExtent(i, j);
            i = j;
            if (!e)
                continue;
            const float x = plot.x + static_cast<float>(c) * slot + (slot - width) / 2;
            // up from 0 to the highest, down from 0 to the lowest (under 0: fainter)
            if (e->high > 0) {
                const float top = py(e->high);
                p.fillRect({x, top, width, std::max(1.0f / ppd, zero - top)}, color);
            }
            if (e->low < 0) {
                const float bottom = py(e->low);
                p.fillRect({x, zero, width, std::max(1.0f / ppd, bottom - zero)}, withAlpha(color, color.a * 0.55f));
            }
        }
    } else {
        // a line through its values (each column of pixels what it shows, when there are more), an area under it
        points_.clear();
        if (static_cast<double>(n) > 2 * columns) {
            const double dx = span / columns;
            std::size_t i = 0;
            for (double c = 0; c < columns && i < n; ++c) {
                const std::size_t j = std::min(n, data_.lowerBound(first + (c + 1) * dx, i));
                if (j <= i)
                    continue;
                if (const auto e = data_.yExtent(i, j)) {
                    const float cx = px(first + (c + 0.5) * dx);
                    points_.push_back({cx, py((e->low + e->high) / 2)});
                }
                i = j;
            }
        } else {
            for (std::size_t i = 0; i < n; ++i)
                if (!std::isnan(data_.y(i)))
                    points_.push_back({px(data_.x(i)), py(data_.y(i))});
        }
        if (points_.size() >= 2) {
            if (props_.area) {
                const float baseline = py(yv.contains(0) ? 0.0 : yv.low);
                p.fillArea(points_, baseline, withAlpha(color, color.a * s.areaOpacity), withAlpha(color, 0));
            }
            p.drawPolyline(points_, s.lineWidth, color);
        }
    }
    // dots: its lowest and highest, its last
    const Color ring = s.plot.a > 0.9f ? s.plot : context.ui.theme.palette.layer;
    const auto mark = [&](std::size_t i, Color c) {
        const Point at{px(data_.x(i)), py(data_.y(i))};
        p.fillCircle(at, dot + 1, ring);
        p.fillCircle(at, dot, c);
    };
    if (props_.extremes && n > 1) {
        std::size_t low = 0, high = 0;
        for (std::size_t i = 1; i < n; ++i) {
            if (data_.y(i) < data_.y(low))
                low = i;
            if (data_.y(i) > data_.y(high))
                high = i;
        }
        mark(low, s.text);
        mark(high, s.text);
    }
    if (props_.last && props_.kind == Kind::Line && !std::isnan(last()))
        mark(n - 1, color);

    // the value under the mouse
    if (mouse_ && plot.inflated(dot).contains(*mouse_)) {
        const double x = first + (mouse_->x - plot.x) / plot.width * span;
        std::size_t i = std::min(n - 1, data_.lowerBound(x));
        if (i > 0 && std::abs(data_.x(i - 1) - x) < std::abs(data_.x(i) - x))
            --i;
        if (!std::isnan(data_.y(i))) {
            p.fillRect({snap(px(data_.x(i)), ppd), plot.y, 1.0f / ppd, plot.height}, s.crosshair);
            mark(i, color);
            const ToolTipRow row{color, std::string(), format(data_.y(i))};
            paintToolTip(context, *mouse_, std::string(), std::span(&row, 1));
        }
    }
}

void Sparkline::onMouse(MouseEvent& event) {
    if (event.type == MouseEventType::Move) {
        mouse_ = event.position;
        invalidatePaint();
    }
}

void Sparkline::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

} // namespace glw
