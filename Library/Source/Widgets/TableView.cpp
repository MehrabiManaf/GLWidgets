// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/TableView.hpp>

#include "Widgets/ItemEditor.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float parkedAt = -100000;     // where a cell showing nothing waits: out of sight, out of the mouse's way
constexpr float edgeGrip = 4;           // how near a header's edge the mouse resizes its column
constexpr float keyStep = 40;           // what Left and Right scroll across
constexpr float wheelStep = 3 * 28;     // ... and a step of the wheel

std::unique_ptr<Label> makeTextCell() {
    auto label = std::make_unique<Label>(Label::Props{.elide = true});
    label->setPadding({12, 0, 12, 0});
    label->setAlignment(Alignment::Stretch, Alignment::Center);
    return label;
}

} // namespace

TableDelegate::~TableDelegate() = default;

std::unique_ptr<Widget> TableTextDelegate::makeCell(std::size_t) {
    return makeTextCell();
}

void TableTextDelegate::bindCell(Widget& cell, std::size_t row, std::size_t column) {
    static_cast<Label&>(cell).setText(text_(row, column));
}

std::unique_ptr<Widget> TableTextDelegate::makeEditor(std::size_t row, std::size_t column) {
    if (!onEdited_)
        return nullptr;
    auto field = std::make_unique<LineEdit>(LineEdit::Props{.text = text_(row, column).source});
    field->selectAll();
    return field;
}

void TableTextDelegate::commitEdit(Widget& editor, std::size_t row, std::size_t column) {
    if (onEdited_)
        onEdited_(row, column, static_cast<LineEdit&>(editor).text());
}

// ---- the list's model ----

class TableView::Rows final : public ListModel {
public:
    explicit Rows(TableView& table) : table_(table) {}
    std::size_t count() const override { return table_.model_ ? table_.model_->rowCount() : 0; }
    using ListModel::notifyChanged;
    using ListModel::notifyInserted;
    using ListModel::notifyRemoved;
    using ListModel::notifyReset;

private:
    TableView& table_;
};

// ---- a row's cells, or the header's ----

class TableView::Strip final : public Widget {
public:
    Strip(TableView& table, bool header) : table_(table), header_(header) {
        setClipsChildren(true);
    }

    // The row it shows now: the cells of the columns that still show bound again.
    void show(std::size_t row) {
        row_ = row;
        for (const Cell& cell : cells_)
            bind(cell);
        invalidateLayout();
    }

    // Its cells made again (the columns, or their kinds, changed).
    void clear() {
        for (const Cell& cell : cells_)
            removeChild(*cell.widget);
        for (const Cell& cell : spare_)
            removeChild(*cell.widget);
        cells_.clear();
        spare_.clear();
        invalidateLayout();
    }

    // Its cells for the columns that show, placed again (scrolled across, a column resized).
    void place() { rearrange(); }

    Widget* cell(std::size_t column) const {
        for (const Cell& cell : cells_) {
            if (cell.column == column)
                return cell.widget;
        }
        return nullptr;
    }

protected:
    Size measureContent(Size available) override {
        sync();
        const float width = std::isfinite(available.width) ? available.width : 0.0f;
        if (header_ || !table_.rowHeight_ || *table_.rowHeight_ > 0)
            return {width, table_.rowHeight_.value_or(table_.style().rowHeight)};
        // as high as its tallest cell
        float height = 0;
        for (const Cell& cell : cells_)
            height = std::max(height, cell.widget->measure({table_.shownWidth(cell.column), unbounded}).height);
        return {width, height};
    }

    void arrangeContent(const Rect& content) override {
        sync();
        const bool rtl = rightToLeft();
        for (const Cell& cell : cells_) {
            const Rect place{content.x + table_.columnX(cell.column, content.width), content.y, table_.shownWidth(cell.column), content.height};
            arrangeInCell(*cell.widget, place, rtl);
        }
    }

private:
    struct Cell {
        Widget* widget = nullptr;
        std::size_t kind = 0;
        std::size_t column = 0;
    };

    void bind(const Cell& cell) {
        if (header_) {
            auto& label = static_cast<Label&>(*cell.widget);
            label.setText(table_.model_->header(cell.column));
            label.setColor(table_.style().headerText);
        } else if (table_.delegate_) {
            table_.delegate_->bindCell(*cell.widget, row_, cell.column);
        }
    }

