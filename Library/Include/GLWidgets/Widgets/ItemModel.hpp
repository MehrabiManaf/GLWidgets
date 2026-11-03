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
//       std::vector<std::string> names;
//   };
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace glw {

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
};

class GLW_EXPORT ListModel {
public:
    virtual ~ListModel();
    ListModel() = default;
    ListModel(const ListModel&) = delete;
    ListModel& operator=(const ListModel&) = delete;

    virtual std::size_t count() const = 0;

    void addObserver(ListModelObserver& observer);
    void removeObserver(ListModelObserver& observer);

protected:
    // What a model calls once it changed: its views follow.
    void notifyInserted(std::size_t first, std::size_t count);
    void notifyRemoved(std::size_t first, std::size_t count);
    void notifyChanged(std::size_t first, std::size_t count);
    void notifyReset();

private:
    std::vector<ListModelObserver*> observers_;
};

// Texts: a list of what a ListView shows with a TextDelegate.
class GLW_EXPORT StringListModel : public ListModel {
public:
    StringListModel() = default;
    explicit StringListModel(std::vector<LocalizedText> items) : items_(std::move(items)) {}

    std::size_t count() const override { return items_.size(); }
    const LocalizedText& text(std::size_t index) const { return items_[index]; }

    void setItems(std::vector<LocalizedText> items);
    void insert(std::size_t index, LocalizedText text);
    void append(LocalizedText text) { insert(items_.size(), std::move(text)); }
    void remove(std::size_t index, std::size_t count = 1);
    void set(std::size_t index, LocalizedText text);

private:
    std::vector<LocalizedText> items_;
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
    // Whether it has children, before they are counted (a folder not read yet: read when it is expanded). By default,
    // whether it has any.
    virtual bool hasChildren(TreeNode node) const { return childCount(node) > 0; }

    void addObserver(TreeModelObserver& observer);
    void removeObserver(TreeModelObserver& observer);

protected:
    void notifyInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count);
    void notifyReset();

private:
    std::vector<TreeModelObserver*> observers_;
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

    void addObserver(TableModelObserver& observer);
    void removeObserver(TableModelObserver& observer);

protected:
    void notifyRowsInserted(std::size_t first, std::size_t count);
    void notifyRowsRemoved(std::size_t first, std::size_t count);
    void notifyRowsChanged(std::size_t first, std::size_t count);
    void notifyColumnsChanged();
    void notifyReset();

private:
    std::vector<TableModelObserver*> observers_;
};

} // namespace glw
