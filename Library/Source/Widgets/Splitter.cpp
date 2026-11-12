// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Splitter.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

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
        if (s.grooveWidth > 0 && s.groove.relief > 0) {
            // a groove along it; under the mouse or dragged, a bar raised out of it
            const float w = std::min(s.grooveWidth, orientation_ == Orientation::Horizontal ? h.width : h.height);
            const Rect bar = orientation_ == Orientation::Horizontal ? Rect{h.center().x - w / 2, h.y + 2, w, h.height - 4}
                                                                     : Rect{h.x + 2, h.center().y - w / 2, h.width - 4, w};
            if (active)
                paintRaised(context.painter, bar, w / 2, s.active, {0, 0, 0, 0}, s.groove);
            else
                paintSunken(context.painter, bar, w / 2, s.handle, {0, 0, 0, 0}, s.groove);
            continue;
        }
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
        event.handled = true;
        if (event.clicks == 2) {
            shareEqually(static_cast<std::size_t>(handle));
            return;
        }
        // a drag takes over from the easing
        shared_.set(0);
        sharing_.reset();
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

namespace {

// What a widget holds, when it only holds that (its one child that shows, about as long along `orientation`: a dock
// area around its split), and so on; a splitter, as it is.
Widget* heldBy(Widget* widget, Orientation orientation) {
    const auto along = [orientation](const Widget& w) { return orientation == Orientation::Horizontal ? w.size().width : w.size().height; };
    while (!dynamic_cast<Splitter*>(widget)) {
        Widget* only = nullptr;
        for (const auto& child : widget->children()) {
            if (!child->visible())
                continue;
            if (only)
                return widget;
            only = child.get();
        }
        if (!only || along(*only) < along(*widget) - 32)
            return widget;
        widget = only;
    }
    return widget;
}

} // namespace

std::vector<std::pair<Splitter*, std::size_t>> Splitter::sideOf(std::size_t index, bool beforeHandle) {
    std::vector<std::pair<Splitter*, std::size_t>> side{{this, index}};
    Widget* pane = heldBy(panes()[index], orientation_);
    while (auto* inner = dynamic_cast<Splitter*>(pane)) {
        const auto list = inner->panes();
        if (inner->orientation_ != orientation_ || list.empty() || inner->lengths_.size() != list.size())
            break;
        const std::size_t k = beforeHandle ? list.size() - 1 : 0;
        side.emplace_back(inner, k);
        pane = heldBy(list[k], orientation_);
    }
    return side;
}

void Splitter::shareEqually(std::size_t handle) {
    const auto list = panes();
    if (handle + 1 >= list.size() || handle + 1 >= lengths_.size())
        return;
    const auto [a, i] = sideOf(handle, true).back();
    const auto [b, k] = sideOf(handle + 1, false).back();
    const float both = a->lengths_[i] + b->lengths_[k];
    if (both <= 0)
        return;
    sharing_ = Sharing{list[handle], list[handle + 1], a->lengths_[i] / both};
    shared_.set(0);
    shared_.animateTo(scheduler(), 1.0f, context() ? Duration(context()->theme.motion.normal) : Duration(std::chrono::milliseconds(200)),
                      Easing::InOutCubic);
    shareStepped();
}

void Splitter::shareStepped() {
    if (!sharing_)
        return;
    // the two panes still side by side (none removed between them meanwhile)
    const auto list = panes();
    const auto at = std::find(list.begin(), list.end(), sharing_->first);
    if (at == list.end() || at + 1 == list.end() || at[1] != sharing_->second || lengths_.size() != list.size()) {
        sharing_.reset();
        shared_.set(0);
        return;
    }
    const auto i = static_cast<std::size_t>(at - list.begin());
    // the two panes by the handle, in nested splitters maybe: what one gains the other loses, and so do the panes of
    // the splitters they are in, the others keeping their lengths
    const auto before = sideOf(i, true);
    const auto after = sideOf(i + 1, false);
    const auto [a, ai] = before.back();
    const auto [b, bi] = after.back();
    const float both = a->lengths_[ai] + b->lengths_[bi];
    const float share = sharing_->from + (0.5f - sharing_->from) * shared_.value();
    const float low = a->minimumOf(*a->panes()[ai]);
    const float wanted = std::clamp(both * share, low, std::max(low, both - b->minimumOf(*b->panes()[bi])));
    const float delta = wanted - a->lengths_[ai];
    for (const auto& [splitter, index] : before)
        splitter->lengths_[index] += delta;
    for (const auto& [splitter, index] : after)
        splitter->lengths_[index] -= delta;
    // the stretches become the lengths: the proportions stay when the splitters are resized
    const auto settle = [](Splitter& splitter) {
        const auto panes = splitter.panes();
        for (std::size_t p = 0; p < panes.size(); ++p)
            panes[p]->setStretch(std::max(1.0f, splitter.lengths_[p]));
    };
    settle(*this);
    for (std::size_t n = 1; n < before.size(); ++n)
        settle(*before[n].first);
    for (std::size_t n = 1; n < after.size(); ++n)
        settle(*after[n].first);
    if (!shared_.running())
        sharing_.reset();
}

void Splitter::onHoverChanged(bool hovered) {
    if (!hovered && hoveredHandle_ >= 0) {
        hoveredHandle_ = -1;
        invalidatePaint();
    }
}

} // namespace glw
