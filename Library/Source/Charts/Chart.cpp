// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/Chart.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace glw {

namespace {

constexpr float swatchSize = 10;
constexpr float legendSpacing = 16;     // between its items
constexpr float legendGap = 6;          // between a swatch and its text

} // namespace

Chart::Chart() = default;

Chart::~Chart() {
    setCursor(nullptr);
}

// ---- a shared crosshair ----

void ChartCursor::set(const Chart* from, std::optional<double> x) {
    if (from_ == from && x_ == x)
        return;
    if (!x && from_ != from)
        return;         // (one left that was not the one it was over)
    from_ = x ? from : nullptr;
    x_ = x;
    for (Chart* chart : charts_)
        if (chart != from)
            chart->invalidatePaint();
}

void Chart::setCursor(std::shared_ptr<ChartCursor> cursor) {
    if (cursor_) {
        std::erase(cursor_->charts_, this);
        if (cursor_->from_ == this)
            cursor_->set(this, std::nullopt);
    }
    cursor_ = std::move(cursor);
    if (cursor_)
        cursor_->charts_.push_back(this);
    invalidatePaint();
}

void Chart::shareCursor(std::optional<double> x) const {
    if (cursor_)
        cursor_->set(this, x);
}

std::optional<double> Chart::sharedX() const {
    if (!cursor_ || cursor_->from_ == this)
        return std::nullopt;
    return cursor_->x_;
}

void Chart::setTitle(LocalizedText title) {
    if (title != title_) {
        title_ = std::move(title);
        invalidatePaint();
    }
}

void Chart::setLegendVisible(bool visible) {
    if (visible != legendVisible_) {
        legendVisible_ = visible;
        invalidatePaint();
    }
}

void Chart::setStyle(std::optional<ChartStyle> style) {
    style_ = std::move(style);
    texts_.clear();
    invalidatePaint();
}

const ChartStyle& Chart::style() const {
    static const ChartStyle unstyled = Theme::dark().chart;
    if (style_)
        return *style_;
    const UiContext* ui = context();
    return ui ? ui->theme.chart : unstyled;
}

Color Chart::seriesColor(std::size_t index) const {
    const ChartStyle& s = style();
    return s.series[index % s.series.size()];
}

void Chart::onStyleChanged() {
    texts_.clear();
    invalidatePaint();
}

void Chart::onRootChanged() {
    texts_.clear();
}

void Chart::onLanguageChanged() {
    invalidatePaint();
}

Size Chart::measureContent(Size available) {
    // it takes what it gets: a little, when nothing says how much
    return {std::min(available.width, 360.0f), std::min(available.height, 220.0f)};
}

// ---- text ----

const text::TextLayout* Chart::textLayout(const std::string& text, float size) const {
    const UiContext* ui = context();
    if (!ui || text.empty())
        return nullptr;
    if (size <= 0)
        size = style().fontSize;
    std::string key = std::to_string(static_cast<int>(std::lround(size * 64)));
    key += '|';
    key += text;
    if (auto found = texts_.find(key); found != texts_.end())
        return found->second.get();
    if (texts_.size() > 4096)       // a feed of ever new labels: begun again
        texts_.clear();
    auto made = std::make_unique<text::TextLayout>(text, text::TextStyle{{&ui->fonts.defaultFont()}, size});
    return texts_.emplace(std::move(key), std::move(made)).first->second.get();
}

Size Chart::textSize(const std::string& text, float size) const {
    const text::TextLayout* layout = textLayout(text, size);
    return layout ? layout->size() : Size{};
}

void Chart::drawText(PaintContext& context, const std::string& text, Point at, Color color, float size, Point align) const {
    const text::TextLayout* layout = textLayout(text, size);
    if (!layout || color.a <= 0)
        return;
    const Size s = layout->size();
    context.drawText(*layout, {at.x - s.width * align.x, at.y - s.height * align.y + layout->opticalOffset()}, color);
}

// ---- the frame ----

