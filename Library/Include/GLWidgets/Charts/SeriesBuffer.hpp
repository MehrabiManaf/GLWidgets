// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The points of a series as they come (a live feed): appended at the end, the oldest dropped past a capacity, each
// change in constant time. They are kept in chunks of 64, each knowing its lowest and highest x and y, so that what a
// chart asks of a million of them is quick: where an x is (by halves, when x never goes down), the lowest and highest y
// between two of them (the chunks between seen as a whole), what a column of pixels shows.
//
// A y that is not a number (NaN) is a gap: a line breaks there, and it is in no range.
#include <GLWidgets/Export.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace glw {

struct ChartPoint {
    double x = 0;
    double y = 0;
};

// The lowest and the highest of something.
struct ChartExtent {
    double low = 0;
    double high = 0;
};

class GLW_EXPORT SeriesBuffer {
public:
    static constexpr std::size_t chunkSize = 64;

    // `capacity`: the newest kept, the oldest dropped as more come; 0: all.
    explicit SeriesBuffer(std::size_t capacity = 0);
    ~SeriesBuffer();
    SeriesBuffer(SeriesBuffer&&) noexcept;
    SeriesBuffer& operator=(SeriesBuffer&&) noexcept;

    void setCapacity(std::size_t capacity);
    std::size_t capacity() const { return capacity_; }

    void append(double x, double y);
    void clear();

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    double x(std::size_t i) const;
    double y(std::size_t i) const;
    ChartPoint at(std::size_t i) const { return {x(i), y(i)}; }
    // Whether x never went down from one point to the next (what a time series is): lowerBound and what depends on it
    // are by halves then.
    bool sorted() const { return sorted_; }
    // The first point whose x is `x` or more (size() when there is none); sorted. From `from` on: a search that goes on
    // from where the last one ended (a column of pixels after another) looks at what is after it only.
    std::size_t lowerBound(double x, std::size_t from = 0) const;
    // The lowest and highest x of all points / y of the points from `first` to `last` (not included). None when there
    // are none (or only gaps).
    std::optional<ChartExtent> xExtent() const;
    std::optional<ChartExtent> yExtent(std::size_t first, std::size_t last) const;
    std::optional<ChartExtent> yExtent() const { return yExtent(0, size_); }
    // Changed each time it changes: what a chart has made of it is out of date when it is not what it was.
    std::uint64_t revision() const { return revision_; }

private:
    struct Chunk {
        std::array<double, chunkSize> x;
        std::array<double, chunkSize> y;
        std::size_t count = 0;          // filled
    };
    // A chunk's extents (of its points from front_ on, the first's): beside the others, not in the chunk, so that
    // the extents of many chunks are read from one array.
    struct Summary {
        double lowX = 0, highX = 0, lowY = 0, highY = 0;
        double lastX = 0;
        bool anyY = false;              // a y that is a number
    };
    // Where point `i` is: its chunk and the place in it.
    std::pair<std::size_t, std::size_t> locate(std::size_t i) const {
        const std::size_t p = i + front_;
        return {p / chunkSize, p % chunkSize};
    }
    // The first chunk's extents again, its points from front_ on (some were dropped).
    void summarizeFront() const;
    void dropOldest();

    // The chunks, the oldest first, and their summaries: vectors the oldest are dropped from the front of by a count
    // (those before head gone), closed up now and then. (A deque of pointers would do, but some keep two in each of
    // their blocks, and a search by halves would go to a chunk at each step.)
    struct Chunks {
        std::vector<std::unique_ptr<Chunk>> items;
        mutable std::vector<Summary> summaries;     // the first's made again when it is read
        std::size_t head = 0;

        bool empty() const { return items.size() == head; }
        std::size_t size() const { return items.size() - head; }
        const Chunk& operator[](std::size_t i) const { return *items[head + i]; }
        Summary& summary(std::size_t i) const { return summaries[head + i]; }
        void push_back(std::unique_ptr<Chunk> chunk) {
            items.push_back(std::move(chunk));
            summaries.push_back({});
        }
        std::unique_ptr<Chunk> pop_front() {
            std::unique_ptr<Chunk> gone = std::move(items[head]);
            ++head;
            if (head >= 64 && head * 2 >= items.size()) {
                items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(head));
                summaries.erase(summaries.begin(), summaries.begin() + static_cast<std::ptrdiff_t>(head));
                head = 0;
            }
            return gone;
        }
        void clear() {
            items.clear();
            summaries.clear();
            head = 0;
        }
    };
    Chunks chunks_;
    std::vector<std::unique_ptr<Chunk>> spare_;     // dropped, to be filled again
    std::size_t front_ = 0;                         // points dropped from the first chunk
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
    bool sorted_ = true;
    mutable bool frontStale_ = false;
    std::uint64_t revision_ = 0;
};

} // namespace glw
