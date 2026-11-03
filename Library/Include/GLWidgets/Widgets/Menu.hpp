// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Menus: a list of items in a popup (UiHost::openPopup), each with a text, maybe a check mark, a shortcut shown at its end
// and a submenu beside it. The mouse chooses an item by going over it (a submenu opens then) and triggers it by letting go
// of a button over it; the keys too: Up and Down (Home, End), Enter or Space, Right into a submenu and Left out of it
// (mirrored in right to left), Escape. A menu closes when an item is triggered, by Escape, and as popups do: a click outside
// it, the application losing the focus.
//
//   glw::showContextMenu(*this, event.position, {
//       {.text = "Cut", .onTriggered = [this] { cut(); }, .shortcut = "Ctrl+X"},
//       glw::MenuItem::separatorItem(),
//       {.text = "Add", .items = {{.text = "Node", .onTriggered = ...}, {.text = "Comment", .onTriggered = ...}}},
//   });
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace glw {

struct MenuItem {
    LocalizedText text;
    std::function<void()> onTriggered;      // after the menu closed
    // Shown at its end (what the shortcut is: the menu does not handle it).
    LocalizedText shortcut;
    bool enabled = true;
    // A check mark when true; none, though the room for one, when false.
    std::optional<bool> checked;
    // A submenu, opened beside it.
    std::vector<MenuItem> items;
    bool separator = false;

    static MenuItem separatorItem() {
        MenuItem item;
        item.separator = true;
        return item;
    }
};

// A menu of `items` at `at` (the widget's coordinates: where the mouse was pressed). False where the widget's window has no
// popups.
GLW_EXPORT bool showContextMenu(Widget& from, Point at, std::vector<MenuItem> items, std::function<void()> onClosed = {});
// A menu of `items` under `anchor` (the widget's coordinates: a button), at least as wide as it.
GLW_EXPORT bool showMenu(Widget& from, const Rect& anchor, std::vector<MenuItem> items, std::function<void()> onClosed = {});

} // namespace glw
