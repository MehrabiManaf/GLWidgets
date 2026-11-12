// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/Gauge.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace glw {

namespace {

constexpr float pi = std::numbers::pi_v<float>;

} // namespace

Gauge::Gauge() : Gauge(Props{}) {}

Gauge::Gauge(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setLegendVisible(false);
    value_ = from_ = props_.range.low;
    progress_.set(1);
}

Gauge::~Gauge() = default;

double Gauge::shownValue() const {
    return from_ + (value_ - from_) * progress_.value();
}

void Gauge::setValue(double value) {
    if (value == value_)
        return;
    from_ = shownValue();
    value_ = value;
    const Duration duration = transition();
    if (duration <= Duration::zero()) {
        progress_.set(1);
    } else {
        progress_.set(0);
        progress_.animateTo(scheduler(), 1, duration, Easing::OutCubic);
    }
    invalidatePaint();
}

void Gauge::setRange(ChartRange range) {
    props_.range = range;
    invalidatePaint();
}

void Gauge::setBands(std::vector<Band> bands) {
    props_.bands = std::move(bands);
    invalidatePaint();
}

void Gauge::setUnit(std::string unit) {
    props_.unit = std::move(unit);
    invalidatePaint();
}

void Gauge::setFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

Color Gauge::colorOf(double value) const {
    for (const Band& band : props_.bands)
        if (value <= band.to)
            return band.color;
    return props_.color ? *props_.color : seriesColor(props_.series);
}

std::string Gauge::format(double value) const {
    if (format_)
        return format_(value);
    // as many decimals as a hundredth of its range wants
    const double step = std::max(props_.range.span() / 100, 1e-12);
    return formatNumber(value, std::clamp(static_cast<int>(std::ceil(-std::log10(step))), 0, 6));
}

Size Gauge::measureContent(Size available) {
    return {std::min(available.width, 180.0f), std::min(available.height, 160.0f)};
}

