// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Layout.hpp>

#include <GLWidgets/Widgets/Widget.hpp>

#include <algorithm>
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

} // namespace glw