    // The cells of the columns that show: those of the columns that went put aside, those of the columns that came
    // taken from them (of their kind) or made, and bound.
    void sync() {
        const bool can = table_.model_ && (header_ || table_.delegate_);
        const auto [first, last] = can ? table_.shownColumns() : std::pair<std::size_t, std::size_t>{0, 0};
        for (std::size_t i = cells_.size(); i-- > 0;) {
            if (cells_[i].column < first || cells_[i].column >= last) {
                cells_[i].widget->arrange({parkedAt, 0, 0, 0});
                spare_.push_back(cells_[i]);
                cells_.erase(cells_.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
        for (std::size_t column = first; column < last; ++column) {
            if (cell(column))
                continue;
            const std::size_t kind = header_ ? 0 : table_.delegate_->cellKind(column);
            Cell made{nullptr, kind, column};
            for (std::size_t i = spare_.size(); i-- > 0;) {
                if (spare_[i].kind == kind) {
                    made.widget = spare_[i].widget;
                    spare_.erase(spare_.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
            if (!made.widget)
                made.widget = &addChild(header_ ? makeTextCell() : table_.delegate_->makeCell(kind));
            bind(made);
            cells_.push_back(made);
        }
    }

    TableView& table_;
    bool header_;
    std::size_t row_ = 0;
    std::vector<Cell> cells_;               // those that show, a column each
    std::vector<Cell> spare_;               // made, showing nothing
};

class TableView::RowDelegate final : public ListDelegate {
public:
    explicit RowDelegate(TableView& table) : table_(table) {}
    std::unique_ptr<Widget> makeRow() override { return std::make_unique<Strip>(table_, false); }
    void bindRow(Widget& row, std::size_t index) override { static_cast<Strip&>(row).show(index); }

private:
    TableView& table_;
};

// ---- the view ----

TableView::TableView() : rows_(std::make_shared<Rows>(*this)), editor_(std::make_unique<ItemEditor>(*this)) {
    setClipsChildren(true);
    list_ = &add(std::make_unique<ListView>());
    list_->setModel(rows_);
    list_->setDelegate(std::make_unique<RowDelegate>(*this));
    header_ = &add(std::make_unique<Strip>(*this, true));
}

TableView::~TableView() {
    if (model_)
        model_->removeObserver(*this);
}

void TableView::setStyle(std::optional<ItemViewStyle> style) {
    style_ = std::move(style);
    onStyleChanged();
}

const ItemViewStyle& TableView::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.table;
    static const ItemViewStyle unstyled = Theme::dark().table;
    return unstyled;
}

void TableView::setModel(std::shared_ptr<TableModel> model) {
    if (model_)
        model_->removeObserver(*this);
    model_ = std::move(model);
    if (model_)
        model_->addObserver(*this);
    widths_.clear();
    modelReset();
}

void TableView::setDelegate(std::unique_ptr<TableDelegate> delegate) {
    for (const auto& [kind, widget] : measurers_)
        removeChild(*widget);
    measurers_.clear();
    delegate_ = std::move(delegate);
    list_->setDelegate(std::make_unique<RowDelegate>(*this));       // its rows made again, with the new delegate's cells
}

void TableView::setRowHeight(std::optional<float> height) {
    rowHeight_ = height;
    list_->setRowHeight(height);
    header_->invalidateLayout();
}

void TableView::setSelectionMode(SelectionMode mode) {
    list_->setSelectionMode(mode);
}

// ---- columns ----

void TableView::resetColumns() {
    widths_.resize(model_ ? model_->columnCount() : 0, 0.0f);
    updateLefts();
}

void TableView::updateLefts() {
    lefts_.assign(widths_.size() + 1, 0.0);
    for (std::size_t i = 0; i < widths_.size(); ++i)
        lefts_[i + 1] = lefts_[i] + columnWidth(i);
}

float TableView::columnWidth(std::size_t column) const {
    if (column >= widths_.size())
        return 0;
    return widths_[column] > 0 ? widths_[column] : style().columnWidth;
}

void TableView::setColumnWidth(std::size_t column, float width) {
    if (column >= widths_.size())
        return;
    width = std::max(minimumColumnWidth, width);
    if (width == widths_[column])
        return;
    widths_[column] = width;
    updateLefts();
    horizontalOffset_ = std::clamp(horizontalOffset_, 0.0, maximumHorizontalOffset());
    rearrangeStrips();
    invalidateLayout();         // the bar across may show, or not
}

void TableView::setStretchLastColumn(bool stretch) {
    if (stretch == stretchLast_)
        return;
    stretchLast_ = stretch;
    rearrangeStrips();
    invalidateLayout();
}

void TableView::setHeaderVisible(bool visible) {
    headerVisible_ = visible;
    header_->setVisible(visible);
    invalidateLayout();
}

float TableView::viewWidth() const {
    return list_->viewport().width;
}

float TableView::shownWidth(std::size_t column) const {
    const float width = columnWidth(column);
    if (stretchLast_ && column + 1 == widths_.size())
        return std::max(width, static_cast<float>(viewWidth() - lefts_[column]));
    return width;
}

double TableView::totalWidth() const {
    if (widths_.empty())
        return 0;
    const std::size_t last = widths_.size() - 1;
    return lefts_[last] + shownWidth(last);
}

float TableView::columnX(std::size_t column, float width) const {
    const float x = static_cast<float>(lefts_[column] - horizontalOffset_);
    return rightToLeft() ? width - x - shownWidth(column) : x;
}

std::pair<std::size_t, std::size_t> TableView::shownColumns() const {
    const std::size_t count = widths_.size();
    const float width = viewWidth();
    if (count == 0 || width <= 0)
        return {0, 0};
    const auto begin = lefts_.begin(), end = lefts_.begin() + static_cast<std::ptrdiff_t>(count);
    // the last starting at or before the offset; those starting before the view ends
    const std::size_t first = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, std::upper_bound(begin, end, horizontalOffset_) - begin - 1));
    const std::size_t last = static_cast<std::size_t>(std::lower_bound(begin, end, horizontalOffset_ + width) - begin);
    return {first, std::max(first + 1, last)};
}

Widget* TableView::cellWidget(std::size_t row, std::size_t column) const {
    auto* strip = static_cast<Strip*>(list_->rowWidget(row));
    return strip ? strip->cell(column) : nullptr;
}

Widget* TableView::headerWidget(std::size_t column) const {
    return header_->cell(column);
}

std::vector<TableView::Strip*> TableView::strips() const {
    std::vector<Strip*> result{header_};
    const auto [first, last] = list_->shownItems();
    for (std::size_t i = first; i < last; ++i) {
        if (Widget* row = list_->rowWidget(list_->modelIndex(i)))
            result.push_back(static_cast<Strip*>(row));
    }
    return result;
}

void TableView::rearrangeStrips() {
    for (Strip* strip : strips())
        strip->place();
    placeEditor();
    invalidatePaint();
}

// ---- sorting ----

void TableView::sortBy(std::optional<std::size_t> column, SortOrder order) {
    if (column && (!model_ || *column >= model_->columnCount()))
        column.reset();
    sortColumn_ = column;
    sortOrder_ = order;
    indicatorColumn_ = column;
    indicatorOrder_ = order;
    cancelEdit();
    if (column)
        list_->setSortOrder(order, [this, c = *column](std::size_t a, std::size_t b) { return model_->compare(a, b, c); });
    else
        list_->setSortOrder(std::nullopt);
    invalidatePaint();
}

void TableView::setSortIndicator(std::optional<std::size_t> column, SortOrder order) {
    indicatorColumn_ = column;
    indicatorOrder_ = order;
    invalidatePaint();
}

// ---- fitting a column to what it shows ----

Widget& TableView::measurer(std::size_t kind) {
    for (const auto& [k, widget] : measurers_) {
        if (k == kind)
            return *widget;
    }
    Widget& made = addChild(delegate_->makeCell(kind));
    made.setHitTestVisible(false);
    made.arrange({parkedAt, 0, 0, 0});
    measurers_.emplace_back(kind, &made);
    return made;
}

float TableView::contentWidth(std::size_t column) {
    if (!model_ || column >= model_->columnCount())
        return minimumColumnWidth;
    const float height = rowHeight_ && *rowHeight_ > 0 ? *rowHeight_ : style().rowHeight;
    float width = minimumColumnWidth;
    const auto measure = [&](Widget& cell, float more) {
        const Size s = cell.measure({unbounded, height});
        width = std::max(width, s.width + cell.margin().horizontal() + more);
    };
    if (headerVisible_) {
        if (!headerMeasurer_) {
            headerMeasurer_ = &addChild(makeTextCell());
            headerMeasurer_->setHitTestVisible(false);
            headerMeasurer_->arrange({parkedAt, 0, 0, 0});
        }
        static_cast<Label&>(*headerMeasurer_).setText(model_->header(column));
        measure(*headerMeasurer_, model_->sortable(column) ? 14.0f : 0.0f);      // the arrow after the title
    }
    if (delegate_) {
        Widget& cell = measurer(delegate_->cellKind(column));
        const std::size_t count = list_->shownCount();
        const auto at = [&](std::size_t position) {
            delegate_->bindCell(cell, list_->modelIndex(position), column);
            measure(cell, 0);
        };
        if (!fitRows_ || count <= *fitRows_) {
            for (std::size_t position = 0; position < count; ++position)
                at(position);
        } else {
            // those that show, and some spread across all of them
            const auto [first, last] = list_->shownItems();
            for (std::size_t position = first; position < last; ++position)
                at(position);
            // spread by the golden ratio, not evenly: rows that repeat in a period (a folder and its files) are not
            // missed by a sample of the same period
            double place = 0;
            for (std::size_t k = 0; k < *fitRows_; ++k) {
                at(std::min(count - 1, static_cast<std::size_t>(place * static_cast<double>(count))));
                place += 0.6180339887498949;
                place -= std::floor(place);
            }
            at(count - 1);
        }
    }
    return std::ceil(width);
}

void TableView::fitColumn(std::size_t column) {
    if (column >= widths_.size())
        return;
    fitting_ = Fit{column, columnWidth(column), contentWidth(column)};
    fitted_.set(0);
    fitted_.animateTo(scheduler(), 1.0f, context() ? Duration(context()->theme.motion.normal) : Duration(std::chrono::milliseconds(200)),
                      Easing::InOutCubic);
    fitStepped();
}

void TableView::fitStepped() {
    if (!fitting_)
        return;
    const Fit fit = *fitting_;
    setColumnWidth(fit.column, fit.from + (fit.to - fit.from) * fitted_.value());
    if (!fitted_.running())
        fitting_.reset();
}

// ---- editing ----

bool TableView::editable(std::size_t row, std::size_t column) const {
    return model_ && delegate_ && row < model_->rowCount() && column < model_->columnCount() &&
           hasFlag(model_->flags(row, column), ItemFlags::Editable);
}

std::optional<std::size_t> TableView::nextEditable(std::size_t row, std::size_t column, bool backwards) const {
    const std::size_t count = widths_.size();
    for (std::size_t step = 1; step < count; ++step) {
        const std::size_t c = backwards ? (column + count - step) % count : (column + step) % count;
        if ((backwards ? c < column : c > column) && editable(row, c))
            return c;
    }
    return std::nullopt;
}

bool TableView::edit(std::size_t row, std::size_t column) {
    if (!editable(row, column) || !list_->position(row))
        return false;
    commitEdit();
    list_->setCurrent(row);
    currentColumn_ = column;
    scrollToColumn(column);
    std::unique_ptr<Widget> editor = delegate_->makeEditor(row, column);
    if (!editor)
        return false;
    edited_ = std::pair{row, column};
    editor_->begin(std::move(editor), cellRect(row, column).value_or(Rect{}), [this, row, column] {
        if (Widget* e = editor_->editor(); e && delegate_)
            delegate_->commitEdit(*e, row, column);
        edited_.reset();
    });
    placeEditor();
    return true;
}

bool TableView::editing() const {
    return editor_->active();
}

Widget* TableView::editor() const {
    return editor_->editor();
}

void TableView::commitEdit() {
    editor_->commit();
    edited_.reset();
}

void TableView::cancelEdit() {
    editor_->cancel();
    edited_.reset();
}

std::optional<Rect> TableView::cellRect(std::size_t row, std::size_t column) const {
    const Widget* cell = cellWidget(row, column);
    if (!cell || !cell->visible())
        return std::nullopt;
    const Point at = mapFromRoot(cell->mapToRoot({}));
    if (editorArea_) {
        const Rect in = editorArea_(*cell, column);
        return Rect{at.x + in.x, at.y + in.y, in.width, in.height};
    }
    return Rect{at.x, at.y, cell->size().width, cell->size().height};
}

std::optional<std::pair<std::size_t, std::size_t>> TableView::cellAt(Point point) const {
    const Rect rows = list_->bounds();
    const std::optional<std::size_t> row = list_->itemAt({point.x - rows.x, point.y - rows.y});
    if (!row)
        return std::nullopt;
    const auto [first, last] = shownColumns();
    const float x = point.x - rows.x - list_->viewport().x;
    for (std::size_t column = first; column < last; ++column) {
        const float left = columnX(column, viewWidth());
        if (x >= left && x < left + shownWidth(column))
            return std::pair{*row, column};
    }
    return std::nullopt;
}

void TableView::placeEditor() {
    if (editor_->active() && edited_)
        editor_->place(cellRect(edited_->first, edited_->second));
}

std::optional<Rect> TableView::childClip(const Widget& child) const {
    // its editor: within the rows (not over the header or the bar), as its cell is
    if (!editor_->active() || child.children().empty() || editor_->editor() != child.children().front().get())
        return std::nullopt;
    const Rect rows = list_->bounds(), view = list_->viewport();
    return Rect{rows.x + view.x, rows.y + view.y, view.width, view.height};
}

void TableView::rebindStrips() {
    header_->clear();
    list_->setDelegate(std::make_unique<RowDelegate>(*this));
}

// ---- across ----

double TableView::maximumHorizontalOffset() const {
    return std::max(0.0, totalWidth() - viewWidth());
}

void TableView::setHorizontalOffset(double offset) {
    smoothAcross_.stop();
    showAcross(offset);
}

void TableView::scrollAcrossSmoothly(double offset, bool aimed) {
    // to the nearest column's edge (or the end): the column at the start shows whole
    const double limit = maximumHorizontalOffset();
    offset = std::clamp(offset, 0.0, limit);
    if (offset < limit - 0.5 && lefts_.size() > 1) {
        const auto after = std::upper_bound(lefts_.begin(), lefts_.end() - 1, offset);
        const double right = after != lefts_.end() - 1 ? *after : limit, left = *(after - 1);
        offset = std::min(limit, offset - left < right - offset ? left : right);
    }
    if (offset == smoothAcross_.target(horizontalOffset_))
        return;
    const UiContext* ui = context();
    smoothAcross_.scrollTo(scheduler(), horizontalOffset_, offset, ui ? Duration(ui->theme.motion.scroll) : Duration::zero(), aimed);
}

void TableView::showAcross(double offset) {
    offset = std::clamp(offset, 0.0, maximumHorizontalOffset());
    if (offset == horizontalOffset_)
        return;
    horizontalOffset_ = offset;
    rearrangeStrips();          // the cells that show now: nothing else changes
}

void TableView::scrollToColumn(std::size_t column) {
    if (column >= widths_.size())
        return;
    const double left = lefts_[column], right = left + shownWidth(column);
    if (left < horizontalOffset_)
        setHorizontalOffset(left);
    else if (right > horizontalOffset_ + viewWidth())
        setHorizontalOffset(std::min(left, right - viewWidth()));
}

// ---- layout ----

Size TableView::measureContent(Size available) {
    // all the room there is; on its own, a few rows' worth
    return {std::isfinite(available.width) ? available.width : 480.0f, std::isfinite(available.height) ? available.height : 360.0f};
}

void TableView::arrangeContent(const Rect& content) {
    area_ = content;
    const float header = headerVisible_ ? style().rowHeight : 0.0f;
    const float barWidth = context() ? context()->theme.scrollBar.width : 8.0f;
    const auto lay = [&](bool bar) {
        list_->arrange({content.x, content.y + header, content.width, std::max(0.0f, content.height - header - (bar ? barWidth : 0.0f))});
    };
    // the rows first (their width less their bar is the columns' room), with the bar across when they are wider
    const double before = horizontalOffset_;
    showBar_ = false;
    lay(false);
    if (totalWidth() > viewWidth() + 0.5) {
        showBar_ = true;
        lay(true);
    }
    horizontalOffset_ = std::clamp(horizontalOffset_, 0.0, maximumHorizontalOffset());
    const Rect rows = list_->bounds(), view = list_->viewport();
    header_->arrange({rows.x + view.x, content.y, view.width, header});
    if (horizontalOffset_ != before)
        rearrangeStrips();
}

// ---- painting ----

Rect TableView::headerRect() const {
    return header_->bounds();
}

void TableView::paint(PaintContext& context) {
    if (!headerVisible_ || widths_.empty())
        return;
    const ItemViewStyle& s = style();
    paint::Painter& p = context.painter;
    const Rect h = headerRect();
    const bool rtl = rightToLeft();
    p.pushClip(h);
    const auto [first, last] = shownColumns();
    for (std::size_t column = first; column < last; ++column) {
        const float x = h.x + columnX(column, h.width), w = shownWidth(column);
        // under the mouse, pressed
        if (hoveredHeader_ == column && !hoveredEdge_ && !resizing_) {
            const VisualState state{.hovered = true, .pressed = pressedHeader_ == column};
            p.fillRoundedRect({x + s.inset, h.y + 2, w - 2 * s.inset, h.height - 4}, s.cornerRadius, s.row.resolve(state));
        }
        // sorted by it: an arrow at its end side, up ascending, down descending
        if (indicatorColumn_ == column) {
            const float ax = rtl ? x + 14 : x + w - 14, ay = h.y + h.height / 2;
            const float d = indicatorOrder_ == SortOrder::Ascending ? -1.0f : 1.0f;
            const Point arrow[] = {{ax - 4, ay - 2 * d}, {ax, ay + 2 * d}, {ax + 4, ay - 2 * d}};
            p.drawPolyline(arrow, 1.4f, s.headerText);
        }
        // a short line at its end side, but for the last one stretched
        if (stretchLast_ && column + 1 == widths_.size())
            continue;
        const bool hot = hoveredEdge_ == column || (resizing_ && resizing_->column == column);
        const float edge = rtl ? x : x + w;
        const float inset = s.columnLines ? 0.0f : hot ? 4.0f : 8.0f;
        p.fillRect({edge - (hot ? 1.0f : 0.5f), h.y + inset, hot ? 2.0f : 1.0f, h.height - 2 * inset}, hot ? s.indicator : s.headerLine);
    }
    p.popClip();
    p.fillRect({area_.x, h.bottom() - 1, area_.width, 1}, s.headerLine);
    placeEditor();          // over its cell, where the rows scrolled
}

Rect TableView::barTrack() const {
    const float width = context() ? context()->theme.scrollBar.width : 8.0f;
    const Rect rows = list_->bounds(), view = list_->viewport();
    return {rows.x + view.x, area_.bottom() - width, view.width, width};
}

Rect TableView::barThumb() const {
    const Rect track = barTrack();
    const double total = std::max(1.0, totalWidth());
    const float minimum = context() ? context()->theme.scrollBar.minimumThumb : 24.0f;
    const float length = std::min(track.width, std::max(minimum, static_cast<float>(track.width * viewWidth() / total)));
    const double room = maximumHorizontalOffset();
    const float at = room > 0 ? static_cast<float>((track.width - length) * horizontalOffset_ / room) : 0.0f;
    return {rightToLeft() ? track.right() - at - length : track.x + at, track.y, length, track.height};
}

void TableView::paintOver(PaintContext& context) {
    const ItemViewStyle& cells = style();
    if (cells.columnLines && !widths_.empty()) {
        paint::Painter& p = context.painter;
        const Rect rows = list_->bounds(), view = list_->viewport();
        const Rect area{rows.x + view.x, rows.y + view.y, view.width, view.height};
        // down to the last row, when it shows
        float bottom = area.bottom();
        const std::size_t lastShown = list_->shownItems().second;
        if (lastShown == list_->shownCount()) {
            if (lastShown == 0)
                bottom = area.y;
            else if (const std::optional<Rect> last = list_->itemRect(list_->modelIndex(lastShown - 1)))
                bottom = std::min(bottom, rows.y + last->bottom());
        }
        const bool rtl = rightToLeft();
        p.pushClip(area);
        const auto [first, last] = shownColumns();
        for (std::size_t column = first; column < last; ++column) {
            if (stretchLast_ && column + 1 == widths_.size())
                break;
            const float x = area.x + columnX(column, area.width);
            const float edge = rtl ? x : x + shownWidth(column);
            p.fillRect({edge - cells.gridLineWidth / 2, area.y, cells.gridLineWidth, bottom - area.y}, cells.gridLine);
        }
        p.popClip();
    }
    if (!showBar_)
        return;
    const ScrollBarStyle& s = context.ui.theme.scrollBar;
    context.painter.fillRect(barTrack(), s.track);
    const VisualState state{.hovered = barHovered_ || thumbGrab_, .pressed = thumbGrab_.has_value()};
    barWide_.animateTo(scheduler(), state.hovered ? 1.0f : 0.0f, context.ui.theme.motion.fast);
    paintScrollThumb(context.painter, barThumb(), false, true, barWide_.value(), s.thumb.resolve(state), s.cornerRadius);
}

// ---- events ----

std::optional<std::size_t> TableView::headerAt(Point point) const {
    const Rect h = headerRect();
    if (!headerVisible_ || !h.contains(point))
        return std::nullopt;
    const auto [first, last] = shownColumns();
    for (std::size_t column = first; column < last; ++column) {
        const float x = h.x + columnX(column, h.width);
        if (point.x >= x && point.x < x + shownWidth(column))
            return column;
    }
    return std::nullopt;
}

std::optional<platform::Cursor> TableView::cursorAt(Point point) const {
    if (resizing_ || edgeAt(point))
        return platform::Cursor::ResizeHorizontal;
    return Widget::cursorAt(point);
}

std::optional<std::size_t> TableView::edgeAt(Point point) const {
    const Rect h = headerRect();
    if (!headerVisible_ || !h.contains(point))
        return std::nullopt;
    const bool rtl = rightToLeft();
    const auto [first, last] = shownColumns();
    std::optional<std::size_t> found;
    for (std::size_t column = first; column < last; ++column) {
        if (stretchLast_ && column + 1 == widths_.size())
            break;
        const float x = h.x + columnX(column, h.width);
        const float edge = rtl ? x : x + shownWidth(column);
        if (std::abs(point.x - edge) <= edgeGrip)
            found = column;
    }
    return found;
}

void TableView::onPreviewMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Wheel: {
        // across: the wheel turned with Shift, or tilted
        const bool shift = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
        const float across = shift ? -event.wheelDelta.y : (rightToLeft() ? -event.wheelDelta.x : event.wheelDelta.x);
        if (across == 0 || (!shift && std::abs(event.wheelDelta.x) < std::abs(event.wheelDelta.y)))
            return;
        if (event.wheelPrecise) {
            // by fractions of a step: eased on, not boosted nor snapped to columns
            const double before = smoothAcross_.target(horizontalOffset_);
            const double to = std::clamp(before + across * wheelStep, 0.0, maximumHorizontalOffset());
            if (to != before) {
                const UiContext* ui = context();
                smoothAcross_.scrollTo(scheduler(), horizontalOffset_, to, ui ? Duration(ui->theme.motion.scroll) : Duration::zero());
            }
            event.handled = to != before;
            break;
        }
        const double before = smoothAcross_.target(horizontalOffset_);
        scrollAcrossSmoothly(smoothAcross_.aim(scheduler(), horizontalOffset_, across * wheelStep, 0, maximumHorizontalOffset()), true);
        event.handled = smoothAcross_.target(horizontalOffset_) != before;
        break;
    }
    case MouseEventType::Move: {
        if (resizing_ || thumbGrab_)
            return;
        const std::optional<std::size_t> edge = edgeAt(event.position);
        const std::optional<std::size_t> over = edge ? std::nullopt : headerAt(event.position);
        const bool overBar = showBar_ && barTrack().contains(event.position);
        if (edge != hoveredEdge_ || over != hoveredHeader_ || overBar != barHovered_) {
            hoveredEdge_ = edge;
            hoveredHeader_ = over;
            barHovered_ = overBar;
            invalidatePaint();
        }
        break;
    }
    case MouseEventType::Press: {
        if (event.button != platform::MouseButton::Left)
            return;
        if (showBar_ && barTrack().contains(event.position)) {
            event.handled = true;
            const Rect thumb = barThumb();
            if (thumb.contains(event.position)) {
                thumbGrab_ = event.position.x - thumb.x;
            } else {        // a page toward where it was pressed
                const bool toward = (event.position.x < thumb.x) == rightToLeft();
                setHorizontalOffset(horizontalOffset_ + (toward ? 1.0 : -1.0) * viewWidth());
            }
            invalidatePaint();
            return;
        }
        if (const std::optional<std::size_t> edge = edgeAt(event.position)) {
            event.handled = true;
            if (event.clicks == 2) {
                fitColumn(*edge);
                return;
            }
            fitted_.set(0);                 // a drag takes over from the easing
            fitting_.reset();
            resizing_ = Resize{*edge, event.position.x, shownWidth(*edge)};
            invalidatePaint();
            return;
        }
        if (const std::optional<std::size_t> column = headerAt(event.position)) {
            event.handled = true;
            pressedHeader_ = column;
            invalidatePaint();
            break;
        }
        if (const auto cell = cellAt(event.position)) {
            currentColumn_ = cell->second;
            // twice on an editable cell: edited (not activated)
            if (event.clicks == 2 && hasTrigger(editTriggers_, EditTriggers::DoubleClick) && edit(cell->first, cell->second))
                event.handled = true;
        }
        break;
    }
    default: break;
    }
}

void TableView::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Move:
        if (resizing_) {
            const float moved = event.position.x - resizing_->startX;
            setColumnWidth(resizing_->column, resizing_->startWidth + (rightToLeft() ? -moved : moved));
            event.handled = true;
        } else if (thumbGrab_) {
            // the thumb follows the mouse; the offset, the thumb
            const Rect track = barTrack();
            const float length = barThumb().width;
            const double room = std::max(1.0f, track.width - length);
            const double at = rightToLeft() ? track.right() - (event.position.x - *thumbGrab_ + length) : event.position.x - *thumbGrab_ - track.x;
            setHorizontalOffset(at / room * maximumHorizontalOffset());
            event.handled = true;
        }
        break;
    case MouseEventType::Release: {
        if (event.button != platform::MouseButton::Left)
            return;
        const std::optional<std::size_t> pressed = std::exchange(pressedHeader_, std::nullopt);
        if (resizing_ || thumbGrab_ || pressed)
            event.handled = true;
        resizing_.reset();
        thumbGrab_.reset();
        invalidatePaint();
        if (pressed && headerAt(event.position) == pressed) {
            // sorted by it: ascending, then descending, then not
            if (sortingEnabled_ && model_ && model_->sortable(*pressed)) {
                if (sortColumn_ != pressed)
                    sortBy(*pressed, SortOrder::Ascending);
                else if (sortOrder_ == SortOrder::Ascending)
                    sortBy(*pressed, SortOrder::Descending);
                else
                    sortBy(std::nullopt);
            }
            if (onHeaderClicked_)
                onHeaderClicked_(*pressed);
        }
        break;
    }
    default: break;
    }
}

