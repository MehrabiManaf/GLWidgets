// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Layout.hpp>

#include <GLWidgets/Widgets/Widget.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace glw {

Layout::~Layout() = default;

namespace {

// Where something `wanted` long goes in [start, start + length): all of it when stretched. `reversed`: Start and End are
// swapped (the end side of a right to left layout is the left).
std::pair<float, float> place(Alignment alignment, float start, float length, float wanted, bool reversed) {
    if (alignment == Alignment::Stretch)
        return {start, length};
    const float size = std::min(wanted, length);
    if (reversed && alignment != Alignment::Center)
        alignment = alignment == Alignment::Start ? Alignment::End : Alignment::Start;
    switch (alignment) {
    case Alignment::Start: return {start, size};
    case Alignment::Center: return {start + (length - size) / 2, size};
    default: return {start + length - size, size};
    }
}

std::vector<Widget*> visibleChildren(Widget& widget) {
    std::vector<Widget*> children;
    for (const auto& child : widget.children()) {
        if (child->visible())
            children.push_back(child.get());
    }
    return children;
}

} // namespace

void arrangeInCell(Widget& child, const Rect& cell, bool rightToLeft) {
    const Rect area = child.margin().deflate(cell, rightToLeft);
    // measured again for the room it really gets: a child measured with the length it wanted (a stack along it, a
    // splitter) may get less, and a wrapped text in it takes more lines then (the same room: the measure is kept)
    const Size wanted = child.measure(area.size());
    const auto [x, width] = place(child.horizontalAlignment(), area.x, area.width, wanted.width, rightToLeft);
    const auto [y, height] = place(child.verticalAlignment(), area.y, area.height, wanted.height, false);
    child.arrange({x, y, width, height});
}

Size StackLayout::measure(Widget& widget, Size available) {
    const bool vertical = orientation_ == Orientation::Vertical;
    float along = 0, across = 0;
    const auto children = visibleChildren(widget);
    for (Widget* child : children) {
        const Insets m = child->margin();
        // along the stack, as much as it wants; across it, what there is
        const Size space = vertical ? Size{std::max(0.0f, available.width - m.horizontal()), unbounded}
                                    : Size{unbounded, std::max(0.0f, available.height - m.vertical())};
        const Size s = child->measure(space);
        along += vertical ? s.height + m.vertical() : s.width + m.horizontal();
        across = std::max(across, vertical ? s.width + m.horizontal() : s.height + m.vertical());
    }
    if (children.size() > 1)
        along += spacing_ * static_cast<float>(children.size() - 1);
    return vertical ? Size{across, along} : Size{along, across};
}

void StackLayout::arrange(Widget& widget, const Rect& content) {
    const bool vertical = orientation_ == Orientation::Vertical;
    const bool rtl = widget.rightToLeft();
    const auto children = visibleChildren(widget);
    if (children.empty())
        return;

    const auto wanted = [&](const Widget* child) {
        const Insets m = child->margin();
        return vertical ? child->desiredSize().height + m.vertical() : child->desiredSize().width + m.horizontal();
    };
    const auto minimum = [&](const Widget* child) {
        const Insets m = child->margin();
        return vertical ? child->minimumSize().height + m.vertical() : child->minimumSize().width + m.horizontal();
    };
    const float room = vertical ? content.height : content.width;
    float total = spacing_ * static_cast<float>(children.size() - 1), shares = 0;
    std::vector<float> lengths;
    for (const Widget* child : children) {
        lengths.push_back(wanted(child));
        total += lengths.back();
        shares += std::max(0.0f, child->stretch());
    }
    if (total <= room) {
        // what is left goes to the children that stretch; without one, it stays at the end
        const float left = room - total;
        for (std::size_t i = 0; i < children.size(); ++i)
            lengths[i] += shares > 0 ? left * std::max(0.0f, children[i]->stretch()) / shares : 0.0f;
    } else {
        // too little room: the children that stretch give up length first, then the others, each in proportion to what it
        // has above its minimum size (a scroll area shrinks to the room, and scrolls)
        float excess = total - room;
        for (const bool stretching : {true, false}) {
            float capacity = 0;
            for (std::size_t i = 0; i < children.size(); ++i) {
                if ((children[i]->stretch() > 0) == stretching)
                    capacity += std::max(0.0f, lengths[i] - minimum(children[i]));
            }
            if (capacity <= 0)
                continue;
            const float taken = std::min(excess, capacity);
            for (std::size_t i = 0; i < children.size(); ++i) {
                if ((children[i]->stretch() > 0) == stretching)
                    lengths[i] -= taken * std::max(0.0f, lengths[i] - minimum(children[i])) / capacity;
            }
            excess -= taken;
        }
    }

    float position = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
        Widget* child = children[i];
        const float length = lengths[i];
        Rect cell;
        if (vertical)
            cell = {content.x, content.y + position, content.width, length};
        else    // from the start side: the right in right to left
            cell = {rtl ? content.right() - position - length : content.x + position, content.y, length, content.height};
        arrangeInCell(*child, cell, rtl);
        position += length + spacing_;
    }
}

// ---- GridLayout ----

GridLayout::GridLayout(std::vector<GridColumn> columns, float columnSpacing, float rowSpacing)
    : columns_(std::move(columns)), columnSpacing_(columnSpacing), rowSpacing_(rowSpacing) {
    if (columns_.empty())
        columns_.push_back({});
}

