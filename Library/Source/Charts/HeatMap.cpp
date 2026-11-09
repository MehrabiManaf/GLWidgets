// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/HeatMap.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace glw {

namespace {

constexpr double zoomStep = 0.85;       // a step of the wheel in: the span this much of what it was
constexpr float dragThreshold = 6;      // a rectangle dragged narrower than this zooms nothing that way
constexpr float smallest = 4;           // cells narrower or lower than this (dips) are an image
constexpr std::size_t mostCells = 40000;    // ... and when more of them show than this
constexpr std::size_t mostEased = 200000;   // cells eased to new values: no more than this many kept
constexpr int tableSize = 256;
constexpr std::size_t whole = std::numeric_limits<std::size_t>::max();
const double none = std::numeric_limits<double>::quiet_NaN();

ChartRange between(ChartRange a, ChartRange b, float t) {
    return {a.low + (b.low - a.low) * t, a.high + (b.high - a.high) * t};
}

bool near(ChartRange a, ChartRange b) {
    const double e = std::max(std::abs(b.span()), 1e-300) * 1e-6;
    return std::abs(a.low - b.low) <= e && std::abs(a.high - b.high) <= e;
}

std::uint64_t mixed(std::uint64_t h, std::uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}
std::uint64_t mixed(std::uint64_t h, double v) {
    return mixed(h, std::bit_cast<std::uint64_t>(v));
}
std::uint64_t mixed(std::uint64_t h, Color c) {
    return mixed(mixed(mixed(mixed(h, double{c.r}), double{c.g}), double{c.b}), double{c.a});
}

// The smallest power of two at least `v` (1 at least).
std::int64_t powerOfTwo(double v) {
    std::int64_t p = 1;
    while (static_cast<double>(p) < v && p < (std::int64_t{1} << 40))
        p *= 2;
    return p;
}

std::int64_t floorDiv(std::int64_t a, std::int64_t b) {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

} // namespace

HeatMap::HeatMap() : HeatMap(Props{}) {}

HeatMap::HeatMap(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), cellsProgress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setLegendVisible(false);
    rows_ = props_.rows.size();
    window_ = props_.window;
    xMode_ = props_.window > 0 ? XMode::Window : XMode::Fit;
    progress_.set(1);
    cellsProgress_.set(1);
}

HeatMap::~HeatMap() = default;

// ---- data ----

void HeatMap::setRows(std::vector<LocalizedText> rows) {
    props_.rows = std::move(rows);
    if (!props_.rows.empty() && props_.rows.size() != rows_) {
        rows_ = props_.rows.size();
        for (auto& c : columns_)
            c.resize(rows_, static_cast<float>(none));
    }
    dataChanged(whole);
}

void HeatMap::setColumnNames(std::vector<LocalizedText> names) {
    columnNames_ = std::move(names);
    invalidatePaint();
}

void HeatMap::appendColumn(std::span<const double> values) {
    snapshot();
    if (rows_ == 0)
        rows_ = values.size();
    std::vector<float> column(rows_, static_cast<float>(none));
    for (std::size_t r = 0; r < std::min(rows_, values.size()); ++r)
        column[r] = static_cast<float>(values[r]);
    // its extremes as they come
    for (float v : column) {
        if (std::isnan(v))
            continue;
        if (extremesKnown_) {
            low_ = std::min(low_, double{v});
            high_ = std::max(high_, double{v});
        }
    }
    columns_.push_back(std::move(column));
    const std::size_t index = first_ + columns_.size() - 1;
    // the oldest dropped: the extremes found again if they were in them
    while (props_.capacity > 0 && columns_.size() > props_.capacity) {
        if (extremesKnown_)
            for (float v : columns_.front())
                if (v == low_ || v == high_)
                    extremesKnown_ = false;
        columns_.pop_front();
        ++first_;
        changed_.push_back(first_);         // its bucket lost a column
    }
    rangeKnown_ = false;
    dataChanged(index);
}

void HeatMap::setValues(std::size_t columns, std::span<const double> values) {
    snapshot();
    columns_.clear();
    first_ = 0;
    const std::size_t rows = columns > 0 ? values.size() / columns : 0;
    if (rows_ == 0 || props_.rows.empty())
        rows_ = rows;
    for (std::size_t c = 0; c < columns; ++c) {
        std::vector<float> column(rows_, static_cast<float>(none));
        for (std::size_t r = 0; r < std::min(rows_, rows); ++r)
            column[r] = static_cast<float>(values[c * rows + r]);
        columns_.push_back(std::move(column));
    }
    extremesKnown_ = rangeKnown_ = false;
    dataChanged(whole);
}

void HeatMap::setValue(std::size_t column, std::size_t row, double value) {
    if (column < first_ || column >= first_ + columns_.size() || row >= rows_)
        return;
    snapshot();
    float& v = columns_[column - first_][row];
    if (extremesKnown_ && (v == low_ || v == high_))
        extremesKnown_ = false;
    v = static_cast<float>(value);
    if (extremesKnown_ && !std::isnan(value)) {
        low_ = std::min(low_, value);
        high_ = std::max(high_, value);
    }
    rangeKnown_ = false;
    dataChanged(column);
}

