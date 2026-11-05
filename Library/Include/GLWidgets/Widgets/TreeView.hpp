// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A tree of a model's nodes (ItemModel.hpp). The nodes that show (the top level's, and the children of those expanded, in
// order) are the rows of a table (TableView): only those on screen are widgets, so a tree of millions of nodes costs what
// shows. A node's children are asked of the model when it is first expanded (a folder read when it is opened); a node
// expanded stays so when its parent is collapsed and expanded again.
//
// Its columns are the model's (TreeModel::columnCount: one by default, the tree), under a header as a table's (shown when
// there are several, or the one has a title; hidden at will): resized by their edges, scrolled across. The first column
// shows a node's indent and its arrow (by a node with children: pressed, it expands or collapses it); a delegate makes
// and fills the cells. Right expands, or goes to the first child; Left collapses, or goes to the parent (mirrored in
// right to left); a double click or Enter activates a node, and expands or collapses one with children.
//
// The children of each node show sorted (by a column: a click on a sortable column's header, or sortBy) and filtered
// (setFilter: the nodes accepted, with their ancestors, which show expanded), without the model changing. A cell its model
// says is editable (ItemFlags::Editable) is edited in a widget its delegate makes (the first column's after its arrow),
// by a double click, F2 or typing.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace glw {

// What makes the widgets of a tree's cells (the first column's without its indent and arrow), and shows a node's cell in
// one.
class GLW_EXPORT TreeDelegate {
public:
    virtual ~TreeDelegate();
    // The kind of a column's cells: a cell of one kind shows any column of it. All one kind by default.
    virtual std::size_t cellKind(std::size_t column) const { return 0; }
    virtual std::unique_ptr<Widget> makeCell(std::size_t kind) = 0;
    // `cell` (made for the column's kind) shows the node's cell in `column` now.
    virtual void bindCell(Widget& cell, TreeNode node, std::size_t column) = 0;
    // An editable cell edited: the widget it is edited in (any widget); none: it is not. Then what was edited goes into
    // the model (not on Escape).
    virtual std::unique_ptr<Widget> makeEditor(TreeNode node, std::size_t column) { return nullptr; }
    virtual void commitEdit(Widget& editor, TreeNode node, std::size_t column) {}
};

// Each cell's text; edited, in a field (when told where the text goes).
class GLW_EXPORT TreeTextDelegate : public TreeDelegate {
public:
    explicit TreeTextDelegate(std::function<LocalizedText(TreeNode, std::size_t column)> text) : text_(std::move(text)) {}
    // A tree of one column: each node's text.
    explicit TreeTextDelegate(std::function<LocalizedText(TreeNode)> text)
        : text_([text = std::move(text)](TreeNode node, std::size_t) { return text(node); }) {}
    void setOnEdited(std::function<void(TreeNode, std::size_t column, std::string)> handler) { onEdited_ = std::move(handler); }
    void setOnEdited(std::function<void(TreeNode, std::string)> handler) {
        onEdited_ = [handler = std::move(handler)](TreeNode node, std::size_t, std::string text) { handler(node, std::move(text)); };
    }
    std::unique_ptr<Widget> makeCell(std::size_t kind) override;
    void bindCell(Widget& cell, TreeNode node, std::size_t column) override;
    std::unique_ptr<Widget> makeEditor(TreeNode node, std::size_t column) override;
    void commitEdit(Widget& editor, TreeNode node, std::size_t column) override;

private:
    std::function<LocalizedText(TreeNode, std::size_t)> text_;
    std::function<void(TreeNode, std::size_t, std::string)> onEdited_;
};

class GLW_EXPORT TreeView : public Widget, private TreeModelObserver {
public:
    TreeView();
    ~TreeView() override;

    void setModel(std::shared_ptr<TreeModel> model);
    TreeModel* model() const { return model_.get(); }
    void setDelegate(std::unique_ptr<TreeDelegate> delegate);
    // Every row this high (none: the theme's); 0: each as high as its tallest cell that shows.
    void setRowHeight(std::optional<float> height);
    // Its own style (no lines between its rows, its columns' borders...), or none: the theme's itemView.
    void setStyle(std::optional<ItemViewStyle> style);
    const ItemViewStyle& style() const { return table_->style(); }

    // ---- columns ----
    float columnWidth(std::size_t column) const { return table_->columnWidth(column); }
    void setColumnWidth(std::size_t column, float width) { table_->setColumnWidth(column, width); }
    // As wide as its header and the cells of the nodes that show need (the first one's indents and arrows with them),
    // eased there; a double click on its header's edge does it.
    void fitColumn(std::size_t column) { table_->fitColumn(column); }
    // Its header: shown when it has several columns, or a title for its one (true by default); hidden at will.
    void setHeaderVisible(bool visible);
    bool headerVisible() const { return headerVisible_; }
    bool headerShown() const { return table_->headerVisible(); }
    // The table it shows its rows in: its columns, its header, its rows' list.
    TableView& table() const { return *table_; }

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
    // The node shown, wherever it is in the tree: its ancestors expanded (as the model says: TreeModel::parent), it made
    // current and scrolled to. False when it does not show even so (its filter leaves it out).
    bool reveal(TreeNode node);
    // Its parent, as the model says (none: at the top level, or the model does not know).
    std::optional<TreeNode> parentOf(TreeNode node) const { return model_ ? model_->parent(node) : std::nullopt; }

