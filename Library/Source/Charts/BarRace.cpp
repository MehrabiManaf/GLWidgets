// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/BarRace.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float band = 0.78f;                       // of a bar's row: the bar
constexpr std::size_t mostShown = 60;
constexpr double wheelRows = 3;                     // a step of the wheel
constexpr auto slowestPace = std::chrono::milliseconds(2000);

double number(double v) {
    return std::isfinite(v) ? std::max(0.0, v) : 0.0;
}

} // namespace

BarRace::BarRace() : BarRace(Props{}) {}

BarRace::BarRace(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), scrolling_([this](double rank) {
          scroll_ = std::clamp(rank, 0.0, lastTop());
          invalidatePaint();
      }) {
    setTitle(props_.title);
    props_.shown = std::clamp<std::size_t>(props_.shown, 1, mostShown);
    firstShown_ = props_.shown;
    hidden_.assign(partCount(), false);
    othersFrom_.assign(partCount(), 0.0);
    othersTo_.assign(partCount(), 0.0);
    progress_.set(1);
}

BarRace::~BarRace() = default;

// ---- data ----

std::size_t BarRace::addItem(LocalizedText name, std::optional<Color> color) {
    const std::size_t parts = partCount();
    const auto index = static_cast<std::uint32_t>(names_.size());
    names_.push_back(std::move(name));
    colors_.push_back(color);
    values_.insert(values_.end(), parts, 0.0);
    from_.insert(from_.end(), parts, 0.0);
    to_.insert(to_.end(), parts, 0.0);
    fromPlace_.push_back(static_cast<float>(index));
    toPlace_.push_back(static_cast<float>(index));
    rank_.push_back(index);
    ranked_.push_back(0);
    order_.push_back(index);
    dirty_ = true;
    invalidatePaint();
    return index;
}

void BarRace::setValue(std::size_t item, double value) {
    if (item >= names_.size())
        return;
    values_[item * partCount()] = value;
    dirty_ = true;
    invalidatePaint();
}

void BarRace::setValues(std::size_t item, std::span<const double> parts) {
    if (item >= names_.size())
        return;
    const std::size_t count = partCount();
    std::copy_n(parts.begin(), std::min(parts.size(), count), values_.begin() + static_cast<std::ptrdiff_t>(item * count));
    dirty_ = true;
    invalidatePaint();
}

double BarRace::target(std::size_t item, std::size_t part) const {
    return partVisible(part) ? number(values_[item * partCount() + part]) : 0.0;
}

double BarRace::total(std::size_t item) const {
    double sum = 0;
    if (item < names_.size())
        for (std::size_t k = 0; k < partCount(); ++k)
            sum += target(item, k);
    return sum;
}

void BarRace::setPeriod(LocalizedText period) {
    period_ = std::move(period);
    invalidatePaint();
}

void BarRace::setShown(std::size_t rows) {
    rows = std::clamp<std::size_t>(rows, 1, mostShown);
    if (rows == props_.shown)
        return;
    props_.shown = rows;
    scroll_ = std::clamp(scroll_, 0.0, lastTop());
    invalidatePaint();
}

void BarRace::setTop(std::size_t top) {
    if (top == props_.top)
        return;
    props_.top = top;
    dirty_ = true;
    invalidatePaint();
}

void BarRace::setPartVisible(std::size_t part, bool visible) {
    if (part >= hidden_.size() || hidden_[part] == !visible)
        return;
    hidden_[part] = !visible;
    dirty_ = true;
    invalidatePaint();
}