void GridLayout::setSpan(const Widget& child, int columns) {
    if (columns <= 1)
        spans_.erase(&child);
    else
        spans_[&child] = columns;
}

int GridLayout::span(const Widget& child) const {
    const auto it = spans_.find(&child);
    const int n = static_cast<int>(columns_.size());
    return it == spans_.end() ? 1 : std::clamp(it->second, 1, n);
}

GridLayout::Grid GridLayout::compute(Widget& widget, float width) {
    Grid grid;
    const int n = static_cast<int>(columns_.size());
    // the cells, in order
    int row = 0, column = 0;
    for (Widget* child : visibleChildren(widget)) {
        const int s = span(*child);
        if (column + s > n) {
            ++row;
            column = 0;
        }
        grid.cells.push_back({child, row, column, s});
        column += s;
        if (column >= n) {
            ++row;
            column = 0;
        }
    }
    const int rows = grid.cells.empty() ? 0 : grid.cells.back().row + 1;

    // the columns: those that do not stretch as wide as their widest child alone in them
    grid.widths.assign(columns_.size(), 0.0f);
    std::vector<float> wanted(columns_.size(), 0.0f);       // what each would take, unbounded
    for (const Cell& c : grid.cells) {
        if (c.span != 1)
            continue;
        const Insets m = c.child->margin();
        const float w = c.child->measure({unbounded, unbounded}).width + m.horizontal();
        auto& most = wanted[static_cast<std::size_t>(c.column)];
        most = std::max(most, w);
    }
    float fixed = columnSpacing_ * static_cast<float>(n - 1), shares = 0;
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        wanted[i] = std::max(wanted[i], columns_[i].minimum);
        if (columns_[i].stretch > 0) {
            shares += columns_[i].stretch;
            fixed += columns_[i].minimum;
        } else {
            grid.widths[i] = wanted[i];
            fixed += wanted[i];
        }
    }
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].stretch <= 0)
            continue;
        if (width >= unbounded || std::isinf(width))
            grid.widths[i] = wanted[i];
        else
            grid.widths[i] = columns_[i].minimum + std::max(0.0f, width - fixed) * columns_[i].stretch / shares;
    }

    // the rows: as tall as their tallest child, for the width it gets
    grid.heights.assign(static_cast<std::size_t>(rows), 0.0f);
    grid.rowStretch.assign(static_cast<std::size_t>(rows), 0.0f);
    for (const Cell& c : grid.cells) {
        float w = columnSpacing_ * static_cast<float>(c.span - 1);
        for (int i = c.column; i < c.column + c.span; ++i)
            w += grid.widths[static_cast<std::size_t>(i)];
        const Insets m = c.child->margin();
        const float h = c.child->measure({std::max(0.0f, w - m.horizontal()), unbounded}).height + m.vertical();
        const auto r = static_cast<std::size_t>(c.row);
        grid.heights[r] = std::max(grid.heights[r], h);
        grid.rowStretch[r] = std::max(grid.rowStretch[r], std::max(0.0f, c.child->stretch()));
    }
    return grid;
}

Size GridLayout::measure(Widget& widget, Size available) {
    const Grid grid = compute(widget, available.width);
    if (grid.cells.empty())
        return {};
    float width = columnSpacing_ * static_cast<float>(grid.widths.size() - 1);
    for (float w : grid.widths)
        width += w;
    float height = rowSpacing_ * static_cast<float>(grid.heights.size() - 1);
    for (float h : grid.heights)
        height += h;
    return {width, height};
}

void GridLayout::arrange(Widget& widget, const Rect& content) {
    Grid grid = compute(widget, content.width);
    if (grid.cells.empty())
        return;
    // what is left of the height, to the rows that stretch
    float total = rowSpacing_ * static_cast<float>(grid.heights.size() - 1), shares = 0;
    for (std::size_t r = 0; r < grid.heights.size(); ++r) {
        total += grid.heights[r];
        shares += grid.rowStretch[r];
    }
    if (shares > 0 && total < content.height) {
        for (std::size_t r = 0; r < grid.heights.size(); ++r)
            grid.heights[r] += (content.height - total) * grid.rowStretch[r] / shares;
    }
    std::vector<float> xs(grid.widths.size()), ys(grid.heights.size());
    for (std::size_t i = 1; i < xs.size(); ++i)
        xs[i] = xs[i - 1] + grid.widths[i - 1] + columnSpacing_;
    for (std::size_t r = 1; r < ys.size(); ++r)
        ys[r] = ys[r - 1] + grid.heights[r - 1] + rowSpacing_;
    const bool rtl = widget.rightToLeft();
    for (const Cell& c : grid.cells) {
        const auto first = static_cast<std::size_t>(c.column);
        const auto last = static_cast<std::size_t>(c.column + c.span - 1);
        const float width = xs[last] + grid.widths[last] - xs[first];
        const float x = rtl ? content.right() - xs[first] - width : content.x + xs[first];
        const auto r = static_cast<std::size_t>(c.row);
        arrangeInCell(*c.child, {x, content.y + ys[r], width, grid.heights[r]}, rtl);
    }
}

} // namespace glw
