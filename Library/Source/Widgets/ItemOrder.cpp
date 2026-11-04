// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/ItemOrder.hpp"

#include <algorithm>
#include <numeric>

namespace glw {

// ---- blocks ----

void BlockList::assign(const std::vector<std::uint32_t>& items, std::size_t handles) {
    clear();
    where_.assign(handles, nullptr);
    for (std::size_t at = 0; at < items.size(); at += blockSize) {
        auto block = std::make_unique<Block>();
        block->items.assign(items.begin() + static_cast<std::ptrdiff_t>(at),
                            items.begin() + static_cast<std::ptrdiff_t>(std::min(items.size(), at + blockSize)));
        block->index = blocks_.size();
        for (const std::uint32_t h : block->items)
            where_[h] = block.get();
        blocks_.push_back(std::move(block));
    }
    size_ = items.size();
    dirty_ = true;
}

void BlockList::clear() {
    blocks_.clear();
    where_.clear();
    starts_.clear();
    size_ = 0;
    dirty_ = true;
}

void BlockList::computeStarts() const {
    if (!dirty_)
        return;
    starts_.resize(blocks_.size());
    std::size_t at = 0;
    for (std::size_t b = 0; b < blocks_.size(); ++b) {
        starts_[b] = at;
        at += blocks_[b]->items.size();
    }
    dirty_ = false;
}

std::pair<std::size_t, std::size_t> BlockList::locate(std::size_t position) const {
    computeStarts();
    const std::size_t b = static_cast<std::size_t>(std::upper_bound(starts_.begin(), starts_.end(), position) - starts_.begin()) - 1;
    return {b, position - starts_[b]};
}

void BlockList::renumber(std::size_t from) {
    for (std::size_t b = from; b < blocks_.size(); ++b)
        blocks_[b]->index = b;
}

std::uint32_t BlockList::at(std::size_t position) const {
    const auto [b, offset] = locate(position);
    return blocks_[b]->items[offset];
}

std::optional<std::size_t> BlockList::find(std::uint32_t handle) const {
    if (handle >= where_.size() || !where_[handle])
        return std::nullopt;
    computeStarts();
    const Block& block = *where_[handle];
    const auto it = std::find(block.items.begin(), block.items.end(), handle);
    return starts_[block.index] + static_cast<std::size_t>(it - block.items.begin());
}

void BlockList::insert(std::size_t position, std::uint32_t handle) {
    if (handle >= where_.size())
        where_.resize(std::max<std::size_t>(handle + 1, where_.size() * 2), nullptr);
    if (blocks_.empty()) {
        blocks_.push_back(std::make_unique<Block>());
        dirty_ = true;
    }
    // at the end: into the last block
    std::size_t b = blocks_.size() - 1, offset = blocks_.back()->items.size();
    if (position < size_)
        std::tie(b, offset) = locate(position);
    Block& block = *blocks_[b];
    block.items.insert(block.items.begin() + static_cast<std::ptrdiff_t>(offset), handle);
    where_[handle] = &block;
    ++size_;
    dirty_ = true;
    // too long: split in two
    if (block.items.size() > 2 * blockSize) {
        auto half = std::make_unique<Block>();
        half->items.assign(block.items.begin() + static_cast<std::ptrdiff_t>(blockSize), block.items.end());
        block.items.resize(blockSize);
        for (const std::uint32_t h : half->items)
            where_[h] = half.get();
        blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(b) + 1, std::move(half));
        renumber(b + 1);
    }
}

void BlockList::erase(std::size_t position) {
    const auto [b, offset] = locate(position);
    Block& block = *blocks_[b];
    where_[block.items[offset]] = nullptr;
    block.items.erase(block.items.begin() + static_cast<std::ptrdiff_t>(offset));
    --size_;
    dirty_ = true;
    // empty: gone; short, and its next one too: one block
    if (block.items.empty()) {
        blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(b));
        renumber(b);
    } else if (b + 1 < blocks_.size() && block.items.size() + blocks_[b + 1]->items.size() < blockSize) {
        Block& next = *blocks_[b + 1];
        for (const std::uint32_t h : next.items)
            where_[h] = &block;
        block.items.insert(block.items.end(), next.items.begin(), next.items.end());
        blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(b) + 1);
        renumber(b + 1);
    }
}

