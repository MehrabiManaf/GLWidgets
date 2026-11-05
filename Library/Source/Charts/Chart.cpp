// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/Chart.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
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
Chart::~Chart() = default;

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
