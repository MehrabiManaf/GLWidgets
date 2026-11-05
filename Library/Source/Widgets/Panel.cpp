// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Panel.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <utility>

namespace glw {

Panel::Panel(Props props)
    : background_(props.background), border_(props.border), borderWidth_(props.borderWidth), cornerRadius_(props.cornerRadius) {}

Panel::Panel(std::optional<Color> background, float cornerRadius) : background_(background), cornerRadius_(cornerRadius) {}

void Panel::setBackground(std::optional<Color> color) {
    background_ = color;
    invalidatePaint();
}

void Panel::setBorder(std::optional<Color> color, float width) {
    border_ = color;
    borderWidth_ = width;
    invalidatePaint();
}

void Panel::setCornerRadius(float radius) {
    cornerRadius_ = radius;
    invalidatePaint();
}

void Panel::paint(PaintContext& context) {
    const Rect r{0, 0, size().width, size().height};
    if (background_)
        context.painter.fillRoundedRect(r, cornerRadius_, *background_);
    if (border_)
        context.painter.strokeRoundedRect(r, cornerRadius_, borderWidth_, *border_);
}

// ---- card ----

void Card::setStyle(std::optional<CardStyle> style) {
    style_ = std::move(style);
    invalidatePaint();
}

const CardStyle& Card::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.card;
    static const CardStyle unstyled = Theme::dark().card;
    return unstyled;
}

void Card::paint(PaintContext& context) {
    const CardStyle& s = style();
    const Rect r{0, 0, size().width, size().height};
    if (s.bevel.relief > 0) {
        // raised as a control (a style's): a slab, lit
        paintRaised(context.painter, r, s.cornerRadius, s.background, {0, 0, 0, 0}, s.bevel);
        return;
    }
    context.painter.fillRoundedRect(r, s.cornerRadius, s.background);
    if (s.borderWidth > 0)
        context.painter.strokeRoundedRect(r, s.cornerRadius, s.borderWidth, s.border);
}

} // namespace glw
