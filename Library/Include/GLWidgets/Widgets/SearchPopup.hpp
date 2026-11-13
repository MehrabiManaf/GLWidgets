// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A search popup: a field over a list of items, the list filtered as the field is typed in. What adds a node to a graph
// from its name, what completes a word in an editor, a command palette.
//
// The focus is in the field. Every word typed must be in an item's text or detail (case aside); those whose text starts
// with what is typed come first, then those with a word that does, then the others, each in their order. Up and Down
// (Page Up, Page Down) choose in the list while the field keeps the focus; Enter or a click on an item takes it and closes
// the popup; Escape closes it without one. An application with words of its own for its items (a node's keywords: "+"
// for Add, its English name in Persian) finds and orders them itself (SearchOptions::search). Items in folders (a node's
// category) show in a tree of them (SearchOptions::tree).
//
//   glw::showSearchPopup(*this, {mouse.x, mouse.y, 0, 0},
//                        {.items = {{"Add", "Math"}, {"Branch", "Flow"}}, .onChosen = [this](std::size_t i) { ... }});
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace glw {

struct SearchItem {
    LocalizedText text;
    LocalizedText detail;                   // shown muted at its end (a category); searched too
    // Its folders, the outermost first: where it is in a tree of them (SearchOptions::tree).
    std::vector<LocalizedText> path{};
};

struct SearchOptions {
    std::vector<SearchItem> items;
    LocalizedText placeholder;              // in the field while it is empty
    std::string text;                       // in the field at first
    std::function<void(std::size_t)> onChosen;      // the item's index in `items`, once the popup closed
    std::function<void()> onClosed;         // after it closed, chosen or not
    Size size{300, 340};
    // The item chosen now (a combo box's): selected, and scrolled to, while the field is empty. None: the first.
    std::optional<std::size_t> current;
    // The items `query` (what the field has) finds, best first: their indices in `items`. None: those with every word of
    // it in their text or detail (searchTexts).
    std::function<std::vector<std::size_t>(const std::string& query)> search;
    // The items in a tree of their folders (SearchItem::path) rather than a list, their details not shown: while nothing
    // is typed, every item, the folders in order, closed (open: `expanded`); what is found, under its folders open, each
    // folder where its best item comes, the best item chosen. Enter or a click on a folder opens or closes it.
    bool tree = false;
    bool expanded = false;
};

// Opens it under `anchor` (the widget's coordinates). False where the widget's window has no popups.
GLW_EXPORT bool showSearchPopup(Widget& from, const Rect& anchor, SearchOptions options);

// Which of `entries` (a text and its detail each) `query` finds, best first, as the popup orders them: their indices.
GLW_EXPORT std::vector<std::size_t> searchTexts(const std::vector<std::pair<std::string, std::string>>& entries, const std::string& query);

} // namespace glw
