// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/SpinBox.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <system_error>

namespace glw {

namespace {

constexpr float arrowRoom = 18;         // its arrows', at the end side of the field
constexpr auto firstRepeat = std::chrono::milliseconds(400);
constexpr auto nextRepeat = std::chrono::milliseconds(50);

std::optional<double> parse(std::string_view text) {
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);
    double value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() || !std::isfinite(value))
        return std::nullopt;
    return value;
}

} // namespace

SpinBox::SpinBox() : SpinBox(Props{}) {}

SpinBox::SpinBox(Props props)
    : minimum_(std::min(props.minimum, props.maximum)),
      maximum_(std::max(props.minimum, props.maximum)),
      step_(props.step),
      decimals_(props.decimals),
      onChanged_(std::move(props.onChanged)) {
    setAlignment(Alignment::Stretch, Alignment::Center);
    auto field = std::make_unique<LineEdit>();
    field_ = &add(std::move(field));
    field_->setEndRoom(arrowRoom);
    field_->setAccepts([this](std::string_view text) { return acceptsTyping(text, decimals_, minimum_ < 0); });
    field_->setOnSubmit([this](const std::string&) { commit(); });
    value_ = fit(props.value);
    show();
}

std::string SpinBox::format(double value, int decimals) {
    char buffer[64];
    std::to_chars_result r{};
    if (decimals < 0)
        r = std::to_chars(buffer, buffer + sizeof buffer, value);
    else
        r = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
    std::string text(buffer, r.ec == std::errc{} ? r.ptr : buffer);
    if (text == "-0" || (text.starts_with("-0.") && text.find_first_not_of("-0.") == std::string::npos))
        text.erase(0, 1);       // no minus zero
    return text;
}

bool SpinBox::acceptsTyping(std::string_view text, int decimals, bool negative) {
    bool point = false, digits = false, exponent = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            digits = true;
        } else if (c == '-' || c == '+') {
            // a sign: first, or after an exponent's e
            const bool first = i == 0;
            const bool afterE = i > 0 && (text[i - 1] == 'e' || text[i - 1] == 'E');
            if (!(first || afterE) || (first && c == '-' && !negative))
                return false;
        } else if (c == '.') {
            if (decimals == 0 || point || exponent)
                return false;
            point = true;
        } else if (c == 'e' || c == 'E') {
            // an exponent: a free count of decimals only, after digits
            if (decimals >= 0 || exponent || !digits)
                return false;
            exponent = true;
        } else {
            return false;
        }
    }
    // more decimals than it shows: refused (a free count takes any)
    if (decimals > 0 && point && !exponent) {
        const std::size_t after = text.size() - text.find('.') - 1;
        if (static_cast<int>(after) > decimals)
            return false;
    }
    return true;
}

double SpinBox::fit(double value) const {
    if (!std::isfinite(value))
        value = 0;
    if (decimals_ >= 0) {
        const double scale = std::pow(10.0, decimals_);
        const double rounded = std::round(value * scale) / scale;
        if (std::isfinite(rounded))
            value = rounded;
    }
    return std::clamp(value, minimum_, maximum_);
}

void SpinBox::setValue(double value) {
    value_ = fit(value);
    show();
}

void SpinBox::setRange(double minimum, double maximum) {
    minimum_ = std::min(minimum, maximum);
    maximum_ = std::max(minimum, maximum);
    setValue(value_);
}

void SpinBox::setDecimals(int decimals) {
    decimals_ = decimals;
    setValue(value_);
}

void SpinBox::show() {
    field_->setText(format(value_, decimals_));
}

void SpinBox::change(double value) {
    const double fitted = fit(value);
    const bool changed = fitted != value_;
    value_ = fitted;
    show();
    if (changed && onChanged_)
        onChanged_(value_);
}

void SpinBox::stepBy(double steps) {
    // from what is typed, if it is a number
    const double from = parse(field_->text()).value_or(value_);
    change(from + steps * step_);
    field_->selectAll();
}

void SpinBox::commit() {
    if (const auto typed = parse(field_->text()))
        change(*typed);
    else
        show();
}

