// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ComboBox.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/SearchPopup.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float arrowRoom = 28;         // its arrow's, at the end side

} // namespace

ComboBox::ComboBox() : ComboBox(Props{}) {}

ComboBox::ComboBox(Props props)
    : items_(std::move(props.items)),
      onChanged_(std::move(props.onChanged)),
      searchable_(props.searchable),
      placeholder_(std::move(props.placeholder)) {
    setFocusable(true);
    setPadding({10, 5, arrowRoom, 5});
    label_ = &add(std::make_unique<Label>(Label::Props{.elide = true}));
    label_->setAlignment(Alignment::Stretch, Alignment::Center);
    label_->setHitTestVisible(false);
    setCurrent(props.current);
}

const ButtonStyle& ComboBox::style() const {
    if (const UiContext* ui = context())
        return ui->theme.button;
    static const ButtonStyle unstyled = Theme::dark().button;
    return unstyled;
}

void ComboBox::setItems(std::vector<LocalizedText> items) {
    items_ = std::move(items);
    setCurrent(std::min(current_, items_.empty() ? 0 : items_.size() - 1));
    invalidateLayout();
}

void ComboBox::setCurrent(std::size_t index) {
    current_ = index;
    label_->setText(index < items_.size() ? items_[index] : LocalizedText{});
}

void ComboBox::choose(std::size_t index) {
    if (index >= items_.size() || index == current_)
        return;
    setCurrent(index);
    if (onChanged_)
        onChanged_(index);
}

void ComboBox::setSearchable(bool searchable, LocalizedText placeholder) {
    placeholder_ = std::move(placeholder);
    if (searchable == searchable_)
        return;
    searchable_ = searchable;
    // with the focus now: the system's text input on or off as it takes typed text or not
    if (hasFocus()) {
        if (UiHost* host = root() ? root()->host() : nullptr) {
            if (searchable)
                host->startTextInput();
            else
                host->stopTextInput();
        }
    }
}

void ComboBox::open(std::string typed) {
    if (open_ || items_.empty())
        return;
    if (searchable_) {
        std::vector<SearchItem> items;
        items.reserve(items_.size());
        for (const LocalizedText& item : items_)
            items.push_back({item, {}});
        const Size room{std::max(size().width, 220.0f), 300};
        open_ = showSearchPopup(*this, {0, 0, size().width, size().height},
                                {.items = std::move(items),
                                 .placeholder = placeholder_,
                                 .text = std::move(typed),
                                 .onChosen = [this](std::size_t i) { choose(i); },
                                 .onClosed =
                                     [this] {
                                         open_ = false;
                                         invalidatePaint();
                                     },
                                 .size = room,
                                 .current = current_ < items_.size() ? std::optional<std::size_t>(current_) : std::nullopt});
        invalidatePaint();
        return;
    }
    std::vector<MenuItem> menu;
    for (std::size_t i = 0; i < items_.size(); ++i)
        menu.push_back({.text = items_[i], .onTriggered = [this, i] { choose(i); }, .checked = i == current_});
    open_ = showMenu(*this, {0, 0, size().width, size().height}, std::move(menu), [this] {
        open_ = false;
        invalidatePaint();
    }) != nullptr;
    invalidatePaint();
}

Size ComboBox::measureContent(Size) {
    // as wide as its widest text
    const UiContext* ui = context();
    if (!ui)
        return {40, 18};
    const text::TextStyle style{{&ui->fonts.defaultFont()}, ui->theme.fontSize};
    float width = 40, height = 0;
    for (const LocalizedText& item : items_) {
        const text::TextLayout layout(ui->translator.resolve(item), style);
        width = std::max(width, layout.size().width);
        height = std::max(height, layout.size().height);
    }
    return {std::ceil(width), std::max(height, label_->measure({unbounded, unbounded}).height)};
}

void ComboBox::paint(PaintContext& context) {
    const ButtonStyle& s = style();
    const VisualState v{.hovered = hovered(), .pressed = open_ || pressed_, .focused = hasFocus(), .disabled = !enabledInTree()};
    paint::Painter& p = context.painter;
    const Rect r{0, 0, size().width, size().height};
    background_.to(scheduler(), s.background.resolve(v), context.ui.theme.motion.fast);
    glow_.to(scheduler(), v.focused && !open_ ? s.focusRing : withAlpha(s.focusRing, 0), context.ui.theme.motion.normal);
    paintFocusGlow(p, r, s.cornerRadius, glow_.value());
    paintRaised(p, r, s.cornerRadius, background_.value(), s.borderWidth > 0 ? s.border.resolve(v) : Color{0, 0, 0, 0}, s.bevel, v.pressed);
    label_->setColor(s.text.resolve(v));
    // a chevron down, at the end side
    const float x = rightToLeft() ? arrowRoom / 2 : r.width - arrowRoom / 2, y = r.height / 2;
    const Point points[] = {{x - 4, y - 2}, {x, y + 2}, {x + 4, y - 2}};
    p.drawPolyline(points, 1.4f, s.text.resolve(v));
}

void ComboBox::paintOver(PaintContext&) {}

void ComboBox::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Wheel)
        return;
    if (event.type == MouseEventType::Press) {
        pressed_ = true;
        focus();
        open();
        event.handled = true;
    } else if (event.type == MouseEventType::Release) {
        pressed_ = false;
        invalidatePaint();
        event.handled = true;
    }
}

void ComboBox::onText(TextEvent& event) {
    // typed into it: searched for
    if (!searchable_ || event.composing || event.text.empty() || open_)
        return;
    event.handled = true;
    open(event.text);
}

void ComboBox::onFocusChanged(bool focused) {
    // a searchable one takes typed text (the system's text input on, while it has the focus)
    if (searchable_) {
        if (UiHost* host = root() ? root()->host() : nullptr) {
            if (focused)
                host->startTextInput();
            else
                host->stopTextInput();
        }
    }
    invalidatePaint();
}

void ComboBox::onKey(KeyEvent& event) {
    if (!event.pressed || items_.empty())
        return;
    const bool alt = platform::hasAny(event.modifiers, platform::KeyModifiers::Alt);
    switch (event.key) {
    case platform::Key::Enter:
    case platform::Key::Space: open(); break;
    case platform::Key::Down:
        if (alt)
            open();
        else
            choose(std::min(current_ + 1, items_.size() - 1));
        break;
    case platform::Key::Up: choose(current_ > 0 ? current_ - 1 : 0); break;
    default: return;
    }
    event.handled = true;
}

} // namespace glw
