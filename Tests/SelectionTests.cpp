// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Selecting several items: in a list, by Ctrl (Cmd on macOS) and Shift with the mouse and the keys, kept as the model
// changes and as the list is sorted and filtered; in a table, its rows; in a tree, its nodes, kept as it is sorted.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace glw;
using platform::KeyModifiers;

namespace {

// A list of ten items, several of them selected at a time.
struct Picked {
    UiRoot root{tests::testContext()};
    ListView* list = nullptr;
    std::shared_ptr<StringListModel> texts = std::make_shared<StringListModel>(
        std::vector<LocalizedText>{"j", "i", "h", "g", "f", "e", "d", "c", "b", "a"});
    int told = 0;

    Picked() {
        auto made = std::make_unique<ListView>();
        list = made.get();
        list->setModel(texts);
        list->setSelectionMode(SelectionMode::Multiple);
        list->setOnSelectionChanged([this] { ++told; });
        root.setSize({300, 600});
        root.setContent(std::move(made));
        root.layout();
        list->focus();
    }
    // A click on the row at `position`, with the modifiers held.
    void click(std::size_t position, KeyModifiers held = KeyModifiers::None) {
        root.dispatch(platform::KeyChanged{1, platform::Key::Unknown, held, true, false});
        const float h = list->style().rowHeight;
        const Point at{100, h * (static_cast<float>(position) + 0.5f)};
        root.dispatch(platform::MouseMoved{1, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
        root.dispatch(platform::KeyChanged{1, platform::Key::Unknown, KeyModifiers::None, false, false});
        root.layout();
    }
    void key(platform::Key k, KeyModifiers held = KeyModifiers::None) {
        root.dispatch(platform::KeyChanged{1, k, held, true, false});
        root.dispatch(platform::KeyChanged{1, k, KeyModifiers::None, false, false});
        root.layout();
    }
    // the texts of those selected, in the model's order
    std::string selected() const {
        std::string out;
        for (std::size_t i : list->selectedItems())
            out += texts->text(i).source;
        return out;
    }
};

constexpr KeyModifiers control = platform::shortcutModifier;
constexpr KeyModifiers shift = KeyModifiers::Shift;

} // namespace

TEST(aListSelectsSeveralItemsByTheMouse) {
    Picked p;
    // a click: it alone; Ctrl: one more, or one less; Shift: from the anchor (the last clicked) to it
    p.click(1);
    CHECK(p.selected() == "i" && p.list->current() == 1u);
    p.click(3, control);
    CHECK(p.selected() == "ig" && p.list->current() == 3u);
    p.click(5, shift);
    CHECK(p.selected() == "gfe");
    p.click(0, control | shift);                // added to what was
    CHECK(p.selected() == "jihgfe");
    p.click(2, control);
    CHECK(p.selected() == "jigfe" && !p.list->isSelected(2) && p.list->selectedCount() == 5);
    p.click(7);
    CHECK(p.selected() == "c" && p.told > 0);
}

TEST(aListSelectsSeveralItemsByTheKeys) {
    Picked p;
    p.click(2);
    // Shift with the arrows: from the anchor; Ctrl: the current item only, Ctrl+Space taking it
    p.key(platform::Key::Down, shift);
    p.key(platform::Key::Down, shift);
    CHECK(p.selected() == "hgf" && p.list->current() == 4u);
    p.key(platform::Key::Up, shift);
    CHECK(p.selected() == "hg");
    p.key(platform::Key::Down, control);
    p.key(platform::Key::Down, control);
    CHECK(p.selected() == "hg" && p.list->current() == 5u);
    p.key(platform::Key::Space, control);
    CHECK(p.selected() == "hge");
    // an arrow alone: that one
    p.key(platform::Key::Down);
    CHECK(p.selected() == "d");
    // all of them
    p.key(platform::Key::A, control);
    CHECK(p.list->selectedCount() == 10);
    p.list->clearSelection();
    CHECK(p.list->selectedCount() == 0);
}

TEST(aListsSelectionStaysItsItemsAsTheModelAndTheOrderChange) {
    Picked p;
    p.click(1);
    p.click(3, shift);                          // i h g
    CHECK(p.selected() == "ihg");
    // inserted before them, removed among them: still theirs
    p.texts->insert(0, LocalizedText("x"));
    CHECK(p.selected() == "ihg" && p.list->isSelected(2));
    p.texts->remove(3);                         // h
    CHECK(p.selected() == "ig");
    // sorted: the same items, wherever they show; Shift selects what shows between
    p.list->setSortOrder(SortOrder::Ascending);  // a b c d e f g i j x
    CHECK(p.selected() == "ig");
    p.click(0);
    p.click(2, shift);                          // a b c
    CHECK(p.selected() == "cba");               // in the model's order
    // filtered: what it leaves out is no longer selected
    p.list->setFilter([&](std::size_t i) { return p.texts->text(i).source != "b"; });
    CHECK(p.selected() == "ca");
    // Single: the current one only
    p.list->setSelectionMode(SelectionMode::Single);
    CHECK(p.list->selectedCount() == 1);
}

namespace {

// a: a1 a2, b
class Nodes final : public TreeModel {
public:
    std::vector<std::string> names{"a", "a1", "a2", "b"};
    std::size_t childCount(std::optional<TreeNode> parent) const override { return !parent ? 2 : parent->id == 0 ? 2 : 0; }
    TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
        if (!parent)
            return {index == 0 ? 0u : 3u};
        return {1 + index};
    }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(TreeNode a, TreeNode b, std::size_t) const override { return names[a.id] <=> names[b.id]; }
};

} // namespace

TEST(aTreeSelectsSeveralNodesKeptAsItIsSorted) {
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<TreeView>();
    TreeView& tree = *made;
    auto nodes = std::make_shared<Nodes>();
    tree.setModel(nodes);
    tree.setDelegate(std::make_unique<TreeTextDelegate>([n = nodes.get()](TreeNode node) { return LocalizedText(n->names[node.id]); }));
    root.setSize({300, 400});
    root.setContent(std::move(made));
    tree.setSelectionMode(SelectionMode::Multiple);
    tree.expandAll();
    root.layout();
    // a1 and b
    tree.setSelected({1}, true);
    tree.setSelected({3}, true);
    CHECK((tree.selectedNodes() == std::vector<TreeNode>{{1}, {3}}) && tree.isSelected({3}) && !tree.isSelected({0}));
    // sorted the other way (the rows made again): the same nodes, in the order they show
    tree.sortBy(0, SortOrder::Descending);
    CHECK((tree.selectedNodes() == std::vector<TreeNode>{{3}, {1}}));
    // a node opened above them: still they
    tree.collapseAll();
    tree.setSelected({3}, true);
    tree.setSelected({0}, true);
    tree.setExpanded({0}, true);
    CHECK(tree.isSelected({3}) && tree.isSelected({0}) && !tree.isSelected({1}));
    // collapsed: those under it no longer
    tree.setSelected({2}, true);
    tree.setExpanded({0}, false);
    CHECK((tree.selectedNodes() == std::vector<TreeNode>{{3}, {0}}) || (tree.selectedNodes() == std::vector<TreeNode>{{0}, {3}}));
}

namespace {

class Grid final : public TableModel {
public:
    std::size_t rowCount() const override { return 20; }
    std::size_t columnCount() const override { return 2; }
};

} // namespace

TEST(aTableSelectsSeveralRows) {
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<TableView>();
    TableView& table = *made;
    table.setModel(std::make_shared<Grid>());
    table.setDelegate(std::make_unique<TableTextDelegate>([](std::size_t r, std::size_t c) { return LocalizedText(std::to_string(r * 10 + c)); }));
    table.setSelectionMode(SelectionMode::Multiple);
    root.setSize({400, 600});
    root.setContent(std::move(made));
    root.layout();
    int told = 0;
    table.setOnSelectionChanged([&] { ++told; });
    table.list().setSelected(2, true);
    table.list().setSelected(5, true);
    CHECK((table.selectedRows() == std::vector<std::size_t>{2, 5}) && table.isSelected(5) && told == 2);
}
