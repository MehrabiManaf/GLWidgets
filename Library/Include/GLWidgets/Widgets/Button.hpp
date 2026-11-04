// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A push button with a label: clicked by the mouse (pressed and released on it), or by Enter or Space when it has the
// focus. It looks as its ButtonStyle says: the theme's, or its own.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <optional>

namespace glw {

class GLW_EXPORT Button : public Widget {
public:
    // What a button is made with (ui::Button): designated initializers, in this order.
    struct Props {
        LocalizedText text;
        std::function<void()> onClick;
        std::optional<ButtonStyle> style;               // none: the theme's
    };

    explicit Button(Props props);
    explicit Button(LocalizedText text = {}, std::function<void()> onClick = {});

    void setText(LocalizedText text);
    const LocalizedText& text() const;
    // Called when it is clicked. It may destroy the button only through something that waits for the event to end
    // (Window::close does).
    void setOnClick(std::function<void()> onClick);
    void setStyle(std::optional<ButtonStyle> style);
    // Its own style, or the theme's.
    const ButtonStyle& style() const;
    Label& label() { return *label_; }
    void click();

protected:
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onFocusChanged(bool focused) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    VisualState state() const;
    void applyStyle();

    Label* label_ = nullptr;
    std::function<void()> onClick_;
    std::optional<ButtonStyle> style_;
    bool pressed_ = false;
    // what it shows eases to its state's colors
    ColorTransition background_{[this] { invalidatePaint(); }};
    ColorTransition border_{[this] { invalidatePaint(); }};
    ColorTransition glow_{[this] { invalidatePaint(); }};
};

} // namespace glw
