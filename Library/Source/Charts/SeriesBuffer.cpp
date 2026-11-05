// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/SeriesBuffer.hpp>

#include <GLWidgets/Core/Assert.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr std::size_t spareChunks = 4;      // dropped chunks kept to be filled again

} // namespace

SeriesBuffer::SeriesBuffer(std::size_t capacity) : capacity_(capacity) {}
SeriesBuffer::~SeriesBuffer() = default;
SeriesBuffer::SeriesBuffer(SeriesBuffer&&) noexcept = default;
SeriesBuffer& SeriesBuffer::operator=(SeriesBuffer&&) noexcept = default;

void SeriesBuffer::setCapacity(std::size_t capacity) {
    capacity_ = capacity;
    while (capacity_ > 0 && size_ > capacity_)
        dropOldest();
    ++revision_;
}

void SeriesBuffer::append(double x, double y) {
    if (size_ > 0 && x < chunks_.summary(chunks_.size() - 1).lastX)
        sorted_ = false;
    if (chunks_.empty() || chunks_[chunks_.size() - 1].count == chunkSize) {
        std::unique_ptr<Chunk> chunk;
        if (!spare_.empty()) {
            chunk = std::move(spare_.back());
            spare_.pop_back();
        } else {
            chunk = std::make_unique<Chunk>();
        }
        chunk->count = 0;
        chunks_.push_back(std::move(chunk));
    }
    Chunk& c = *chunks_.items.back();
    Summary& s = chunks_.summaries.back();
    c.x[c.count] = x;
    c.y[c.count] = y;
    if (c.count == 0) {
        s.lowX = s.highX = x;
        s.anyY = false;
    } else {
        s.lowX = std::min(s.lowX, x);
        s.highX = std::max(s.highX, x);
    }
    s.lastX = x;
    if (!std::isnan(y)) {
        if (!s.anyY) {
            s.lowY = s.highY = y;
            s.anyY = true;
        } else {
            s.lowY = std::min(s.lowY, y);
            s.highY = std::max(s.highY, y);
        }
    }
    ++c.count;
    ++size_;
    if (capacity_ > 0 && size_ > capacity_)
        dropOldest();
    ++revision_;
}

void SeriesBuffer::dropOldest() {
    ++front_;
    --size_;
    if (front_ == chunks_[0].count) {
        // the first chunk all dropped: kept to be filled again
        std::unique_ptr<Chunk> gone = chunks_.pop_front();
        if (spare_.size() < spareChunks)
            spare_.push_back(std::move(gone));
        front_ = 0;
        frontStale_ = false;
        if (chunks_.empty())
            sorted_ = true;
    } else {
        frontStale_ = true;
    }
}

void SeriesBuffer::clear() {
    for (std::size_t i = chunks_.head; i < chunks_.items.size() && spare_.size() < spareChunks; ++i)
        spare_.push_back(std::move(chunks_.items[i]));
    chunks_.clear();
    front_ = size_ = 0;
    sorted_ = true;
    frontStale_ = false;
    ++revision_;
}

double SeriesBuffer::x(std::size_t i) const {
    GLW_EXPECTS(i < size_);
    const auto [c, k] = locate(i);
    return chunks_[c].x[k];
}

double SeriesBuffer::y(std::size_t i) const {
    GLW_EXPECTS(i < size_);
    const auto [c, k] = locate(i);
    return chunks_[c].y[k];
}