Rect Chart::paintFrame(PaintContext& context) {
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const bool rtl = rightToLeft();
    if (s.background.a > 0)
        p.fillRect({0, 0, size().width, size().height}, s.background);
    Rect area = contentRect();
    // the title, at the start of the top
    if (!title_.source.empty()) {
        const std::string title = translate(title_);
        const Size t = textSize(title, s.titleSize);
        drawText(context, title, {rtl ? area.right() : area.x, area.y}, s.title, s.titleSize, {rtl ? 1.0f : 0.0f, 0});
        area.y += t.height + 6;
        area.height = std::max(0.0f, area.height - t.height - 6);
    }
    // the legend under it: its items one after the other from the start side, in as many rows as they need
    legendRects_.clear();
    const std::vector<LegendItem> items = legendVisible_ ? legendItems() : std::vector<LegendItem>{};
    if (!items.empty()) {
        const float rowHeight = std::max(swatchSize, textSize("Ag").height) + 4;
        float x = 0, y = 0;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const float width = swatchSize + legendGap + textSize(items[i].text).width;
            if (x > 0 && x + width > area.width) {
                x = 0;
                y += rowHeight;
            }
            const Rect rect{rtl ? area.right() - x - width : area.x + x, area.y + y, width, rowHeight};
            legendRects_.push_back({rect.inflated(2), i});
            const float alpha = items[i].shown ? 1.0f : 0.35f;
            const float sx = rtl ? rect.right() - swatchSize : rect.x;
            p.fillRoundedRect({sx, rect.y + (rowHeight - swatchSize) / 2, swatchSize, swatchSize}, std::min(2.0f, s.barRadius + 1),
                              withAlpha(items[i].color, items[i].color.a * alpha));
            const float tx = rtl ? sx - legendGap : sx + swatchSize + legendGap;
            drawText(context, items[i].text, {tx, rect.y + rowHeight / 2}, withAlpha(s.text, s.text.a * (items[i].shown ? 1.0f : 0.5f)),
                     0, {rtl ? 1.0f : 0.0f, 0.5f});
            x += width + legendSpacing;
        }
        const float height = y + rowHeight + 8;
        area.y += height;
        area.height = std::max(0.0f, area.height - height);
    }
    return area;
}

bool Chart::legendMouse(MouseEvent& event) {
    if (event.type != MouseEventType::Press || event.button != platform::MouseButton::Left)
        return false;
    for (const Item& item : legendRects_) {
        if (item.rect.contains(event.position)) {
            legendItemClicked(item.index);
            event.handled = true;
            return true;
        }
    }
    return false;
}

// ---- the tooltip ----

void Chart::paintToolTip(PaintContext& context, Point at, const std::string& title, std::span<const ToolTipRow> rows) const {
    paint::Painter& p = context.painter;
    const Theme& theme = context.ui.theme;
    const ToolTipStyle& tip = theme.toolTip;
    const bool rtl = rightToLeft();
    constexpr float pad = 8, swatch = 8, gap = 6, between = 18;
    const float line = textSize("Ag").height + 4;
    float width = textSize(title).width;
    bool swatches = false;
    for (const ToolTipRow& row : rows) {
        swatches = swatches || row.swatch.has_value();
        width = std::max(width, (row.swatch ? swatch + gap : 0.0f) + textSize(row.name).width + between + textSize(row.value).width);
    }
    const Size box{width + 2 * pad, 2 * pad + (title.empty() ? 0.0f : line) + line * static_cast<float>(rows.size()) - 4};
    // beside the mouse, on the other side when there is no room
    const Size room = size();
    float x = rtl ? at.x - 16 - box.width : at.x + 16;
    if (x + box.width > room.width - 2 || x < 2)
        x = rtl ? at.x + 16 : at.x - 16 - box.width;
    x = std::clamp(x, 2.0f, std::max(2.0f, room.width - box.width - 2));
    float y = std::clamp(at.y - box.height / 2, 2.0f, std::max(2.0f, room.height - box.height - 2));
    const Rect r{x, y, box.width, box.height};
    const float radius = std::min(theme.frame.popupRadius, 8.0f);
    p.drawShadow({r.x, r.y + 2, r.width, r.height}, radius, 10, Color{0, 0, 0, theme.isDark() ? 0.45f : 0.18f});
    p.fillRoundedRect(r, radius, tip.background);
    p.strokeRoundedRect(r, radius, 1, tip.border);
    const float start = rtl ? r.right() - pad : r.x + pad, end = rtl ? r.x + pad : r.right() - pad;
    const float sign = rtl ? -1.0f : 1.0f;
    float ty = r.y + pad;
    if (!title.empty()) {
        drawText(context, title, {start, ty}, tip.text, 0, {rtl ? 1.0f : 0.0f, 0});
        ty += line;
    }
    for (const ToolTipRow& row : rows) {
        float nx = start;
        if (row.swatch) {
            const float sx = rtl ? start - swatch : start;
            p.fillRoundedRect({sx, ty + (line - 4 - swatch) / 2, swatch, swatch}, 2, *row.swatch);
            nx += sign * (swatch + gap);
        } else if (swatches) {
            nx += sign * (swatch + gap);
        }
        drawText(context, row.name, {nx, ty}, withAlpha(tip.text, tip.text.a * 0.72f), 0, {rtl ? 1.0f : 0.0f, 0});
        drawText(context, row.value, {end, ty}, tip.text, 0, {rtl ? 0.0f : 1.0f, 0});
        ty += line;
    }
}