// ---- the order ----

void ItemOrder::assign(std::size_t count, std::vector<std::uint32_t> shown) {
    // handle i: the model's item i, now
    std::vector<std::uint32_t> handles(count);
    std::iota(handles.begin(), handles.end(), 0u);
    model_.assign(handles, count);
    view_.assign(shown, count);
    free_.clear();
    next_ = static_cast<std::uint32_t>(count);
}

void ItemOrder::clear() {
    model_.clear();
    view_.clear();
    free_.clear();
    next_ = 0;
}

std::size_t ItemOrder::modelIndex(std::size_t position) const {
    return indexOf(view_.at(position));
}

std::optional<std::size_t> ItemOrder::position(std::size_t index) const {
    if (index >= model_.size())
        return std::nullopt;
    return view_.find(model_.at(index));
}

std::uint32_t ItemOrder::newHandle() {
    if (!free_.empty()) {
        const std::uint32_t h = free_.back();
        free_.pop_back();
        return h;
    }
    return next_++;
}

std::size_t ItemOrder::placeOf(std::size_t index, const Before& before) const {
    // among those shown: after those that go before it (by the model's order without a comparison)
    return view_.lowerBound([&](std::uint32_t h) {
        const std::size_t other = indexOf(h);
        return before ? before(other, index) : other < index;
    });
}

ItemOrder::Span ItemOrder::inserted(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before) {
    std::vector<std::uint32_t> made(count);
    for (std::size_t k = 0; k < count; ++k) {
        made[k] = newHandle();
        model_.insert(first + k, made[k]);
    }
    Span span{shown(), shown(), false};
    for (std::size_t k = 0; k < count; ++k) {
        const std::size_t index = first + k;
        if (accepts && !accepts(index))
            continue;
        const std::size_t at = placeOf(index, before);
        view_.insert(at, made[k]);
        span.from = std::min(span.from, at);
        span.counted = true;
    }
    span.to = shown();
    return span;
}

ItemOrder::Span ItemOrder::removed(std::size_t first, std::size_t count) {
    Span span{shown(), shown(), false};
    for (std::size_t k = 0; k < count && first < model_.size(); ++k) {
        const std::uint32_t h = model_.at(first);
        if (const std::optional<std::size_t> at = view_.find(h)) {
            view_.erase(*at);
            span.from = std::min(span.from, *at);
            span.counted = true;
        }
        model_.erase(first);
        free_.push_back(h);
    }
    return span;
}

ItemOrder::Span ItemOrder::changed(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before) {
    Span span{shown(), 0, false};
    const std::size_t shownBefore = shown();
    const std::size_t end = std::min(first + count, model_.size());
    const auto touched = [&](std::size_t at) {
        span.from = std::min(span.from, at);
        span.to = std::max(span.to, at + 1);
    };
    if (count == 1 && first < end) {
        // one: where it is when it still sorts there (its neighbors say)
        const std::uint32_t h = model_.at(first);
        const std::optional<std::size_t> at = view_.find(h);
        const bool accepted = !accepts || accepts(first);
        if (at && accepted) {
            const auto goes = [&](std::size_t a, std::size_t b) { return before ? before(a, b) : a < b; };
            const bool afterPrevious = *at == 0 || !goes(first, indexOf(view_.at(*at - 1)));
            const bool beforeNext = *at + 1 == view_.size() || !goes(indexOf(view_.at(*at + 1)), first);
            if (afterPrevious && beforeNext) {
                touched(*at);
                return span;
            }
        }
    }
    // out of the order, then each where it sorts now, as the filter says
    std::vector<std::size_t> accepted;
    for (std::size_t i = first; i < end; ++i) {
        const std::uint32_t h = model_.at(i);
        if (const std::optional<std::size_t> at = view_.find(h)) {
            view_.erase(*at);
            touched(*at);
        }
        if (!accepts || accepts(i))
            accepted.push_back(i);
    }
    for (const std::size_t i : accepted) {
        const std::size_t at = placeOf(i, before);
        view_.insert(at, model_.at(i));
        touched(at);
    }
    // how many show: changed when some went in or out
    span.counted = shown() != shownBefore;
    if (span.counted)
        span.to = std::max(shown(), shownBefore);
    span.to = std::max(span.to, span.from + 1);
    return span;
}

} // namespace glw
