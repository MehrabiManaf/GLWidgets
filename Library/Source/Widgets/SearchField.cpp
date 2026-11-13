// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/SearchField.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <utility>

namespace glw {

namespace {

constexpr float buttonRoom = 24;        // the icon's, at the end side of the field

} // namespace

SearchField::SearchField() : SearchField(Props{}) {}

SearchField::SearchField(Props props) : onChanged_(std::move(props.onChanged)) {
    setAlignment(Alignment::Stretch, Alignment::Center);
    field_ = &add(std::make_unique<LineEdit>(LineEdit::Props{
        .text = std::move(props.text),
        .placeholder = props.placeholder.empty() ? tr("Search") : std::move(props.placeholder),
        .onChanged = [this](const std::string& text) {
            textChanged();
            if (onChanged_)
                onChanged_(text);
        },
        .onSubmit = std::move(props.onSubmit)}));
    field_->setEndRoom(buttonRoom);
    clearing_.set(field_->text().empty() ? 0.0f : 1.0f);
}

const std::string& SearchField::text() const {
    return field_->text();
}

void SearchField::setText(std::string text) {
    field_->setText(std::move(text));
    textChanged();
}

void SearchField::clear() {
    if (field_->text().empty())
        return;
    field_->setText({});
    textChanged();
    if (onChanged_)
        onChanged_(field_->text());
}

void SearchField::setPlaceholder(LocalizedText placeholder) {
    field_->setPlaceholder(std::move(placeholder));
}

void SearchField::setOnSubmit(std::function<void(const std::string&)> handler) {
    field_->setOnSubmit(std::move(handler));
}

void SearchField::textChanged() {
    const float to = field_->text().empty() ? 0.0f : 1.0f;
    if (clearing_.target() != to) {
        const UiContext* ui = context();
        clearing_.animateTo(scheduler(), to, ui ? ui->theme.motion.fast : Duration{});
    }
    if (to == 0 && (hovered_ || pressed_)) {
        hovered_ = pressed_ = false;
    }
    invalidatePaint();
}

bool SearchField::onButton(Point point) const {
    const float x = rightToLeft() ? point.x : size().width - point.x;
    return x >= 0 && x <= buttonRoom && point.y >= 0 && point.y <= size().height;
}

void SearchField::paintOver(PaintContext& context) {
    paint::Painter& p = context.painter;
    const LineEditStyle& s = field_->style();
    const float cx = rightToLeft() ? buttonRoom / 2 : size().width - buttonRoom / 2;
    const float cy = size().height / 2;
    const float k = std::clamp(clearing_.value(), 0.0f, 1.0f);
    // the magnifier, going as the clear button comes (turning into it: smaller, fading)
    if (k < 1) {
        const float a = 1 - k, r = 4.2f * (1 - 0.4f * k);
        const Color ink = withAlpha(s.placeholder, s.placeholder.a * a);
        p.strokeCircle({cx - 1.2f, cy - 1.2f}, r, 1.4f, ink);
        p.drawLine({cx - 1.2f + r * 0.72f, cy - 1.2f + r * 0.72f}, {cx + 4.2f, cy + 4.2f}, 1.6f, ink);
    }
    if (k > 0) {
        const bool enabled = enabledInTree();
        if (enabled && (hovered_ || pressed_))
            p.fillCircle({cx, cy}, 8, withAlpha(s.text, pressed_ ? 0.22f : 0.12f));
        const Color ink = withAlpha(hovered_ ? s.text : s.placeholder, (hovered_ ? s.text.a : s.placeholder.a) * k);
        const float d = 3.2f * (0.6f + 0.4f * k);
        p.drawLine({cx - d, cy - d}, {cx + d, cy + d}, 1.5f, ink);
        p.drawLine({cx - d, cy + d}, {cx + d, cy - d}, 1.5f, ink);
    }
}

void SearchField::onPreviewMouse(MouseEvent& event) {
    // a press on the clear button: it clears when let go on it
    if (field_->text().empty() || !enabledInTree() || event.type != MouseEventType::Press ||
        event.button != platform::MouseButton::Left || !onButton(event.position))
        return;
    event.handled = true;
    pressed_ = hovered_ = true;
    invalidatePaint();
}

void SearchField::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Move: {
        // over the field (its own moves come here too, unhandled) or, held, anywhere
        const bool on = !field_->text().empty() && onButton(event.position);
        if (on != hovered_) {
            hovered_ = on;
            invalidatePaint();
        }
        if (pressed_)
            event.handled = true;
        break;
    }
    case MouseEventType::Release:
        if (!pressed_)
            return;
        event.handled = true;
        pressed_ = false;
        if (onButton(event.position)) {
            clear();
            field_->focus();
        }
        invalidatePaint();
        break;
    default: break;
    }
}

void SearchField::onKey(KeyEvent& event) {
    // Escape clears what was typed (an empty field leaves it to what is around: a popup closes)
    if (event.pressed && event.key == platform::Key::Escape && !field_->text().empty()) {
        clear();
        event.handled = true;
    }
}

void SearchField::onHoverChanged(bool hovered) {
    if (!hovered && hovered_) {
        hovered_ = false;
        invalidatePaint();
    }
}

} // namespace glw
