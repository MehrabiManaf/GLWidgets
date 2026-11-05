// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Panes side by side (or one above the other), with a handle between each two that the mouse drags to share the room
// between them differently. The panes are its children; each takes a share of the room by its stretch (1 when it has
// none), and dragging a handle sets the stretches to the lengths the panes have then, so a resized splitter keeps their
// proportions. A pane is never made shorter than its minimum size. A double click on a handle shares the room of the two
// panes beside it equally, eased (in a pane that is, or only holds, a splitter the same way, its pane by the handle:
// docks side by side in nested splits). Side by side panes start on the start side (the right in right to left).
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace glw {

class GLW_EXPORT Splitter : public Widget {
public:
    // What a splitter is made with (ui::Splitter): designated initializers, in this order.
    struct Props {
        Orientation orientation = Orientation::Horizontal;  // horizontal: side by side
        std::optional<SplitterStyle> style;                 // none: the theme's
    };

    Splitter();
    explicit Splitter(Props props);

    Orientation orientation() const { return orientation_; }
    void setStyle(std::optional<SplitterStyle> style);
    const SplitterStyle& style() const;
    // The handles, as arranged: between the panes, in their order.
    const std::vector<Rect>& handles() const { return handles_; }
    // The two panes beside a handle given equal room (as far as their minimums let them), eased there; of a pane that is
    // a splitter of the same orientation, its pane by the handle (and so on), the others keeping theirs.
    void shareEqually(std::size_t handle);

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    std::vector<Widget*> panes() const;
    int handleAt(Point point) const;
    // Along the panes, from the start: x from the start side, or y.
    float along(Point point) const;
    float minimumOf(const Widget& pane) const;
    void shareStepped();
    // The pane at `index` and, while it is (or only holds) a splitter the same way, its pane by the handle (its last
    // before the handle, its first after it), and so on: each splitter with the index of that pane in it.
    std::vector<std::pair<Splitter*, std::size_t>> sideOf(std::size_t index, bool beforeHandle);

    Orientation orientation_ = Orientation::Horizontal;
    std::optional<SplitterStyle> style_;
    Rect content_;
    std::vector<Rect> handles_;
    std::vector<float> starts_;                         // of the panes, along, as arranged
    std::vector<float> lengths_;
    int dragging_ = -1;
    int hoveredHandle_ = -1;
    float grab_ = 0;                                    // from the handle's start to the pointer, along
    // the panes beside a handle sharing their room equally: the first one's share of it, from (to a half)
    struct Sharing {
        Widget* first = nullptr;            // the panes of this splitter beside it
        Widget* second = nullptr;
        float from = 0;
    };
    std::optional<Sharing> sharing_;
    Animation shared_{[this] { shareStepped(); }};      // from 0 to 1
};

} // namespace glw