float Gauge::paintNeedle(PaintContext& context, Point at, float angle, float length, float radius, Color color) const {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const Bevel& bevel = s.barBevel;
    const bool flat = bevel.relief <= 0 && bevel.gradient <= 0 && bevel.highlight.a <= 0;
    // its shape: widest at the hub, tapering to a round tip, a short round tail behind the hub (half widths)
    const float wide = std::max(2.0f, radius * 0.05f);
    const float tip = std::max(0.9f, wide * 0.3f), end = wide * 0.65f;
    const float tail = std::max(5.0f, radius * 0.16f);
    const Point dir{std::cos(angle), std::sin(angle)}, across{-dir.y, dir.x};
    const auto point = [&](Point origin, float along, float side) {
        return Point{origin.x + dir.x * along + across.x * side, origin.y + dir.y * along + across.y * side};
    };
    // a half of it (`side` 1 or -1): from the tip's front around its end to where its side leaves it, to the hub, along to
    // the tail's end and around it to its back; its round ends meet its sides where they are tangent to them
    const float tipCenter = length - tip, tailCenter = -tail + end;
    const float toTip = std::atan2(wide - tip, tipCenter), toTail = std::atan2(wide - end, tail - end);
    constexpr int steps = 6;
    const auto half = [&](Point origin, float side, std::vector<Point>& out) {
        for (int k = 0; k <= steps; ++k) {
            const float t = (pi / 2 - toTip) * static_cast<float>(k) / steps;
            out.push_back(point(origin, tipCenter + tip * std::cos(t), side * tip * std::sin(t)));
        }
        out.push_back(point(origin, 0, side * wide));
        for (int k = 0; k <= steps; ++k) {
            const float t = pi / 2 + toTail + (pi / 2 - toTail) * static_cast<float>(k) / steps;
            out.push_back(point(origin, tailCenter + end * std::cos(t), side * end * std::sin(t)));
        }
    };
    // all of it: a half, the other back (without their shared front and back points)
    const auto outline = [&](Point origin, std::vector<Point>& out) {
        out.clear();
        half(origin, 1, out);
        std::vector<Point> other;
        half(origin, -1, other);
        out.insert(out.end(), other.rbegin() + 1, other.rend() - 1);
    };
    std::vector<Point> shape, face;
    const float hub = wide * 1.7f;
    if (flat) {
        outline(at, shape);
        p.fillConvexPolygon(shape, color);
        p.fillCircle(at, hub, s.text);
        p.fillCircle(at, hub * 0.4f, color);
        return hub;
    }
    // lit by the theme's light (from the upper left): its shadow away from it, the face toward it lighter than the other
    const paint::Lighting& light = context.ui.theme.lighting;
    const float lightLength = std::max(1e-3f, std::hypot(light.x, light.y));
    const Point toward{light.x / lightLength, light.y / lightLength};
    const Point drop{at.x - toward.x * std::max(1.5f, wide * 0.6f), at.y - toward.y * std::max(1.5f, wide * 0.6f)};
    const Color black{0, 0, 0, 1};
    const Color shadow = bevel.shadow.a > 0 ? withAlpha(bevel.shadow, bevel.shadow.a * 0.8f) : withAlpha(black, 0.25f);
    outline(drop, shape);
    p.fillConvexPolygon(shape, shadow);
    outline(at, shape);
    p.fillConvexPolygon(shape, lit(color, -0.14f));
    half(at, across.x * toward.x + across.y * toward.y > 0 ? 1.0f : -1.0f, face);
    p.fillConvexPolygon(face, lit(color, 0.16f));
    // the hub: a cap over its root, raised as the style's bars are, its middle of the needle's color
    const Rect cap{at.x - hub, at.y - hub, 2 * hub, 2 * hub};
    p.fillCircle({drop.x, drop.y}, hub, shadow);
    if (bevel.relief > 0)
        p.fillRelief(cap, hub, bevel.relief * 1.4f, hub * 0.7f, s.text);
    else
        p.fillRoundedRect(cap, hub, lit(s.text, 0.10f), lit(s.text, -0.18f));
    p.fillCircle(at, hub * 0.4f, color);
    return hub;
}

