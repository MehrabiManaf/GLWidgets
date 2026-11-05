// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A set of indices kept as ranges: what a view has selected (a million items selected are one range), and kept the
// model's as items are inserted and removed before, among and after them.
#include <cstddef>
#include <map>

namespace glw {

class IndexSet {
public:
    bool contains(std::size_t index) const;
    bool empty() const { return ranges_.empty(); }
    std::size_t count() const { return count_; }
    void clear();
    // [first, last) in it / out of it.
    void insert(std::size_t first, std::size_t last);
    void erase(std::size_t first, std::size_t last);

    // The model's changes: `count` inserted at `first` (those at or after it move on, none of them in it), or those of
    // [first, first + count) removed (those after them move back).
    void inserted(std::size_t first, std::size_t count);
    void removed(std::size_t first, std::size_t count);

    // Its ranges, [first, last), in order.
    const std::map<std::size_t, std::size_t>& ranges() const { return ranges_; }

private:
    std::map<std::size_t, std::size_t> ranges_;     // first -> last, apart (neither overlapping nor touching)
    std::size_t count_ = 0;
};

} // namespace glw