    // ---- the selection ----
    // Single (by default), Multiple (Ctrl and Shift, as a list's), None. The nodes selected stay so as the tree is sorted
    // and filtered; those under a node collapsed are not any more.
    void setSelectionMode(SelectionMode mode) { table_->setSelectionMode(mode); }
    SelectionMode selectionMode() const { return list_->selectionMode(); }
    bool isSelected(TreeNode node) const;
    // In the order they show.
    std::vector<TreeNode> selectedNodes() const;
    // Multiple: when it shows.
    void setSelected(TreeNode node, bool selected);
    void selectAll() { list_->selectAll(); }
    void clearSelection() { list_->clearSelection(); }
    void setOnSelectionChanged(std::function<void()> handler) { list_->setOnSelectionChanged(std::move(handler)); }

    // ---- sorting and filtering ----
    // The children of each node sorted by a column, as the model compares them in it; none: in the model's order. A click
    // on a sortable column's header does it: ascending, then descending, then not.
    void sortBy(std::optional<std::size_t> column, SortOrder order = SortOrder::Ascending);
    std::optional<std::size_t> sortColumn() const { return sortOrder_ ? std::optional<std::size_t>(sortColumn_) : std::nullopt; }
    // ... by the first column.
    void setSortOrder(std::optional<SortOrder> order);
    std::optional<SortOrder> sortOrder() const { return sortOrder_; }
    // Only the nodes `accepts` says yes to, and their ancestors (expanded, so that they show); none: all of them. Every
    // node of the model is asked (its children read).
    void setFilter(std::function<bool(TreeNode)> accepts);
    // Sorted and filtered again (what the filter says changed).
    void reorder();

    // ---- editing ----
    void setEditTriggers(EditTriggers triggers) { table_->setEditTriggers(triggers); }
    // The node's cell edited, when it shows, is editable, and its delegate makes an editor (true then).
    bool edit(TreeNode node, std::size_t column = 0);
    bool editing() const { return table_->editing(); }
    Widget* editor() const { return table_->editor(); }
    void commitEdit() { table_->commitEdit(); }
    void cancelEdit() { table_->cancelEdit(); }

    // The nodes that show, in order (the list's items).
    std::size_t shownCount() const;
    TreeNode shownNode(std::size_t index) const;
    std::uint32_t depthOf(std::size_t index) const;
    std::optional<std::size_t> shownIndexOf(TreeNode node) const;
    // The list its rows are: scrolled, its rows' widgets.
    ListView& list() const { return *list_; }

    void setOnCurrentChanged(std::function<void(std::optional<TreeNode>)> handler) { onCurrentChanged_ = std::move(handler); }
    // A double click or Enter on a node.
    void setOnActivated(std::function<void(TreeNode)> handler) { onActivated_ = std::move(handler); }

protected:
    void onPreviewKey(KeyEvent& event) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    struct Entry {
        TreeNode node;
        std::uint32_t depth = 0;
        bool expandable = false;            // it has children, or may
        bool expanded = false;
    };
    class Rows;                             // the entries, as the table's model
    class CellDelegate;                     // the user's cells; the first column's with its indent and arrow

    // the model's changes
    void childrenInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void childrenRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void childrenChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count) override;
    void modelReset() override;
    void changesBegun() override;
    void changesEnded() override;

    // The children of `parent` (with those of its expanded children, and so on) as entries, at `depth`.
    void appendChildren(std::optional<TreeNode> parent, std::uint32_t depth, std::vector<Entry>& out) const;
    void rebuild();
    // Its children as the view shows them: filtered, sorted.
    std::vector<TreeNode> shownChildren(std::optional<TreeNode> parent) const;
    bool ordered() const { return sortOrder_.has_value() || static_cast<bool>(filter_); }
    // Whether it shows expanded: expanded by the user, or opened by the filter to show what it accepts in it.
    bool expandedNow(TreeNode node) const;
    void refilter();                        // the nodes kept by the filter, again
    void showHeader();                      // shown or not, as its columns and the user say
    void headerClicked(std::size_t column);
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
    TableView* table_ = nullptr;
    ListView* list_ = nullptr;              // the table's
    std::optional<ItemViewStyle> style_;
    bool headerVisible_ = true;
    std::unordered_set<std::uint64_t> expanded_;
    std::optional<SortOrder> sortOrder_;
    std::size_t sortColumn_ = 0;
    std::function<bool(TreeNode)> filter_;
    std::unordered_set<std::uint64_t> kept_;        // filtered: the nodes accepted, and their ancestors
    std::unordered_set<std::uint64_t> opened_;      // ... the ancestors, shown expanded (until collapsed)
    int batch_ = 0;                                 // in a batch of the model's changes
    bool reorderLater_ = false;                     // ... sorted or filtered: all of it again at its end
    std::unordered_map<std::uint64_t, std::unique_ptr<Animation>> turning_;    // the arrows turning, by node
    std::function<void(std::optional<TreeNode>)> onCurrentChanged_;
    std::function<void(TreeNode)> onActivated_;
};

} // namespace glw
