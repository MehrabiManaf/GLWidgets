// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/ItemOrder.hpp"

#include <algorithm>
#include <numeric>

namespace glw {

// ---- blocks ----

void BlockList::place(Block& block, std::size_t from) {
    for (std::size_t k = from; k < block.items.size(); ++k) {
        where_[block.items[k]] = &block;
        offsets_[block.items[k]] = static_cast<std::uint32_t>(k);
    }
}

void BlockList::assign(const std::vector<std::uint32_t>& items, std::size_t handles) {
    clear();
    where_.assign(handles, nullptr);
    offsets_.assign(handles, 0);
    for (std::size_t at = 0; at < items.size(); at += blockSize) {
        auto block = std::make_unique<Block>();
        block->items.assign(items.begin() + static_cast<std::ptrdiff_t>(at),
                            items.begin() + static_cast<std::ptrdiff_t>(std::min(items.size(), at + blockSize)));
        block->index = blocks_.size();
        place(*block, 0);
        blocks_.push_back(std::move(block));
    }
    size_ = items.size();
    dirty_ = true;
}

void BlockList::clear() {
    blocks_.clear();
    where_.clear();
    offsets_.clear();
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

std::vector<std::uint32_t> BlockList::items() const {
    std::vector<std::uint32_t> all;
    all.reserve(size_);
    for (const auto& block : blocks_)
        all.insert(all.end(), block->items.begin(), block->items.end());
    return all;
}

std::uint32_t BlockList::at(std::size_t position) const {
    const auto [b, offset] = locate(position);
    return blocks_[b]->items[offset];
}

std::optional<std::size_t> BlockList::find(std::uint32_t handle) const {
    if (handle >= where_.size() || !where_[handle])
        return std::nullopt;
    computeStarts();
    return starts_[where_[handle]->index] + offsets_[handle];
}

void BlockList::insert(std::size_t position, std::uint32_t handle) {
    if (handle >= where_.size()) {
        where_.resize(std::max<std::size_t>(handle + 1, where_.size() * 2), nullptr);
        offsets_.resize(where_.size(), 0);
    }
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
    place(block, offset);
    ++size_;
    dirty_ = true;
    // too long: split in two
    if (block.items.size() > 2 * blockSize) {
        auto half = std::make_unique<Block>();
        half->items.assign(block.items.begin() + static_cast<std::ptrdiff_t>(blockSize), block.items.end());
        block.items.resize(blockSize);
        place(*half, 0);
        blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(b) + 1, std::move(half));
        renumber(b + 1);
    }
}

void BlockList::erase(std::size_t position) {
    const auto [b, offset] = locate(position);
    Block& block = *blocks_[b];
    where_[block.items[offset]] = nullptr;
    block.items.erase(block.items.begin() + static_cast<std::ptrdiff_t>(offset));
    place(block, offset);
    --size_;
    dirty_ = true;
    // empty: gone; short, and its next one too: one block
    if (block.items.empty()) {
        blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(b));
        renumber(b);
    } else if (b + 1 < blocks_.size() && block.items.size() + blocks_[b + 1]->items.size() < blockSize) {
        Block& next = *blocks_[b + 1];
        const std::size_t from = block.items.size();
        block.items.insert(block.items.end(), next.items.begin(), next.items.end());
        place(block, from);
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
    holding_ = false;
    waiting_.clear();
    marked_.clear();
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

std::optional<std::uint32_t> ItemOrder::handle(std::size_t index) const {
    if (index >= model_.size())
        return std::nullopt;
    return model_.at(index);
}

void ItemOrder::hold() {
    holding_ = true;
}

void ItemOrder::mark(std::uint32_t h) {
    if (h >= marked_.size())
        marked_.resize(std::max<std::size_t>(h + 1, marked_.size() * 2), 0);
    if (!marked_[h]) {
        marked_[h] = 1;
        waiting_.push_back(h);
    }
}

ItemOrder::Span ItemOrder::settle(const Accepts& accepts, const Before& before) {
    if (waiting_.size() > 32 + shown() / 2000)
        return merge(accepts, before);
    holding_ = false;
    Span span{shown(), 0, false};
    const std::size_t shownBefore = shown();
    // those still in the model, out of the order (where they were may not be where they sort), then each where it sorts
    std::vector<std::size_t> indices;
    indices.reserve(waiting_.size());
    for (const std::uint32_t h : waiting_) {
        // once each (a handle freed and taken again in the batch is in it twice), and only if still waiting
        if (!marked_[h])
            continue;
        marked_[h] = 0;
        const std::optional<std::size_t> index = model_.find(h);
        if (!index)
            continue;
        if (const std::optional<std::size_t> at = view_.find(h)) {
            view_.erase(*at);
            span.from = std::min(span.from, *at);
        }
        if (!accepts || accepts(*index))
            indices.push_back(*index);
    }
    waiting_.clear();
    for (const std::size_t index : indices) {
        const std::size_t at = placeOf(index, before);
        view_.insert(at, model_.at(index));
        span.from = std::min(span.from, at);
        span.to = std::max(span.to, at + 1);
    }
    span.counted = shown() != shownBefore;
    span.to = std::max({span.to, span.from + 1, span.counted ? std::max(shown(), shownBefore) : 0});
    return span;
}

ItemOrder::Span ItemOrder::merge(const Accepts& accepts, const Before& before) {
    holding_ = false;
    // each handle's index now, the model being as it is
    const std::vector<std::uint32_t> byModel = model_.items();
    const std::size_t count = byModel.size();
    std::vector<std::uint32_t> indexOf(std::max<std::size_t>(next_, marked_.size()), 0);
    for (std::size_t i = 0; i < count; ++i)
        indexOf[byModel[i]] = static_cast<std::uint32_t>(i);
    // those that waited (out of the order, accepted or not), by their indices; those accepted, sorted among themselves
    std::vector<char> waited(indexOf.size(), 0);
    std::vector<std::uint32_t> came;
    for (const std::uint32_t h : waiting_) {
        if (!marked_[h])
            continue;           // gone from the model, or there twice
        marked_[h] = 0;
        waited[h] = 1;
        const std::uint32_t index = indexOf[h];
        if (!accepts || accepts(index))
            came.push_back(index);
    }
    waiting_.clear();
    const auto goes = [&](std::uint32_t a, std::uint32_t b) { return before ? before(a, b) : a < b; };
    std::sort(came.begin(), came.end(), goes);
    // the others in their order still
    std::vector<std::uint32_t> stayed;
    stayed.reserve(view_.size());
    for (const std::uint32_t h : view_.items()) {
        if (!waited[h])
            stayed.push_back(indexOf[h]);
    }
    // merged: each that came where it goes, found from where the one before it went (a gallop, then a binary search)
    std::vector<std::uint32_t> shown;
    shown.reserve(stayed.size() + came.size());
    std::size_t from = 0;
    for (const std::uint32_t index : came) {
        std::size_t step = 1, lo = from, hi = from;
        while (hi < stayed.size() && goes(stayed[hi], index)) {
            lo = hi + 1;
            hi = from + step;
            step *= 2;
        }
        hi = std::min(hi, stayed.size());
        const std::size_t at = static_cast<std::size_t>(
            std::lower_bound(stayed.begin() + static_cast<std::ptrdiff_t>(lo), stayed.begin() + static_cast<std::ptrdiff_t>(hi), index, goes) -
            stayed.begin());
        shown.insert(shown.end(), stayed.begin() + static_cast<std::ptrdiff_t>(from), stayed.begin() + static_cast<std::ptrdiff_t>(at));
        shown.push_back(index);
        from = at;
    }
    shown.insert(shown.end(), stayed.begin() + static_cast<std::ptrdiff_t>(from), stayed.end());
    // the handles made again: handle i is the model's item i
    assign(count, std::move(shown));
    free_.clear();
    next_ = static_cast<std::uint32_t>(count);
    return {0, this->shown(), true};
}

ItemOrder::Span ItemOrder::inserted(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before) {
    std::vector<std::uint32_t> made(count);
    for (std::size_t k = 0; k < count; ++k) {
        made[k] = newHandle();
        model_.insert(first + k, made[k]);
    }
    if (holding_) {
        for (const std::uint32_t h : made)
            mark(h);
        return {};
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
        if (h < marked_.size() && marked_[h])
            marked_[h] = 0;         // waiting no more (settle skips it: it is not in the model)
        free_.push_back(h);
    }
    return span;
}

ItemOrder::Span ItemOrder::changed(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before) {
    Span span{shown(), 0, false};
    const std::size_t shownBefore = shown();
    const std::size_t end = std::min(first + count, model_.size());
    if (holding_) {
        for (std::size_t i = first; i < end; ++i)
            mark(model_.at(i));
        return {};
    }
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