SpinBox::Arrow SpinBox::arrowAt(Point point) const {
    const float x = rightToLeft() ? point.x : size().width - point.x;
    if (x < 0 || x > arrowRoom || point.y < 0 || point.y > size().height)
        return Arrow::None;
    return point.y < size().height / 2 ? Arrow::Up : Arrow::Down;
}

void SpinBox::paintOver(PaintContext& context) {
    paint::Painter& p = context.painter;
    const LineEditStyle& s = field_->style();
    const bool enabled = enabledInTree();
    const float x0 = rightToLeft() ? 0.0f : size().width - arrowRoom;
    const float half = size().height / 2;
    for (const Arrow arrow : {Arrow::Up, Arrow::Down}) {
        const Rect area{x0, arrow == Arrow::Up ? 0.0f : half, arrowRoom, half};
        if (enabled && (pressed_ == arrow || (pressed_ == Arrow::None && hovered_ == arrow)))
            p.fillRect(Rect{area.x + 2, area.y + 2, area.width - 4, area.height - 4}, withAlpha(s.text, pressed_ == arrow ? 0.22f : 0.12f));
        const float cx = area.x + arrowRoom / 2, cy = area.y + half / 2 + (arrow == Arrow::Up ? 1.0f : -1.0f);
        const float d = arrow == Arrow::Up ? -1.0f : 1.0f;
        const Point points[] = {{cx - 3.5f, cy - 1.75f * d}, {cx, cy + 1.75f * d}, {cx + 3.5f, cy - 1.75f * d}};
        p.drawPolyline(points, 1.3f, enabled ? s.text : s.placeholder);
    }
}

void SpinBox::onPreviewMouse(MouseEvent& event) {
    // a press on an arrow: it steps, and has the mouse until let go (its moves and its release: onMouse)
    if (!enabledInTree() || event.type != MouseEventType::Press || event.button != platform::MouseButton::Left)
        return;
    const Arrow at = arrowAt(event.position);
    if (at == Arrow::None)
        return;
    event.handled = true;
    pressed_ = hovered_ = at;
    field_->focus();
    stepBy(at == Arrow::Up ? 1 : -1);
    if (Scheduler* s = scheduler()) {
        // held: again, and again faster, while the mouse is on it
        repeat_.start(*s, firstRepeat, [this, s] {
            repeat_.start(*s, nextRepeat, [this] {
                if (pressed_ != Arrow::None && hovered_ == pressed_)
                    stepBy(pressed_ == Arrow::Up ? 1 : -1);
            }, true);
        });
    }
    invalidatePaint();
}

void SpinBox::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Move:
        // over the field (the field's own moves come here too, unhandled) or, held, anywhere
        if (const Arrow at = arrowAt(event.position); at != hovered_) {
            hovered_ = at;
            invalidatePaint();
        }
        if (pressed_ != Arrow::None)
            event.handled = true;
        break;
    case MouseEventType::Release:
        if (pressed_ == Arrow::None)
            return;
        event.handled = true;
        pressed_ = Arrow::None;
        repeat_.stop();
        invalidatePaint();
        break;
    case MouseEventType::Wheel:
        // while it has the focus (scrolling past it does not change it)
        if (field_->hasFocus() && event.wheelDelta.y != 0) {
            stepBy(event.wheelDelta.y > 0 ? 1 : -1);
            event.handled = true;
        }
        break;
    default: break;
    }
}

void SpinBox::onHoverChanged(bool hovered) {
    if (!hovered && hovered_ != Arrow::None) {
        hovered_ = Arrow::None;
        invalidatePaint();
    }
}

void SpinBox::onKey(KeyEvent& event) {
    if (!event.pressed)
        return;
    using platform::Key;
    switch (event.key) {
    case Key::Up: stepBy(1); break;
    case Key::Down: stepBy(-1); break;
    case Key::PageUp: stepBy(10); break;
    case Key::PageDown: stepBy(-10); break;
    default: return;
    }
    event.handled = true;
}

void SpinBox::onFocusWithinChanged(bool within) {
    if (!within)
        commit();
}

} // namespace glw
