// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The data views show (ListView), apart from the widgets that show it: views make widgets for what shows only, so a model
// may hold millions of items. A model says how many items it has and tells its views what changed, by ranges; what an item
// is, is its own (a delegate that knows the model shows it), so nothing goes through a value of any type.
//
//   class Files : public glw::ListModel {
//   public:
//       std::size_t count() const override { return names.size(); }
//       void add(std::string name) { names.push_back(std::move(name)); notifyInserted(names.size() - 1, 1); }
//       // sorted by name (ListView::setSortOrder), renamed in place (ItemFlags::Editable)
//       std::partial_ordering compare(std::size_t a, std::size_t b) const override { return names[a] <=> names[b]; }
//       glw::ItemFlags flags(std::size_t) const override { return glw::ItemFlags::Editable; }
//       std::vector<std::string> names;
//   };
//
// A model says what can be done with an item (flags: editable), and how two of its items compare, for the views to sort
// them: the data is the model's, so are its order and its kinds of values.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace glw {

// What can be done with an item, as its model says. None by default: it is shown and chosen.
enum class ItemFlags : std::uint8_t {
    None = 0,
    Editable = 1 << 0,      // edited in place (a double click, F2, typing): in a widget its delegate makes
};
constexpr ItemFlags operator|(ItemFlags a, ItemFlags b) {
    return static_cast<ItemFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr ItemFlags operator&(ItemFlags a, ItemFlags b) {
    return static_cast<ItemFlags>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr bool hasFlag(ItemFlags flags, ItemFlags flag) {
    return (flags & flag) != ItemFlags::None;
}

enum class SortOrder : std::uint8_t {
    Ascending,              // the least first
    Descending,
};

// What a view of a list model is told, after the model changed.
class GLW_EXPORT ListModelObserver {
public:
    virtual ~ListModelObserver();
    // `count` items now at `first` (those after them moved on).
    virtual void itemsInserted(std::size_t first, std::size_t count) = 0;
    // The `count` items that were at `first` went (those after them moved back).
    virtual void itemsRemoved(std::size_t first, std::size_t count) = 0;
    // The `count` items at `first` are others, or show otherwise.
    virtual void itemsChanged(std::size_t first, std::size_t count) = 0;
    // All of it changed.
    virtual void modelReset() = 0;
    // A batch of changes begins / ends (beginChanges): told in between as they happen still.
    virtual void changesBegun() {}
    virtual void changesEnded() {}
};

class GLW_EXPORT ListModel {
public:
    virtual ~ListModel();
    ListModel() = default;
    ListModel(const ListModel&) = delete;
    ListModel& operator=(const ListModel&) = delete;

    virtual std::size_t count() const = 0;
    virtual ItemFlags flags(std::size_t index) const { return ItemFlags::None; }
    // How two items compare, to sort them (ListView::setSortOrder): by default equivalent (the model's order kept).
    virtual std::partial_ordering compare(std::size_t a, std::size_t b) const { return std::partial_ordering::equivalent; }

    void addObserver(ListModelObserver& observer);
    void removeObserver(ListModelObserver& observer);

    // A batch of changes (all that came since the last frame): each still told as it happens, the views put what they mean
    // in order once, at the end (sorted: each item inserted or changed placed once, where it sorts then). Nested: the
    // outermost counts. Nothing is to be asked of the views in between.
    void beginChanges();
    void endChanges();

protected:
    // What a model calls once it changed: its views follow.
    void notifyInserted(std::size_t first, std::size_t count);
    void notifyRemoved(std::size_t first, std::size_t count);
    void notifyChanged(std::size_t first, std::size_t count);
    void notifyReset();

private:
    std::vector<ListModelObserver*> observers_;
    int batch_ = 0;
};

// Texts: a list of what a ListView shows with a TextDelegate.
class GLW_EXPORT StringListModel : public ListModel {
public:
    StringListModel() = default;
    explicit StringListModel(std::vector<LocalizedText> items) : items_(std::move(items)) {}

    std::size_t count() const override { return items_.size(); }
    const LocalizedText& text(std::size_t index) const { return items_[index]; }
    // Sorted by their texts, as they are written (case aside, for Latin letters).
    std::partial_ordering compare(std::size_t a, std::size_t b) const override;
    // Edited in place (a ListView shows them in fields, and sets them): off by default.
    void setEditable(bool editable) { editable_ = editable; }
    ItemFlags flags(std::size_t) const override { return editable_ ? ItemFlags::Editable : ItemFlags::None; }

    void setItems(std::vector<LocalizedText> items);
    void insert(std::size_t index, LocalizedText text);
    void append(LocalizedText text) { insert(items_.size(), std::move(text)); }
    void remove(std::size_t index, std::size_t count = 1);
    void set(std::size_t index, LocalizedText text);

private:
    std::vector<LocalizedText> items_;
    bool editable_ = false;
};

// ---- trees ----

// A node of a tree model: what the model knows it by (an index, a pointer as a number...), its own. A node's id stays its
// while it is in the model.
struct TreeNode {
    std::uint64_t id = 0;
    friend bool operator==(const TreeNode&, const TreeNode&) = default;
};

// What a view of a tree model is told, after the model changed. `parent`: none for the top level.
class GLW_EXPORT TreeModelObserver {
public:
    virtual ~TreeModelObserver();
    virtual void childrenInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count) = 0;
    virtual void childrenRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count) = 0;
    virtual void childrenChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count) = 0;
    virtual void modelReset() = 0;
    // A batch of changes begins / ends (beginChanges): told in between as they happen still.
    virtual void changesBegun() {}
    virtual void changesEnded() {}
};

