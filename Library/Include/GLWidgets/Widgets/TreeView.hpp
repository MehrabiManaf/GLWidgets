// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A tree of a model's nodes (ItemModel.hpp). The nodes that show (the top level's, and the children of those expanded, in
// order) are the rows of a ListView: only those on screen are widgets, so a tree of millions of nodes costs what shows. A
// node's children are asked of the model when it is first expanded (a folder read when it is opened); a node expanded stays
// so when its parent is collapsed and expanded again.
//
// The view draws a row's indent and its arrow (by a node with children: pressed, it expands or collapses it); a delegate
// makes and fills the rest. Right expands, or goes to the first child; Left collapses, or goes to the parent (mirrored in
// right to left); a double click or Enter activates a node, and expands or collapses one with children.
//
// The children of each node show sorted (setSortOrder: the model's compare) and filtered (setFilter: the nodes accepted,
// with their ancestors, which show expanded), without the model changing. A node its model says is editable
// (ItemFlags::Editable) is edited in a widget its delegate makes, after its arrow, by a double click, F2 or typing.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace glw {

// What makes the widgets of a tree's rows (without their indent and arrow), and shows a node in one.
class GLW_EXPORT TreeDelegate {
public:
    virtual ~TreeDelegate();
    virtual std::unique_ptr<Widget> makeRow() = 0;
    virtual void bindRow(Widget& row, TreeNode node) = 0;
    // An editable node edited: the widget it is edited in (any widget); none: it is not. Then what was edited goes into
    // the model (not on Escape).
    virtual std::unique_ptr<Widget> makeEditor(TreeNode node) { return nullptr; }
    virtual void commitEdit(Widget& editor, TreeNode node) {}
};

// Each node's text in a row; edited, in a field (when told where the text goes).
class GLW_EXPORT TreeTextDelegate : public TreeDelegate {
public:
    explicit TreeTextDelegate(std::function<LocalizedText(TreeNode)> text) : text_(std::move(text)) {}
    void setOnEdited(std::function<void(TreeNode, std::string)> handler) { onEdited_ = std::move(handler); }
    std::unique_ptr<Widget> makeRow() override;
    void bindRow(Widget& row, TreeNode node) override;
    std::unique_ptr<Widget> makeEditor(TreeNode node) override;
    void commitEdit(Widget& editor, TreeNode node) override;

private:
    std::function<LocalizedText(TreeNode)> text_;
    std::function<void(TreeNode, std::string)> onEdited_;
};

class GLW_EXPORT TreeView : public Widget, private TreeModelObserver {
public:
    TreeView();
    ~TreeView() override;

    void setModel(std::shared_ptr<TreeModel> model);
    TreeModel* model() const { return model_.get(); }
    void setDelegate(std::unique_ptr<TreeDelegate> delegate);
    // Every row this high (none: the theme's); 0: each as high as its widget measures.
    void setRowHeight(std::optional<float> height);

    // Its children shown (asked of the model then), those of them that were expanded expanded again. A node that does not
    // show yet is expanded when it shows.
    void setExpanded(TreeNode node, bool expanded);
    bool isExpanded(TreeNode node) const { return expanded_.contains(node.id); }
    // Every node of the model with children (all of them asked of the model) / none.
    void expandAll();
    void collapseAll();

    std::optional<TreeNode> current() const;
    // When it shows.
    void setCurrent(TreeNode node);

    // ---- sorting and filtering ----
    // The children of each node sorted as the model compares them; none: in the model's order.
    void setSortOrder(std::optional<SortOrder> order);
    std::optional<SortOrder> sortOrder() const { return sortOrder_; }
    // Only the nodes `accepts` says yes to, and their ancestors (expanded, so that they show); none: all of them. Every
    // node of the model is asked (its children read).
    void setFilter(std::function<bool(TreeNode)> accepts);
    // Sorted and filtered again (what the filter says changed).
    void reorder();

    // ---- editing ----
    void setEditTriggers(EditTriggers triggers) { list_->setEditTriggers(triggers); }
    // The node edited, when it shows, is editable, and its delegate makes an editor (true then).
    bool edit(TreeNode node);
    bool editing() const { return list_->editing(); }
    Widget* editor() const { return list_->editor(); }
    void commitEdit() { list_->commitEdit(); }
    void cancelEdit() { list_->cancelEdit(); }

    // The nodes that show, in order (the list's items).
    std::size_t shownCount() const;
    TreeNode shownNode(std::size_t index) const;
    std::uint32_t depthOf(std::size_t index) const;
    std::optional<std::size_t> shownIndexOf(TreeNode node) const;
    // The list it shows them in: scrolled, its rows' widgets.
    ListView& list() const { return *list_; }

    void setOnCurrentChanged(std::function<void(std::optional<TreeNode>)> handler) { onCurrentChanged_ = std::move(handler); }
    // A double click or Enter on a node.
    void setOnActivated(std::function<void(TreeNode)> handler) { onActivated_ = std::move(handler); }

protected:
    void onPreviewKey(KeyEvent& event) override;

private:
    struct Entry {
        TreeNode node;
        std::uint32_t depth = 0;
        bool expandable = false;            // it has children, or may
        bool expanded = false;
    };
    class Rows;                             // the entries, as the list's model
    class RowDelegate;                      // the user's rows, with their indent and arrow

    // the model's changes
    void childrenInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void childrenRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void childrenChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void modelReset() override;

    // The children of `parent` (with those of its expanded children, and so on) as entries, at `depth`.
    void appendChildren(std::optional<TreeNode> parent, std::uint32_t depth, std::vector<Entry>& out) const;
    void rebuild();
    // Its children as the view shows them: filtered, sorted.
    std::vector<TreeNode> shownChildren(std::optional<TreeNode> parent) const;
    bool ordered() const { return sortOrder_.has_value() || static_cast<bool>(filter_); }
    // Whether it shows expanded: expanded by the user, or opened by the filter to show what it accepts in it.
    bool expandedNow(TreeNode node) const;
    void refilter();                        // the nodes kept by the filter, again
    // After the entry at `index` and those under it: the next one at its depth or above.
    std::size_t subtreeEnd(std::size_t index) const;
    // Where the `child`-th child of `parent` is (or goes) among the entries; none when `parent` does not show expanded.
    std::optional<std::size_t> childPosition(std::optional<TreeNode> parent, std::size_t child) const;
    void expandAt(std::size_t index);
    void collapseAt(std::size_t index);
    void toggle(std::size_t index);
    // A node's arrow: from 0 (collapsed) to 1 (expanded), turning while it opens or closes. Kept by node, not by row:
    // the rows are bound again (others show the node) as the entries change.
    float turnOf(TreeNode node, bool expanded) const;
    void turn(TreeNode node, bool expanded);

    std::shared_ptr<TreeModel> model_;
    std::unique_ptr<TreeDelegate> delegate_;
    std::shared_ptr<Rows> rows_;
    ListView* list_ = nullptr;
    std::unordered_set<std::uint64_t> expanded_;
    std::optional<SortOrder> sortOrder_;
    std::function<bool(TreeNode)> filter_;
    std::unordered_set<std::uint64_t> kept_;        // filtered: the nodes accepted, and their ancestors
    std::unordered_set<std::uint64_t> opened_;      // ... the ancestors, shown expanded (until collapsed)
    std::unordered_map<std::uint64_t, std::unique_ptr<Animation>> turning_;    // the arrows turning, by node
    std::function<void(std::optional<TreeNode>)> onCurrentChanged_;
    std::function<void(TreeNode)> onActivated_;
};

} // namespace glw
