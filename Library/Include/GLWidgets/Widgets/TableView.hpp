// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A table of a model's rows (ItemModel.hpp), in columns under a header that stays as the rows scroll. Its rows are those of
// a ListView, and only the cells that show are widgets, in both directions: a table of millions of rows and hundreds of
// columns costs what shows. Cells are made by a delegate, by kinds (cells of one kind show any column of it), and used
// again for others as the table scrolls.
//
// Columns are as wide as set (the theme's until then), the last one taking what room the others leave; dragging the edge
// of a header resizes its column. The table scrolls across with its bar, the wheel turned with Shift (or tilted), or Left
// and Right. Rows are chosen and activated as a list's are (ListView).
//
//   class Sales : public glw::TableModel { ... rowCount, columnCount, header ... };
//   auto table = std::make_unique<glw::TableView>();
//   table->setModel(sales);
//   table->setDelegate(std::make_unique<glw::TableTextDelegate>([&](std::size_t row, std::size_t column) { ... }));
//   table->setColumnWidth(0, 220);
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace glw {

// What makes the widgets of a table's cells, and shows a cell in one.
class GLW_EXPORT TableDelegate {
public:
    virtual ~TableDelegate();
    // The kind of widget a column's cells are: a cell of one kind shows any column of that kind. All one kind by default.
    virtual std::size_t cellKind(std::size_t column) const { return 0; }
    virtual std::unique_ptr<Widget> makeCell(std::size_t kind) = 0;
    // `cell` (made for the column's kind) shows the cell at `row`, `column` now.
    virtual void bindCell(Widget& cell, std::size_t row, std::size_t column) = 0;
};

// Each cell's text.
class GLW_EXPORT TableTextDelegate : public TableDelegate {
public:
    explicit TableTextDelegate(std::function<LocalizedText(std::size_t row, std::size_t column)> text) : text_(std::move(text)) {}
    std::unique_ptr<Widget> makeCell(std::size_t kind) override;
    void bindCell(Widget& cell, std::size_t row, std::size_t column) override;

private:
    std::function<LocalizedText(std::size_t, std::size_t)> text_;
};

class GLW_EXPORT TableView : public Widget, private TableModelObserver {
public:
    static constexpr float minimumColumnWidth = 24;

    TableView();
    ~TableView() override;

    void setModel(std::shared_ptr<TableModel> model);
    TableModel* model() const { return model_.get(); }
    void setDelegate(std::unique_ptr<TableDelegate> delegate);
    // Every row this high (none: the theme's); 0: each as high as its tallest cell that shows.
    void setRowHeight(std::optional<float> height);
    void setSelectionMode(SelectionMode mode);

    // ---- columns ----
    // As set (the theme's until then); the last one shows wider when there is room left.
    float columnWidth(std::size_t column) const;
    void setColumnWidth(std::size_t column, float width);
    // The last column takes the room the others leave (true by default).
    void setStretchLastColumn(bool stretch);
    void setHeaderVisible(bool visible);
    bool headerVisible() const { return headerVisible_; }
    // The width of all columns, the last one as it shows.
    double totalWidth() const;
    // The columns that show now: [first, last).
    std::pair<std::size_t, std::size_t> shownColumns() const;

    // ---- across ----
    // Dips scrolled from the start side (the left, or the right in right to left), and how far it goes.
    double horizontalOffset() const { return horizontalOffset_; }
    void setHorizontalOffset(double offset);
    double maximumHorizontalOffset() const;
    // Scrolled across so that the column shows (as little as it takes).
    void scrollToColumn(std::size_t column);

    // ---- rows ----
    std::optional<std::size_t> current() const { return list_->current(); }
    void setCurrent(std::optional<std::size_t> row) { list_->setCurrent(row); }
    bool isSelected(std::size_t row) const { return list_->isSelected(row); }
    // The widget showing a cell, while it shows.
    Widget* cellWidget(std::size_t row, std::size_t column) const;
    // A header's widget, while it shows.
    Widget* headerWidget(std::size_t column) const;
    // The list its rows are: scrolled down, its rows' widgets.
    ListView& list() const { return *list_; }

    void setOnCurrentChanged(std::function<void(std::optional<std::size_t>)> handler) { list_->setOnCurrentChanged(std::move(handler)); }
    // A double click or Enter on a row.
    void setOnActivated(std::function<void(std::size_t)> handler) { list_->setOnActivated(std::move(handler)); }
    // A header clicked (to sort by its column, say).
    void setOnHeaderClicked(std::function<void(std::size_t)> handler) { onHeaderClicked_ = std::move(handler); }

    const ItemViewStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onPreviewMouse(MouseEvent& event) override;
    void onMouse(MouseEvent& event) override;
    void onPreviewKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    class Rows;                             // the model's rows, as the list's model
    class RowDelegate;                      // a row: its cells that show
    class Strip;                            // the cells of the columns that show, of a row or the header

    // the model's changes
    void rowsInserted(std::size_t first, std::size_t count) override;
    void rowsRemoved(std::size_t first, std::size_t count) override;
    void rowsChanged(std::size_t first, std::size_t count) override;
    void columnsChanged() override;
    void modelReset() override;

    void resetColumns();                    // the widths for the model's columns: those set kept
    void updateLefts();
    // Where the columns show: the width of the list's rows.
    float viewWidth() const;
    double columnLeft(std::size_t column) const { return lefts_[column]; }
    // As it shows: the last stretched.
    float shownWidth(std::size_t column) const;
    // Where a column is in a strip `width` wide (the start side mirrored in right to left).
    float columnX(std::size_t column, float width) const;
    // The rows and the header, their cells for the columns that show (after scrolling across, or resizing a column).
    void rearrangeStrips();
    void rebindStrips();                    // all of them bound again (the columns changed)
    std::vector<Strip*> strips() const;
    Rect headerRect() const;                // its coordinates
    // The edge of the column at `point` (in the header), that resizes it.
    std::optional<std::size_t> edgeAt(Point point) const;
    std::optional<std::size_t> headerAt(Point point) const;
    Rect barTrack() const;
    Rect barThumb() const;

    std::shared_ptr<TableModel> model_;
    std::unique_ptr<TableDelegate> delegate_;
    std::shared_ptr<Rows> rows_;
    ListView* list_ = nullptr;
    Strip* header_ = nullptr;
    std::optional<float> rowHeight_;

    std::vector<float> widths_;             // as set (0: the theme's)
    std::vector<double> lefts_;             // where each starts, one more: where the last ends (not stretched)
    bool stretchLast_ = true;
    bool headerVisible_ = true;
    double horizontalOffset_ = 0;

    Rect area_;                             // its content rectangle
    bool showBar_ = false;                  // across
    std::optional<std::size_t> hoveredHeader_;
    std::optional<std::size_t> pressedHeader_;
    std::optional<std::size_t> hoveredEdge_;
    struct Resize {
        std::size_t column;
        float startX;                       // where it was pressed
        float startWidth;
    };
    std::optional<Resize> resizing_;
    bool barHovered_ = false;
    std::optional<double> thumbGrab_;       // the thumb dragged: where it was pressed, along it
    std::function<void(std::size_t)> onHeaderClicked_;
};

} // namespace glw
