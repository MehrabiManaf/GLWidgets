// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ItemModel.hpp>

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>

namespace glw {

ListModelObserver::~ListModelObserver() = default;

ListModel::~ListModel() = default;

void ListModel::addObserver(ListModelObserver& observer) {
    if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end())
        observers_.push_back(&observer);
}

void ListModel::removeObserver(ListModelObserver& observer) {
    std::erase(observers_, &observer);
}

// a copy: an observer may come or go while it is told
void ListModel::notifyInserted(std::size_t first, std::size_t count) {
    for (ListModelObserver* o : std::vector(observers_))
        o->itemsInserted(first, count);
}

void ListModel::notifyRemoved(std::size_t first, std::size_t count) {
    for (ListModelObserver* o : std::vector(observers_))
        o->itemsRemoved(first, count);
}

void ListModel::notifyChanged(std::size_t first, std::size_t count) {
    for (ListModelObserver* o : std::vector(observers_))
        o->itemsChanged(first, count);
}

void ListModel::notifyReset() {
    for (ListModelObserver* o : std::vector(observers_))
        o->modelReset();
}

void StringListModel::setItems(std::vector<LocalizedText> items) {
    items_ = std::move(items);
    notifyReset();
}

void StringListModel::insert(std::size_t index, LocalizedText text) {
    index = std::min(index, items_.size());
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index), std::move(text));
    notifyInserted(index, 1);
}

void StringListModel::remove(std::size_t index, std::size_t count) {
    GLW_EXPECTS(index <= items_.size());
    count = std::min(count, items_.size() - index);
    if (count == 0)
        return;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index), items_.begin() + static_cast<std::ptrdiff_t>(index + count));
    notifyRemoved(index, count);
}

void StringListModel::set(std::size_t index, LocalizedText text) {
    items_[index] = std::move(text);
    notifyChanged(index, 1);
}

// ---- trees ----

TreeModelObserver::~TreeModelObserver() = default;

TreeModel::~TreeModel() = default;

void TreeModel::addObserver(TreeModelObserver& observer) {
    if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end())
        observers_.push_back(&observer);
}

void TreeModel::removeObserver(TreeModelObserver& observer) {
    std::erase(observers_, &observer);
}

void TreeModel::notifyInserted(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    for (TreeModelObserver* o : std::vector(observers_))
        o->childrenInserted(parent, first, count);
}

void TreeModel::notifyRemoved(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    for (TreeModelObserver* o : std::vector(observers_))
        o->childrenRemoved(parent, first, count);
}

void TreeModel::notifyChanged(std::optional<TreeNode> parent, std::size_t first, std::size_t count) {
    for (TreeModelObserver* o : std::vector(observers_))
        o->childrenChanged(parent, first, count);
}

void TreeModel::notifyReset() {
    for (TreeModelObserver* o : std::vector(observers_))
        o->modelReset();
}

// ---- tables ----

TableModelObserver::~TableModelObserver() = default;

TableModel::~TableModel() = default;

void TableModel::addObserver(TableModelObserver& observer) {
    if (std::find(observers_.begin(), observers_.end(), &observer) == observers_.end())
        observers_.push_back(&observer);
}

void TableModel::removeObserver(TableModelObserver& observer) {
    std::erase(observers_, &observer);
}

void TableModel::notifyRowsInserted(std::size_t first, std::size_t count) {
    for (TableModelObserver* o : std::vector(observers_))
        o->rowsInserted(first, count);
}

void TableModel::notifyRowsRemoved(std::size_t first, std::size_t count) {
    for (TableModelObserver* o : std::vector(observers_))
        o->rowsRemoved(first, count);
}

void TableModel::notifyRowsChanged(std::size_t first, std::size_t count) {
    for (TableModelObserver* o : std::vector(observers_))
        o->rowsChanged(first, count);
}

void TableModel::notifyColumnsChanged() {
    for (TableModelObserver* o : std::vector(observers_))
        o->columnsChanged();
}

void TableModel::notifyReset() {
    for (TableModelObserver* o : std::vector(observers_))
        o->modelReset();
}

} // namespace glw