void HeatMap::clear() {
    snapshot();
    columns_.clear();
    first_ = 0;
    extremesKnown_ = rangeKnown_ = false;
    dataChanged(whole);
}

double HeatMap::value(std::size_t column, std::size_t row) const {
    if (column < first_ || column >= first_ + columns_.size() || row >= rows_)
        return none;
    return columns_[column - first_][row];
}

void HeatMap::snapshot() {
    // large cells: eased from what they show now (when not too many to keep); just snapshotted, what shows is that still
    const std::size_t cells = columns_.size() * rows_;
    easing_ = drawn_ == Drawn::Cells && transition() > Duration::zero() && cells <= mostEased;
    if (!easing_ || (cellsProgress_.value() == 0 && fromColumns_ > 0))
        return;
    std::vector<float> from(cells, static_cast<float>(none));
    const float t = cellsProgress_.value();
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        const std::size_t index = first_ + c;
        for (std::size_t r = 0; r < rows_; ++r) {
            float v = columns_[c][r];
            if (t < 1 && index >= fromFirst_ && index < fromFirst_ + fromColumns_) {
                const float was = from_[(index - fromFirst_) * rows_ + r];
                if (!std::isnan(was) && !std::isnan(v))
                    v = was + (v - was) * t;
            }
            from[c * rows_ + r] = v;
        }
    }
    from_ = std::move(from);
    fromFirst_ = first_;
    fromColumns_ = columns_.size();
}

void HeatMap::dataChanged(std::size_t column) {
    if (column == whole) {
        allChanged_ = true;
        extremesKnown_ = rangeKnown_ = false;
    } else {
        changed_.push_back(column);
    }
    // not painted for long (hidden): all of it made again when it shows, rather than a list of every column that came
    if (changed_.size() > 4096) {
        changed_.clear();
        allChanged_ = true;
    }
    if (easing_) {
        easing_ = false;
        cellsProgress_.set(0);
        cellsProgress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
    }
    invalidatePaint();
}

// ---- colors ----

void HeatMap::setScale(ColorScale scale) {
    props_.scale = std::move(scale);
    rangeKnown_ = false;
    invalidatePaint();
}

void HeatMap::setReduce(Reduce reduce) {
    props_.reduce = reduce;
    allChanged_ = true;
    invalidatePaint();
}

void HeatMap::setXFormatter(std::function<std::string(double, double)> format) {
    xFormat_ = std::move(format);
    xAxis_.format = xFormat_;
    invalidatePaint();
}

void HeatMap::setValueFormatter(std::function<std::string(double)> format) {
    valueFormat_ = std::move(format);
    invalidatePaint();
}

void HeatMap::setOnCellClicked(std::function<void(std::size_t, std::size_t)> clicked) {
    clicked_ = std::move(clicked);
}

void HeatMap::fitColors() const {
    const ColorScale& scale = props_.scale;
    if (scale.range) {
        range_ = *scale.range;
        rangeKnown_ = true;
        return;
    }
    if (!extremesKnown_) {
        bool any = false;
        for (const auto& c : columns_) {
            for (float v : c) {
                if (std::isnan(v))
                    continue;
                low_ = any ? std::min(low_, double{v}) : v;
                high_ = any ? std::max(high_, double{v}) : v;
                any = true;
            }
        }
        if (!any)
            low_ = 0, high_ = 1;
        extremesKnown_ = true;
    }
    double low = low_, high = high_;
    // a quantile of them for its high end (of a sample: a few tens of thousands of them)
    if (scale.clip < 1) {
        std::vector<float> sample;
        const std::size_t total = rows_ > 0 ? columns_.size() * rows_ : 0;
        const std::size_t stride = std::max<std::size_t>(1, total / 65536);
        for (std::size_t i = 0; i < total; i += stride) {
            const float v = columns_[i / rows_][i % rows_];
            if (!std::isnan(v))
                sample.push_back(v);
        }
        if (!sample.empty()) {
            const auto k = static_cast<std::size_t>(std::clamp(double{scale.clip}, 0.0, 1.0) * static_cast<double>(sample.size() - 1));
            std::nth_element(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(k), sample.end());
            high = std::max(low, double{sample[k]});
        }
    }
    ChartRange r;
    if (scale.kind == ColorScale::Kind::Diverging) {
        const double far = std::max({std::abs(low - scale.center), std::abs(high - scale.center), 1e-12});
        r = niceRange({scale.center - far, scale.center + far}, 4);
        const double out = std::max(scale.center - r.low, r.high - scale.center);
        r = {scale.center - out, scale.center + out};
    } else if (scale.logarithmic) {
        const double floor = high > 0 ? (low > 0 ? low : high / 1000) : 1;
        r = {std::pow(10.0, std::floor(std::log10(floor))), std::pow(10.0, std::ceil(std::log10(std::max(high, floor * 10))))};
    } else {
        if (!(high > low))
            high = low + 1;
        r = niceRange({low, high}, 5);
    }
    range_ = r;
    rangeKnown_ = true;
}

ChartRange HeatMap::colorRange() const {
    if (!rangeKnown_)
        fitColors();
    return range_;
}

