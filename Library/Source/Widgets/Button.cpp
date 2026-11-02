// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Button.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <utility>

namespace glw {

Button::Button(Props props) : onClick_(std::move(props.onClick)) {
    setFocusable(true);
    setPadding(Insets::symmetric(14, 6));
    setAlignment(Alignment::Start, Alignment::Center);
    label_ = &add(std::make_unique<Label>(Label::Props{.text = std::move(props.text), .alignment = text::TextAlignment::Center}));
}

Button::Button(std::string text, std::function<void()> onClick) : Button(Props{std::move(text), std::move(onClick)}) {}

void Button::setText(std::string text) {
    label_->setText(std::move(text));
}

const std::string& Button::text() const {
    return label_->text();
}

void Button::setOnClick(std::function<void()> onClick) {
    onClick_ = std::move(onClick);
}

void Button::click() {
    // a copy: the handler may replace itself
    if (const auto handler = onClick_)
        handler();
}

void Button::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Move || event.type == MouseEventType::Wheel)
        return;
    event.handled = true;
    if (event.type == MouseEventType::Press) {
        pressed_ = true;
        invalidatePaint();
        return;
    }
    const bool wasPressed = std::exchange(pressed_, false);
    invalidatePaint();
    if (wasPressed && Rect{0, 0, size().width, size().height}.contains(event.position))
        click();
}

void Button::onKey(KeyEvent& event) {
    if (event.pressed && !event.repeat && (event.key == platform::Key::Enter || event.key == platform::Key::Space)) {
        event.handled = true;
        click();
    }
}

void Button::onHoverChanged(bool) {
    invalidatePaint();
}

void Button::paint(PaintContext& context) {
    const Theme& theme = context.ui.theme;
    const Rect r{0, 0, size().width, size().height};
    context.painter.fillRoundedRect(r, theme.cornerRadius, theme.surface);
    if (hovered())
        context.painter.fillRoundedRect(r, theme.cornerRadius, pressed_ ? theme.pressed : theme.hover);
    context.painter.strokeRoundedRect(r, theme.cornerRadius, 1, theme.border);
}

void Button::paintOver(PaintContext& context) {
    if (!hasFocus())
        return;
    const Theme& theme = context.ui.theme;
    context.painter.strokeRoundedRect(Rect{0, 0, size().width, size().height}.inflated(2), theme.cornerRadius + 2, 1.5f, theme.accent);
}

} // namespace glw
