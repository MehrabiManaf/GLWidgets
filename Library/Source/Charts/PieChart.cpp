// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/PieChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float pi = 3.14159265358979f;
constexpr float turn = 2 * pi;
constexpr float popOut = 7;             // how far the slice under the mouse comes out, dips

} // namespace

PieChart::PieChart(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), sweep_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    progress_.set(1);
    sweep_.set(0);
}

PieChart::~PieChart() = default;

double PieChart::target(std::size_t index) const {
    return index < slices_.size() && !hidden_[index] ? std::max(0.0, slices_[index].value) : 0.0;
}

double PieChart::shownValue(std::size_t index) const {
    if (index >= from_.size() || index >= to_.size())
        return target(index);
    return from_[index] + (to_[index] - from_[index]) * progress_.value();
}

void PieChart::setSlices(std::vector<PieSlice> slices) {
    // from what shows now: a slice that is new from nothing
    std::vector<double> now(slices.size(), 0.0);
    for (std::size_t i = 0; i < std::min(now.size(), slices_.size()); ++i)
        now[i] = shownValue(i);
    slices_ = std::move(slices);
    hidden_.resize(slices_.size(), false);
    while (out_.size() < slices_.size())
        out_.push_back(std::make_unique<Animation>([this] { invalidatePaint(); }));
    out_.resize(slices_.size());
    if (hovered_ && *hovered_ >= slices_.size())
        hovered_.reset();
    from_ = std::move(now);
    to_.resize(slices_.size());
    for (std::size_t i = 0; i < slices_.size(); ++i)
        to_[i] = target(i);
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, transition());
    invalidatePaint();
}

void PieChart::setValue(std::size_t index, double value) {
    if (index >= slices_.size() || slices_[index].value == value)
        return;
    slices_[index].value = value;
    valuesChanged();
}

void PieChart::valuesChanged() {
    std::vector<double> now(slices_.size());
    for (std::size_t i = 0; i < slices_.size(); ++i)
        now[i] = shownValue(i);
    from_ = std::move(now);
    to_.resize(slices_.size());
    for (std::size_t i = 0; i < slices_.size(); ++i)
        to_[i] = target(i);
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, transition());
    invalidatePaint();
}

void PieChart::setSliceVisible(std::size_t index, bool visible) {
    if (index >= slices_.size() || hidden_[index] == !visible)
        return;
    hidden_[index] = !visible;
    valuesChanged();
}

void PieChart::setHole(float hole) {
    props_.hole = hole;
    invalidatePaint();
}

void PieChart::setStartAngle(float degrees) {
    props_.startAngle = degrees;
    invalidatePaint();
}

void PieChart::setFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string PieChart::format(double value) const {
    if (format_)
        return format_(value);
    const int decimals = value == std::floor(value) ? 0 : std::abs(value) < 10 ? 2 : 1;
    return formatNumber(value, decimals);
}

Color PieChart::colorOf(std::size_t index) const {
    return slices_[index].color ? *slices_[index].color : seriesColor(index);
}

std::optional<std::size_t> PieChart::sliceAt(Point at) const {
    const Point d = at - center_;
    const float r = std::sqrt(d.x * d.x + d.y * d.y);
    if (radius_ <= 0 || r < inner_ || r > radius_ + popOut)
        return std::nullopt;
    double total = 0;
    for (std::size_t i = 0; i < slices_.size(); ++i)
        total += shownValue(i);
    if (total <= 0)
        return std::nullopt;
    // its angle from the first slice's start, clockwise, in turns
    const float start = props_.startAngle * pi / 180;
    float a = std::fmod(std::atan2(d.y, d.x) - start, turn);
    if (a < 0)
        a += turn;
    const double where = a / turn * total;
    double sum = 0;
    for (std::size_t i = 0; i < slices_.size(); ++i) {
        sum += shownValue(i);
        if (where < sum && shownValue(i) > 0)
            return i;
    }
    return std::nullopt;
}

void PieChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    double total = 0;
    for (std::size_t i = 0; i < slices_.size(); ++i)
        total += shownValue(i);
    // the first time it shows with something to show: around into place
    if (!swept_ && total > 0) {
        swept_ = true;
        const Duration duration = transition() * 3;
        if (duration > Duration::zero())
            sweep_.animateTo(scheduler(), 1, duration, Easing::InOutCubic);
        else
            sweep_.set(1);
    }
    center_ = area.center();
    radius_ = std::max(0.0f, std::min(area.width, area.height) / 2 - popOut - 2);
    inner_ = radius_ * std::clamp(props_.hole, 0.0f, 0.95f);
    if (radius_ < 8)
        return;
    if (total <= 0) {
        p.fillArc(center_, inner_, radius_, 0, turn, 0, s.grid);
        return;
    }
    const float swept = turn * sweep_.value();
    float angle = props_.startAngle * pi / 180;
    // a relief style: its slices darker toward the center, as lit from outside
    const float shade = s.barBevel.relief > 0 ? 0.20f : s.barBevel.gradient * 1.5f;
    for (std::size_t i = 0; i < slices_.size(); ++i) {
        const double v = shownValue(i);
        if (v <= 0)
            continue;
        const float sweep = static_cast<float>(v / total) * swept;
        const float middle = angle + sweep / 2;
        const Point direction{std::cos(middle), std::sin(middle)};
        const Point at = center_ + direction * (out_[i]->value() * popOut);
        Color color = colorOf(i);
        if (hovered_ == i)
            color = mix(color, Color{1, 1, 1, color.a}, 0.10f);
        p.fillArc(at, inner_, radius_, angle, sweep, s.gap / 2, mix(color, Color{0, 0, 0, color.a}, shade), color);
        // its share in it, where it fits
        if (props_.shares && sweep_.value() >= 1) {
            const double share = v / total;
            const std::string text = formatNumber(share * 100, share < 0.1 ? 1 : 0) + "%";
            const Size size = textSize(text);
            const float r = inner_ > 0 ? (inner_ + radius_) / 2 : radius_ * 0.64f;
            if (r * sweep > size.width + 8 && radius_ - inner_ > size.height + 6)
                drawText(context, text, at + direction * r, textOn(color), 0, {0.5f, 0.5f});
        }
        angle += sweep;
    }
    // a donut's hole: its total, or the value of the slice under the mouse
    if (inner_ > 28) {
        const bool one = hovered_.has_value();
        const std::string value = format(one ? shownValue(*hovered_) : total);
        const std::string label = one ? translate(slices_[*hovered_].label) : translate(props_.totalLabel);
        const float big = std::min(inner_ * 0.42f, s.titleSize * 1.6f);
        const float valueHeight = textSize(value, big).height;
        const float labelHeight = label.empty() ? 0.0f : textSize(label).height;
        const float top = center_.y - (valueHeight + labelHeight) / 2;
        if (textSize(value, big).width < inner_ * 1.8f)
            drawText(context, value, {center_.x, top}, s.title, big, {0.5f, 0});
        if (!label.empty() && textSize(label).width < inner_ * 1.8f)
            drawText(context, label, {center_.x, top + valueHeight}, s.text, 0, {0.5f, 0});
    }
    if (hovered_ && mouse_ && *hovered_ < slices_.size()) {
        const double v = shownValue(*hovered_);
        const ToolTipRow row{colorOf(*hovered_), format(v), formatNumber(v / total * 100, 1) + "%"};
        paintToolTip(context, *mouse_, translate(slices_[*hovered_].label), std::span(&row, 1));
    }
}

void PieChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    if (event.type == MouseEventType::Move) {
        mouse_ = event.position;
        const std::optional<std::size_t> over = sliceAt(event.position);
        if (over != hovered_) {
            if (hovered_ && *hovered_ < out_.size())
                out_[*hovered_]->animateTo(scheduler(), 0, transition(true));
            if (over)
                out_[*over]->animateTo(scheduler(), 1, transition(true));
            hovered_ = over;
        }
        invalidatePaint();
    } else if (event.type == MouseEventType::Press && event.button == platform::MouseButton::Left) {
        if (const auto at = sliceAt(event.position)) {
            if (onSliceClicked_)
                onSliceClicked_(*at);
            event.handled = true;
        }
    }
}

void PieChart::onHoverChanged(bool hovered) {
    if (hovered)
        return;
    if (hovered_ && *hovered_ < out_.size())
        out_[*hovered_]->animateTo(scheduler(), 0, transition(true));
    hovered_.reset();
    mouse_.reset();
    invalidatePaint();
}

std::vector<Chart::LegendItem> PieChart::legendItems() const {
    std::vector<LegendItem> items;
    for (std::size_t i = 0; i < slices_.size(); ++i)
        items.push_back({translate(slices_[i].label), colorOf(i), !hidden_[i]});
    return items;
}

void PieChart::legendItemClicked(std::size_t index) {
    setSliceVisible(index, hidden_[index]);
}

} // namespace glw