double HeatMap::fraction(double value) const {
    const ChartRange r = colorRange();
    if (std::isnan(value))
        return none;
    if (props_.scale.logarithmic) {
        if (value <= 0 || r.low <= 0)
            return 0;
        return std::clamp((std::log10(value) - std::log10(r.low)) / (std::log10(r.high) - std::log10(r.low)), 0.0, 1.0);
    }
    return r.span() > 0 ? std::clamp((value - r.low) / r.span(), 0.0, 1.0) : 0.5;
}

Color HeatMap::colorOf(double value) const {
    const double t = fraction(value);
    if (std::isnan(t) || table_.empty())
        return {0, 0, 0, 0};
    return table_[static_cast<std::size_t>(std::lround(t * (tableSize - 1)))];
}

void HeatMap::buildColors(const PaintContext& context) {
    const ChartStyle& s = style();
    const Theme& theme = context.ui.theme;
    const ColorScale& scale = props_.scale;
    const Color base = s.plot.a > 0.5f ? s.plot : theme.palette.layer;
    const Color accent = seriesColor(0);
    const ChartRange r = colorRange();
    std::uint64_t key = mixed(mixed(mixed(std::uint64_t{17}, r.low), r.high), static_cast<std::uint64_t>(scale.kind) * 2 + scale.logarithmic);
    key = mixed(mixed(mixed(key, base), accent), seriesColor(1));
    for (const Color& c : scale.stops)
        key = mixed(key, c);
    if (band_)
        key = mixed(mixed(key, double{band_->first}), double{band_->second});
    if (key == tableKey_ && !table_.empty())
        return;
    tableKey_ = key;
    // its stops
    std::vector<Color> stops;
    const bool dark = theme.isDark();
    switch (scale.kind) {
    case ColorScale::Kind::Sequential:
        stops = {mix(base, accent, 0.10f), accent, dark ? mix(accent, Color{1.0f, 0.98f, 0.86f, 1}, 0.72f) : lit(accent, -0.42f)};
        break;
    case ColorScale::Kind::Diverging:
        stops = {seriesColor(1), mix(base, s.text, 0.16f), accent};
        break;
    case ColorScale::Kind::Custom:
        stops = scale.stops.empty() ? std::vector<Color>{base, accent} : scale.stops;
        break;
    }
    table_.resize(tableSize);
    pixels_.resize(tableSize);
    for (int i = 0; i < tableSize; ++i) {
        const float t = static_cast<float>(i) / (tableSize - 1);
        Color c = stops.front();
        if (stops.size() > 1) {
            const float at = t * static_cast<float>(stops.size() - 1);
            const auto k = std::min(static_cast<std::size_t>(at), stops.size() - 2);
            c = mix(stops[k], stops[k + 1], at - static_cast<float>(k));
        }
        // a band of the scale under the mouse: the rest faint
        if (band_ && (t < band_->first - 1e-4f || t > band_->second + 1e-4f))
            c = mix(c, base, 0.82f);
        table_[static_cast<std::size_t>(i)] = c;
        pixels_[static_cast<std::size_t>(i)] = ImageStore::pixel(c);
    }
}

std::string HeatMap::format(double value, bool precise) const {
    if (valueFormat_)
        return valueFormat_(value);
    if (std::isnan(value))
        return "-";
    return formatNumber(value, scaleAxis_.decimals() + (precise ? 1 : 0));
}

std::string HeatMap::columnLabel(std::size_t column) const {
    if (column < columnNames_.size())
        return translate(columnNames_[column]);
    return xAxis_.precise(xOf(static_cast<double>(column)));
}

// ---- the view ----

void HeatMap::setWindow(double window) {
    props_.window = window_ = window;
    xMode_ = window > 0 ? XMode::Window : XMode::Fit;
    invalidatePaint();
}

ChartRange HeatMap::fitX() const {
    const auto end = static_cast<double>(first_ + columns_.size());
    switch (xMode_) {
    case XMode::Manual:
        return userX_;
    case XMode::Window: {
        const double span = std::max(1.0, window_ / std::max(props_.step, 1e-300));
        return {end - span, end};
    }
    case XMode::Fit:
        break;
    }
    return {static_cast<double>(first_), std::max(end, static_cast<double>(first_) + 1)};
}

ChartRange HeatMap::fitY() const {
    return userY_ ? *userY_ : ChartRange{0, std::max<double>(1, static_cast<double>(rows_))};
}

void HeatMap::updateView() {
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
    view_ = {columnView(), tx, rowView(), ty};
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    quick_ = false;
}

ChartRange HeatMap::columnView() const {
    return hasView_ ? between(view_.fromX, view_.toX, progress_.value()) : fitX();
}

ChartRange HeatMap::rowView() const {
    return hasView_ ? between(view_.fromY, view_.toY, progress_.value()) : fitY();
}

void HeatMap::setColumnView(ChartRange columns) {
    xMode_ = XMode::Manual;
    userX_ = columns;
    invalidatePaint();
}

void HeatMap::resetView() {
    xMode_ = props_.window > 0 ? XMode::Window : XMode::Fit;
    window_ = props_.window;
    userY_.reset();
    invalidatePaint();
}

bool HeatMap::following() const {
    return xMode_ == XMode::Window;
}

