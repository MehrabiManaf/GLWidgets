// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/TableView.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
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

TableView::TableView() : rows_(std::make_shared<Rows>(*this)) {
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

const ItemViewStyle& TableView::style() const {
    if (const UiContext* ui = context())
        return ui->theme.itemView;
    static const ItemViewStyle unstyled = Theme::dark().itemView;
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
        if (Widget* row = list_->rowWidget(i))
            result.push_back(static_cast<Strip*>(row));
    }
    return result;
}

void TableView::rearrangeStrips() {
    for (Strip* strip : strips())
        strip->place();
    invalidatePaint();
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
        // a short line at its end side, but for the last one stretched
        if (stretchLast_ && column + 1 == widths_.size())
            continue;
        const bool hot = hoveredEdge_ == column || (resizing_ && resizing_->column == column);
        const float edge = rtl ? x : x + w;
        const float inset = hot ? 4.0f : 8.0f;
        p.fillRect({edge - (hot ? 1.0f : 0.5f), h.y + inset, hot ? 2.0f : 1.0f, h.height - 2 * inset}, hot ? s.indicator : s.headerLine);
    }
    p.popClip();
    p.fillRect({area_.x, h.bottom() - 1, area_.width, 1}, s.headerLine);
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
    if (!showBar_)
        return;
    const ScrollBarStyle& s = context.ui.theme.scrollBar;
    context.painter.fillRect(barTrack(), s.track);
    const Rect thumb = barThumb().inflated(-2);
    const VisualState state{.hovered = barHovered_ || thumbGrab_, .pressed = thumbGrab_.has_value()};
    context.painter.fillRoundedRect(thumb, std::min(thumb.width, thumb.height) / 2, s.thumb.resolve(state));
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
        const double before = horizontalOffset_;
        setHorizontalOffset(horizontalOffset_ + across * wheelStep);
        event.handled = horizontalOffset_ != before;
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
            resizing_ = Resize{*edge, event.position.x, shownWidth(*edge)};
            invalidatePaint();
            return;
        }
        if (const std::optional<std::size_t> column = headerAt(event.position)) {
            event.handled = true;
            pressedHeader_ = column;
            invalidatePaint();
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
        if (pressed && headerAt(event.position) == pressed && onHeaderClicked_)
            onHeaderClicked_(*pressed);
        break;
    }
    default: break;
    }
}

void TableView::onPreviewKey(KeyEvent& event) {
    if (!event.pressed || !list_->hasFocus())
        return;
    if (event.key != platform::Key::Left && event.key != platform::Key::Right)
        return;
    // toward the end side: on across
    const bool forward = event.key == (rightToLeft() ? platform::Key::Left : platform::Key::Right);
    setHorizontalOffset(horizontalOffset_ + (forward ? keyStep : -keyStep));
    event.handled = true;
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
    updateLefts();              // the theme's column width
    header_->clear();           // its colors
    invalidateLayout();
}

// ---- the model's changes ----

void TableView::rowsInserted(std::size_t first, std::size_t count) {
    rows_->notifyInserted(first, count);
}

void TableView::rowsRemoved(std::size_t first, std::size_t count) {
    rows_->notifyRemoved(first, count);
}

void TableView::rowsChanged(std::size_t first, std::size_t count) {
    rows_->notifyChanged(first, count);
}

void TableView::columnsChanged() {
    resetColumns();
    horizontalOffset_ = std::clamp(horizontalOffset_, 0.0, maximumHorizontalOffset());
    rebindStrips();
    invalidateLayout();
}

void TableView::modelReset() {
    resetColumns();
    horizontalOffset_ = 0;
    header_->clear();
    rows_->notifyReset();
    invalidateLayout();
}

} // namespace glw