// ---- helpers ----

void Chart::paintArc(paint::Painter& p, Point at, float r0, float r1, float a0, float sweep, float gap, Color color, float lift) const {
    const Bevel& bevel = style().barBevel;
    if (bevel.relief > 0) {
        // its shadow a little down (what shows of it: along its lower edges), then the relief
        if (bevel.shadow.a > 0)
            p.fillArc({at.x, at.y + bevel.relief * 0.7f}, r0, r1, a0, sweep, gap, withAlpha(bevel.shadow, bevel.shadow.a * 0.6f));
        p.fillArcRelief(at, r0, r1, a0, sweep, gap, bevel.relief * lift, bevel.roundness * 1.5f, color, style().barRadius);
    } else if (bevel.gradient > 0 || bevel.highlight.a > 0) {
        p.fillArc(at, r0, r1, a0, sweep, gap, lit(color, bevel.gradient * 1.5f), lit(color, -bevel.gradient * 0.75f),
                  paint::Painter::ArcShading::Vertical);
        if (bevel.highlight.a > 0)
            p.fillArc(at, r1 - 1.5f, r1, a0, sweep, gap, bevel.highlight, withAlpha(bevel.highlight, 0), paint::Painter::ArcShading::Vertical);
    } else {
        p.fillArc(at, r0, r1, a0, sweep, gap, color);
    }
}

// ---- its pieces coming in ----

void Chart::playEntrance() {
    entered_ = false;
    entering_ = false;
    entrance_.set(1);
    invalidatePaint();
}

void Chart::beginEntrance() {
    if (entered_)
        return;
    entered_ = true;
    const Duration normal = transition();
    if (normal <= Duration::zero())
        return;
    const UiContext* ui = context();
    const std::chrono::milliseconds own = style().entrance;
    entranceWay_ = ui && ui->theme.motion.varied ? randomWay(solidPieces() ? 7 : 3) : 0;
    entering_ = true;
    entrance_.set(0);
    entrance_.animateTo(scheduler(), 1, own.count() > 0 ? Duration(own) : normal * 4, Easing::Linear);
}

float Chart::entranceProgress() const {
    return entering_ ? entrance_.value() : 1.0f;
}

float Chart::arrived(float order) const {
    if (!entering_)
        return 1;
    // each a little more than half of the time, the last starting as the first is nearly there
    return std::clamp((entrance_.value() - 0.45f * std::clamp(order, 0.0f, 1.0f)) / 0.55f, 0.0f, 1.0f);
}

bool Chart::solidPieces() const {
    const UiContext* ui = context();
    return ui && ui->theme.reliefBricks && style().barBevel.relief > 0;
}