std::optional<std::pair<std::size_t, std::size_t>> HeatMap::cellAt(Point at) const {
    if (!plot_.contains(at) || plot_.width <= 0 || plot_.height <= 0)
        return std::nullopt;
    const ChartRange xv = columnView(), yv = rowView();
    const double c = std::floor(xv.low + (at.x - plot_.x) / plot_.width * xv.span());
    const double r = std::floor(yv.low + (at.y - plot_.y) / plot_.height * yv.span());
    if (c < static_cast<double>(first_) || c >= static_cast<double>(first_ + columns_.size()) || r < 0 || r >= static_cast<double>(rows_))
        return std::nullopt;
    return std::pair{static_cast<std::size_t>(c), static_cast<std::size_t>(r)};
}

// ---- painting ----

void HeatMap::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    drawn_ = Drawn::Nothing;
    if (!rangeKnown_)
        fitColors();
    buildColors(context);

    // the scale's bar at the end side, its values beside it; the rows' names at the start side; the columns' under it
    Rect plot = area;
    plot.y += line / 2;
    plot.height = std::max(0.0f, plot.height - line / 2 - line - 6);
    scaleAxis_.orientation = Orientation::Vertical;
    scaleAxis_.fit(*this, colorRange(), plot.height);
    const float barWidth = 10, scaleWidth = barWidth + 6 + scaleAxis_.labelWidth() + 10;
    float rowsWidth = 0;
    for (std::size_t r = 0; r < rows_; ++r)
        rowsWidth = std::max(rowsWidth, textSize(r < props_.rows.size() ? translate(props_.rows[r]) : std::to_string(r + 1)).width);
    rowsWidth = std::min(rowsWidth + 8, area.width * 0.38f);
    plot.width = std::max(0.0f, plot.width - scaleWidth - rowsWidth);
    plot.x += rtl ? scaleWidth : rowsWidth;
    plot_ = plot;
    // (right to left: its values, then the bar, then the plot)
    bar_ = {rtl ? area.x + scaleAxis_.labelWidth() + 6 : plot.right() + 10, plot.y, barWidth, plot.height};
    if (plot.width < 4 || plot.height < 4)
        return;
    updateView();
    const ChartRange xv = columnView(), yv = rowView();
    if (!(xv.span() > 0) || !(yv.span() > 0))
        return;
    if (s.plot.a > 0)
        p.fillRect(plot, s.plot);

    // the cells: one by one when they are large enough and not too many, else an image of them
    p.pushClip(plot);
    const double cellWidth = plot.width / xv.span(), cellHeight = plot.height / yv.span();
    const double shown = std::min(xv.span(), static_cast<double>(columns_.size())) * std::min(yv.span(), static_cast<double>(rows_));
    if (cellWidth >= smallest && cellHeight >= smallest && shown <= static_cast<double>(mostCells))
        paintCells(context, xv, yv);
    else
        paintImage(context, xv, yv);
    p.popClip();
    changed_.clear();

    paintRowLabels(context, yv, {rtl ? plot.right() : area.x, plot.y, rowsWidth, plot.height});
    paintColumns(context, xv, area);
    paintScale(context, bar_, area);

    if (drag_ == Drag::Box) {
        const float ppd = p.pixelsPerDip();
        const Rect box = Rect{std::min(dragFrom_.x, dragTo_.x), std::min(dragFrom_.y, dragTo_.y), std::abs(dragTo_.x - dragFrom_.x),
                              std::abs(dragTo_.y - dragFrom_.y)}
                             .intersected(plot);
        p.fillRect(box, s.selection);
        p.strokeRoundedRect(box, 0, 1.0f / ppd, s.crosshair);
    } else if (mouse_ && drag_ == Drag::None) {
        paintHover(context, xv, yv);
    }
}

void HeatMap::paintCells(PaintContext& context, ChartRange xv, ChartRange yv) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    drawn_ = Drawn::Cells;
    allChanged_ = true;         // the image, if it shows again, is made again
    const Rect plot = plot_;
    const double cw = plot.width / xv.span(), ch = plot.height / yv.span();
    const float gap = static_cast<float>(std::min<double>(s.gap, std::min(cw, ch) * 0.12));
    const float radius = static_cast<float>(std::min<double>(s.barRadius, std::min(cw, ch) / 4));
    const float line = textSize("0").height;
    const auto c0 = static_cast<std::size_t>(std::max(static_cast<double>(first_), std::floor(xv.low)));
    const auto c1 = static_cast<std::size_t>(std::clamp(std::ceil(xv.high), static_cast<double>(first_), static_cast<double>(first_ + columns_.size())));
    const auto r0 = static_cast<std::size_t>(std::max(0.0, std::floor(yv.low)));
    const auto r1 = static_cast<std::size_t>(std::clamp(std::ceil(yv.high), 0.0, static_cast<double>(rows_)));
    const float t = cellsProgress_.value();
    for (std::size_t c = c0; c < c1; ++c) {
        const float x = static_cast<float>(plot.x + (static_cast<double>(c) - xv.low) * cw);
        for (std::size_t r = r0; r < r1; ++r) {
            double v = columns_[c - first_][r];
            if (std::isnan(v))
                continue;
            // on its way from what it showed
            if (t < 1 && c >= fromFirst_ && c < fromFirst_ + fromColumns_) {
                const float was = from_[(c - fromFirst_) * rows_ + r];
                if (!std::isnan(was))
                    v = was + (v - was) * t;
            }
            const float y = static_cast<float>(plot.y + (static_cast<double>(r) - yv.low) * ch);
            const Rect cell{x + gap / 2, y + gap / 2, static_cast<float>(cw) - gap, static_cast<float>(ch) - gap};
            const Color color = colorOf(v);
            p.fillRoundedRect(cell, radius, color);
            // its value in it, when it fits
            if (props_.values && cell.height >= line + 2) {
                const std::string text = format(columns_[c - first_][r], false);
                if (textSize(text).width + 6 <= cell.width)
                    drawText(context, text, {cell.x + cell.width / 2, cell.y + cell.height / 2}, textOn(color), 0, {0.5f, 0.5f});
            }
        }
    }
}

