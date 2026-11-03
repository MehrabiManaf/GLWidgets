// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// One of a few texts, chosen from a menu under it (Menu.hpp): a click, Enter, Space or Alt+Down opens it, the current
// text checked; Up and Down choose the one before or after without opening it.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace glw {

class Label;

class GLW_EXPORT ComboBox : public Widget {
public:
    // What a combo box is made with (ui::ComboBox): designated initializers, in this order.
    struct Props {
        std::vector<LocalizedText> items;
        std::size_t current = 0;
        std::function<void(std::size_t)> onChanged;     // by the user (not by setCurrent)
    };

    ComboBox();
    explicit ComboBox(Props props);

    void setItems(std::vector<LocalizedText> items);
    const std::vector<LocalizedText>& items() const { return items_; }
    std::size_t current() const { return current_; }
    void setCurrent(std::size_t index);
    void setOnChanged(std::function<void(std::size_t)> handler) { onChanged_ = std::move(handler); }
    // Its menu, as by a click.
    void open();
    bool isOpen() const { return open_; }

protected:
    Size measureContent(Size available) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool) override { invalidatePaint(); }
    void onFocusChanged(bool) override { invalidatePaint(); }

private:
    const ButtonStyle& style() const;
    void choose(std::size_t index);         // by the user

    std::vector<LocalizedText> items_;
    std::size_t current_ = 0;
    std::function<void(std::size_t)> onChanged_;
    Label* label_ = nullptr;
    bool open_ = false;
    bool pressed_ = false;
    ColorTransition background_{[this] { invalidatePaint(); }};
};

} // namespace glw
