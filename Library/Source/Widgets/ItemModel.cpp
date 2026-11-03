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

} // namespace glw