void HeatMap::paintImage(PaintContext& context, ChartRange xv, ChartRange yv) {
    paint::Painter& p = context.painter;
    drawn_ = Drawn::Image;
    const Rect plot = plot_;
    const float ppd = p.pixelsPerDip();
    if (columns_.empty() || rows_ == 0)
        return;
    // buckets: as many columns (rows) as a pixel shows, a power of two of them, so that they stay as the view moves
    const std::int64_t kc = powerOfTwo(xv.span() / std::max(1.0, static_cast<double>(plot.width * ppd)));
    std::int64_t kr = powerOfTwo(yv.span() / std::max(1.0, static_cast<double>(plot.height * ppd)));
    kr = std::max(kr, powerOfTwo(static_cast<double>(rows_) / 8192));
    const auto first = static_cast<std::int64_t>(first_), end = static_cast<std::int64_t>(first_ + columns_.size());
    const std::int64_t b0 = std::max(floorDiv(static_cast<std::int64_t>(std::floor(xv.low)), kc), floorDiv(first, kc));
    const std::int64_t b1 = std::min(floorDiv(static_cast<std::int64_t>(std::ceil(xv.high)) + kc - 1, kc), floorDiv(end + kc - 1, kc));
    const auto rows = static_cast<std::int64_t>(rows_);
    const std::int64_t rb0 = std::max<std::int64_t>(0, floorDiv(static_cast<std::int64_t>(std::floor(yv.low)), kr));
    const std::int64_t rb1 = std::min(floorDiv(static_cast<std::int64_t>(std::ceil(yv.high)) + kr - 1, kr), (rows + kr - 1) / kr);
    if (b1 <= b0 || rb1 <= rb0)
        return;
    const auto width = static_cast<int>(xv.span() / static_cast<double>(kc)) + 3;
    const auto height = static_cast<int>(rb1 - rb0);

    // what the image is of: all of it made again when that changes
    std::uint64_t key = mixed(mixed(mixed(mixed(std::uint64_t{29}, static_cast<std::uint64_t>(kc)), static_cast<std::uint64_t>(kr)),
                                    static_cast<std::uint64_t>(rb0)),
                              static_cast<std::uint64_t>(width) << 32 | static_cast<std::uint64_t>(height));
    key = mixed(mixed(key, tableKey_), static_cast<std::uint64_t>(props_.reduce));
    if (!image_ && context.ui.images)
        image_ = context.ui.images->make(rhi::Filter::Nearest);
    std::vector<std::uint32_t> fallback;            // without a store of images: its pixels here, drawn as rectangles
    const bool stored = static_cast<bool>(image_);
    if (key != imageKey_ || allChanged_ || !stored) {
        imageKey_ = key;
        allChanged_ = false;
        bucketColumns_ = kc;
        bucketRows_ = kr;
        rowBase_ = rb0;
        held_.assign(static_cast<std::size_t>(width), std::numeric_limits<std::int64_t>::min());
        if (stored) {
            image_.resize({width, height});
        }
    } else {
        // the buckets of the columns that changed: made again
        for (std::size_t c : changed_) {
            const std::int64_t b = floorDiv(static_cast<std::int64_t>(c), kc);
            std::int64_t& held = held_[static_cast<std::size_t>(((b % width) + width) % width)];
            if (held == b)
                held = std::numeric_limits<std::int64_t>::min();
        }
    }
    if (!stored)
        fallback.assign(static_cast<std::size_t>(width) * height, 0);

    // each bucket that shows and is not in the image: its rows' buckets reduced (the highest, or the mean), colored
    sums_.resize(static_cast<std::size_t>(height));
    counts_.resize(static_cast<std::size_t>(height));
    const bool mean = props_.reduce == Reduce::Mean;
    int changedLow = width, changedHigh = -1;
    for (std::int64_t b = b0; b < b1; ++b) {
        const auto t = static_cast<int>(((b % width) + width) % width);
        if (stored && held_[static_cast<std::size_t>(t)] == b)
            continue;
        held_[static_cast<std::size_t>(t)] = b;
        changedLow = std::min(changedLow, t);
        changedHigh = std::max(changedHigh, t);
        std::fill(sums_.begin(), sums_.end(), mean ? 0.0f : -std::numeric_limits<float>::infinity());
        std::fill(counts_.begin(), counts_.end(), 0u);
        const std::int64_t from = std::max(b * kc, first), to = std::min((b + 1) * kc, end);
        for (std::int64_t c = from; c < to; ++c) {
            const float* column = columns_[static_cast<std::size_t>(c - first)].data();
            // each bucket of rows: its rows' values (a NaN is not equal to itself: none there)
            for (int y = 0; y < height; ++y) {
                const std::int64_t r0 = (rb0 + y) * kr, r1 = std::min(r0 + kr, rows);
                float m = sums_[static_cast<std::size_t>(y)];
                std::uint32_t n = counts_[static_cast<std::size_t>(y)];
                if (mean) {
                    for (std::int64_t r = r0; r < r1; ++r) {
                        const float v = column[r];
                        if (v == v) {
                            m += v;
                            ++n;
                        }
                    }
                } else {
                    for (std::int64_t r = r0; r < r1; ++r) {
                        const float v = column[r];
                        if (v == v) {
                            m = v > m ? v : m;
                            ++n;
                        }
                    }
                }
                sums_[static_cast<std::size_t>(y)] = m;
                counts_[static_cast<std::size_t>(y)] = n;
            }
        }
        for (int y = 0; y < height; ++y) {
            std::uint32_t px = 0;
            if (counts_[static_cast<std::size_t>(y)] > 0) {
                const double v = mean ? double{sums_[static_cast<std::size_t>(y)]} / counts_[static_cast<std::size_t>(y)]
                                      : double{sums_[static_cast<std::size_t>(y)]};
                px = pixels_[static_cast<std::size_t>(std::lround(fraction(v) * (tableSize - 1)))];
            }
            if (stored)
                image_.row(y)[static_cast<std::size_t>(t)] = px;
            else
                fallback[static_cast<std::size_t>(y) * width + t] = px;
        }
    }
    if (stored && changedHigh >= changedLow)
        image_.changed({changedLow, 0, changedHigh - changedLow + 1, height});

    // the buckets that show, where they are: the ring's in at most two runs
    const auto px = [&](std::int64_t column) { return static_cast<float>(plot.x + (static_cast<double>(column) - xv.low) / xv.span() * plot.width); };
    const float top = static_cast<float>(plot.y + (static_cast<double>(rb0 * kr) - yv.low) / yv.span() * plot.height);
    const float bottom = static_cast<float>(plot.y + (static_cast<double>(rb1 * kr) - yv.low) / yv.span() * plot.height);
    for (std::int64_t b = b0; b < b1;) {
        const auto t = static_cast<int>(((b % width) + width) % width);
        const std::int64_t run = std::min<std::int64_t>(b1 - b, width - t);
        if (stored) {
            const Rect rect{px(b * kc), top, px((b + run) * kc) - px(b * kc), bottom - top};
            const Rect uv{static_cast<float>(t) / static_cast<float>(width), 0, static_cast<float>(run) / static_cast<float>(width), 1};
            p.drawImage(rect, image_.texture(), {1, 1, 1, 1}, uv);
        } else {
            // runs of a color along each row, as rectangles
            const float rowHeight = (bottom - top) / static_cast<float>(height);
            for (int y = 0; y < height; ++y) {
                for (std::int64_t k = 0; k < run;) {
                    const std::uint32_t color = fallback[static_cast<std::size_t>(y) * width + t + k];
                    std::int64_t n = 1;
                    while (k + n < run && fallback[static_cast<std::size_t>(y) * width + t + k + n] == color)
                        ++n;
                    if (color >> 24) {
                        const float a = static_cast<float>(color >> 24) / 255.0f;
                        const Color c{static_cast<float>(color & 0xff) / 255.0f / a, static_cast<float>((color >> 8) & 0xff) / 255.0f / a,
                                      static_cast<float>((color >> 16) & 0xff) / 255.0f / a, a};
                        p.fillRect({px((b + k) * kc), top + static_cast<float>(y) * rowHeight, px((b + k + n) * kc) - px((b + k) * kc), rowHeight}, c);
                    }
                    k += n;
                }
            }
        }
        b += run;
    }
}