void TableView::onPreviewKey(KeyEvent& event) {
    if (!event.pressed)
        return;
    // its editor's: Enter commits, Escape cancels, Tab goes on to the next editable cell of the row
    if (editor_->active()) {
        if (event.key == platform::Key::Tab && edited_) {
            const auto [row, column] = *edited_;
            const bool back = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
            const std::optional<std::size_t> next = nextEditable(row, column, back);
            commitEdit();
            if (next)
                edit(row, *next);
            event.handled = true;
        } else if (editor_->key(event)) {
            event.handled = true;
        }
        return;
    }
    if (!list_->hasFocus())
        return;
    // F2: the current cell edited (its column, or the row's first editable one)
    if (event.key == platform::Key::F2 && hasTrigger(editTriggers_, EditTriggers::F2)) {
        if (const std::optional<std::size_t> row = list_->current()) {
            std::optional<std::size_t> column = currentColumn_;
            if (!column || !editable(*row, *column))
                column = editable(*row, 0) ? std::optional<std::size_t>(0) : nextEditable(*row, 0, false);
            if (column && edit(*row, *column))
                event.handled = true;
        }
        return;
    }
    if (event.key != platform::Key::Left && event.key != platform::Key::Right)
        return;
    // toward the end side: on across
    const bool forward = event.key == (rightToLeft() ? platform::Key::Left : platform::Key::Right);
    setHorizontalOffset(horizontalOffset_ + (forward ? keyStep : -keyStep));
    event.handled = true;
}

