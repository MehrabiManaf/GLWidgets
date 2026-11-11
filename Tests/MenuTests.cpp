// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Menus and combo boxes: popups opened where they were asked for, items triggered by the mouse and the keys (separators
// and disabled items passed over), submenus opened beside their items and closed with their menus, right to left.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/ComboBox.hpp>
#include <GLWidgets/Widgets/Dialog.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/SearchPopup.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace glw;

namespace {

// Items 28 high after 4 of padding; a separator 9.
constexpr float itemHeight = 28, padding = 4;

float itemY(int index, int separatorsBefore = 0) {
    return padding + static_cast<float>(index - separatorsBefore) * itemHeight + static_cast<float>(separatorsBefore) * 9 + itemHeight / 2;
}

struct Menued {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    Widget* content = nullptr;
    std::string log;

    Menued() {
        auto made = std::make_unique<Widget>();
        made->setFocusable(true);
        content = made.get();
        root.setSize({400, 300});
        root.setContent(std::move(made));
        root.layout();
    }

    std::vector<tests::FakePopup*> popups() const { return host.open(); }
    UiRoot& popup(std::size_t i) const { return popups()[i]->root; }

    static void key(UiRoot& r, platform::Key k) { r.dispatch(platform::KeyChanged{1, k, platform::KeyModifiers::None, true, false}); }
    static void click(UiRoot& r, Point at) {
        r.dispatch(platform::MouseMoved{1, at});
        r.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
        r.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
    }
    MenuItem item(std::string text, bool enabled = true) {
        return {.text = LocalizedText(text), .onTriggered = [this, text] { log += text + ";"; }, .enabled = enabled};
    }
};

} // namespace

TEST(aMenuTriggersAnItemByTheMouseAndCloses) {
    Menued m;
    bool closed = false;
    MenuItem checked = m.item("C");
    checked.checked = true;
    CHECK(showContextMenu(*m.content, {50, 60}, {m.item("A"), MenuItem::separatorItem(), m.item("B", false), checked}, [&] { closed = true; }));
    CHECK(m.popups().size() == 1);
    CHECK(m.popups()[0]->anchor == Rect{50, 60, 0, 0} && m.popups()[0]->placement == PopupPlacement::Below);
    UiRoot& menu = m.popup(0);
    CHECK(menu.content()->hasFocus());
    CHECK(menu.size().width >= 160 && menu.size().height == 2 * padding + 3 * itemHeight + 9);

    // the disabled one: nothing
    Menued::click(menu, {40, itemY(2, 1)});
    CHECK(m.log.empty() && m.popups().size() == 1);
    // the last: triggered, the menu closed
    Menued::click(menu, {40, itemY(3, 1)});
    CHECK(m.log == "C;" && m.popups().empty() && closed);
}

TEST(aMenuIsUsedByTheKeys) {
    Menued m;
    showContextMenu(*m.content, {0, 0}, {m.item("A"), MenuItem::separatorItem(), m.item("B", false), m.item("C")});
    UiRoot& menu = m.popup(0);
    // Down: A; Down: C (past the separator and B); Down: A again, round; Up: C; Enter
    Menued::key(menu, platform::Key::Down);
    Menued::key(menu, platform::Key::Down);
    Menued::key(menu, platform::Key::Down);
    Menued::key(menu, platform::Key::Up);
    Menued::key(menu, platform::Key::Enter);
    CHECK(m.log == "C;" && m.popups().empty());
    // Escape: closed, nothing triggered
    showContextMenu(*m.content, {0, 0}, {m.item("A")});
    Menued::key(m.popup(0), platform::Key::Escape);
    CHECK(m.log == "C;" && m.popups().empty());
}

