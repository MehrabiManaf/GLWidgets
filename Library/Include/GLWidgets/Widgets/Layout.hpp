// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How a widget places its children (Widget::setLayout). A layout measures and arranges the children of the widget it
// belongs to, in the widget's content rectangle; the children's margins, alignments and stretch are for it to honour.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>

namespace glw {

class Widget;

class GLW_EXPORT Layout {
public:
    virtual ~Layout();

    // The size the children of `widget` take together for `available` (its content, without its padding).
    virtual Size measure(Widget& widget, Size available) = 0;
    // Arranges the children of `widget` in `content` (the widget's coordinates).
    virtual void arrange(Widget& widget, const Rect& content) = 0;
};

enum class Orientation : std::uint8_t { Horizontal, Vertical };

// Children one after the other: top to bottom, or from the start side to the end side (left to right, or right to left).
// What is left of the length once every child has its desired size goes to the children with a stretch, by their shares.
class GLW_EXPORT StackLayout : public Layout {
public:
    explicit StackLayout(Orientation orientation, float spacing = 0) : orientation_(orientation), spacing_(spacing) {}

    Size measure(Widget& widget, Size available) override;
    void arrange(Widget& widget, const Rect& content) override;

    Orientation orientation() const { return orientation_; }
    float spacing() const { return spacing_; }

private:
    Orientation orientation_;
    float spacing_;
};

// Arranges `child` in `cell` (its parent's coordinates) as its margin and alignments say.
GLW_EXPORT void arrangeInCell(Widget& child, const Rect& cell, bool rightToLeft);

} // namespace glw
