// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/RowHeights.hpp"

#include <algorithm>
#include <cmath>

namespace glw {

void RowHeights::reset(std::size_t count, double height, bool uniform) {
    count_ = count;
    height_ = std::max(1.0, height);
    uniform_ = uniform;
    heights_.clear();
    measured_.clear();
    tree_.clear();
    if (!uniform_) {
        heights_.assign(count_, height_);
        measured_.assign(count_, 0);
        build();
    }
}

void RowHeights::build() {
    // O(n): each node adds itself to the one above it
    tree_.assign(count_ + 1, 0.0);
    for (std::size_t i = 1; i <= count_; ++i) {
        tree_[i] += heights_[i - 1];
        if (const std::size_t up = i + (i & (~i + 1)); up <= count_)
            tree_[up] += tree_[i];
    }
}

void RowHeights::insert(std::size_t at, std::size_t count) {
    at = std::min(at, count_);
    count_ += count;
    if (uniform_)
        return;
    heights_.insert(heights_.begin() + static_cast<std::ptrdiff_t>(at), count, height_);
    measured_.insert(measured_.begin() + static_cast<std::ptrdiff_t>(at), count, 0);
    build();
}

void RowHeights::remove(std::size_t at, std::size_t count) {
    at = std::min(at, count_);
    count = std::min(count, count_ - at);
    count_ -= count;
    if (uniform_)
        return;
    heights_.erase(heights_.begin() + static_cast<std::ptrdiff_t>(at), heights_.begin() + static_cast<std::ptrdiff_t>(at + count));
    measured_.erase(measured_.begin() + static_cast<std::ptrdiff_t>(at), measured_.begin() + static_cast<std::ptrdiff_t>(at + count));
    build();
}

bool RowHeights::set(std::size_t index, double height) {
    if (uniform_ || index >= count_)
        return false;
    measured_[index] = 1;
    const double delta = height - heights_[index];
    if (std::abs(delta) < 1e-6)
        return false;
    heights_[index] = height;
    for (std::size_t i = index + 1; i <= count_; i += i & (~i + 1))
        tree_[i] += delta;
    return true;
}

double RowHeights::height(std::size_t index) const {
    return uniform_ || index >= count_ ? height_ : heights_[index];
}

bool RowHeights::measured(std::size_t index) const {
    return uniform_ || (index < count_ && measured_[index]);
}

double RowHeights::top(std::size_t index) const {
    index = std::min(index, count_);
    if (uniform_)
        return static_cast<double>(index) * height_;
    double sum = 0;
    for (std::size_t i = index; i > 0; i -= i & (~i + 1))
        sum += tree_[i];
    return sum;
}

std::size_t RowHeights::indexAt(double y) const {
    if (count_ == 0 || y <= 0)
        return 0;
    if (uniform_)
        return std::min(count_ - 1, static_cast<std::size_t>(y / height_));
    // down the tree: the most rows whose heights add to no more than y
    std::size_t at = 0;
    std::size_t step = 1;
    while (step * 2 <= count_)
        step *= 2;
    for (; step > 0; step /= 2) {
        if (at + step <= count_ && tree_[at + step] <= y) {
            at += step;
            y -= tree_[at];
        }
    }
    return std::min(at, count_ - 1);
}

} // namespace glw
