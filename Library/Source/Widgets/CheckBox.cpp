// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/CheckBox.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
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
        // which way its mark comes (a style's that varies it): one of three at random
        const UiContext* ui = context();
        markWay_ = ui && ui->theme.motion.varied ? randomWay(3) : 0;
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
    fill_.to(scheduler(), s.checkedBackground.resolve(v), fast);
    glow_.to(scheduler(), v.focused ? s.focusRing : withAlpha(s.focusRing, 0), context.ui.theme.motion.normal);
    if (!markShown_) {          // the first paint: as it is
        mark_.set(checked_ ? 1.0f : 0.0f);
        markShown_ = true;
    } else {
        mark_.animateTo(scheduler(), checked_ ? 1.0f : 0.0f, context.ui.theme.motion.normal, Easing::OutCubic);
    }
    const float m = mark_.value();
    paintFocusGlow(p, box_, s.cornerRadius, glow_.value());
    // a sunken box; checked, a raised one of the accent fading in over it
    if (m < 1)
        paintSunken(p, box_, s.cornerRadius, s.background.resolve(v), s.border.resolve(v), s.bevel);
    if (m > 0) {
        // falling into the box as it shows (a style's), from nearer the one looking
        const bool dropping = s.checkDrop > 0 && m < 1;
        const paint::Painter::Mark mark = dropping ? p.mark() : paint::Painter::Mark{};
        const Color fill = fill_.value();
        Bevel bevel = s.checkedBevel;
        bevel.highlight.a *= m;
        bevel.shadow.a *= m;
        paintRaised(p, box_, s.cornerRadius, withAlpha(fill, fill.a * m), withAlpha(s.checkedBorder, s.checkedBorder.a * m), bevel);
        // the mark: two strokes, as a hand draws them, drawn as it checks
        const Point a{box_.x + box_.width * 0.24f, box_.y + box_.height * 0.53f};
        const Point b{box_.x + box_.width * 0.43f, box_.y + box_.height * 0.71f};
        const Point c{box_.x + box_.width * 0.77f, box_.y + box_.height * 0.31f};
        const auto length = [](Point from, Point to) { return std::hypot(to.x - from.x, to.y - from.y); };
        const float first = length(a, b), second = length(b, c);
        const float drawn = m * (first + second);
        const Color ink = withAlpha(s.check, s.check.a * std::min(1.0f, m * 2));
        if (drawn <= first) {
            const Point points[] = {a, a + (b - a) * (drawn / first)};
            p.drawPolyline(points, 2, ink);
        } else {
            const Point points[] = {a, b, b + (c - b) * ((drawn - first) / second)};
            p.drawPolyline(points, 2, ink);
        }
        if (dropping) {
            const float fall = 1 - m;
            const Point middle = p.transform().map(box_.center());
            constexpr float pi = 3.14159265f;
            if (markWay_ == 1)          // turning in about its upright middle, from edge on, as a coin flipped onto it
                p.place(mark, middle, {.lift = s.checkDrop * 0.5f * std::sin(pi * m), .turn = paint::Quaternion::axisAngle({0, 1, 0}, 1.5f * fall)});
            else if (markWay_ == 2)     // growing out of a small one, a little past its size and back
                p.place(mark, middle, {.lift = s.checkDrop * 0.3f * fall, .scale = 1 + 0.3f * std::sin(pi * m) - 0.6f * fall});
            else
                p.place(mark, middle, {.lift = s.checkDrop * fall * fall});
        }
    }
    // the text paints after this, in the color of the state
    label_->setColor(v.disabled ? std::optional<Color>(context.ui.theme.palette.disabledText) : std::nullopt);
}

void CheckBox::paintOver(PaintContext&) {}

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
