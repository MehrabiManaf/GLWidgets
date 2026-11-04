// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Action.hpp>

#include <algorithm>

namespace glw {

namespace {

std::string keyName(platform::Key key) {
    using platform::Key;
    const auto k = static_cast<int>(key);
    if (key >= Key::A && key <= Key::Z)
        return std::string(1, static_cast<char>('A' + (k - static_cast<int>(Key::A))));
    if (key >= Key::Digit0 && key <= Key::Digit9)
        return std::string(1, static_cast<char>('0' + (k - static_cast<int>(Key::Digit0))));
    if (key >= Key::F1 && key <= Key::F12)
        return "F" + std::to_string(k - static_cast<int>(Key::F1) + 1);
    switch (key) {
    case Key::Escape: return "Esc";
    case Key::Enter: return "Enter";
    case Key::Tab: return "Tab";
    case Key::Backspace: return "Backspace";
    case Key::Space: return "Space";
    case Key::Insert: return "Ins";
    case Key::Delete: return "Del";
    case Key::Left: return "Left";
    case Key::Right: return "Right";
    case Key::Up: return "Up";
    case Key::Down: return "Down";
    case Key::Home: return "Home";
    case Key::End: return "End";
    case Key::PageUp: return "PgUp";
    case Key::PageDown: return "PgDn";
    default: return "";
    }
}

} // namespace

std::string Shortcut::text() const {
    using platform::KeyModifiers;
    if (empty())
        return {};
    std::string out;
#ifdef __APPLE__
    if (platform::hasAny(modifiers, KeyModifiers::Control))
        out += "\xE2\x8C\x83";      // ⌃
    if (platform::hasAny(modifiers, KeyModifiers::Alt))
        out += "\xE2\x8C\xA5";      // ⌥
    if (platform::hasAny(modifiers, KeyModifiers::Shift))
        out += "\xE2\x87\xA7";      // ⇧
    if (platform::hasAny(modifiers, KeyModifiers::Super))
        out += "\xE2\x8C\x98";      // ⌘
    return out + keyName(key);
#else
    if (platform::hasAny(modifiers, KeyModifiers::Control))
        out += "Ctrl+";
    if (platform::hasAny(modifiers, KeyModifiers::Alt))
        out += "Alt+";
    if (platform::hasAny(modifiers, KeyModifiers::Shift))
        out += "Shift+";
    if (platform::hasAny(modifiers, KeyModifiers::Super))
        out += "Win+";
    return out + keyName(key);
#endif
}

std::string withoutMnemonic(const std::string& text, char* mnemonic) {
    std::string out;
    if (mnemonic)
        *mnemonic = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '&' && i + 1 < text.size()) {
            ++i;
            if (text[i] != '&' && mnemonic && !*mnemonic) {
                const char c = text[i];
                *mnemonic = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
            }
        }
        out += text[i];
    }
    return out;
}

Action::Action(LocalizedText text, std::function<void()> onTriggered) : text_(std::move(text)), onTriggered_(std::move(onTriggered)) {}

void Action::setText(LocalizedText text) {
    text_ = std::move(text);
    changed();
}

void Action::setIcon(Icon icon) {
    icon_ = std::move(icon);
    changed();
}

void Action::setShortcut(Shortcut shortcut) {
    shortcut_ = shortcut;
    changed();
}

void Action::setToolTip(LocalizedText text) {
    toolTip_ = std::move(text);
    changed();
}

void Action::setEnabled(bool enabled) {
    if (enabled == enabled_)
        return;
    enabled_ = enabled;
    changed();
}

void Action::setCheckable(bool checkable) {
    checkable_ = checkable;
    changed();
}

void Action::setChecked(bool checked) {
    if (checked == checked_)
        return;
    checked_ = checked;
    changed();
}

void Action::trigger() {
    if (!enabled_)
        return;
    if (checkable_)
        setChecked(!checked_);
    if (const auto handler = onTriggered_)
        handler();
}

std::size_t Action::observe(std::function<void()> observer) {
    observers_.emplace_back(nextToken_, std::move(observer));
    return nextToken_++;
}

void Action::unobserve(std::size_t token) {
    std::erase_if(observers_, [&](const auto& o) { return o.first == token; });
}

void Action::changed() {
    for (const auto& o : std::vector(observers_))
        o.second();
}

} // namespace glw
