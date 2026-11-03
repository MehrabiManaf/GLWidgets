// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/CheckBox.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <utility>

namespace glw {

CheckBox::CheckBox() : CheckBox(Props{}) {}

CheckBox::CheckBox(Props props) : checked_(props.checked), onToggled_(std::move(props.onToggled)), style_(std::move(props.style)) {
    setFocusable(true);
    setAlignment(Alignment::Start, Alignment::Center);
    label_ = &add(std::make_unique<Label>(std::move(props.text)));
}

void CheckBox::setChecked(bool checked) {
    if (checked != checked_) {
        checked_ = checked;
        invalidatePaint();
    }
}

void CheckBox::toggle() {
    setChecked(!checked_);
    if (const auto handler = onToggled_)
        handler(checked_);
}

void CheckBox::setStyle(std::optional<CheckBoxStyle> style) {
    style_ = std::move(style);
    invalidateLayout();
}

const CheckBoxStyle& CheckBox::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.checkBox;
    static const CheckBoxStyle unstyled = Theme::dark().checkBox;
    return unstyled;
}

VisualState CheckBox::state() const {
    return {.hovered = hovered(), .pressed = pressed_ && hovered(), .focused = hasFocus(), .disabled = !enabledInTree()};
}

Size CheckBox::measureContent(Size available) {
    const CheckBoxStyle& s = style();
    const Size text = label_->measure({std::max(0.0f, available.width - s.size - s.spacing), available.height});
    return {s.size + s.spacing + text.width, std::max(s.size, text.height)};
}

void CheckBox::arrangeContent(const Rect& content) {
    const CheckBoxStyle& s = style();
    const bool rtl = rightToLeft();
    // the box on the start side, the text after it
    box_ = {rtl ? content.right() - s.size : content.x, content.y + (content.height - s.size) / 2, s.size, s.size};
    const float rest = std::max(0.0f, content.width - s.size - s.spacing);
    arrangeInCell(*label_, {rtl ? content.x : content.x + s.size + s.spacing, content.y, rest, content.height}, rtl);
}

void CheckBox::paint(PaintContext& context) {
    const CheckBoxStyle& s = style();
    const VisualState v = state();
    paint::Painter& p = context.painter;
    const Duration fast = context.ui.theme.motion.fast;
    fill_.to(scheduler(), checked_ ? s.checkedBackground.resolve(v) : s.background.resolve(v), fast);
    if (!markShown_) {          // the first paint: as it is
        mark_.set(checked_ ? 1.0f : 0.0f);
        markShown_ = true;
    } else {
        mark_.animateTo(scheduler(), checked_ ? 1.0f : 0.0f, fast);
    }
    p.fillRoundedRect(box_, s.cornerRadius, fill_.value());
    // its border while unchecked, fading as the box fills
    const Color border = s.border.resolve(v);
    p.strokeRoundedRect(box_, s.cornerRadius, 1, Color{border.r, border.g, border.b, border.a * (1 - mark_.value())});
    if (mark_.value() > 0) {
        // the mark: two strokes, as a hand would draw it
        const Point points[] = {{box_.x + box_.width * 0.24f, box_.y + box_.height * 0.53f},
                                {box_.x + box_.width * 0.43f, box_.y + box_.height * 0.71f},
                                {box_.x + box_.width * 0.77f, box_.y + box_.height * 0.31f}};
        p.drawPolyline(points, 2, Color{s.check.r, s.check.g, s.check.b, s.check.a * mark_.value()});
    }
    // the text paints after this, in the color of the state
    label_->setColor(v.disabled ? std::optional<Color>(context.ui.theme.palette.disabledText) : std::nullopt);
}

void CheckBox::paintOver(PaintContext& context) {
    if (hasFocus()) {
        const CheckBoxStyle& s = style();
        context.painter.strokeRoundedRect(box_.inflated(2), s.cornerRadius + 2, 1.5f, s.focusRing);
    }
}

void CheckBox::onMouse(MouseEvent& event) {
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
        toggle();
}

void CheckBox::onKey(KeyEvent& event) {
    if (event.pressed && !event.repeat && event.key == platform::Key::Space) {
        event.handled = true;
        toggle();
    }
}

void CheckBox::onHoverChanged(bool) {
    invalidatePaint();
}

void CheckBox::onFocusChanged(bool) {
    invalidatePaint();
}

} // namespace glw