void Gauge::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const ChartRange range = props_.range.span() > 0 ? props_.range : ChartRange{props_.range.low, props_.range.low + 1};
    const float sweep = std::clamp(props_.sweep, 10.0f, 360.0f) * pi / 180;
    const float start = pi / 2 + (2 * pi - sweep) / 2;          // open at the bottom, as much on each side
    const auto angleOf = [&](double v) {
        return start + static_cast<float>(std::clamp((v - range.low) / range.span(), 0.0, 1.0)) * sweep;
    };

    // how far the arc reaches each way from its center (a unit radius): its ends, the turns it passes, its center
    float left = 0, right = 0, top = 0, bottom = 0;
    const auto reach = [&](float a) {
        left = std::min(left, std::cos(a));
        right = std::max(right, std::cos(a));
        top = std::min(top, std::sin(a));
        bottom = std::max(bottom, std::sin(a));
    };
    reach(start);
    reach(start + sweep);
    for (float a = 0; a < 4 * pi; a += pi / 2)
        if (a > start && a < start + sweep)
            reach(a);
    // under its middle: its value's unit (and with a needle, its value); under the ends: theirs
    const float line = textSize("0").height;
    const float under = (props_.ends ? line + 4 : 0) + (props_.needle ? 2 * line + 8 : 0);
    const float radius = std::max(0.0f, std::min(area.width / (right - left), (area.height - under) / (bottom - top)) - 1);
    if (radius < 8)
        return;
    // across the middle of its room, at the top of it (under its title)
    const Point at{area.x + area.width / 2 - radius * (left + right) / 2, area.y - radius * top};
    const float thickness = std::clamp(props_.thickness, 0.02f, 1.0f) * radius;
    const float inner = radius - thickness;
    // coming in (Chart::beginEntrance): the dial as one piece standing on its bottom, its value sweeping up from its low
    // end as it comes
    beginEntrance();
    const bool coming = entering();
    const paint::Painter::Mark mark = coming ? p.markHere() : paint::Painter::Mark{};
    const Rect dial{at.x + radius * left, at.y + radius * top, radius * (right - left), radius * (bottom - top)};

    // its track; its bands along its inside (a thin ring, a gap in from the track); the value over the track
    // (flat; its ends as round as the value's in a style of reliefs: a relief of no height)
    const Color track = withAlpha(s.text, s.text.a * 0.12f);
    if (s.barBevel.relief > 0)
        p.fillArcRelief(at, inner, radius, start, sweep, 0, 0, 0.5f, track, s.barRadius);
    else
        p.fillArc(at, inner, radius, start, sweep, 0, track);
    if (!props_.bands.empty()) {
        const float band = std::max(2.0f, thickness * 0.18f);
        double from = range.low;
        for (const Band& b : props_.bands) {
            const float a0 = angleOf(from), a1 = angleOf(b.to);
            if (a1 > a0)
                p.fillArc(at, inner - s.gap - band, inner - s.gap, a0, a1 - a0, 0, b.color);
            from = std::max(from, b.to);
        }
    }
    double shown = shownValue();
    if (coming) {
        const float x = std::min(1.0f, entranceProgress() / 0.8f);
        shown = range.low + (shown - range.low) * static_cast<double>(x < 0.5f ? 4 * x * x * x : 1 - std::pow(-2 * x + 2, 3.0f) / 2);
    }
    const Color color = colorOf(shown);
    const float a1 = angleOf(shown);
    if (a1 > start + 1e-4f)
        paintArc(p, at, inner, radius, start, a1 - start, 0, color);

    // a needle from its middle to the value, a hub over its root
    const float valueSize = std::clamp(radius * (props_.needle ? 0.22f : 0.42f), s.fontSize, 64.0f);
    float hubRadius = 0;
    if (props_.needle) {
        hubRadius = paintNeedle(context, at, a1, inner - std::max(2.0f, thickness * 0.18f) - s.gap - 2, radius, color);
        // the value under the hub, the unit under that
        const float y = at.y + hubRadius + 6;
        drawText(context, format(shown), {at.x, y}, s.title, valueSize, {0.5f, 0});
        if (!props_.unit.empty())
            drawText(context, props_.unit, {at.x, y + textSize("0", valueSize).height}, s.text, 0, {0.5f, 0});
    } else {
        // the value in its middle (its middle: the hole's, lowered to its line when it is a half circle), the unit under it
        const float unitHeight = props_.unit.empty() ? 0 : line;
        const float middle = bottom < 0.3f ? at.y - textSize("0", valueSize).height / 2 - unitHeight : at.y;
        drawText(context, format(shown), {at.x, middle - unitHeight / 2}, s.title, valueSize, {0.5f, 0.5f});
        if (!props_.unit.empty())
            drawText(context, props_.unit, {at.x, middle + textSize("0", valueSize).height / 2 - unitHeight / 2 + 2}, s.text, 0,
                     {0.5f, 0});
    }

    // its range's ends, under the arc's
    if (props_.ends && sweep < 2 * pi - 0.01f) {
        const auto end = [&](float a, double v) {
            const float r = (inner + radius) / 2;
            // a round end without decimals
            const std::string text = format_ ? format_(v) : v == std::floor(v) ? formatNumber(v, 0) : format(v);
            drawText(context, text, {at.x + std::cos(a) * r, at.y + std::sin(a) * r + thickness / 2 + 3}, s.text, 0, {0.5f, 0});
        };
        end(start, range.low);
        end(start + sweep, range.high);
    }
    if (coming)
        placeArriving(p, mark, arrived(0), dial, Point{dial.center().x, dial.bottom()}, {0, -1});
}

} // namespace glw
