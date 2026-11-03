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
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace glw {

// What makes the widgets of a tree's rows (without their indent and arrow), and shows a node in one.
class GLW_EXPORT TreeDelegate {
public:
    virtual ~TreeDelegate();
    virtual std::unique_ptr<Widget> makeRow() = 0;
    virtual void bindRow(Widget& row, TreeNode node) = 0;
};

// Each node's text in a row.
class GLW_EXPORT TreeTextDelegate : public TreeDelegate {
public:
    explicit TreeTextDelegate(std::function<LocalizedText(TreeNode)> text) : text_(std::move(text)) {}
    std::unique_ptr<Widget> makeRow() override;
    void bindRow(Widget& row, TreeNode node) override;

private:
    std::function<LocalizedText(TreeNode)> text_;
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
    // After the entry at `index` and those under it: the next one at its depth or above.
    std::size_t subtreeEnd(std::size_t index) const;
    // Where the `child`-th child of `parent` is (or goes) among the entries; none when `parent` does not show expanded.
    std::optional<std::size_t> childPosition(std::optional<TreeNode> parent, std::size_t child) const;
    void expandAt(std::size_t index);
    void collapseAt(std::size_t index);
    void toggle(std::size_t index);

    std::shared_ptr<TreeModel> model_;
    std::unique_ptr<TreeDelegate> delegate_;
    std::shared_ptr<Rows> rows_;
    ListView* list_ = nullptr;
    std::unordered_set<std::uint64_t> expanded_;
    std::function<void(std::optional<TreeNode>)> onCurrentChanged_;
    std::function<void(TreeNode)> onActivated_;
};

} // namespace glw