void HeatMap::paintRowLabels(PaintContext& context, ChartRange yv, const Rect& labels) {
    const ChartStyle& s = style();
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    const double rowHeight = plot_.height / yv.span();
    // every row's name when they are high enough, else every n-th
    const auto every = static_cast<std::size_t>(std::max(1.0, std::ceil(line * 1.15 / rowHeight)));
    context.painter.pushClip({labels.x, labels.y - line, labels.width, labels.height + 2 * line});
    for (std::size_t r = static_cast<std::size_t>(std::max(0.0, std::floor(yv.low))); r < rows_ && static_cast<double>(r) < yv.high; ++r) {
        if (r % every != 0)
            continue;
        const float y = static_cast<float>(plot_.y + (static_cast<double>(r) + 0.5 - yv.low) * rowHeight);
        const std::string name = r < props_.rows.size() ? translate(props_.rows[r]) : std::to_string(r + 1);
        drawText(context, name, {rtl ? labels.x + 8 : labels.right() - 8, y}, s.text, 0, {rtl ? 0.0f : 1.0f, 0.5f});
    }
    context.painter.popClip();
}

void HeatMap::paintColumns(PaintContext& context, ChartRange xv, const Rect& area) {
    const ChartStyle& s = style();
    const Rect plot = plot_;
    if (!columnNames_.empty()) {
        // named: each under its column, as many as fit
        const double cw = plot.width / xv.span();
        float widest = 0;
        for (const auto& name : columnNames_)
            widest = std::max(widest, textSize(translate(name)).width);
        const auto every = static_cast<std::size_t>(std::max(1.0, std::ceil((widest + 12) / cw)));
        for (std::size_t c = static_cast<std::size_t>(std::max(0.0, std::floor(xv.low))); c < columnNames_.size() && static_cast<double>(c) < xv.high; ++c) {
            if (c % every != 0)
                continue;
            const float x = static_cast<float>(plot.x + (static_cast<double>(c) + 0.5 - xv.low) * cw);
            if (x >= plot.x - 1 && x <= plot.right() + 1)
                drawText(context, translate(columnNames_[c]), {x, plot.bottom() + 5}, s.text, 0, {0.5f, 0});
        }
        return;
    }
    xAxis_.orientation = Orientation::Horizontal;
    xAxis_.time = props_.xAxis == ChartAxisKind::Time;
    xAxis_.format = xFormat_;
    xAxis_.fit(*this, {xOf(xv.low), xOf(xv.high)}, plot.width);
    xAxis_.paintLabels(context, *this, plot, Axis::Side::Bottom, s.text, area);
}