void Chart::placeArriving(paint::Painter& p, const paint::Painter::Mark& mark, float u, const Rect& rect, std::optional<Point> base, Point out) const {
    if (u >= 1)
        return;
    constexpr float pi = 3.14159265f;
    const auto outCubic = [](float x) { return 1 - (1 - x) * (1 - x) * (1 - x); };
    const auto outBack = [](float x, float over) {
        const float v = x - 1;
        return 1 + (over + 1) * v * v * v + over * v * v;
    };
    const auto smooth = [](float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * (3 - 2 * x);
    };
    const float length = std::hypot(out.x, out.y);
    const Point way = length > 1e-4f ? Point{out.x / length, out.y / length} : Point{0, 1};
    // a tile hinges on its top, grows out of its middle
    const Point hinge = base ? *base : Point{rect.center().x, rect.y};
    const Point grows = base ? *base : rect.center();
    const float e = outCubic(u);
    const float size = std::max(rect.width, rect.height);
    paint::Lifted lifted;
    Point pivot = rect.center();
    // (the middle of all of it, its own coordinates; the way to its start side)
    const Point middle{Widget::size().width / 2, Widget::size().height / 2};
    const float start = rightToLeft() ? 1.0f : -1.0f;
    const auto turned = [](Point v, float angle) {
        const float c = std::cos(angle), s = std::sin(angle);
        return Point{v.x * c - v.y * s, v.x * s + v.y * c};
    };
    if (solidPieces()) {
        switch (entranceWay_) {
        case 3: {
            // turning in from edge on about its upright middle, up off the plot over an arc and down again (a little past
            // facing, and back), the light sweeping across it as it turns
            const float facing = outBack(u, 1.4f);
            lifted = {.lift = std::min(36.0f + 0.25f * size, 90.0f) * std::sin(pi * e),
                      .turn = paint::Quaternion::axisAngle({0, 1, 0}, start * 1.45f * (1 - facing)) * paint::towardLight(0.25f * std::sin(pi * e)),
                      .alpha = smooth(u / 0.2f)};
            break;
        }
        case 4: {
            // spiralling in from far behind, round the middle of the chart: a part of a turn still to go round it, smaller
            // and further the more is left; tipped as it comes round, flat as it lands
            const float left = (1 - e) * (1 - e);
            const float round = 1.7f * left;
            const Point from = rect.center() - middle;
            const Point at = middle + turned(from, -round);
            lifted = {.lift = -std::min(160.0f + 0.6f * size, 380.0f) * left,
                      .turn = paint::Quaternion::axisAngle({0, 0, 1}, -round) * paint::Quaternion::axisAngle({from.y, -from.x, 0}, 0.6f * left) *
                              paint::towardLight(0.3f * std::sin(pi * e)),
                      .shift = at - rect.center(),
                      .alpha = smooth(u / 0.3f),
                      .scale = 0.55f + 0.45f * outBack(u, 1.2f)};
            break;
        }
        case 5: {
            // a wave across the plot: from under it, up through it toward the one looking and back down onto it, rocking
            // with the wave as it passes (its front edge up first)
            const float z = -90 * (1 - u) * (1 - u) + 28 * std::sin(pi * u) * (1 - 0.4f * u);
            const float rock = 0.5f * std::cos(pi * u) * (1 - u);
            lifted = {.lift = z, .turn = paint::Quaternion::axisAngle({0, 1, 0}, -start * rock) * paint::towardLight(0.2f * std::sin(pi * u)),
                      .alpha = smooth(u / 0.3f)};
            break;
        }
        case 6: {
            // thrown in from beyond its start side, high over the plot, tumbling over about its long middle, landing with
            // a small bounce
            constexpr float landed = 0.82f;
            const float s = std::min(1.0f, u / landed);
            const float c = 1 - (1 - s) * (1 - s);
            float lift = std::min(110.0f + 0.4f * size, 220.0f) * std::sin(pi * 0.5f * (1 + c)) * (1 - c * 0.15f);
            lift *= 1 - c;
            if (u > landed) {
                const float b = (u - landed) / (1 - landed);
                lift = 5 * std::sin(pi * b) * (1 - b);
            }
            const paint::Float3 along = rect.width >= rect.height ? paint::Float3{1, 0, 0} : paint::Float3{0, 1, 0};
            lifted = {.lift = lift,
                      .turn = paint::Quaternion::axisAngle(along, 2 * pi * (1 - c)) * paint::towardLight(0.2f * std::sin(pi * c)),
                      .shift = {start * (Widget::size().width * 0.75f) * (1 - c), -Widget::size().height * 0.25f * std::sin(pi * c)},
                      .alpha = smooth(u / 0.15f)};
            break;
        }
        case 1: {
            // standing on its base toward the one looking, swinging down onto the plot (a little past, and back)
            const float angle = 1.35f * (1 - outBack(u, 1.3f));
            lifted = {.turn = paint::Quaternion::axisAngle({-way.y, way.x, 0}, -angle), .alpha = std::min(1.0f, u * 5)};
            pivot = hinge;
            break;
        }
        case 2: {
            // from behind the plot, growing as it comes nearer, the light catching it on the way
            lifted = {.lift = -std::min(60.0f + 0.4f * size, 160.0f) * (1 - e) * (1 - e),
                      .turn = paint::towardLight(0.3f * std::sin(pi * e)),
                      .alpha = smooth(u / 0.35f),
                      .scale = 0.7f + 0.3f * outBack(u, 1.5f)};
            break;
        }
        default: {
            // from in front: down faster and faster, turned a little (and catching the light), a small bounce as it lands
            constexpr float landed = 0.8f;
            const float left = 1 - std::min(1.0f, u / landed);
            float lift = std::min(70.0f + 0.3f * size, 150.0f) * left * left;
            if (u > landed) {
                const float b = (u - landed) / (1 - landed);
                lift = 4 * std::sin(pi * b) * (1 - b);
            }
            lifted = {.lift = lift,
                      .turn = paint::towardLight(0.35f * left * left) * paint::Quaternion::axisAngle({way.x, way.y, 0}, 0.25f * left * left),
                      .alpha = std::min(1.0f, u * 5)};
            break;
        }
        }
    } else {
        switch (entranceWay_) {
        case 1:
            // sliding out of where it stands (a tile: in from the start side), fading in
            lifted = {.shift = (length > 1e-4f ? way : Point{rightToLeft() ? -1.0f : 1.0f, 0}) * (-28 * (1 - e)), .alpha = smooth(u / 0.6f)};
            break;
        case 2:
            // settling from a little larger, fading in
            lifted = {.alpha = smooth(u / 0.5f), .scale = 1 + 0.12f * (1 - e)};
            break;
        default:
            // growing out of where it stands, a little past its size and back
            lifted = {.alpha = std::min(1.0f, u * 4), .scale = std::max(0.0f, outBack(u, 1.2f))};
            pivot = grows;
            break;
        }
    }
    p.place(mark, p.transform().map(pivot), lifted);
}

