// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A widget that paints a background and a border, with rounded corners: a container for others.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Export.hpp>
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

} // namespace glw