TEST(submenusOpenBesideTheirItemsAndCloseWithTheirMenus) {
    Menued m;
    MenuItem recent{.text = "Recent", .items = {m.item("x"), m.item("y")}};
    MenuItem file{.text = "File", .items = {m.item("Open"), recent}};
    showMenu(*m.content, {10, 10, 60, 24}, {file, m.item("Edit")});
    CHECK(m.popups()[0]->anchor == Rect{10, 10, 60, 24});
    UiRoot& menu = m.popup(0);

    // the mouse over File: its submenu beside it, its top level with File's
    menu.dispatch(platform::MouseMoved{1, {30, itemY(0)}});
    CHECK(m.popups().size() == 2 && m.popups()[1]->placement == PopupPlacement::After);
    CHECK(m.popups()[1]->anchor.y == 0 && m.popups()[1]->anchor.width == menu.size().width);
    // over Edit: the submenu goes
    menu.dispatch(platform::MouseMoved{1, {30, itemY(1)}});
    CHECK(m.popups().size() == 1);

    // the keys: Up to File, Right into it (Open chosen), Down to Recent, Right into it, Left out of it, Escape out of File's
    Menued::key(menu, platform::Key::Up);
    Menued::key(menu, platform::Key::Right);
    CHECK(m.popups().size() == 2);
    UiRoot& files = m.popup(1);
    Menued::key(files, platform::Key::Down);
    Menued::key(files, platform::Key::Right);
    CHECK(m.popups().size() == 3);
    Menued::key(m.popup(2), platform::Key::Left);
    CHECK(m.popups().size() == 2);
    Menued::key(files, platform::Key::Escape);
    CHECK(m.popups().size() == 1);

    // an item of a submenu's submenu: triggered, every menu closed
    Menued::key(menu, platform::Key::Right);
    Menued::key(m.popup(1), platform::Key::Down);
    Menued::key(m.popup(1), platform::Key::Enter);
    Menued::key(m.popup(2), platform::Key::Down);
    Menued::key(m.popup(2), platform::Key::Enter);
    CHECK(m.log == "y;" && m.popups().empty());

    // closing the menu closes its submenus
    showMenu(*m.content, {10, 10, 60, 24}, {file});
    m.popup(0).dispatch(platform::MouseMoved{1, {30, itemY(0)}});
    CHECK(m.popups().size() == 2);
    m.popups()[0]->host.closeWindow();
    CHECK(m.popups().empty());
}

TEST(aMenuRightToLeftOpensSubmenusByLeft) {
    Menued m;
    m.root.setLayoutDirection(LayoutDirection::RightToLeft);
    showContextMenu(*m.content, {0, 0}, {{.text = "File", .items = {m.item("Open")}}});
    UiRoot& menu = m.popup(0);
    menu.setLayoutDirection(LayoutDirection::RightToLeft);
    Menued::key(menu, platform::Key::Down);
    Menued::key(menu, platform::Key::Right);
    CHECK(m.popups().size() == 1);
    Menued::key(menu, platform::Key::Left);
    CHECK(m.popups().size() == 2);
    m.popup(1).setLayoutDirection(LayoutDirection::RightToLeft);      // as its window's, from the one it opened from
    Menued::key(m.popup(1), platform::Key::Right);
    CHECK(m.popups().size() == 1);
}

TEST(aComboBoxChoosesFromItsMenu) {
    Menued m;
    std::vector<std::size_t> changed;
    auto combo = std::make_unique<ComboBox>(ComboBox::Props{.items = {"One", "Two", "Three"}, .current = 1,
                                                            .onChanged = [&](std::size_t i) { changed.push_back(i); }});
    ComboBox& box = *combo;
    box.setAlignment(Alignment::Start, Alignment::Start);
    m.root.setContent(std::move(combo));
    m.root.layout();
    // a click: the menu under it, as wide as it at least, the current one checked
    Menued::click(m.root, {10, 10});
    CHECK(box.isOpen() && m.popups().size() == 1);
    CHECK(m.popups()[0]->anchor == (Rect{0, 0, box.size().width, box.size().height}));
    UiRoot& menu = m.popup(0);
    CHECK(menu.size().width >= box.size().width);
    // the third
    Menued::click(menu, {20, itemY(2)});
    CHECK(box.current() == 2 && changed == std::vector<std::size_t>{2} && !box.isOpen());
    // Up and Down without opening it; not past the ends
    box.focus();
    Menued::key(m.root, platform::Key::Up);
    Menued::key(m.root, platform::Key::Up);
    Menued::key(m.root, platform::Key::Up);
    CHECK(box.current() == 0 && changed == (std::vector<std::size_t>{2, 1, 0}) && m.popups().empty());
    // Enter opens it
    Menued::key(m.root, platform::Key::Enter);
    CHECK(box.isOpen());
}

