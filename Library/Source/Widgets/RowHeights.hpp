// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Where each row of a view is, for any number of rows: all of one height (no memory per row), or each its own (measured
// when it shows, estimated until then) in a Fenwick tree: where a row starts, and which row is at a place, in O(log n).
// In doubles: millions of rows of 28 dips are farther than a float is exact.
#include <cstddef>
#include <vector>

namespace glw {

class RowHeights {
public:
    // `count` rows, all `height` high (uniform), or estimated so until measured.
    void reset(std::size_t count, double height, bool uniform);
    std::size_t count() const { return count_; }
    bool uniform() const { return uniform_; }

    void insert(std::size_t at, std::size_t count);         // estimated
    void remove(std::size_t at, std::size_t count);
    // A row's height measured (not uniform). True when it changed.
    bool set(std::size_t index, double height);
    double height(std::size_t index) const;
    bool measured(std::size_t index) const;

    double top(std::size_t index) const;                    // where it starts: the heights of those before it
    double total() const { return top(count_); }
    // The row at `y` (the last when past them all, 0 when there are none).
    std::size_t indexAt(double y) const;

private:
    void build();                                           // the tree from heights_

    std::size_t count_ = 0;
    double height_ = 28;                                    // uniform, or the estimate
    bool uniform_ = true;
    std::vector<double> heights_;                           // not uniform: each row's
    std::vector<unsigned char> measured_;
    std::vector<double> tree_;                              // Fenwick: 1-based, tree_[i] sums a range ending at i
};

} // namespace glw