// ---- the piece under the mouse ----

void Chart::liftPiece(std::optional<std::string> key, std::optional<Point> press) {
    std::string now = solidPieces() ? key.value_or(std::string()) : std::string();
    if (now == lifted_) {
        if (!lifted_.empty())
            liftedPress_ = press;
        return;
    }
    // the one that was lifted, down from where it is; the new one up
    const Duration fast = transition(true);
    if (!lifted_.empty()) {
        lowered_ = std::move(lifted_);
        loweredPress_ = liftedPress_;
        lowering_.set(lifting_.value());
        lowering_.animateTo(scheduler(), 0, fast);
    }
    lifted_ = std::move(now);
    liftedPress_ = press;
    lifting_.set(0);
    if (!lifted_.empty())
        lifting_.animateTo(scheduler(), 1, fast * 2, Easing::OutCubic);
    invalidatePaint();
}

float Chart::pieceLift(std::string_view key) const {
    if (key.empty())
        return 0;
    if (key == lifted_)
        return lifting_.value();
    if (key == lowered_)
        return lowering_.value();
    return 0;
}

std::optional<Point> Chart::piecePress(std::string_view key) const {
    if (key.empty())
        return std::nullopt;
    return key == lifted_ ? liftedPress_ : key == lowered_ ? loweredPress_ : std::nullopt;
}

