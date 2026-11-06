// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Tree views: nodes expanded and collapsed (and expanded again as they were), by the arrows and the keys; the model's
// changes followed; a million nodes; children asked for when a node expands.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace glw;

namespace {

// Folders and files, by index: a node's id is its index.
class Folders final : public TreeModel {
public:
    struct Node {
        std::string name;
        std::vector<std::size_t> children;
    };
    std::vector<Node> nodes;
    std::vector<std::size_t> top;
    mutable int counted = 0;        // childCount asked: children read

    std::size_t add(std::optional<std::size_t> parent, std::string name) {
        nodes.push_back({std::move(name), {}});
        (parent ? nodes[*parent].children : top).push_back(nodes.size() - 1);
        return nodes.size() - 1;
    }
    std::size_t childCount(std::optional<TreeNode> parent) const override {
        ++counted;
        return parent ? nodes[parent->id].children.size() : top.size();
    }
    TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
        return {parent ? nodes[parent->id].children[index] : top[index]};
    }
    bool hasChildren(TreeNode node) const override { return !nodes[node.id].children.empty(); }

    void insert(std::optional<std::size_t> parent, std::size_t at, std::string name) {
        nodes.push_back({std::move(name), {}});
        auto& list = parent ? nodes[*parent].children : top;
        list.insert(list.begin() + static_cast<std::ptrdiff_t>(at), nodes.size() - 1);
        notifyInserted(of(parent), at, 1);
    }
    void remove(std::optional<std::size_t> parent, std::size_t at) {
        auto& list = parent ? nodes[*parent].children : top;
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(at));
        notifyRemoved(of(parent), at, 1);
    }
    void rename(std::optional<std::size_t> parent, std::size_t at, std::string name) {
        nodes[(parent ? nodes[*parent].children : top)[at]].name = std::move(name);
        notifyChanged(of(parent), at, 1);
    }

private:
    static std::optional<TreeNode> of(std::optional<std::size_t> index) {
        return index ? std::optional<TreeNode>(TreeNode{*index}) : std::nullopt;
    }
};

struct Treed {
    UiRoot root{tests::testContext()};
    TreeView* tree = nullptr;
    std::shared_ptr<Folders> folders = std::make_shared<Folders>();
    std::size_t a = 0, a1 = 0, a2 = 0, a2x = 0, b = 0;

    Treed() {
        a = folders->add(std::nullopt, "a");
        a1 = folders->add(a, "a1");
        a2 = folders->add(a, "a2");
        a2x = folders->add(a2, "a2x");
        b = folders->add(std::nullopt, "b");
        auto made = std::make_unique<TreeView>();
        tree = made.get();
        tree->setModel(folders);
        tree->setDelegate(std::make_unique<TreeTextDelegate>([f = folders.get()](TreeNode n) { return LocalizedText(f->nodes[n.id].name); }));
        root.setSize({300, 400});
        root.setContent(std::move(made));
        root.layout();
    }

    // the names that show, in order, each after a dot for each level it is in
    std::string shown() const {
        std::string out;
        for (std::size_t i = 0; i < tree->shownCount(); ++i) {
            if (i > 0)
                out += ' ';
            out += std::string(tree->depthOf(i), '.') + folders->nodes[tree->shownNode(i).id].name;
        }
        return out;
    }
    void key(platform::Key k) {
        root.dispatch(platform::KeyChanged{1, k, platform::KeyModifiers::None, true, false});
        root.layout();
    }
};

} // namespace

// The top of the row widget showing `index`, in the list's coordinates.
float rowTop(const TreeView& tree, std::size_t index) {
    const Widget* row = tree.list().rowWidget(index);
    return row ? row->bounds().y : -1;
}