void BarRace::setFormatter(std::function<std::string(double, double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string BarRace::format(double value, double step) const {
    if (format_)
        return format_(value, step);
    if (step > 0)
        return formatNumber(value, std::max(0, static_cast<int>(-std::floor(std::log10(step) + 1e-9))), step);
    return formatNumber(value, value >= 100 ? 0 : value >= 10 ? 1 : 2);
}

Color BarRace::colorOf(std::size_t item, std::size_t part) const {
    if (props_.parts.size() > 1)
        return seriesColor(part);
    return colors_[item] ? *colors_[item] : seriesColor(item);
}

// ---- the race ----

std::vector<std::size_t> BarRace::ranking() const {
    return {order_.begin(), order_.end()};
}

double BarRace::shownPart(std::size_t item, std::size_t part) const {
    const std::size_t at = item * partCount() + part;
    return from_[at] + (to_[at] - from_[at]) * progress_.value();
}

double BarRace::shownTotal(std::size_t item) const {
    double sum = 0;
    if (item < names_.size())
        for (std::size_t k = 0; k < partCount(); ++k)
            sum += shownPart(item, k);
    return sum;
}

float BarRace::shownPlace(std::size_t item) const {
    if (item >= names_.size())
        return 0;
    return fromPlace_[item] + (toPlace_[item] - fromPlace_[item]) * progress_.value();
}

std::size_t BarRace::rowCount() const {
    return limit_ + (othersCount_ > 0 ? 1 : 0);
}

double BarRace::lastTop() const {
    return std::max(0.0, static_cast<double>(rowCount()) - static_cast<double>(props_.shown));
}

double BarRace::shownOthers(std::size_t part) const {
    return othersFrom_[part] + (othersTo_[part] - othersFrom_[part]) * progress_.value();
}

void BarRace::settle() {
    if (!dirty_)
        return;
    dirty_ = false;
    const std::size_t parts = partCount(), n = names_.size();
    // what shows now is where it eases from
    const auto t = static_cast<double>(progress_.value());
    for (std::size_t i = 0; i < from_.size(); ++i)
        from_[i] += (to_[i] - from_[i]) * t;
    for (std::size_t i = 0; i < n; ++i)
        fromPlace_[i] += (toPlace_[i] - fromPlace_[i]) * static_cast<float>(t);
    for (std::size_t k = 0; k < parts; ++k)
        othersFrom_[k] += (othersTo_[k] - othersFrom_[k]) * t;
    // all ranked, or the top ones (the others one bar under them)
    const std::size_t limit = props_.top > 0 && props_.top < n ? props_.top : n;
    // where it goes: the parts, and the ranking again from the one it had, in its order (ties stay so)
    std::vector<char> visible(parts);
    for (std::size_t k = 0; k < parts; ++k)
        visible[k] = partVisible(k);
    keys_.resize(n);
    for (std::size_t r = 0; r < n; ++r) {
        const std::uint32_t id = order_[r];
        double sum = 0;
        for (std::size_t k = 0; k < parts; ++k) {
            const double v = visible[k] ? number(values_[id * parts + k]) : 0.0;
            to_[id * parts + k] = v;
            sum += v;
        }
        keys_[r] = {sum, static_cast<std::uint32_t>(r), id};
    }
    const auto before = [](const Key& a, const Key& b) { return a.total != b.total ? a.total > b.total : a.rank < b.rank; };
    // most move a place or two, if at all: by insertion, as long as that stays near linear; else sorted (the top ones
    // only: by a partial sort)
    std::size_t moves = 0;
    const std::size_t budget = 8 * n + 64;
    bool sorted = !shuffled_ && limit == n;
    for (std::size_t i = 1; i < n && sorted; ++i) {
        const Key key = keys_[i];
        std::size_t j = i;
        while (j > 0 && before(key, keys_[j - 1])) {
            keys_[j] = keys_[j - 1];
            --j;
            if (++moves > budget) {
                sorted = false;
                break;
            }
        }
        keys_[j] = key;
    }
    // (all over the place: likely again the next time, then sorted at once; tried by insertion again after)
    if (limit < n) {
        std::partial_sort(keys_.begin(), keys_.begin() + static_cast<std::ptrdiff_t>(limit), keys_.end(), before);
    } else {
        shuffled_ = !sorted && !shuffled_;
        if (!sorted)
            std::sort(keys_.begin(), keys_.end(), before);
    }
    std::fill(othersTo_.begin(), othersTo_.end(), 0.0);
    for (std::size_t r = 0; r < n; ++r) {
        const std::uint32_t id = keys_[r].item;
        order_[r] = id;
        rank_[id] = static_cast<std::uint32_t>(r);
        // past the top ones: in the others' row (into it, out of sight there), its parts in the others'
        toPlace_[id] = static_cast<float>(std::min(r, limit));
        if (r >= limit)
            for (std::size_t k = 0; k < parts; ++k)
                othersTo_[k] += to_[id * parts + k];
        // one going far (past more rows than show): from a screen away, the way it comes, not across all of them
        const auto rows = static_cast<float>(props_.shown);
        if (std::abs(fromPlace_[id] - toPlace_[id]) > rows)
            fromPlace_[id] = toPlace_[id] + (fromPlace_[id] > toPlace_[id] ? rows : -rows);
        if (!ranked_[id]) {
            // a new one: where it goes, as large as it is
            ranked_[id] = 1;
            fromPlace_[id] = toPlace_[id];
            std::copy_n(to_.begin() + static_cast<std::ptrdiff_t>(id * parts), parts, from_.begin() + static_cast<std::ptrdiff_t>(id * parts));
        }
    }
    limit_ = limit;
    othersCount_ = n - limit;
    scroll_ = std::clamp(scroll_, 0.0, lastTop());

    // as long as since the last change (a race flows on at its data's pace), linearly; a change alone, eased
    Duration duration = transition();
    Easing easing = Easing::InOutCubic;
    if (const Scheduler* clock = scheduler(); duration > Duration::zero() && clock) {
        const TimePoint now = clock->now();
        if (props_.pace.count() > 0) {
            duration = props_.pace;
        } else if (lastChange_ && now - *lastChange_ <= Duration(slowestPace)) {
            duration = std::max(duration, now - *lastChange_);
            easing = Easing::Linear;
        }
        lastChange_ = now;
    }
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, duration, easing);
}

// ---- the view ----

void BarRace::scrollTo(double rank, bool animated) {
    rank = std::clamp(rank, 0.0, lastTop());
    const UiContext* ui = context();
    if (animated && ui && scheduler())
        scrolling_.scrollTo(scheduler(), scroll_, rank, ui->theme.motion.scroll);
    else {
        scrolling_.stop();
        scroll_ = rank;
        invalidatePaint();
    }
}

// ---- painting ----

void BarRace::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    settle();
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const float hair = 1.0f / ppd;
    const bool rtl = rightToLeft();
    const float line = textSize("0").height;
    const auto rows = static_cast<float>(props_.shown);
    const auto top = static_cast<float>(scroll_);

    // the value axis's labels along the top; the style's scroll bar at the end side when there is more than shows
    const ScrollBarStyle& bar = context.ui.theme.scrollBar;
    Rect plot = area;
    plot.y += line + 6;
    plot.height -= line + 6;
    const auto rowsInAll = static_cast<float>(rowCount());
    const bool more = rowsInAll > rows;
    const float thumbRoom = bar.width + 4;
    const auto limit = static_cast<float>(limit_);
    if (plot.height < 10 || plot.width < 40)
        return;
    slot_ = plot.height / rows;
    // the rows that show (a part of one at either end too); the names as wide as the widest of theirs (a third of it at
    // most), the highest bar's total at the end: the scale
    visible_.clear();
    float names = 0;
    double highest = 0;
    // (the places side by side: a race of many read through quickly)
    const float t = progress_.value();
    for (std::size_t id = 0; id < names_.size(); ++id) {
        // (one among the others, there and staying: not a bar of its own)
        if (fromPlace_[id] >= limit && toPlace_[id] >= limit)
            continue;
        const float place = fromPlace_[id] + (toPlace_[id] - fromPlace_[id]) * t - top;
        if (place <= -1 || place >= rows)
            continue;
        visible_.push_back(id);
        names = std::max(names, textSize(translate(names_[id])).width);
        // the scale: of those wholly in view (one sliding out does not stretch it)
        if (place > -0.5f && place < rows - 0.5f)
            highest = std::max(highest, shownTotal(id));
    }
    if (highest <= 0)
        highest = 1;
    std::string othersName;
    const float othersPlace = limit - top;
    const bool othersShow = othersCount_ > 0 && othersPlace > -1 && othersPlace < rows;
    if (othersShow) {
        othersName = translate(props_.others) + " (" + formatNumber(static_cast<double>(othersCount_), 0) + ")";
        names = std::max(names, textSize(othersName).width);
    }
    names = std::min(names + 10, area.width * 0.3f);
    const float totals = props_.totals ? textSize(format(highest, 0)).width + 12 : 6.0f;
    const float thumb = more ? thumbRoom : 0.0f;
    plot.x += rtl ? totals + thumb : names;
    plot.width -= names + totals + thumb;
    plot_ = plot;
    if (plot.width < 10)
        return;
    const auto x = [&](double v) {
        const auto f = static_cast<float>(v / highest);
        return rtl ? plot.right() - f * plot.width : plot.x + f * plot.width;
    };

    // the grid at round values, gliding as the scale changes
    valueAxis_.orientation = Orientation::Horizontal;
    valueAxis_.reversed = rtl;
    valueAxis_.format = format_;
    valueAxis_.fit(*this, {0, highest}, plot.width);
    valueAxis_.paintGrid(p, plot, s.grid);
    p.fillRect({snap(x(0), ppd), plot.y, hair, plot.height}, s.axis);
    valueAxis_.paintLabels(context, *this, plot, Axis::Side::Top, s.text, area);
    // the period, large in the end corner at the bottom, under the bars
    if (!period_.source.empty()) {
        const float size = std::clamp(plot.height * 0.16f, s.titleSize, s.titleSize * 5);
        drawText(context, translate(period_), {rtl ? plot.x + 8 : plot.right() - 8, plot.bottom() - 4}, withAlpha(s.title, s.title.a * 0.28f),
                 size, {rtl ? 0.0f : 1.0f, 1});
    }

    // the bars: those lower first, so that one going up passes over one going down
    const float thickness = std::max(1.0f, slot_ * band);
    std::sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) { return shownPlace(a) > shownPlace(b); });
    const float labelSize = thickness >= s.fontSize * 1.15f ? s.fontSize : std::max(7.0f, thickness * 0.85f);
    const bool labels = thickness >= 6;
    bars_.clear();
    for (std::size_t id : visible_) {
        const float place = shownPlace(id) - top;
        const float y = plot.y + place * slot_ + (slot_ - thickness) / 2;
        const float middle = y + thickness / 2;
        p.pushClip(plot);
        double at = 0;
        for (std::size_t k = 0; k < partCount(); ++k) {
            const double v = shownPart(id, k);
            if (v <= 0)
                continue;
            const float x0 = x(at), x1 = x(at + v);
            Rect r{std::min(x0, x1), y, std::abs(x1 - x0), thickness};
            if (k > 0 && r.width > 2) {
                // the parts of a bar a hairline apart
                r.width -= 1;
                if (!rtl)
                    r.x += 1;
            }
            Color color = colorOf(id, k);
            if (hovered_ == id)
                color = mix(color, Color{1, 1, 1, color.a}, 0.14f);
            const float radius = std::min({s.barRadius, r.width / 2, r.height / 2});
            if (r.width >= 3 && r.height >= 3)
                paintRaised(p, r, radius, color, {0, 0, 0, 0}, s.barBevel);
            else
                p.fillRect(r, color);
            at += v;
        }
        p.popClip();
        const float end = x(at);
        bars_.push_back({Rect{std::min(x(0), end), y, std::abs(end - x(0)), thickness}, id});
        if (!labels || middle < plot.y || middle > plot.bottom())
            continue;
        // its name in the column at the start side (cut to it), its total at its end
        p.pushClip({rtl ? plot.right() + thumb : area.x, plot.y, names, plot.height});
        drawText(context, translate(names_[id]), {rtl ? plot.right() + thumb + 8 : plot.x - 8, middle}, s.title, labelSize,
                 {rtl ? 0.0f : 1.0f, 0.5f});
        p.popClip();
        if (props_.totals) {
            p.pushClip({area.x, plot.y, area.width, plot.height});
            drawText(context, format(at, 0), {end + (rtl ? -6.0f : 6.0f), middle}, s.text, labelSize, {rtl ? 1.0f : 0.0f, 0.5f});
            p.popClip();
        }
    }
    // the others: one bar under the top ones, gray (its parts' grays apart), cut at the end of the plot when it is longer
    if (othersShow) {
        const float y = plot.y + othersPlace * slot_ + (slot_ - thickness) / 2;
        const float middle = y + thickness / 2;
        p.pushClip(plot);
        double at = 0;
        for (std::size_t k = 0; k < partCount(); ++k) {
            const double v = shownOthers(k);
            if (v <= 0)
                continue;
            const float x0 = x(at), x1 = x(at + v);
            Rect r{std::min(x0, x1), y, std::abs(x1 - x0), thickness};
            if (k > 0 && r.width > 2) {
                r.width -= 1;
                if (!rtl)
                    r.x += 1;
            }
            Color color = props_.parts.size() > 1 ? mix(seriesColor(k), s.text, 0.6f) : s.text;
            if (hovered_ == othersItem)
                color = mix(color, Color{1, 1, 1, color.a}, 0.14f);
            const float radius = std::min({s.barRadius, r.width / 2, r.height / 2});
            if (r.width >= 3 && r.height >= 3)
                paintRaised(p, r, radius, color, {0, 0, 0, 0}, s.barBevel);
            else
                p.fillRect(r, color);
            at += v;
        }
        p.popClip();
        const float end = std::clamp(x(at), plot.x, plot.right());
        bars_.push_back({Rect{std::min(x(0), end), y, std::abs(end - x(0)), thickness}, othersItem});
        if (labels && middle >= plot.y && middle <= plot.bottom()) {
            p.pushClip({rtl ? plot.right() + thumb : area.x, plot.y, names, plot.height});
            drawText(context, othersName, {rtl ? plot.right() + thumb + 8 : plot.x - 8, middle}, s.text, labelSize, {rtl ? 0.0f : 1.0f, 0.5f});
            p.popClip();
            if (props_.totals) {
                p.pushClip({area.x, plot.y, area.width, plot.height});
                const float past = rtl ? std::max(end - 6, area.x + totals) : std::min(end + 6, area.right() - thumb - totals + 6);
                drawText(context, format(at, 0), {past, middle}, s.text, labelSize, {rtl ? 1.0f : 0.0f, 0.5f});
                p.popClip();
            }
        }
    }
    // the scroll bar, as a scroll area's: thin until the mouse is over it or its thumb is dragged
    if (more) {
        barTrack_ = {rtl ? area.x : area.right() - bar.width, plot.y, bar.width, plot.height};
    } else {
        barTrack_ = {};
    }
    placeBar();
    bar_.paint(context, bar);

    // the bar under the mouse: its parts, its total and its rank by it (the others': their parts, total and count)
    if (hovered_ == othersItem && mouse_ && othersCount_ > 0 && !dragging_) {
        std::vector<ToolTipRow> lines;
        double sum = 0;
        for (std::size_t k = 0; k < partCount(); ++k) {
            sum += othersTo_[k];
            if (props_.parts.size() > 1 && partVisible(k))
                lines.push_back({mix(seriesColor(k), s.text, 0.6f), translate(props_.parts[k]), format(othersTo_[k], 0)});
        }
        lines.push_back({std::nullopt, "\xCE\xA3", format(sum, 0)});     // Σ
        paintToolTip(context, *mouse_, othersName, lines);
    }
    if (hovered_ && mouse_ && *hovered_ < names_.size() && !dragging_) {
        const std::size_t id = *hovered_;
        std::vector<ToolTipRow> lines;
        if (props_.parts.size() > 1)
            for (std::size_t k = 0; k < partCount(); ++k)
                if (partVisible(k))
                    lines.push_back({colorOf(id, k), translate(props_.parts[k]), format(target(id, k), 0)});
        lines.push_back({props_.parts.size() > 1 ? std::nullopt : std::optional(colorOf(id, 0)), "#" + std::to_string(rank_[id] + 1),
                        format(total(id), 0)});
        paintToolTip(context, *mouse_, translate(names_[id]), lines);
    }
}

