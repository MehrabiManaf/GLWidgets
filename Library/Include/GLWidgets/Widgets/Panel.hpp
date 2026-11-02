// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Containers that paint something behind their children. A Panel paints the colors it is given; a Card paints the card
// style of the theme (its own, if it was given one), so it follows a change of theme.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <optional>

namespace glw {

class GLW_EXPORT Panel : public Widget {
public:
    // What a panel is made with (ui::Panel): designated initializers, in this order.
    struct Props {
        std::optional<Color> background;
        std::optional<Color> border;
        float borderWidth = 1;
        float cornerRadius = 0;
    };

    Panel() = default;
    explicit Panel(Props props);
    explicit Panel(std::optional<Color> background, float cornerRadius = 0);

    void setBackground(std::optional<Color> color);
    void setBorder(std::optional<Color> color, float width = 1);
    void setCornerRadius(float radius);

protected:
    void paint(PaintContext& context) override;

private:
    std::optional<Color> background_;
    std::optional<Color> border_;
    float borderWidth_ = 1;
    float cornerRadius_ = 0;
};

class GLW_EXPORT Card : public Widget {
public:
    explicit Card(std::optional<CardStyle> style = std::nullopt) : style_(std::move(style)) {}

    void setStyle(std::optional<CardStyle> style);
    const CardStyle& style() const;

protected:
    void paint(PaintContext& context) override;

private:
    std::optional<CardStyle> style_;
};

} // namespace glw