void TableView::onText(TextEvent& event) {
    // typed on the current row: its current cell (or first editable one) edited, what was typed into its editor
    if (event.composing || event.text.empty() || editor_->active() || !hasTrigger(editTriggers_, EditTriggers::Typing))
        return;
    const std::optional<std::size_t> row = list_->current();
    if (!row || static_cast<unsigned char>(event.text[0]) < 0x20)
        return;
    std::optional<std::size_t> column = currentColumn_;
    if (!column || !editable(*row, *column))
        column = editable(*row, 0) ? std::optional<std::size_t>(0) : nextEditable(*row, 0, false);
    if (!column || !edit(*row, *column))
        return;
    event.handled = true;
    if (UiRoot* r = root())
        r->dispatch(platform::TextInput{0, event.text});
}

void TableView::onHoverChanged(bool hovered) {
    if (!hovered && (hoveredHeader_ || hoveredEdge_ || barHovered_)) {
        hoveredHeader_.reset();
        hoveredEdge_.reset();
        barHovered_ = false;
        invalidatePaint();
    }
}

void TableView::onRootChanged() {
    onStyleChanged();
}

void TableView::onStyleChanged() {
    list_->setStyle(style());   // its rows: the table's style
    updateLefts();              // the theme's column width
    header_->clear();           // its colors
    invalidateLayout();
}

// ---- the model's changes ----

void TableView::rowsInserted(std::size_t first, std::size_t count) {
    cancelEdit();
    rows_->notifyInserted(first, count);
}

void TableView::rowsRemoved(std::size_t first, std::size_t count) {
    cancelEdit();
    rows_->notifyRemoved(first, count);
}

void TableView::rowsChanged(std::size_t first, std::size_t count) {
    rows_->notifyChanged(first, count);
}

void TableView::changesBegun() {
    rows_->beginChanges();
}

void TableView::changesEnded() {
    rows_->endChanges();
}

void TableView::columnsChanged() {
    cancelEdit();
    if (sortColumn_ && *sortColumn_ >= (model_ ? model_->columnCount() : 0))
        sortBy(std::nullopt);
    resetColumns();
    horizontalOffset_ = std::clamp(horizontalOffset_, 0.0, maximumHorizontalOffset());
    rebindStrips();
    invalidateLayout();
}

void TableView::modelReset() {
    cancelEdit();
    sortColumn_.reset();
    indicatorColumn_.reset();
    list_->setSortOrder(std::nullopt);
    resetColumns();
    horizontalOffset_ = 0;
    header_->clear();
    rows_->notifyReset();
    invalidateLayout();
}

} // namespace glw