TEST(aNodesChildrenComeOutFromUnderItAndGoBack) {
    tests::testScheduler().setAnimationsEnabled(true);
    Treed t;
    const float h = tests::testContext().theme.itemView.rowHeight;
    CHECK(rowTop(*t.tree, 1) == h);                                 // b, under a
    // opened: its two children come out from under it, b going down with them (the theme's normal motion, 200 ms)
    t.tree->setExpanded({t.a}, true);
    t.root.layout();
    CHECK(t.shown() == "a .a1 .a2 b");
    CHECK(rowTop(*t.tree, 3) < 1.5f * h);                           // b: still near where it was
    tests::advance(std::chrono::milliseconds(60));
    t.root.layout();
    const float midway = rowTop(*t.tree, 3);
    CHECK(midway > 1.5f * h && midway < 3 * h);
    tests::advance(std::chrono::milliseconds(200));
    t.root.layout();
    CHECK(rowTop(*t.tree, 3) == 3 * h && !tests::testScheduler().animating());
    // closed: they go back under it, b coming up; what they showed shows meanwhile
    t.tree->setExpanded({t.a}, false);
    t.root.layout();
    CHECK(t.shown() == "a b");
    CHECK(rowTop(*t.tree, 1) > 2.5f * h);
    tests::advance(std::chrono::milliseconds(300));
    t.root.layout();
    CHECK(rowTop(*t.tree, 1) == h && !tests::testScheduler().animating());
    tests::testScheduler().setAnimationsEnabled(false);
}

TEST(aTreeExpandsAndCollapses) {
    Treed t;
    CHECK(t.shown() == "a b");
    t.tree->setExpanded({t.a}, true);
    t.root.layout();
    CHECK(t.shown() == "a .a1 .a2 b");
    t.tree->setExpanded({t.a2}, true);
    CHECK(t.shown() == "a .a1 .a2 ..a2x b");
    // collapsed and expanded again: a2 is expanded still
    t.tree->setExpanded({t.a}, false);
    CHECK(t.shown() == "a b" && t.tree->isExpanded({t.a2}));
    t.tree->setExpanded({t.a}, true);
    CHECK(t.shown() == "a .a1 .a2 ..a2x b");
    t.tree->collapseAll();
    CHECK(t.shown() == "a b");
    t.tree->expandAll();
    CHECK(t.shown() == "a .a1 .a2 ..a2x b");
}

TEST(aTreeIsUsedByTheArrowsAndTheKeys) {
    Treed t;
    // the arrow of a: expanded (a is not chosen for it)
    const Rect row = *t.tree->list().itemRect(0);
    const Point arrow{row.x + 8, row.center().y};
    t.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, arrow});
    t.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, arrow});
    t.root.layout();
    CHECK(t.shown() == "a .a1 .a2 b" && !t.tree->current());

    // the keys: Down to a1, Down to a2, Right expands it, Right to a2x, Left to a2, Left collapses a2, Left to a, Left
    // collapses a
    t.tree->setCurrent({t.a});
    t.tree->list().focus();
    t.key(platform::Key::Down);
    t.key(platform::Key::Down);
    CHECK(t.tree->current() == TreeNode{t.a2});
    t.key(platform::Key::Right);
    CHECK(t.shown() == "a .a1 .a2 ..a2x b");
    t.key(platform::Key::Right);
    CHECK(t.tree->current() == TreeNode{t.a2x});
    t.key(platform::Key::Left);
    CHECK(t.tree->current() == TreeNode{t.a2});
    t.key(platform::Key::Left);
    CHECK(t.shown() == "a .a1 .a2 b");
    t.key(platform::Key::Left);
    CHECK(t.tree->current() == TreeNode{t.a});
    t.key(platform::Key::Left);
    CHECK(t.shown() == "a b");
    // Enter on a node with children: expanded, and activated
    std::vector<std::size_t> activated;
    t.tree->setOnActivated([&](TreeNode n) { activated.push_back(n.id); });
    t.key(platform::Key::Enter);
    CHECK(t.shown() == "a .a1 .a2 b" && activated == std::vector<std::size_t>{t.a});     // a2 was collapsed by Left
    // collapsed with the current one under it: the collapsed node is current
    t.tree->setExpanded({t.a2}, true);
    t.tree->setCurrent({t.a2x});
    t.tree->setExpanded({t.a}, false);
    CHECK(t.tree->current() == TreeNode{t.a});
}

