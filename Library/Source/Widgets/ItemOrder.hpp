// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The order a view shows a model's items in, sorted and filtered, as the model changes all the time: an item inserted goes
// where it sorts, one changed moves only if it sorts elsewhere, one removed goes, without the rest of the order being
// touched. A million items, a thousand changes a second.
//
// Each item is a handle (a number of its own while it is in the model), in two sequences: the model's order (its handle's
// place there is its index) and the order shown (its place there is its position; the items filtered out are not in it).
// Each sequence is in blocks of a few hundred, with where each block starts: inserted, erased, found and read in about
// the square root of their length, and no index shifted for an item inserted before others.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

// Handles in an order, in blocks.
class BlockList {
public:
    // `items` in this order; `handles`: the most a handle is, and one.
    void assign(const std::vector<std::uint32_t>& items, std::size_t handles);
    void clear();
    std::size_t size() const { return size_; }
    std::uint32_t at(std::size_t position) const;
    std::optional<std::size_t> find(std::uint32_t handle) const;
    void insert(std::size_t position, std::uint32_t handle);
    void erase(std::size_t position);
    // All of them, in order.
    std::vector<std::uint32_t> items() const;
    // The first position whose handle `goesBefore` says no of (what it says is yes, then no, along the order).
    template <class F>
    std::size_t lowerBound(F&& goesBefore) const;

private:
    struct Block {
        std::vector<std::uint32_t> items;
        std::size_t index = 0;              // among the blocks
    };
    static constexpr std::size_t blockSize = 512;
    void computeStarts() const;
    std::pair<std::size_t, std::size_t> locate(std::size_t position) const;     // the block, and the place in it
    void renumber(std::size_t from);

    void place(Block& block, std::size_t from);    // where its handles are, from `from` on

    std::vector<std::unique_ptr<Block>> blocks_;
    std::vector<Block*> where_;             // by handle: the block it is in (none: not in it)
    std::vector<std::uint32_t> offsets_;    // ... and where in it: found at once
    mutable std::vector<std::size_t> starts_;
    mutable bool dirty_ = true;
    std::size_t size_ = 0;
};

template <class F>
std::size_t BlockList::lowerBound(F&& goesBefore) const {
    computeStarts();
    // the first block whose last handle does not go before; in it, the first handle that does not
    std::size_t lo = 0, hi = blocks_.size();
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (goesBefore(blocks_[mid]->items.back()))
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == blocks_.size())
        return size_;
    const std::vector<std::uint32_t>& items = blocks_[lo]->items;
    std::size_t a = 0, b = items.size();
    while (a < b) {
        const std::size_t mid = (a + b) / 2;
        if (goesBefore(items[mid]))
            a = mid + 1;
        else
            b = mid;
    }
    return starts_[lo] + a;
}

class ItemOrder {
public:
    using Accepts = std::function<bool(std::size_t)>;               // the model's index
    using Before = std::function<bool(std::size_t, std::size_t)>;   // ... of two: whether the first shows before

    // The model's `count` items, those accepted, sorted (`before` called as it is: a sort of a million items makes twenty
    // million comparisons).
    template <class Less>
    void build(std::size_t count, const Accepts& accepts, Less&& before);
    // ... in the model's order.
    void build(std::size_t count, const Accepts& accepts) { build(count, accepts, nullptr); }
    void clear();

    std::size_t shown() const { return view_.size(); }
    std::size_t modelIndex(std::size_t position) const;
    std::optional<std::size_t> position(std::size_t index) const;

    // What the model's changes did: the positions [from, to) changed; `counted`: so did how many show.
    struct Span {
        std::size_t from = 0;
        std::size_t to = 0;
        bool counted = false;
    };
    Span inserted(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before);
    Span removed(std::size_t first, std::size_t count);
    Span changed(std::size_t first, std::size_t count, const Accepts& accepts, const Before& before);

    // A batch of the model's changes (ListModel::beginChanges): from `hold` to `settle`, the items inserted and changed
    // wait out of the order, and are put in at the end, each once, where it sorts then; what was removed goes at once.
    // A position asked meanwhile may not be the item's. Many waiting (more than a two thousandth of what shows): sorted
    // among themselves, and merged into the order in one pass (the handles made again: those held before are not
    // theirs after it).
    void hold();
    bool holding() const { return holding_; }
    Span settle(const Accepts& accepts, const Before& before);

    // An item by its handle (which stays its as the model changes around it), and back.
    std::optional<std::uint32_t> handle(std::size_t index) const;
    std::optional<std::size_t> indexOfHandle(std::uint32_t handle) const { return model_.find(handle); }

private:
    void assign(std::size_t count, std::vector<std::uint32_t> shown);
    std::size_t indexOf(std::uint32_t handle) const { return *model_.find(handle); }
    std::size_t placeOf(std::size_t index, const Before& before) const;    // where the model's item goes, shown
    std::uint32_t newHandle();

    void mark(std::uint32_t handle);        // waiting, in a batch
    Span merge(const Accepts& accepts, const Before& before);       // settle, many waiting

    BlockList model_;
    BlockList view_;
    std::vector<std::uint32_t> free_;
    std::uint32_t next_ = 0;
    bool holding_ = false;
    std::vector<std::uint32_t> waiting_;    // in a batch: the items inserted and changed
    std::vector<char> marked_;              // ... by handle
};

template <class Less>
void ItemOrder::build(std::size_t count, const Accepts& accepts, Less&& before) {
    std::vector<std::uint32_t> shown;
    shown.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!accepts || accepts(i))
            shown.push_back(static_cast<std::uint32_t>(i));
    }
    if constexpr (!std::is_same_v<std::decay_t<Less>, std::nullptr_t>)
        std::stable_sort(shown.begin(), shown.end(), before);
    assign(count, std::move(shown));
}

} // namespace glw
