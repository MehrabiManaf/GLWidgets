// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What the user can do, once for every place it is offered: a menu's item, a toolbar's button, a shortcut. An action has
// a text, an icon, a shortcut, a tooltip; it is enabled or not, and may be checked; what shows it follows it when it
// changes (an Undo disabled when there is nothing to undo is disabled in the menu and in the toolbar).
//
//   auto save = std::make_shared<glw::Action>("&Save", [this] { save(); });
//   save->setShortcut({glw::platform::Key::S, glw::platform::shortcutModifier});
//   save->setIcon(glw::Icon::fromSvgFile("save.svg", true));
//   window.ui().addShortcuts(this, {save});      // the menu bar and the toolbar do it for their own
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Platform/Keys.hpp>
#include <GLWidgets/Widgets/Icon.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace glw {

// A key and the modifiers held with it.
struct Shortcut {
    platform::Key key = platform::Key::Unknown;
    platform::KeyModifiers modifiers = platform::KeyModifiers::None;

    bool empty() const { return key == platform::Key::Unknown; }
    // As menus show it: "Ctrl+Shift+S" ("⌘⇧S" on macOS).
    GLW_EXPORT std::string text() const;
    // Pressed by this key event.
    bool matches(platform::Key k, platform::KeyModifiers m) const {
        using platform::KeyModifiers;
        const auto relevant = KeyModifiers::Shift | KeyModifiers::Control | KeyModifiers::Alt | KeyModifiers::Super;
        return !empty() && k == key && (m & relevant) == modifiers;
    }
    friend bool operator==(const Shortcut&, const Shortcut&) = default;
};

class GLW_EXPORT Action {
public:
    explicit Action(LocalizedText text = {}, std::function<void()> onTriggered = {});
    Action(const Action&) = delete;
    Action& operator=(const Action&) = delete;

    // Its text; a & before a letter makes it its mnemonic (Alt and that letter, in a menu bar), && is a &.
    void setText(LocalizedText text);
    const LocalizedText& text() const { return text_; }
    void setIcon(Icon icon);
    const Icon& icon() const { return icon_; }
    void setShortcut(Shortcut shortcut);
    Shortcut shortcut() const { return shortcut_; }
    // What a toolbar's button shows by the mouse; none: its text (and its shortcut).
    void setToolTip(LocalizedText text);
    const LocalizedText& toolTip() const { return toolTip_; }

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }
    // Checkable: triggering it checks or unchecks it (a check mark in a menu, a button pressed in a toolbar).
    void setCheckable(bool checkable);
    bool checkable() const { return checkable_; }
    void setChecked(bool checked);
    bool checked() const { return checked_; }

    void setOnTriggered(std::function<void()> handler) { onTriggered_ = std::move(handler); }
    // When enabled: checked or unchecked (when checkable), then its handler.
    void trigger();

    // `observer` called whenever it changes (its text, icon, shortcut, state): what shows it. Gives what stops it.
    std::size_t observe(std::function<void()> observer);
    void unobserve(std::size_t token);

private:
    void changed();

    LocalizedText text_;
    Icon icon_;
    Shortcut shortcut_;
    LocalizedText toolTip_;
    bool enabled_ = true;
    bool checkable_ = false;
    bool checked_ = false;
    std::function<void()> onTriggered_;
    std::vector<std::pair<std::size_t, std::function<void()>>> observers_;
    std::size_t nextToken_ = 1;
};

// A text without its mnemonic's & ("&File": "File"), and the mnemonic's letter, lowered (0: none).
GLW_EXPORT std::string withoutMnemonic(const std::string& text, char* mnemonic = nullptr);

} // namespace glw