TEST(aDialogIsClosedByItsButtonsEnterAndEscape) {
    Menued m;
    std::vector<std::optional<std::size_t>> closed;
    const auto onClosed = [&](std::optional<std::size_t> button) { closed.push_back(button); };
    CHECK(showMessage(*m.content, "Close the window?", "What is not saved is lost.", {"Close", "Cancel"}, onClosed));
    tests::FakePopup& popup = *m.popups()[0];
    CHECK(popup.modal && popup.placement == PopupPlacement::Center && popup.anchor == (Rect{0, 0, 400, 300}));
    auto& dialog = static_cast<Dialog&>(*popup.root.content());
    CHECK(dialog.button(0)->hasFocus());
    // Enter: the default button
    Menued::key(popup.root, platform::Key::Enter);
    CHECK(m.popups().empty() && closed.size() == 1 && closed[0] == 0u);
    // Escape: no choice
    showMessage(*m.content, "Close the window?", "", {"Close", "Cancel"}, onClosed);
    Menued::key(m.popup(0), platform::Key::Escape);
    CHECK(m.popups().empty() && closed.size() == 2 && !closed[1]);
    // a click on the second
    showMessage(*m.content, "Close the window?", "", {"Close", "Cancel"}, onClosed);
    auto& again = static_cast<Dialog&>(*m.popup(0).content());
    const Point at = again.button(1)->mapToRoot({10, 10});
    Menued::click(m.popup(0), at);
    CHECK(m.popups().empty() && closed.size() == 3 && closed[2] == 1u);
}

TEST(aDialogsFocusStartsOnItsFirstFieldAndEnterChoosesTheDefault) {
    Menued m;
    std::optional<std::size_t> chosen;
    auto form = std::make_unique<Widget>();
    form->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 8.0f));
    form->add(std::make_unique<Label>("Name"));
    LineEdit& field = form->add(std::make_unique<LineEdit>());
    CHECK(showDialog(*m.content, {.title = "Rename", .content = std::move(form), .buttons = {"Rename", "Cancel"},
                                  .onClosed = [&](std::optional<std::size_t> b) { chosen = b; }}));
    UiRoot& dialog = m.popup(0);
    CHECK(field.hasFocus());
    dialog.dispatch(platform::TextInput{1, "Graph"});
    CHECK(field.text() == "Graph");
    // Enter in the field (it has no handler of its own): the dialog's default button
    Menued::key(dialog, platform::Key::Enter);
    CHECK(chosen == 0u && m.popups().empty());
    // the text wraps at the dialog's width
    showMessage(*m.content, "A long one", std::string(400, 'x') + " " + std::string(10, 'y'), {"OK"});
    CHECK(m.popup(0).size().width <= 460);
}

TEST(searchFindsEveryWordAndRanksWhereTheyAre) {
    const std::vector<std::pair<std::string, std::string>> entries{
        {"Add", "Math"}, {"Multiply", "Math"}, {"Print String", "Text"}, {"To Text", "Text"}, {"Branch", "Flow"}};
    CHECK(searchTexts(entries, "") == (std::vector<std::size_t>{0, 1, 2, 3, 4}));
    // at the start of the text, then of a word of it, then anywhere (its detail too); case aside
    CHECK(searchTexts(entries, "TEXT") == (std::vector<std::size_t>{3, 2}));
    CHECK(searchTexts(entries, "m") == (std::vector<std::size_t>{1, 0}));
    CHECK(searchTexts(entries, "math add") == std::vector<std::size_t>{0});
    CHECK(searchTexts(entries, "str  pri") == std::vector<std::size_t>{2});
    CHECK(searchTexts(entries, "nothing").empty());
}