void Chart::placeLifted(paint::Painter& p, const paint::Painter::Mark& mark, float amount, const Rect& rect, std::optional<Point> press) const {
    if (amount <= 0)
        return;
    // up toward the one looking, as much as a little of its size (its shadow staying under it), turned toward the light
    const float up = std::clamp(0.08f * std::min(rect.width, rect.height), 4.0f, 9.0f);
    paint::Quaternion turn = paint::towardLight(0.12f * amount);
    if (press) {
        // leaning where it is pressed: about the line across the way from its middle to there, that side down
        const Point d = *press - rect.center();
        const float length = std::hypot(d.x, d.y);
        const float reach = std::max(1.0f, std::max(rect.width, rect.height) / 2);
        if (length > 0.5f) {
            const float lean = 0.26f * std::min(1.0f, length / reach) * amount;
            turn = paint::Quaternion::axisAngle({-d.y / length, d.x / length, 0}, lean) * paint::towardLight(0.05f * amount);
        }
    }
    p.place(mark, p.transform().map(rect.center()), {.lift = up * amount, .turn = turn});
}

Duration Chart::transition(bool fast) const {
    const UiContext* ui = context();
    if (!animated_ || !ui || !scheduler())
        return Duration::zero();
    return fast ? Duration(ui->theme.motion.fast) : Duration(ui->theme.motion.normal);
}

float Chart::snap(float v, float pixelsPerDip) {
    return std::round(v * pixelsPerDip) / pixelsPerDip;
}

Color Chart::textOn(Color background) {
    const float luminance = 0.2126f * background.r + 0.7152f * background.g + 0.0722f * background.b;
    return luminance > 0.58f ? Color{0.06f, 0.07f, 0.09f, 0.88f} : Color{1, 1, 1, 0.96f};
}

} // namespace glw

// ---- an axis ----