std::size_t SeriesBuffer::lowerBound(double x, std::size_t from) const {
    if (from >= size_)
        return size_;
    const auto [c0, k0] = locate(from);
    if (chunks_[c0].x[k0] >= x)
        return from;
    // the first chunk from there whose last x is `x` or more (by the summaries, side by side), then in it: by steps
    // that double from there (what is looked for is most often a chunk or two on: a column of pixels after another),
    // then by halves between the last two steps
    std::size_t low = c0, high = chunks_.size();
    for (std::size_t step = 1; low + step < high; step *= 2) {
        if (chunks_.summary(low + step).lastX >= x) {
            high = low + step + 1;
            break;
        }
        low += step;
    }
    while (low < high) {
        const std::size_t middle = (low + high) / 2;
        if (chunks_.summary(middle).lastX < x)
            low = middle + 1;
        else
            high = middle;
    }
    if (low == chunks_.size())
        return size_;
    const Chunk& c = chunks_[low];
    const std::size_t begin = low == c0 ? k0 : low == 0 ? front_ : 0;
    const auto at = static_cast<std::size_t>(std::lower_bound(c.x.begin() + static_cast<std::ptrdiff_t>(begin),
                                                              c.x.begin() + static_cast<std::ptrdiff_t>(c.count), x) -
                                             c.x.begin());
    return low * chunkSize + at - front_;
}

void SeriesBuffer::summarizeFront() const {
    if (!frontStale_ || chunks_.empty())
        return;
    const Chunk& c = chunks_[0];
    Summary& s = chunks_.summary(0);
    s.anyY = false;
    s.lowX = s.highX = c.x[front_];
    for (std::size_t k = front_; k < c.count; ++k) {
        s.lowX = std::min(s.lowX, c.x[k]);
        s.highX = std::max(s.highX, c.x[k]);
        const double y = c.y[k];
        if (std::isnan(y))
            continue;
        if (!s.anyY) {
            s.lowY = s.highY = y;
            s.anyY = true;
        } else {
            s.lowY = std::min(s.lowY, y);
            s.highY = std::max(s.highY, y);
        }
    }
    frontStale_ = false;
}

std::optional<ChartExtent> SeriesBuffer::xExtent() const {
    if (size_ == 0)
        return std::nullopt;
    if (sorted_)
        return ChartExtent{x(0), x(size_ - 1)};
    summarizeFront();
    ChartExtent e{chunks_.summary(0).lowX, chunks_.summary(0).highX};
    for (std::size_t c = 1; c < chunks_.size(); ++c) {
        e.low = std::min(e.low, chunks_.summary(c).lowX);
        e.high = std::max(e.high, chunks_.summary(c).highX);
    }
    return e;
}

std::optional<ChartExtent> SeriesBuffer::yExtent(std::size_t first, std::size_t last) const {
    last = std::min(last, size_);
    if (first >= last)
        return std::nullopt;
    summarizeFront();
    bool any = false;
    double low = 0, high = 0;
    const auto take = [&](double l, double h) {
        if (!any) {
            low = l;
            high = h;
            any = true;
        } else {
            low = std::min(low, l);
            high = std::max(high, h);
        }
    };
    const auto whole = [&](std::size_t c) {
        if (const Summary& s = chunks_.summary(c); s.anyY)
            take(s.lowY, s.highY);
    };
    const auto scan = [&](std::size_t c, std::size_t from, std::size_t to) {
        const Chunk& chunk = chunks_[c];
        for (std::size_t k = from; k < to; ++k)
            if (const double y = chunk.y[k]; !std::isnan(y))
                take(y, y);
    };
    const auto [c0, k0] = locate(first);
    const auto [c1, k1] = locate(last - 1);
    const std::size_t start0 = c0 == 0 ? front_ : 0;
    if (c0 == c1) {
        if (k0 == start0 && k1 + 1 == chunks_[c0].count)
            whole(c0);
        else
            scan(c0, k0, k1 + 1);
    } else {
        // the first and last chunks in part, those between as a whole
        if (k0 == start0)
            whole(c0);
        else
            scan(c0, k0, chunks_[c0].count);
        for (std::size_t c = c0 + 1; c < c1; ++c)
            whole(c);
        if (k1 + 1 == chunks_[c1].count)
            whole(c1);
        else
            scan(c1, 0, k1 + 1);
    }
    if (!any)
        return std::nullopt;
    return ChartExtent{low, high};
}

} // namespace glw
