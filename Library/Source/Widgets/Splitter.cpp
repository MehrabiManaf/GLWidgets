// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Splitter.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <numeric>
#include <utility>

namespace glw {

Splitter::Splitter() : Splitter(Props{}) {}

Splitter::Splitter(Props props) : orientation_(props.orientation), style_(std::move(props.style)) {}

void Splitter::setStyle(std::optional<SplitterStyle> style) {
    style_ = std::move(style);
    invalidateLayout();
}

const SplitterStyle& Splitter::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.splitter;
    static const SplitterStyle unstyled = Theme::dark().splitter;
    return unstyled;
}

std::vector<Widget*> Splitter::panes() const {
    std::vector<Widget*> result;
    for (const auto& child : children()) {
        if (child->visible())
            result.push_back(child.get());
    }
    return result;
}

float Splitter::minimumOf(const Widget& pane) const {
    const Insets m = pane.margin();
    return orientation_ == Orientation::Horizontal ? pane.minimumSize().width + m.horizontal() : pane.minimumSize().height + m.vertical();
}

float Splitter::along(Point p) const {
    if (orientation_ == Orientation::Vertical)
        return p.y - content_.y;
    return rightToLeft() ? content_.right() - p.x : p.x - content_.x;
}

Size Splitter::measureContent(Size available) {
    const bool horizontal = orientation_ == Orientation::Horizontal;
    const auto list = panes();
    float length = style().thickness * static_cast<float>(list.empty() ? 0 : list.size() - 1), across = 0;
    for (Widget* pane : list) {
        const Insets m = pane->margin();
        const Size s = pane->measure(horizontal ? Size{unbounded, std::max(0.0f, available.height - m.vertical())}
                                                : Size{std::max(0.0f, available.width - m.horizontal()), unbounded});
        length += horizontal ? s.width + m.horizontal() : s.height + m.vertical();
        across = std::max(across, horizontal ? s.height + m.vertical() : s.width + m.horizontal());
    }
    return horizontal ? Size{length, across} : Size{across, length};
}

void Splitter::arrangeContent(const Rect& content) {
    content_ = content;
    handles_.clear();
    starts_.clear();
    lengths_.clear();
    const auto list = panes();
    if (list.empty())
        return;
    const bool horizontal = orientation_ == Orientation::Horizontal;
    const bool rtl = rightToLeft();
    const float thickness = style().thickness;
    const float room = std::max(0.0f, (horizontal ? content.width : content.height) - thickness * static_cast<float>(list.size() - 1));

    // shares by stretch, then the panes below their minimum lengthened at the expense of the longest
    std::vector<float> shares;
    for (Widget* pane : list)
        shares.push_back(pane->stretch() > 0 ? pane->stretch() : 1.0f);
    const float total = std::accumulate(shares.begin(), shares.end(), 0.0f);
    for (float share : shares)
        lengths_.push_back(room * share / total);
    for (std::size_t i = 0; i < list.size(); ++i) {
        const float missing = minimumOf(*list[i]) - lengths_[i];
        if (missing <= 0)
            continue;
        const auto longest = std::max_element(lengths_.begin(), lengths_.end()) - lengths_.begin();
        if (static_cast<std::size_t>(longest) == i)
            continue;
        const float taken = std::min(missing, std::max(0.0f, lengths_[longest] - minimumOf(*list[longest])));
        lengths_[i] += taken;
        lengths_[longest] -= taken;
    }

    float position = 0;
    for (std::size_t i = 0; i < list.size(); ++i) {
        starts_.push_back(position);
        const float length = lengths_[i];
        const auto rect = [&](float start, float size) {
            if (!horizontal)
                return Rect{content.x, content.y + start, content.width, size};
            return Rect{rtl ? content.right() - start - size : content.x + start, content.y, size, content.height};
        };
        arrangeInCell(*list[i], rect(position, length), rtl);
        position += length;
        if (i + 1 < list.size()) {
            handles_.push_back(rect(position, thickness));
            position += thickness;
        }
    }
}

std::optional<platform::Cursor> Splitter::cursorAt(Point point) const {
    if (dragging_ >= 0 || handleAt(point) >= 0)
        return orientation_ == Orientation::Horizontal ? platform::Cursor::ResizeHorizontal : platform::Cursor::ResizeVertical;
    return Widget::cursorAt(point);
}

int Splitter::handleAt(Point p) const {
    for (std::size_t i = 0; i < handles_.size(); ++i) {
        if (handles_[i].contains(p))
            return static_cast<int>(i);
    }
    return -1;
}

void Splitter::paint(PaintContext& context) {
    const SplitterStyle& s = style();
    for (std::size_t i = 0; i < handles_.size(); ++i) {
        const bool active = static_cast<int>(i) == dragging_ || static_cast<int>(i) == hoveredHandle_;
        const Rect& h = handles_[i];
        // a line in the middle of the handle, thicker while it is under the mouse
        const float width = active ? 2.0f : 1.0f;
        const Rect line = orientation_ == Orientation::Horizontal ? Rect{h.center().x - width / 2, h.y, width, h.height}
                                                                  : Rect{h.x, h.center().y - width / 2, h.width, width};
        context.painter.fillRect(line, active ? s.active : s.handle);
    }
}

void Splitter::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Press: {
        const int handle = event.button == platform::MouseButton::Left ? handleAt(event.position) : -1;
        if (handle < 0)
            return;
        dragging_ = handle;
        grab_ = along(event.position) - (starts_[handle] + lengths_[handle]);
        event.handled = true;
        invalidatePaint();
        break;
    }
    case MouseEventType::Move: {
        if (dragging_ >= 0) {
            const auto list = panes();
            const auto i = static_cast<std::size_t>(dragging_);
            // the two panes around the handle share their room anew; the others keep theirs
            const float start = starts_[i];
            const float both = lengths_[i] + lengths_[i + 1];
            const float wanted = along(event.position) - grab_ - start;
            const float first = std::clamp(wanted, minimumOf(*list[i]), std::max(minimumOf(*list[i]), both - minimumOf(*list[i + 1])));
            lengths_[i] = first;
            lengths_[i + 1] = both - first;
            // the stretches become the lengths: the proportions stay when the splitter is resized
            for (std::size_t k = 0; k < list.size(); ++k)
                list[k]->setStretch(std::max(1.0f, lengths_[k]));
            event.handled = true;
        }
        if (const int handle = handleAt(event.position); handle != hoveredHandle_) {
            hoveredHandle_ = handle;
            invalidatePaint();
        }
        break;
    }
    case MouseEventType::Release:
        if (dragging_ >= 0) {
            dragging_ = -1;
            event.handled = true;
            invalidatePaint();
        }
        break;
    case MouseEventType::Wheel:
        break;
    }
}

void Splitter::onHoverChanged(bool hovered) {
    if (!hovered && hoveredHandle_ >= 0) {
        hoveredHandle_ = -1;
        invalidatePaint();
    }
}

} // namespace glw
