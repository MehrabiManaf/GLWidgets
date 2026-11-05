// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/IndexSet.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace glw {

bool IndexSet::contains(std::size_t index) const {
    auto it = ranges_.upper_bound(index);
    if (it == ranges_.begin())
        return false;
    --it;
    return index < it->second;
}

void IndexSet::clear() {
    ranges_.clear();
    count_ = 0;
}

void IndexSet::insert(std::size_t first, std::size_t last) {
    if (first >= last)
        return;
    // merged with those it overlaps or touches
    auto it = ranges_.upper_bound(first);
    if (it != ranges_.begin()) {
        if (const auto before = std::prev(it); before->second >= first) {
            first = before->first;
            last = std::max(last, before->second);
            count_ -= before->second - before->first;
            it = ranges_.erase(before);
        }
    }
    while (it != ranges_.end() && it->first <= last) {
        last = std::max(last, it->second);
        count_ -= it->second - it->first;
        it = ranges_.erase(it);
    }
    ranges_.emplace(first, last);
    count_ += last - first;
}

void IndexSet::erase(std::size_t first, std::size_t last) {
    if (first >= last || ranges_.empty())
        return;
    auto it = ranges_.upper_bound(first);
    if (it != ranges_.begin() && std::prev(it)->second > first)
        --it;
    // what of them is outside [first, last): kept
    std::vector<std::pair<std::size_t, std::size_t>> kept;
    while (it != ranges_.end() && it->first < last) {
        if (it->first < first)
            kept.emplace_back(it->first, first);
        if (it->second > last)
            kept.emplace_back(last, it->second);
        count_ -= it->second - it->first;
        it = ranges_.erase(it);
    }
    for (const auto& [a, b] : kept) {
        ranges_.emplace(a, b);
        count_ += b - a;
    }
}

void IndexSet::inserted(std::size_t first, std::size_t count) {
    if (count == 0 || ranges_.empty())
        return;
    // those from `first` on moved on; a range across it split around the items inserted
    std::vector<std::pair<std::size_t, std::size_t>> moved;
    auto it = ranges_.upper_bound(first);
    if (it != ranges_.begin() && std::prev(it)->second > first)
        --it;
    while (it != ranges_.end()) {
        if (it->first < first) {
            moved.emplace_back(first + count, it->second + count);
            it->second = first;
            ++it;
        } else {
            moved.emplace_back(it->first + count, it->second + count);
            it = ranges_.erase(it);
        }
    }
    for (const auto& [a, b] : moved)
        ranges_.emplace(a, b);
}

void IndexSet::removed(std::size_t first, std::size_t count) {
    if (count == 0 || ranges_.empty())
        return;
    erase(first, first + count);
    // those after them moved back (one before them and one after may touch now: merged)
    std::vector<std::pair<std::size_t, std::size_t>> moved;
    for (auto it = ranges_.lower_bound(first + count); it != ranges_.end();) {
        moved.emplace_back(it->first - count, it->second - count);
        count_ -= it->second - it->first;
        it = ranges_.erase(it);
    }
    for (const auto& [a, b] : moved)
        insert(a, b);
}

} // namespace glw