TEST(aTreeFollowsItsModel) {
    Treed t;
    t.tree->setExpanded({t.a}, true);
    // a child inserted under a, which shows expanded: it shows
    t.folders->insert(t.a, 1, "new");
    CHECK(t.shown() == "a .a1 .new .a2 b");
    // under b, collapsed and without children: b can be expanded now, nothing more shows
    t.folders->insert(t.b, 0, "b1");
    CHECK(t.shown() == "a .a1 .new .a2 b");
    t.tree->setExpanded({t.b}, true);
    CHECK(t.shown() == "a .a1 .new .a2 b .b1");
    // renamed; removed (a2, with what is under it)
    t.folders->rename(t.a, 0, "first");
    t.root.layout();
    CHECK(t.shown() == "a .first .new .a2 b .b1");
    t.tree->setExpanded({t.a2}, true);
    t.folders->remove(t.a, 2);
    CHECK(t.shown() == "a .first .new b .b1");
    // at the top level
    t.folders->insert(std::nullopt, 0, "zero");
    CHECK(t.shown() == "zero a .first .new b .b1");
}

TEST(aTreeOfAMillionNodes) {
    // a thousand folders of a thousand files, made when asked
    class Generated final : public TreeModel {
    public:
        std::size_t childCount(std::optional<TreeNode> parent) const override { return !parent ? 1000 : parent->id < 1000 ? 1000 : 0; }
        TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
            return {!parent ? index : 1000 + parent->id * 1000 + index};
        }
        bool hasChildren(TreeNode node) const override { return node.id < 1000; }
    };
    UiRoot root(tests::testContext());
    root.setSize({300, 400});
    auto made = std::make_unique<TreeView>();
    TreeView& tree = *made;
    tree.setModel(std::make_shared<Generated>());
    tree.setDelegate(std::make_unique<TreeTextDelegate>([](TreeNode n) { return LocalizedText(std::to_string(n.id)); }));
    root.setContent(std::move(made));
    tree.expandAll();
    root.layout();
    CHECK(tree.shownCount() == 1'001'000);
    // widgets for what shows only; far down, the right nodes
    CHECK(tree.list().children().size() < 30);
    tree.list().setOffset(tree.list().maximumOffset() / 2);
    root.layout();
    const std::size_t first = tree.list().shownItems().first;
    CHECK(first > 400'000 && first < 600'000 && tree.depthOf(first) <= 1);
}

TEST(aTreeAsksForChildrenWhenItExpands) {
    Treed t;
    const int before = t.folders->counted;
    // a2's children are not asked for until a2 is expanded (hasChildren says it has some)
    t.tree->setExpanded({t.a}, true);
    const int afterA = t.folders->counted;
    CHECK(afterA == before + 1);
    t.tree->setExpanded({t.a2}, true);
    CHECK(t.folders->counted == afterA + 1);
}

TEST(aTreesScrollBarEasesWithItsRows) {
    tests::testScheduler().setAnimationsEnabled(true);
    Treed t;
    const float h = tests::testContext().theme.itemView.rowHeight;
    for (int i = 0; i < 6; ++i)
        t.folders->add(t.a, "more " + std::to_string(i));
    t.root.setSize({300, 4 * h});
    t.root.layout();
    paint::DrawList list;
    t.root.paint(list);
    CHECK(!t.tree->list().scrollBar().shown());
    // opened: eight rows come out of a; its bar's thumb shrinks as they do, not at once
    t.tree->setExpanded({t.a}, true);
    t.root.layout();
    tests::advance(std::chrono::milliseconds(300));
    t.root.layout();
    t.root.paint(list);
    const float settled = t.tree->list().scrollBar().thumb().height;
    t.tree->setExpanded({t.a}, false);
    t.root.layout();
    tests::advance(std::chrono::milliseconds(300));
    t.root.layout();
    t.root.paint(list);
    t.tree->setExpanded({t.a}, true);
    t.root.layout();
    tests::advance(std::chrono::milliseconds(70));
    t.root.layout();
    t.root.paint(list);
    const ScrollBar& bar = t.tree->list().scrollBar();
    CHECK(bar.shown() && bar.thumb().height > settled + 2 && bar.thumb().height < bar.track().height);
    tests::advance(std::chrono::milliseconds(300));
    t.root.layout();
    t.root.paint(list);
    CHECK(std::abs(t.tree->list().scrollBar().thumb().height - settled) < 0.5f);
    tests::testScheduler().setAnimationsEnabled(false);
}
