// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A window's menus, by their titles in a row (Window::setMenuBar: in the title bar, or under it). A click opens a title's
// menu under it; while one is open, the mouse over another title opens that one; Left and Right go to the menu beside.
// Alt and a title's mnemonic (its letter after a &: "&File") opens it, F10 the first. The shortcuts of its menus' actions
// work in its window.
//
//   auto bar = std::make_unique<glw::MenuBar>();
//   bar->setMenus({{.text = "&File", .items = {{.action = open}, {.action = save}, glw::MenuItem::separatorItem(), {.action = quit}}},
//                  {.text = "&Edit", .items = {{.action = undo}, {.action = redo}}}});
//   window.setMenuBar(std::move(bar));
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

class Label;
class UiRoot;

// Where a window's menu bar goes: in its title bar (before what is in it: its title, its tabs), or in a row under it.
enum class MenuBarPlacement : std::uint8_t { TitleBar, BelowTitleBar };

class GLW_EXPORT MenuBar : public Widget {
public:
    MenuBar();
    ~MenuBar() override;

    // Its menus: each an item with a title (its text) and the items of its menu.
    void setMenus(std::vector<MenuItem> menus);
    const std::vector<MenuItem>& menus() const { return menus_; }
    // Opens the menu at `index` (its first item chosen when by the keys).
    void open(std::size_t index, bool byKeys = false);
    std::optional<std::size_t> openMenu() const { return open_; }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onRootChanged() override;
    void onLanguageChanged() override;

private:
    void showTitles();                      // its labels' texts, translated, without their mnemonics' &
    void registerShortcuts();
    std::optional<std::size_t> titleAt(Point point) const;
    const MenuBarStyle& style() const;

    std::vector<MenuItem> menus_;
    std::vector<Label*> titles_;
    std::vector<char> mnemonics_;
    std::optional<std::size_t> open_;
    std::optional<std::size_t> hovered_;
    UiRoot* menuPopup_ = nullptr;           // the menu open
    std::uint64_t opening_ = 0;             // which opening the menu open is (an old one closing says nothing)
    UiRoot* registered_ = nullptr;          // the root its shortcuts are in
};

} // namespace glw