void HeatMap::paintScale(PaintContext& context, const Rect& bar, const Rect& area) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    // the bar: its colors from the high end at its top to the low end, a strip each
    const int strips = std::max(1, static_cast<int>(bar.height / 2));
    for (int i = 0; i < strips; ++i) {
        const float t = 1 - (static_cast<float>(i) + 0.5f) / static_cast<float>(strips);
        const auto k = static_cast<std::size_t>(std::lround(t * (tableSize - 1)));
        p.fillRect({bar.x, bar.y + bar.height * static_cast<float>(i) / static_cast<float>(strips), bar.width,
                    bar.height / static_cast<float>(strips) + 0.5f},
                   table_[k]);
    }
    // its values beside it: at its ticks (a logarithmic one's: at powers of ten)
    const bool rtl = rightToLeft();
    const ChartRange r = colorRange();
    const auto yOf = [&](double v) { return static_cast<float>(bar.bottom() - fraction(v) * bar.height); };
    const float ppd = p.pixelsPerDip();
    const auto label = [&](double v, const std::string& text) {
        const float y = yOf(v);
        p.fillRect({rtl ? bar.x - 3 : bar.right(), snap(y, ppd), 3, 1.0f / ppd}, s.axis);
        drawText(context, text, {rtl ? bar.x - 6 : bar.right() + 6, y}, s.text, 0, {rtl ? 1.0f : 0.0f, 0.5f});
    };
    if (props_.scale.logarithmic && r.low > 0) {
        for (double v = r.low; v <= r.high * 1.0001; v *= 10)
            label(v, formatNumber(v, v < 1 ? static_cast<int>(std::ceil(-std::log10(v))) : 0));
    } else {
        for (std::size_t i = 0; i < scaleAxis_.values().size(); ++i)
            label(scaleAxis_.values()[i], valueFormat_ ? valueFormat_(scaleAxis_.values()[i]) : scaleAxis_.labels()[i]);
    }
    (void)area;
}

void HeatMap::paintHover(PaintContext& context, ChartRange xv, ChartRange yv) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const auto cell = cellAt(*mouse_);
    if (!cell)
        return;
    const auto [c, r] = *cell;
    const double cw = plot_.width / xv.span(), ch = plot_.height / yv.span();
    const float x = static_cast<float>(plot_.x + (static_cast<double>(c) - xv.low) * cw);
    const float y = static_cast<float>(plot_.y + (static_cast<double>(r) - yv.low) * ch);
    const float ppd = p.pixelsPerDip();
    const float w = std::max(static_cast<float>(cw), 1.0f / ppd), h = std::max(static_cast<float>(ch), 1.0f / ppd);
    p.pushClip(plot_);
    if (drawn_ == Drawn::Image) {
        // its row and its column: lines across along them
        p.fillRect({plot_.x, y, plot_.width, h}, s.crosshair);
        p.fillRect({x, plot_.y, w, plot_.height}, s.crosshair);
    }
    const float out = drawn_ == Drawn::Cells ? 0.0f : 2.0f;
    p.strokeRoundedRect({x - out, y - out, w + 2 * out, h + 2 * out}, drawn_ == Drawn::Cells ? std::min(s.barRadius, w / 4) : 2.0f, 1.5f, s.text);
    p.popClip();
    const double v = value(c, r);
    const std::string name = r < props_.rows.size() ? translate(props_.rows[r]) : std::to_string(r + 1);
    const ToolTipRow row{std::isnan(v) ? std::optional<Color>() : colorOf(v), name, format(v, true)};
    paintToolTip(context, *mouse_, columnLabel(c), std::span(&row, 1));
}

// ---- the mouse ----

void HeatMap::zoomX(double factor, double anchor) {
    const ChartRange t = view_.toX;
    if (xMode_ == XMode::Window) {
        window_ = std::max(props_.step, window_ * factor);
    } else {
        const ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
        if (r.span() < 1)
            return;
        xMode_ = XMode::Manual;
        userX_ = r;
    }
    quick_ = true;
    invalidatePaint();
}