// ---- the mouse ----

std::optional<std::size_t> BarRace::itemAt(Point at) const {
    for (std::size_t i = bars_.size(); i-- > 0;)
        if (bars_[i].rect.inflated(1).contains(at) && plot_.contains(at))
            return bars_[i].item;
    return std::nullopt;
}

void BarRace::placeBar() {
    const UiContext* ui = context();
    if (barTrack_.empty() || !ui) {
        bar_.hide();
        return;
    }
    bar_.place(barTrack_, static_cast<double>(rowCount()), static_cast<double>(props_.shown), scroll_, scrolling_.target(scroll_),
               rightToLeft(), ui->theme.scrollBar.minimumThumb);
}

void BarRace::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    placeBar();
    // its scroll bar first: pressed, its thumb dragged, let go
    if (event.type != MouseEventType::Wheel && bar_.mouse(event)) {
        event.handled = true;
        invalidatePaint();
        return;
    }
    switch (event.type) {
    case MouseEventType::Move:
        mouse_ = event.position;
        if (dragging_) {
            // the rows follow the mouse
            const double by = (dragFrom_.y - event.position.y) / std::max(slot_, 1.0f);
            scrollTo(dragScroll_ + by, false);
        } else {
            hovered_ = bar_.hovered() ? std::nullopt : itemAt(event.position);
        }
        invalidatePaint();
        return;
    case MouseEventType::Wheel:
        if (platform::hasAny(event.modifiers, platform::KeyModifiers::Control)) {
            // in: fewer rows, larger; out: more
            const int steps = static_cast<int>(std::lround(event.wheelDelta.y));
            if (steps != 0)
                setShown(static_cast<std::size_t>(std::max<long long>(1, static_cast<long long>(props_.shown) - steps)));
            event.handled = true;
        } else {
            // as every scrolling widget: further as the wheel turns faster; not taken when it moves nothing
            const WheelScroll w = wheelScroll(scrolling_, scheduler(), scroll_, -event.wheelDelta.y * wheelRows, event.wheelPrecise, lastTop());
            const UiContext* ui = context();
            if (w.moved)
                scrolling_.scrollTo(scheduler(), scroll_, w.target, ui ? Duration(ui->theme.motion.scroll) : Duration::zero(), w.aimed);
            event.handled = w.moved;
        }
        return;
    case MouseEventType::Press:
        if (event.button != platform::MouseButton::Left)
            return;
        if (event.clicks == 2) {
            setShown(firstShown_);
            scrollTo(0);
        } else {
            dragging_ = true;
            dragFrom_ = event.position;
            dragScroll_ = scroll_;
            scrolling_.stop();
        }
        event.handled = true;
        return;
    case MouseEventType::Release:
        if (dragging_) {
            dragging_ = false;
            event.handled = true;
            invalidatePaint();
        }
        return;
    }
}

void BarRace::onHoverChanged(bool hovered) {
    if (!hovered) {
        hovered_.reset();
        mouse_.reset();
        bar_.leave();
    }
    invalidatePaint();
}

std::optional<platform::Cursor> BarRace::cursorAt(Point point) const {
    if (dragging_)
        return platform::Cursor::Move;
    return Widget::cursorAt(point);
}

std::vector<Chart::LegendItem> BarRace::legendItems() const {
    std::vector<LegendItem> items;
    if (props_.parts.size() < 2)
        return items;
    for (std::size_t k = 0; k < props_.parts.size(); ++k)
        items.push_back({translate(props_.parts[k]), seriesColor(k), partVisible(k)});
    return items;
}

void BarRace::legendItemClicked(std::size_t index) {
    setPartVisible(index, !partVisible(index));
}

} // namespace glw