// Nodes, each with its children (TreeView). The top level is the children of no node.
class GLW_EXPORT TreeModel {
public:
    virtual ~TreeModel();
    TreeModel() = default;
    TreeModel(const TreeModel&) = delete;
    TreeModel& operator=(const TreeModel&) = delete;

    virtual std::size_t childCount(std::optional<TreeNode> parent) const = 0;
    virtual TreeNode child(std::optional<TreeNode> parent, std::size_t index) const = 0;
    // The node a node is a child of (none: it is at the top level). What TreeView::reveal opens the way to a node by;
    // a model that does not know (by default) has its nodes revealed only where they show.
    virtual std::optional<TreeNode> parent(TreeNode node) const { return std::nullopt; }
    // Whether it has children, before they are counted (a folder not read yet: read when it is expanded). By default,
    // whether it has any.
    virtual bool hasChildren(TreeNode node) const { return childCount(node) > 0; }
    virtual ItemFlags flags(TreeNode node) const { return ItemFlags::None; }
    // How two children of one node compare, to sort them (TreeView::setSortOrder): by default equivalent.
    virtual std::partial_ordering compare(TreeNode a, TreeNode b) const { return std::partial_ordering::equivalent; }

    // Its columns: one by default (the tree), more for a tree of columns (TreeView shows them as a table does, their
    // headers over them); the first one shows the tree. A column's flags and comparison are the first one's by default,
    // the others' none.
    virtual std::size_t columnCount() const { return 1; }
    virtual LocalizedText header(std::size_t column) const { return {}; }
    virtual ItemFlags flags(TreeNode node, std::size_t column) const { return column == 0 ? flags(node) : ItemFlags::None; }
    // Whether the children can be sorted by a column (a click on its header), and how two of one node compare in it.
    virtual bool sortable(std::size_t column) const { return false; }
    virtual std::partial_ordering compare(TreeNode a, TreeNode b, std::size_t column) const {
        return column == 0 ? compare(a, b) : std::partial_ordering::equivalent;
    }

    void addObserver(TreeModelObserver& observer);
    void removeObserver(TreeModelObserver& observer);

    // A batch of changes (all that came since the last frame): each still told as it happens, the views put what they mean
    // in order once, at the end (sorted: each item inserted or changed placed once, where it sorts then). Nested: the
    // outermost counts. Nothing is to be asked of the views in between.
    void beginChanges();
    void endChanges();

protected:
    void notifyInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyReset();

private:
    std::vector<TreeModelObserver*> observers_;
    int batch_ = 0;
};

// ---- tables ----

// What a view of a table model is told, after the model changed.
class GLW_EXPORT TableModelObserver {
public:
    virtual ~TableModelObserver();
    virtual void rowsInserted(std::size_t first, std::size_t count) = 0;
    virtual void rowsRemoved(std::size_t first, std::size_t count) = 0;
    // The cells of the `count` rows at `first` are others, or show otherwise.
    virtual void rowsChanged(std::size_t first, std::size_t count) = 0;
    // The columns: how many, their headers.
    virtual void columnsChanged() = 0;
    virtual void modelReset() = 0;
    // A batch of changes begins / ends (beginChanges): told in between as they happen still.
    virtual void changesBegun() {}
    virtual void changesEnded() {}
};

// Rows of cells, in columns with headers (TableView).
class GLW_EXPORT TableModel {
public:
    virtual ~TableModel();
    TableModel() = default;
    TableModel(const TableModel&) = delete;
    TableModel& operator=(const TableModel&) = delete;

    virtual std::size_t rowCount() const = 0;
    virtual std::size_t columnCount() const = 0;
    virtual LocalizedText header(std::size_t column) const { return {}; }
    virtual ItemFlags flags(std::size_t row, std::size_t column) const { return ItemFlags::None; }
    // Whether the rows can be sorted by a column (a click on its header), and how two rows compare in it.
    virtual bool sortable(std::size_t column) const { return false; }
    virtual std::partial_ordering compare(std::size_t rowA, std::size_t rowB, std::size_t column) const {
        return std::partial_ordering::equivalent;
    }

    void addObserver(TableModelObserver& observer);
    void removeObserver(TableModelObserver& observer);

    // A batch of changes (all that came since the last frame): each still told as it happens, the views put what they mean
    // in order once, at the end (sorted: each item inserted or changed placed once, where it sorts then). Nested: the
    // outermost counts. Nothing is to be asked of the views in between.
    void beginChanges();
    void endChanges();

protected:
    void notifyRowsInserted(std::size_t first, std::size_t count);
    void notifyRowsRemoved(std::size_t first, std::size_t count);
    void notifyRowsChanged(std::size_t first, std::size_t count);
    void notifyColumnsChanged();
    void notifyReset();

private:
    std::vector<TableModelObserver*> observers_;
    int batch_ = 0;
};

} // namespace glw