TEST(aSearchPopupFiltersAsItIsTypedAndChooses) {
    Menued m;
    std::vector<std::size_t> chosen;
    int closed = 0;
    const auto open = [&] {
        return showSearchPopup(*m.content, {40, 50, 0, 0},
                               {.items = {{"Add", "Math"}, {"Multiply", "Math"}, {"Print String", "Text"}},
                                .onChosen = [&](std::size_t i) { chosen.push_back(i); },
                                .onClosed = [&] { ++closed; }});
    };
    CHECK(open());
    UiRoot& popup = m.popup(0);
    CHECK(m.popups()[0]->anchor == (Rect{40, 50, 0, 0}) && popup.size() == (Size{300, 340}));
    // typed in its field (which has the focus): the list follows; Enter takes the first
    popup.dispatch(platform::TextInput{1, "mul"});
    Menued::key(popup, platform::Key::Enter);
    CHECK(chosen == std::vector<std::size_t>{1} && m.popups().empty() && closed == 1);
    // Down and Up choose while the field keeps the focus
    open();
    Menued::key(m.popup(0), platform::Key::Down);
    Menued::key(m.popup(0), platform::Key::Down);
    Menued::key(m.popup(0), platform::Key::Down);
    Menued::key(m.popup(0), platform::Key::Up);
    Menued::key(m.popup(0), platform::Key::Enter);
    CHECK(chosen == (std::vector<std::size_t>{1, 1}));
    // Escape: none
    open();
    Menued::key(m.popup(0), platform::Key::Escape);
    CHECK(chosen.size() == 2 && m.popups().empty() && closed == 3);
}

// Searchable: a click, or typing into it, opens a search popup; only its texts can be chosen.
TEST(aSearchableComboBoxChoosesBySearching) {
    Menued m;
    std::vector<std::size_t> changed;
    auto combo = std::make_unique<ComboBox>(ComboBox::Props{.items = {"bool", "int", "float", "string", "vector2"},
                                                            .current = 3,
                                                            .onChanged = [&](std::size_t i) { changed.push_back(i); },
                                                            .searchable = true});
    ComboBox& box = *combo;
    box.setAlignment(Alignment::Start, Alignment::Start);
    m.root.setContent(std::move(combo));
    m.root.layout();
    // a click: the search popup under it, at least 220 wide, the current one chosen at first
    Menued::click(m.root, {10, 10});
    CHECK(box.isOpen() && m.popups().size() == 1);
    CHECK(m.popup(0).size().width >= 220);
    Menued::key(m.popup(0), platform::Key::Down);       // the one after it
    Menued::key(m.popup(0), platform::Key::Enter);
    CHECK(box.current() == 4 && changed == std::vector<std::size_t>{4} && !box.isOpen());
    // with the focus, it takes typed text: what was typed is searched for
    box.focus();
    CHECK(m.host.textInput);
    m.root.dispatch(platform::TextInput{1, "fl"});
    CHECK(box.isOpen());
    Menued::key(m.popup(0), platform::Key::Enter);
    CHECK(box.current() == 2);
    // Escape: nothing chosen
    m.root.dispatch(platform::TextInput{1, "zz"});
    Menued::key(m.popup(0), platform::Key::Escape);
    CHECK(box.current() == 2 && changed.size() == 2 && !box.isOpen());
}

// An application that finds its items itself: by words they do not show ("+" for Add), in its order.
TEST(aSearchPopupFindsAsTheApplicationSays) {
    Menued m;
    std::vector<std::size_t> chosen;
    const std::vector<std::string> keywords{"+ plus sum", "- minus", "branch if"};
    CHECK(showSearchPopup(*m.content, {40, 50, 0, 0},
                          {.items = {{"Add", "Math"}, {"Subtract", "Math"}, {"Branch", "Flow"}},
                           .onChosen = [&](std::size_t i) { chosen.push_back(i); },
                           .search = [&](const std::string& query) {
                               std::vector<std::size_t> found;
                               for (std::size_t i = 0; i < keywords.size(); ++i) {
                                   if (query.empty() || keywords[i].find(query) != std::string::npos)
                                       found.push_back(i);
                               }
                               found.push_back(99);        // not an item: left out
                               return found;
                           }}));
    UiRoot& popup = m.popup(0);
    popup.dispatch(platform::TextInput{1, "+"});
    Menued::key(popup, platform::Key::Enter);
    CHECK(chosen == std::vector<std::size_t>{0});
    // nothing found but what is not an item: nothing chosen
    showSearchPopup(*m.content, {40, 50, 0, 0},
                    {.items = {{"Add", "Math"}}, .onChosen = [&](std::size_t i) { chosen.push_back(i); }, .search = [](const std::string&) {
                         return std::vector<std::size_t>{5};
                     }});
    Menued::key(m.popup(0), platform::Key::Enter);
    CHECK(chosen.size() == 1);
}
