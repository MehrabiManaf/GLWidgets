// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How a widget places its children (Widget::setLayout). A layout measures and arranges the children of the widget it
// belongs to, in the widget's content rectangle; the children's margins, alignments and stretch are for it to honour.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

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

// A column of a GridLayout.
struct GridColumn {
    // 0: as wide as the widest of its children (that span it alone); above 0, it shares what is left with the other
    // stretching columns, by these shares.
    float stretch = 0;
    float minimum = 0;                      // dips, at least
};

// Children in rows and columns, in the order they are: across the columns, then on the next row; a child may span several
// columns (setSpan), and starts a new row when it does not fit in what is left of one. A row is as tall as its tallest
// child; what is left of the height goes to the rows with a child that stretches (Widget::setStretch), by the largest
// share in each. Columns go from the start side (the right in right to left). A form, its labels and their fields:
//
//   auto grid = std::make_unique<GridLayout>(std::vector<GridColumn>{{}, {.stretch = 1}});
//   GridLayout& form = *grid;
//   panel.setLayout(std::move(grid));
//   panel.add(label("Name")); panel.add(nameField);
//   form.setSpan(panel.add(wideNote), 2);
class GLW_EXPORT GridLayout : public Layout {
public:
    explicit GridLayout(std::vector<GridColumn> columns, float columnSpacing = 8, float rowSpacing = 6);

    Size measure(Widget& widget, Size available) override;
    void arrange(Widget& widget, const Rect& content) override;

    const std::vector<GridColumn>& columns() const { return columns_; }
    // `child` takes `columns` columns (1 by default; at most all of them). Its parent is laid out again by whoever calls
    // this after it was laid out (Widget::invalidateLayout).
    void setSpan(const Widget& child, int columns);
    int span(const Widget& child) const;
    float columnSpacing() const { return columnSpacing_; }
    float rowSpacing() const { return rowSpacing_; }

private:
    struct Cell {
        Widget* child;
        int row, column, span;
    };
    struct Grid {
        std::vector<Cell> cells;
        std::vector<float> widths;          // of the columns
        std::vector<float> heights;         // of the rows
        std::vector<float> rowStretch;      // of the rows: the largest of their children's
    };
    // Where the children go, the columns' widths for `width` (unbounded: as wide as they want), the rows' heights.
    Grid compute(Widget& widget, float width);

    std::vector<GridColumn> columns_;
    float columnSpacing_;
    float rowSpacing_;
    std::unordered_map<const Widget*, int> spans_;
};

// Arranges `child` in `cell` (its parent's coordinates) as its margin and alignments say.
GLW_EXPORT void arrangeInCell(Widget& child, const Rect& cell, bool rightToLeft);

} // namespace glw
