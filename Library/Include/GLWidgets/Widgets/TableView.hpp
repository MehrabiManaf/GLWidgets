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
// A click on the header of a column its model can sort by (TableModel::sortable, compare) sorts the rows by it: ascending,
// then descending, then not; an arrow in the header says which. A filter shows some rows only. Neither changes the
// model: the delegate and the handlers speak of its rows still. A cell its model says is editable (ItemFlags::Editable)
// is edited in a widget its delegate makes (makeEditor: a field, a check box, a combo box...), by a double click, F2 or
// typing; Enter or the focus leaving it puts what was edited in (commitEdit), Tab goes on to the next editable cell,
// Escape leaves it as it was.
//
//   class Sales : public glw::TableModel { ... rowCount, columnCount, header ... };
//   auto table = std::make_unique<glw::TableView>();
//   table->setModel(sales);
//   table->setDelegate(std::make_unique<glw::TableTextDelegate>([&](std::size_t row, std::size_t column) { ... }));
//   table->setColumnWidth(0, 220);
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ItemModel.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/ScrollBar.hpp>
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
    // An editable cell edited: the widget it is edited in, showing its value (any widget); none: it is not. Then what was
    // edited goes into the model (not on Escape).
    virtual std::unique_ptr<Widget> makeEditor(std::size_t row, std::size_t column) { return nullptr; }
    virtual void commitEdit(Widget& editor, std::size_t row, std::size_t column) {}
};

// Each cell's text.
class GLW_EXPORT TableTextDelegate : public TableDelegate {
public:
    explicit TableTextDelegate(std::function<LocalizedText(std::size_t row, std::size_t column)> text) : text_(std::move(text)) {}
    // What an editable cell's text is set to, edited in a field (none: it is not edited).
    void setOnEdited(std::function<void(std::size_t row, std::size_t column, std::string text)> handler) { onEdited_ = std::move(handler); }
    std::unique_ptr<Widget> makeCell(std::size_t kind) override;
    void bindCell(Widget& cell, std::size_t row, std::size_t column) override;
    std::unique_ptr<Widget> makeEditor(std::size_t row, std::size_t column) override;
    void commitEdit(Widget& editor, std::size_t row, std::size_t column) override;

private:
    std::function<LocalizedText(std::size_t, std::size_t)> text_;
    std::function<void(std::size_t, std::size_t, std::string)> onEdited_;
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
    // As wide as its header and its cells need, eased there; a double click on its header's edge does it.
    void fitColumn(std::size_t column);
    // ... that width: its header's (with room for the arrow, when it sorts) and its widest cell's, measured in a cell of
    // its kind bound to each row. Those of the rows that show are measured and, of more rows than `fitRows`, that many
    // spread across them (none: all of them; a thousand by default).
    float contentWidth(std::size_t column);
    void setFitRows(std::optional<std::size_t> rows) { fitRows_ = rows; }
    // The last column takes the room the others leave (true by default).
    void setStretchLastColumn(bool stretch);
    void setHeaderVisible(bool visible);
    bool headerVisible() const { return headerVisible_; }
    // The width of all columns, the last one as it shows.
    double totalWidth() const;
    // The columns that show now: [first, last).
    std::pair<std::size_t, std::size_t> shownColumns() const;

    // ---- across ----
    // Dips scrolled from the start side (the left, or the right in right to left), and how far it goes. Set at once, or
    // eased there (the wheel).
    double horizontalOffset() const { return horizontalOffset_; }
    void setHorizontalOffset(double offset);
    void scrollAcrossSmoothly(double offset, bool aimed = false);
    double maximumHorizontalOffset() const;
    // Scrolled across so that the column shows (as little as it takes).
    void scrollToColumn(std::size_t column);

    // ---- rows ----
    std::optional<std::size_t> current() const { return list_->current(); }
    void setCurrent(std::optional<std::size_t> row) { list_->setCurrent(row); }
    bool isSelected(std::size_t row) const { return list_->isSelected(row); }
    // The rows selected (the model's, in its order): its list's selection (ListView), Multiple as Single.
    std::vector<std::size_t> selectedRows() const { return list_->selectedItems(); }
    void setOnSelectionChanged(std::function<void()> handler) { list_->setOnSelectionChanged(std::move(handler)); }
    // The widget showing a cell, while it shows.
    Widget* cellWidget(std::size_t row, std::size_t column) const;
    // A header's widget, while it shows.
    Widget* headerWidget(std::size_t column) const;
    // The list its rows are: scrolled down, its rows' widgets.
    ListView& list() const { return *list_; }

    void setOnCurrentChanged(std::function<void(std::optional<std::size_t>)> handler) { list_->setOnCurrentChanged(std::move(handler)); }
    // A double click or Enter on a row.
    void setOnActivated(std::function<void(std::size_t)> handler) { list_->setOnActivated(std::move(handler)); }
    // A header clicked (it sorts by its column, too, when it can).
    void setOnHeaderClicked(std::function<void(std::size_t)> handler) { onHeaderClicked_ = std::move(handler); }

