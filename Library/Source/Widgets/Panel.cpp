// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Panel.hpp>

#include <GLWidgets/Paint/Painter.hpp>

namespace glw {

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

} // namespace glw