void HeatMap::zoomY(double factor, double anchor) {
    const ChartRange t = view_.toY;
    const ChartRange r{anchor - (anchor - t.low) * factor, anchor + (t.high - anchor) * factor};
    if (r.span() < 1)
        return;
    userY_ = r;
    quick_ = true;
    invalidatePaint();
}

void HeatMap::onMouse(MouseEvent& event) {
    const Point at = event.position;
    const auto columnAt = [&](Point point) {
        const ChartRange xv = columnView();
        return xv.low + (point.x - plot_.x) / std::max(1.0f, plot_.width) * xv.span();
    };
    const auto rowAt = [&](Point point) {
        const ChartRange yv = rowView();
        return yv.low + (point.y - plot_.y) / std::max(1.0f, plot_.height) * yv.span();
    };
    const auto newest = [&] { return static_cast<double>(first_ + columns_.size()); };
    switch (event.type) {
    case MouseEventType::Wheel: {
        if (!plot_.contains(at))
            return;
        const Point delta = event.wheelDelta;
        const bool sideways = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift) || std::abs(delta.x) > std::abs(delta.y);
        if (sideways) {
            const double steps = std::abs(delta.x) > std::abs(delta.y) ? delta.x : delta.y;
            const ChartRange t = view_.toX;
            const double by = -steps * t.span() * 0.1;
            userX_ = {t.low + by, t.high + by};
            xMode_ = XMode::Manual;
            if (props_.window > 0 && userX_.high >= newest()) {
                window_ = userX_.span() * props_.step;
                xMode_ = XMode::Window;
            }
            quick_ = true;
            invalidatePaint();
        } else if (platform::hasAny(event.modifiers, platform::KeyModifiers::Control)) {
            zoomY(std::pow(zoomStep, delta.y), rowAt(at));
        } else {
            zoomX(std::pow(zoomStep, delta.y), columnAt(at));
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
                dragX_ = columnView();
                dragY_ = rowView();
                dragged_ = false;
            }
            event.handled = true;
        } else if (event.button == platform::MouseButton::Right) {
            drag_ = Drag::Box;
            dragFrom_ = dragTo_ = at;
            event.handled = true;
        }
        return;
    case MouseEventType::Move: {
        mouse_ = at;
        if (drag_ == Drag::Pan && plot_.width > 0 && plot_.height > 0) {
            if (std::abs(at.x - dragFrom_.x) + std::abs(at.y - dragFrom_.y) >= 3)
                dragged_ = true;
            if (dragged_) {
                const double dx = (at.x - dragFrom_.x) / plot_.width * dragX_.span();
                xMode_ = XMode::Manual;
                userX_ = {dragX_.low - dx, dragX_.high - dx};
                if (userY_) {
                    const double dy = (at.y - dragFrom_.y) / plot_.height * dragY_.span();
                    userY_ = ChartRange{dragY_.low - dy, dragY_.high - dy};
                }
                immediate_ = true;
            }
        } else if (drag_ == Drag::Box) {
            dragTo_ = at;
        }
        // over the scale's bar: the band of its values there (between its ticks) stands out
        std::optional<std::pair<float, float>> band;
        if (drag_ == Drag::None && bar_.inflated(4).contains(at) && bar_.height > 0) {
            const float t = std::clamp((bar_.bottom() - at.y) / bar_.height, 0.0f, 1.0f);
            const auto& values = scaleAxis_.values();
            float lowT = 0, highT = 1;
            for (double v : values) {
                const auto f = static_cast<float>(fraction(v));
                if (f <= t)
                    lowT = std::max(lowT, f);
                else
                    highT = std::min(highT, f);
            }
            band = std::pair{lowT, highT};
        }
        if (band != band_) {
            band_ = band;
            allChanged_ = true;
        }
        invalidatePaint();
        return;
    }
    case MouseEventType::Release:
        if (drag_ == Drag::Pan) {
            if (!dragged_) {
                if (clicked_)
                    if (const auto cell = cellAt(at))
                        clicked_(cell->first, cell->second);
            } else if (props_.window > 0 && xMode_ == XMode::Manual && userX_.high >= newest() - userX_.span() * 0.02) {
                // a live one dragged back to its newest follows it again
                window_ = userX_.span() * props_.step;
                xMode_ = XMode::Window;
                immediate_ = true;
            }
        } else if (drag_ == Drag::Box) {
            if (std::abs(dragTo_.x - dragFrom_.x) >= dragThreshold) {
                xMode_ = XMode::Manual;
                userX_ = {std::min(columnAt(dragFrom_), columnAt(dragTo_)), std::max(columnAt(dragFrom_), columnAt(dragTo_))};
            }
            if (std::abs(dragTo_.y - dragFrom_.y) >= dragThreshold)
                userY_ = ChartRange{std::min(rowAt(dragFrom_), rowAt(dragTo_)), std::max(rowAt(dragFrom_), rowAt(dragTo_))};
        }
        drag_ = Drag::None;
        invalidatePaint();
        event.handled = true;
        return;
    }
}

void HeatMap::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        if (band_) {
            band_.reset();
            allChanged_ = true;
        }
        invalidatePaint();
    }
}

std::optional<platform::Cursor> HeatMap::cursorAt(Point point) const {
    if (drag_ == Drag::Pan && dragged_)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

} // namespace glw