    // ---- sorting and filtering ----
    // Sorted by a column, as the model compares its rows in it; none: in the model's order.
    void sortBy(std::optional<std::size_t> column, SortOrder order = SortOrder::Ascending);
    std::optional<std::size_t> sortColumn() const { return sortColumn_; }
    SortOrder sortOrder() const { return sortOrder_; }
    // A header clicked sorts by its column, if the model can (on by default).
    void setSortingEnabled(bool enabled) { sortingEnabled_ = enabled; }
    // The arrow in a header, as sortBy shows it, its rows left as they are (what shows them sorts them: a tree).
    void setSortIndicator(std::optional<std::size_t> column, SortOrder order = SortOrder::Ascending);
    // Only the rows `accepts` says yes to (the model's rows); none: all of them.
    void setFilter(std::function<bool(std::size_t)> accepts) { list_->setFilter(std::move(accepts)); }
    // Sorted and filtered again (what the filter says changed).
    void reorder() { list_->reorder(); }
    std::size_t shownRowCount() const { return list_->shownCount(); }

    // ---- editing ----
    void setEditTriggers(EditTriggers triggers) { editTriggers_ = triggers; }
    // The cell edited, if it is editable and its delegate makes an editor (true then).
    bool edit(std::size_t row, std::size_t column);
    bool editing() const;
    std::optional<std::pair<std::size_t, std::size_t>> editedCell() const { return edited_; }
    Widget* editor() const;
    void commitEdit();
    void cancelEdit();
    // The column F2 and typing edit, on the current row: the last cell pressed (none: its first editable one).
    std::optional<std::size_t> currentColumn() const { return currentColumn_; }
    // Where a cell's editor goes in it (the cell's coordinates): all of it by default (a tree's first column: after its
    // arrow).
    void setEditorArea(std::function<Rect(const Widget& cell, std::size_t column)> area) { editorArea_ = std::move(area); }

    // Its own (no cells' borders, say), or none: the theme's table.
    void setStyle(std::optional<ItemViewStyle> style);
    const ItemViewStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onPreviewMouse(MouseEvent& event) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    void onMouse(MouseEvent& event) override;
    void onPreviewKey(KeyEvent& event) override;
    void onText(TextEvent& event) override;
    std::optional<Rect> childClip(const Widget& child) const override;
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
    void changesBegun() override;
    void changesEnded() override;

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
    // Its bar across where it is, as it is laid out and scrolled.
    void placeBar();
    // where a cell shows (its coordinates), while it does
    std::optional<Rect> cellRect(std::size_t row, std::size_t column) const;
    // the cell at a point of it, in the rows
    std::optional<std::pair<std::size_t, std::size_t>> cellAt(Point point) const;
    bool editable(std::size_t row, std::size_t column) const;
    // the next editable cell after `column` in `row` (Tab), or before it (Shift+Tab)
    std::optional<std::size_t> nextEditable(std::size_t row, std::size_t column, bool backwards) const;
    void placeEditor();
    Widget& measurer(std::size_t kind);     // a cell of the kind, out of sight, that measures cells
    void fitStepped();

    std::shared_ptr<TableModel> model_;
    std::unique_ptr<TableDelegate> delegate_;
    std::shared_ptr<Rows> rows_;
    ListView* list_ = nullptr;
    Strip* header_ = nullptr;
    std::optional<float> rowHeight_;
    std::optional<ItemViewStyle> style_;

    std::vector<float> widths_;             // as set (0: the theme's)
    std::vector<double> lefts_;             // where each starts, one more: where the last ends (not stretched)
    bool stretchLast_ = true;
    bool headerVisible_ = true;
    double horizontalOffset_ = 0;
    void showAcross(double offset);
    SmoothScroll smoothAcross_{[this](double offset) { showAcross(offset); }};
    ScrollBar bar_{Orientation::Horizontal,
                   [this](double offset, bool eased) {
                       if (eased)
                           scrollAcrossSmoothly(offset);
                       else
                           setHorizontalOffset(offset);
                   },
                   [this] { invalidatePaint(); }};

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
    std::vector<std::pair<std::size_t, Widget*>> measurers_;    // by kind
    Widget* headerMeasurer_ = nullptr;
    std::optional<std::size_t> fitRows_ = 1000;
    struct Fit {
        std::size_t column;
        float from;
        float to;
    };
    std::optional<Fit> fitting_;            // a column eased to the width it needs
    Animation fitted_{[this] { fitStepped(); }};
    std::function<void(std::size_t)> onHeaderClicked_;

    std::optional<std::size_t> sortColumn_;
    SortOrder sortOrder_ = SortOrder::Ascending;
    std::optional<std::size_t> indicatorColumn_;    // the arrow shown
    SortOrder indicatorOrder_ = SortOrder::Ascending;
    std::function<Rect(const Widget&, std::size_t)> editorArea_;
    bool sortingEnabled_ = true;
    EditTriggers editTriggers_ = EditTriggers::All;
    std::unique_ptr<ItemEditor> editor_;
    std::optional<std::pair<std::size_t, std::size_t>> edited_;     // the row and column edited
    std::optional<std::size_t> currentColumn_;
};

} // namespace glw