namespace glw {

// ---- annotations ----

std::size_t Chart::addAnnotation(ChartAnnotation annotation) {
    const std::size_t id = nextAnnotation_++;
    annotations_.emplace_back(id, std::move(annotation));
    invalidatePaint();
    return id;
}

void Chart::removeAnnotation(std::size_t id) {
    std::erase_if(annotations_, [&](const auto& a) { return a.first == id; });
    invalidatePaint();
}

void Chart::clearAnnotations() {
    annotations_.clear();
    invalidatePaint();
}

void Chart::paintAnnotations(PaintContext& context, const Rect& plot, const Axis& x, const Axis& y, bool above, const Axis* y2) const {
    paintAnnotations(
        context, plot, [&](double v) { return x.at(v, plot.x, plot.width); },
        [&](double v, bool second) { return (second && y2 ? *y2 : y).at(v, plot.y, plot.height); }, above);
}

void Chart::paintAnnotations(PaintContext& context, const Rect& plot, const std::function<float(double)>& xAt,
                             const std::function<float(double, bool)>& yAt, bool above, bool lying) const {
    if (annotations_.empty() || plot.width <= 0 || plot.height <= 0)
        return;
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const float hair = std::max(1.0f, 1.25f * ppd) / ppd;
    const bool rtl = rightToLeft();
    const float small = std::max(9.0f, s.fontSize - 1);
    const float line = textSize("0", small).height;
    // a line down the plot (at a place across it) or across it, whole or in dashes
    const auto rule = [&](bool down, float at, Color color, bool dashed) {
        const float length = down ? plot.height : plot.width;
        const float step = dashed ? 9.0f : length;
        for (float t = 0; t < length; t += step) {
            const float run = std::min(dashed ? 5.0f : length, length - t);
            if (down)
                p.fillRect({snap(at, ppd) - hair / 2, plot.y + t, hair, run}, color);
            else
                p.fillRect({plot.x + t, snap(at, ppd) - hair / 2, run, hair}, color);
        }
    };
    p.pushClip(plot);
    for (const auto& [id, a] : annotations_) {
        using Kind = ChartAnnotation::Kind;
        const bool band = a.kind == Kind::XBand || a.kind == Kind::YBand;
        if (band == above)
            continue;
        const Color color = a.color ? *a.color : s.text;
        const std::string label = translate(a.label);
        // where its value is: an x's or a y's place; whether its line goes down the plot (across: along the other)
        const bool ofX = a.kind == Kind::XLine || a.kind == Kind::XBand;
        const auto place = [&](double v) { return ofX ? xAt(v) : yAt(v, a.secondAxis); };
        const bool down = ofX != lying;
        switch (a.kind) {
        case Kind::XBand:
        case Kind::YBand: {
            const float p0 = place(a.from), p1 = place(a.to);
            const Rect r = down ? Rect{std::min(p0, p1), plot.y, std::abs(p1 - p0), plot.height}
                                : Rect{plot.x, std::min(p0, p1), plot.width, std::abs(p1 - p0)};
            p.fillRect(r, withAlpha(color, color.a * 0.12f));
            if (!label.empty())
                drawText(context, label, {rtl ? r.right() - 4 : r.x + 4, r.y + 3}, withAlpha(color, color.a * 0.9f), small, {rtl ? 1.0f : 0.0f, 0});
            break;
        }
        case Kind::XLine:
        case Kind::YLine: {
            const float at = place(a.from);
            rule(down, at, withAlpha(color, color.a * 0.85f), a.dashed);
            if (label.empty())
                break;
            if (down) {
                // its label at the top, at its end side (at its start side when there is no room there)
                const float w = textSize(label, small).width;
                const bool after = rtl ? at - w - 4 >= plot.x : at + w + 4 > plot.right();
                drawText(context, label, {after == rtl ? at + 4 : at - 4, plot.y + 3}, color, small, {after == rtl ? 0.0f : 1.0f, 0});
            } else {
                // above it at the end side (under it when it is at the top)
                const bool under = at - line - 2 < plot.y;
                drawText(context, label, {rtl ? plot.x + 4 : plot.right() - 4, under ? at + 2 : at - 2}, color, small,
                         {rtl ? 0.0f : 1.0f, under ? 0.0f : 1.0f});
            }
            break;
        }
        case Kind::Point: {
            const float px = xAt(a.from), py = yAt(a.to, a.secondAxis);
            const Point at = lying ? Point{py, px} : Point{px, py};
            const Color ring = s.plot.a > 0.9f ? s.plot : context.ui.theme.palette.layer;
            p.fillCircle(at, 5, ring);
            p.fillCircle(at, 3.5f, color);
            if (!label.empty()) {
                const float w = textSize(label, small).width;
                const bool before = rtl ? at.x - w - 8 >= plot.x : at.x + w + 8 > plot.right();
                drawText(context, label, {before == rtl ? at.x + 7 : at.x - 7, at.y - 4}, color, small, {before == rtl ? 0.0f : 1.0f, 1});
            }
            break;
        }
        }
    }
    p.popClip();
}

const Locale& Chart::locale() const {
    if (const UiContext* ui = context())
        return ui->translator.locale();
    return Locale::english();
}

std::string Chart::Axis::label(double value) const {
    const double step = time ? timeTicks_.step : step_;
    if (format)
        return format(value, step);
    if (log && !time && step_ == 0) {
        // each tick to its own decimals (0.01, 0.1, 1), shortened as its size is (10k, 1M)
        const int decimals = value > 0 ? std::clamp(static_cast<int>(-std::floor(std::log10(value) + 1e-9)), 0, 12) : 0;
        return glw::formatNumber(value, decimals, value >= 1000 ? value : 0, *locale_) + suffix;
    }
    return (time ? glw::formatTime(value, timeTicks_, *locale_) : glw::formatNumber(value, decimals_, step_, *locale_)) + suffix;
}

void Chart::Axis::fit(const Chart& chart, ChartRange range, float length) {
    range_ = range;
    locale_ = &chart.locale();
    height_ = chart.textSize("0").height;
    const bool across = orientation == Orientation::Horizontal;
    // as many as fit: down, a label's height and some apart; across, by their widths (tried, then fewer if they are wide)
    const auto make = [&](int count) {
        values_.clear();
        labels_.clear();
        if (time) {
            timeTicks_ = glw::timeTicks(range, count, *locale_);
            values_ = timeTicks_.values;
        } else {
            const ChartTicks ticks = log ? logTicks(range, count) : niceTicks(range, count);
            values_ = ticks.values;
            step_ = ticks.step;
            decimals_ = ticks.decimals;
        }
        widest_ = 0;
        for (double v : values_) {
            labels_.push_back(label(v));
            widest_ = std::max(widest_, chart.textSize(labels_.back()).width);
        }
    };
    int count = std::max(2, static_cast<int>(length / (across ? 90.0f : 34.0f)));
    make(count);
    if (across && (widest_ + 20) * static_cast<float>(labels_.size()) > length) {
        count = std::max(2, static_cast<int>(length / (widest_ + 20)));
        make(count);
    }
}

float Chart::Axis::at(double value, float start, float length) const {
    double f64 = 0;
    if (log && range_.low > 0 && range_.high > range_.low) {
        // what is at or below 0: far past its low end
        const double lo = std::log10(range_.low), span = std::log10(range_.high) - lo;
        f64 = value > 0 ? (std::log10(value) - lo) / span : -1e3;
    } else {
        const double span = range_.span();
        f64 = span > 0 ? (value - range_.low) / span : 0.0;
    }
    const auto f = static_cast<float>(f64);
    const bool fromEnd = orientation == Orientation::Horizontal ? reversed : !reversed;
    return fromEnd ? start + length - f * length : start + f * length;
}

void Chart::Axis::paintGrid(paint::Painter& painter, const Rect& plot, Color color) const {
    const float ppd = painter.pixelsPerDip();
    const float hair = 1.0f / ppd;
    const bool across = orientation == Orientation::Horizontal;
    for (double v : values_) {
        if (across) {
            const float x = snap(at(v, plot.x, plot.width), ppd);
            if (x >= plot.x - 0.5f && x <= plot.right() + 0.5f)
                painter.fillRect({x, plot.y, hair, plot.height}, color);
        } else {
            const float y = snap(at(v, plot.y, plot.height), ppd);
            if (y >= plot.y - 0.5f && y <= plot.bottom() + 0.5f)
                painter.fillRect({plot.x, y, plot.width, hair}, color);
        }
    }
}

void Chart::Axis::paintLabels(PaintContext& context, const Chart& chart, const Rect& plot, Side side, Color color, const Rect& bounds) const {
    const bool across = orientation == Orientation::Horizontal;
    for (std::size_t i = 0; i < values_.size(); ++i) {
        if (across) {
            const float x = at(values_[i], plot.x, plot.width);
            const float half = chart.textSize(labels_[i]).width / 2;
            if (x < plot.x - 1 || x > plot.right() + 1 || x - half < bounds.x || x + half > bounds.right())
                continue;
            if (side == Side::Top)
                chart.drawText(context, labels_[i], {x, plot.y - 4}, color, 0, {0.5f, 1});
            else
                chart.drawText(context, labels_[i], {x, plot.bottom() + 5}, color, 0, {0.5f, 0});
        } else {
            const float y = at(values_[i], plot.y, plot.height);
            if (y < plot.y - 1 || y > plot.bottom() + 1)
                continue;
            if (side == Side::Right)
                chart.drawText(context, labels_[i], {plot.right() + 8, y}, color, 0, {0, 0.5f});
            else
                chart.drawText(context, labels_[i], {plot.x - 8, y}, color, 0, {1, 0.5f});
        }
    }
}

std::string Chart::Axis::precise(double value) const {
    if (format)
        return format(value, 0);
    if (time) {
        // to the second (to the millisecond when its ticks are), its day too when more than a day shows
        const TimeUnit tick = timeTicks_.unit;
        const TimeUnit unit = tick == TimeUnit::Millisecond ? TimeUnit::Millisecond : tick <= TimeUnit::Hour ? TimeUnit::Second : tick;
        return glw::formatTimeFully(value, unit, range_.span() > 86400, *locale_);
    }
    if (log && value > 0) {
        // three figures, however small or large
        const int decimals = std::clamp(2 - static_cast<int>(std::floor(std::log10(value) + 1e-9)), 0, 12);
        return glw::formatNumber(value, decimals, 0, *locale_) + suffix;
    }
    return glw::formatNumber(value, decimals_ + 1, 0, *locale_) + suffix;
}

} // namespace glw
