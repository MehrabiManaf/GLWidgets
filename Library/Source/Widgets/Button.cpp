// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Button.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <utility>

namespace glw {

Button::Button(Props props) : onClick_(std::move(props.onClick)), style_(std::move(props.style)) {
    setFocusable(true);
    setAlignment(Alignment::Start, Alignment::Center);
    label_ = &add(std::make_unique<Label>(Label::Props{.text = std::move(props.text), .alignment = text::TextAlignment::Center}));
    applyStyle();
}

Button::Button(LocalizedText text, std::function<void()> onClick) : Button(Props{std::move(text), std::move(onClick), {}}) {}

void Button::setText(LocalizedText text) {
    label_->setText(std::move(text));
}

const LocalizedText& Button::text() const {
    return label_->text();
}

void Button::setOnClick(std::function<void()> onClick) {
    onClick_ = std::move(onClick);
}

void Button::setStyle(std::optional<ButtonStyle> style) {
    style_ = std::move(style);
    applyStyle();
}

const ButtonStyle& Button::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.button;
    static const ButtonStyle unstyled = Theme::dark().button;      // not shown yet: what it will most likely look like
    return unstyled;
}

void Button::applyStyle() {
    setPadding(style().padding);
    invalidatePaint();
}

void Button::onRootChanged() {
    applyStyle();
}

void Button::onStyleChanged() {
    applyStyle();
}

VisualState Button::state() const {
    return {.hovered = hovered(), .pressed = pressed_ && hovered(), .focused = hasFocus(), .disabled = !enabledInTree()};
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

void Button::onFocusChanged(bool) {
    invalidatePaint();
}

void Button::paint(PaintContext& context) {
    const ButtonStyle& s = style();
    const VisualState v = state();
    const Rect r{0, 0, size().width, size().height};
    context.painter.fillRoundedRect(r, s.cornerRadius, s.background.resolve(v));
    if (s.borderWidth > 0)
        context.painter.strokeRoundedRect(r, s.cornerRadius, s.borderWidth, s.border.resolve(v));
    // the label paints after this, in the color of the state
    label_->setColor(s.text.resolve(v));
}

void Button::paintOver(PaintContext& context) {
    if (!hasFocus())
        return;
    const ButtonStyle& s = style();
    context.painter.strokeRoundedRect(Rect{0, 0, size().width, size().height}.inflated(2), s.cornerRadius + 2, 1.5f, s.focusRing);
}

} // namespace glw
