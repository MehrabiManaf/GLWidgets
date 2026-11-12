// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ListView.hpp>

#include "Widgets/IndexSet.hpp"
#include "Widgets/ItemEditor.hpp"
#include "Widgets/ItemOrder.hpp"
#include "Widgets/RowHeights.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <tuple>
#include <cmath>
#include <numeric>

namespace glw {

namespace {

constexpr float wheelRows = 3;          // rows a step of the wheel scrolls
constexpr float parkedAt = -100000;     // where a row widget showing nothing waits: out of sight, out of the mouse's way

} // namespace

ListDelegate::~ListDelegate() = default;

// ---- the text delegate ----

std::unique_ptr<Widget> TextDelegate::makeRow() {
    auto row = std::make_unique<Widget>();
    row->setPadding({12, 0, 12, 0});
    auto& label = row->add(std::make_unique<Label>(Label::Props{.elide = true}));
    label.setAlignment(Alignment::Start, Alignment::Center);
    return row;
}

void TextDelegate::bindRow(Widget& row, std::size_t index) {
    static_cast<Label&>(*row.children()[0]).setText(text_(index));
}

std::unique_ptr<Widget> TextDelegate::makeEditor(std::size_t index) {
    if (!onEdited_)
        return nullptr;
    auto field = std::make_unique<LineEdit>(LineEdit::Props{.text = text_(index).source});
    field->selectAll();
    return field;
}

void TextDelegate::commitEdit(Widget& editor, std::size_t index) {
    if (onEdited_)
        onEdited_(index, static_cast<LineEdit&>(editor).text());
}

// ---- the view ----

ListView::ListView()
    : heights_(std::make_unique<RowHeights>()), order_(std::make_unique<ItemOrder>()), selection_(std::make_unique<IndexSet>()),
      editor_(std::make_unique<ItemEditor>(*this)) {
    setFocusable(true);
    setClipsChildren(true);
}

ListView::~ListView() {
    if (model_)
        model_->removeObserver(*this);
}

void ListView::setStyle(std::optional<ItemViewStyle> style) {
    style_ = std::move(style);
    onStyleChanged();       // its rows' height, what they show
}

const ItemViewStyle& ListView::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.itemView;
    static const ItemViewStyle unstyled = Theme::dark().itemView;
    return unstyled;
}

void ListView::setModel(std::shared_ptr<ListModel> model) {
    if (model_)
        model_->removeObserver(*this);
    model_ = std::move(model);
    if (model_)
        model_->addObserver(*this);
    // texts: shown as they are, unless told otherwise; edited, set
    if (auto* texts = dynamic_cast<StringListModel*>(model_.get()); texts && !delegate_) {
        auto delegate = std::make_unique<TextDelegate>([texts](std::size_t i) { return texts->text(i); });
        delegate->setOnEdited([texts](std::size_t i, std::string text) { texts->set(i, LocalizedText(std::move(text))); });
        delegate_ = std::move(delegate);
    }
    modelReset();
}

void ListView::setDelegate(std::unique_ptr<ListDelegate> delegate) {
    delegate_ = std::move(delegate);
    // the rows were the other delegate's
    for (const Row& row : rows_)
        spare_.push_back(row.widget);
    rows_.clear();
    while (!spare_.empty()) {
        removeChild(*spare_.back());
        spare_.pop_back();
    }
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::setRowHeight(std::optional<float> height) {
    rowHeight_ = height;
    resetHeights();
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::setSelectionMode(SelectionMode mode) {
    if (mode == selectionMode_)
        return;
    // Multiple: the current item selected, as it was; else none kept
    selection_->clear();
    anchor_.reset();
    selectionMode_ = mode;
    if (mode == SelectionMode::Multiple && current_) {
        const std::size_t index = modelIndex(*current_);
        selection_->insert(index, index + 1);
        anchor_ = index;
    }
    invalidatePaint();
    selectionChanged();
}

void ListView::resetHeights() {
    const std::size_t count = shownCount();
    const float height = rowHeight_.value_or(style().rowHeight);
    // measured: estimated as the theme's row until then
    heights_->reset(count, height > 0 ? height : style().rowHeight, height > 0);
}

std::optional<std::size_t> ListView::current() const {
    return current_ ? std::optional<std::size_t>(modelIndex(*current_)) : std::nullopt;
}

bool ListView::isSelected(std::size_t index) const {
    if (selectionMode_ == SelectionMode::Multiple)
        return selection_->contains(index);
    return selectionMode_ == SelectionMode::Single && current_ && modelIndex(*current_) == index;
}

std::pair<double, double> ListView::selectionPill() const {
    const double top = current_ && *current_ < heights_->count() ? heights_->top(*current_) : 0;
    const double height = current_ && *current_ < heights_->count() ? heights_->height(*current_) : 0;
    if (!slide_.running())
        return {top, height};
    const double t = slide_.value();
    return {slideFrom_ + (top - slideFrom_) * t, slideFromHeight_ + (height - slideFromHeight_) * t};
}

bool ListView::selectedAt(std::size_t position) const {
    if (selectionMode_ == SelectionMode::Multiple)
        return selection_->contains(modelIndex(position));
    return selectionMode_ == SelectionMode::Single && current_ == position;
}

std::vector<std::size_t> ListView::selectedItems() const {
    std::vector<std::size_t> out;
    if (selectionMode_ == SelectionMode::Single) {
        if (const std::optional<std::size_t> index = current())
            out.push_back(*index);
    } else if (selectionMode_ == SelectionMode::Multiple) {
        out.reserve(selection_->count());
        for (const auto& [first, last] : selection_->ranges())
            for (std::size_t i = first; i < last; ++i)
                out.push_back(i);
    }
    return out;
}

std::size_t ListView::selectedCount() const {
    if (selectionMode_ == SelectionMode::Multiple)
        return selection_->count();
    return selectionMode_ == SelectionMode::Single && current_ ? 1 : 0;
}

void ListView::setSelected(std::size_t index, bool selected) {
    if (selectionMode_ != SelectionMode::Multiple || !model_ || index >= model_->count() || selection_->contains(index) == selected)
        return;
    if (selected)
        selection_->insert(index, index + 1);
    else
        selection_->erase(index, index + 1);
    selectionChanged();
}

void ListView::selectAll() {
    if (selectionMode_ != SelectionMode::Multiple || shownCount() == 0)
        return;
    selectBetween(0, shownCount() - 1, false);
    selectionChanged();
}

void ListView::clearSelection() {
    if (selection_->empty())
        return;
    selection_->clear();
    selectionChanged();
}

void ListView::setSelectedItems(const std::vector<std::size_t>& indices) {
    if (selectionMode_ != SelectionMode::Multiple)
        return;
    selection_->clear();
    const std::size_t count = model_ ? model_->count() : 0;
    for (std::size_t index : indices) {
        if (index < count)
            selection_->insert(index, index + 1);
    }
    selectionChanged();
}

void ListView::selectBetween(std::size_t a, std::size_t b, bool keep) {
    if (!keep)
        selection_->clear();
    const std::size_t from = std::min(a, b), to = std::max(a, b);
    if (!ordered()) {
        selection_->insert(from, to + 1);       // positions are the model's indices: a range
        return;
    }
    for (std::size_t at = from; at <= to; ++at) {
        const std::size_t index = modelIndex(at);
        selection_->insert(index, index + 1);
    }
}

void ListView::chose(std::size_t position, bool shift, bool control) {
    const std::size_t index = modelIndex(position);
    const std::optional<std::size_t> anchorAt = anchor_ ? this->position(*anchor_) : std::nullopt;
    if (shift) {
        // from the anchor (the current item without one) to it; with Ctrl, added to what was
        const std::size_t from = anchorAt.value_or(current_.value_or(position));
        if (!anchorAt)
            anchor_ = modelIndex(from);
        selectBetween(from, position, control);
    } else if (control) {
        if (selection_->contains(index))
            selection_->erase(index, index + 1);
        else
            selection_->insert(index, index + 1);
        anchor_ = index;
    } else {
        selection_->clear();
        selection_->insert(index, index + 1);
        anchor_ = index;
    }
    selectionChanged();
}

void ListView::selectionChanged() {
    invalidatePaint();
    if (onSelectionChanged_)
        onSelectionChanged_();
}

void ListView::setCurrent(std::optional<std::size_t> index) {
    const std::size_t count = model_ ? model_->count() : 0;
    if (index && *index >= count)
        index = count > 0 ? std::optional<std::size_t>(count - 1) : std::nullopt;
    setCurrentAt(index ? position(*index) : std::nullopt);
}

void ListView::setCurrentAt(std::optional<std::size_t> at) {
    const std::size_t count = shownCount();
    if (at && *at >= count)
        at = count > 0 ? std::optional<std::size_t>(count - 1) : std::nullopt;
    if (at)
        scrollToPosition(*at);
    if (at == current_)
        return;
    // the selection slides from where its pill shows to the row (a style's motion)
    const UiContext* ui = context();
    if (ui && style().slideSelection && selectionMode_ == SelectionMode::Single && current_ && at && *current_ < heights_->count()) {
        std::tie(slideFrom_, slideFromHeight_) = selectionPill();
        slide_.set(0);
        slide_.animateTo(scheduler(), 1, ui->theme.motion.normal, Easing::OutCubic);
    } else {
        slide_.set(1);
    }
    current_ = at;
    invalidatePaint();
    if (onCurrentChanged_)
        onCurrentChanged_(current());
}

void ListView::activate(std::size_t at) {
    if (onActivated_)
        onActivated_(modelIndex(at));
}

// ---- the header ----

void ListView::setHeader(LocalizedText title) {
    headerTitle_ = std::move(title);
    if (!headerLabel_) {
        headerLabel_ = static_cast<Label*>(&addChild(std::make_unique<Label>(Label::Props{.elide = true})));
        headerLabel_->setHitTestVisible(false);
    }
    headerLabel_->setText(headerTitle_);
    headerLabel_->setVisible(headerShown());
    invalidateLayout();
}

void ListView::setHeaderVisible(bool visible) {
    headerVisible_ = visible;
    if (headerLabel_)
        headerLabel_->setVisible(headerShown());
    invalidateLayout();
}

Rect ListView::childrenClip() const {
    // its rows' and its header's
    return headerShown() ? Rect{viewport_.x, headerRect_.y, viewport_.width, viewport_.bottom() - headerRect_.y} : viewport_;
}

// ---- sorting and filtering ----

std::size_t ListView::shownCount() const {
    if (!model_)
        return 0;
    return ordered() ? order_->shown() : model_->count();
}

std::size_t ListView::modelIndex(std::size_t at) const {
    return ordered() && at < order_->shown() ? order_->modelIndex(at) : at;
}

std::optional<std::size_t> ListView::position(std::size_t index) const {
    if (!ordered())
        return index;
    return order_->position(index);
}

void ListView::setSortOrder(std::optional<SortOrder> order, std::function<std::partial_ordering(std::size_t, std::size_t)> compare) {
    // its own header turning over (a style's)
    if (order != sortOrder_ && order && headerShown() && style().flipSorted && scheduler()) {
        if (const UiContext* ui = context()) {
            flipFrom_ = sortOrder_;
            flip_.set(0);
            flip_.animateTo(scheduler(), 1, Duration(ui->theme.motion.slow), Easing::Linear);
        }
    }
    sortOrder_ = order;
    compare_ = std::move(compare);
    reorder();
}

void ListView::setFilter(std::function<bool(std::size_t)> accepts) {
    filter_ = std::move(accepts);
    reorder();
}

void ListView::reorder() {
    // the current item kept (if it still shows), the offset where it was
    const std::optional<std::size_t> was = current();
    cancelEdit();
    endChange(false);
    smooth_.stop();
    order_->clear();
    if (ordered())
        buildOrder();
    resetHeights();
    offset_ = std::clamp(offset_, 0.0, maximumOffset());
    hovered_.reset();
    pressed_.reset();
    const std::optional<std::size_t> now = was ? position(*was) : std::nullopt;
    const bool told = now != current_ || (current_ && was != current());
    current_ = now;
    if (told && onCurrentChanged_)
        onCurrentChanged_(current());
    // the selection: what the filter leaves out, no longer
    if (filter_ && !selection_->empty()) {
        std::vector<std::size_t> hidden;
        for (const auto& [first, last] : selection_->ranges())
            for (std::size_t i = first; i < last; ++i)
                if (!position(i))
                    hidden.push_back(i);
        for (std::size_t i : hidden)
            selection_->erase(i, i + 1);
        if (!hidden.empty() && onSelectionChanged_)
            onSelectionChanged_();
    }
    rebindAll_ = true;
    invalidateLayout();
}

// ---- the order, item by item ----

void ListView::buildOrder() {
    const std::size_t count = model_ ? model_->count() : 0;
    if (!sortOrder_) {
        order_->build(count, filter_);
        return;
    }
    // the comparison as it is, without going through `before`: a million items sort in twenty million of them
    const bool ascending = *sortOrder_ == SortOrder::Ascending;
    if (compare_) {
        order_->build(count, filter_, [&](std::uint32_t a, std::uint32_t b) {
            const std::partial_ordering o = compare_(a, b);
            return ascending ? o < 0 : o > 0;
        });
    } else {
        order_->build(count, filter_, [&](std::uint32_t a, std::uint32_t b) {
            const std::partial_ordering o = model_->compare(a, b);
            return ascending ? o < 0 : o > 0;
        });
    }
}

bool ListView::before(std::size_t a, std::size_t b) const {
    if (sortOrder_) {
        const std::partial_ordering o = compare_ ? compare_(a, b) : model_->compare(a, b);
        if (o < 0)
            return *sortOrder_ == SortOrder::Ascending;
        if (o > 0)
            return *sortOrder_ == SortOrder::Descending;
    }
    return a < b;
}

ListView::Kept ListView::keep() const {
    Kept kept;
    kept.current = current();
    kept.currentAt = current_.value_or(0);
    if (!rows_.empty()) {
        const std::size_t first = heights_->indexAt(offset_);
        kept.top = modelIndex(first);
        kept.within = offset_ - heights_->top(first);
    }
    return kept;
}

void ListView::reordered(Kept kept, std::size_t from, std::size_t to, bool counted) {
    // the rows' places (uniform: nothing kept per row; measured: estimated again)
    if (counted || !heights_->uniform())
        resetHeights();
    // the item at the top stays where it was
    if (kept.top) {
        if (const std::optional<std::size_t> at = position(*kept.top))
            offset_ = heights_->top(*at) + kept.within;
    }
    offset_ = std::clamp(offset_, 0.0, maximumOffset());
    hovered_.reset();
    pressed_.reset();
    // the current item kept; gone, the one in its place
    std::optional<std::size_t> now = kept.current ? position(*kept.current) : std::nullopt;
    if ((kept.current || kept.lost) && !now && shownCount() > 0)
        now = std::min(kept.currentAt, shownCount() - 1);
    const bool told = kept.lost || kept.current != (now ? std::optional<std::size_t>(modelIndex(*now)) : std::nullopt);
    current_ = now;
    if (told && onCurrentChanged_)
        onCurrentChanged_(current());
    // what shows, bound again when the change reached it
    const bool reached = counted || rows_.empty() || (from <= rows_.back().index && to > rows_.front().index);
    if (reached)
        rebindAll_ = true;
    invalidateLayout();
}

// ---- editing ----

bool ListView::edit(std::size_t index) {
    const std::optional<std::size_t> at = position(index);
    return at && editAt(*at);
}

bool ListView::editAt(std::size_t at) {
    if (!model_ || !delegate_ || at >= shownCount())
        return false;
    const std::size_t index = modelIndex(at);
    if (!hasFlag(model_->flags(index), ItemFlags::Editable))
        return false;
    commitEdit();
    setCurrentAt(at);
    std::unique_ptr<Widget> editor = delegate_->makeEditor(index);
    if (!editor)
        return false;
    const std::optional<Rect> rect = std::optional<Rect>(itemRect(index));
    edited_ = at;
    editor_->begin(std::move(editor), rect.value_or(Rect{}), [this, index] {
        if (Widget* e = editor_->editor(); e && delegate_)
            delegate_->commitEdit(*e, index);
        edited_.reset();
    });
    placeEditor();
    return true;
}

bool ListView::editing() const {
    return editor_->active();
}

std::optional<std::size_t> ListView::editedItem() const {
    return edited_ ? std::optional<std::size_t>(modelIndex(*edited_)) : std::nullopt;
}

Widget* ListView::editor() const {
    return editor_->editor();
}

void ListView::commitEdit() {
    editor_->commit();
    edited_.reset();
}

void ListView::cancelEdit() {
    editor_->cancel();
    edited_.reset();
}

std::optional<Rect> ListView::editorRect() const {
    if (!edited_)
        return std::nullopt;
    const std::optional<Rect> row = itemRect(modelIndex(*edited_));
    if (!row)
        return std::nullopt;
    if (!editorArea_)
        return row;
    const Widget* widget = nullptr;
    for (const Row& r : rows_) {
        if (r.index == *edited_)
            widget = r.widget;
    }
    if (!widget)
        return row;
    const Rect in = editorArea_(*widget);
    return Rect{row->x + in.x, row->y + in.y, in.width, in.height};
}

void ListView::placeEditor() {
    if (editor_->active())
        editor_->place(editorRect());
}

// ---- scrolling ----

double ListView::maximumOffset() const {
    return std::max(0.0, heights_->total() - viewport_.height);
}

void ListView::setOffset(double offset) {
    smooth_.stop();
    show(offset);
}

double ListView::snapped(double offset) const {
    const double limit = maximumOffset();
    offset = std::clamp(offset, 0.0, limit);
    if (offset >= limit - 0.5 || heights_->count() == 0)
        return offset;
    const std::size_t at = heights_->indexAt(offset);
    const double top = heights_->top(at), bottom = top + heights_->height(at);
    return std::min(limit, offset - top < bottom - offset ? top : bottom);
}

void ListView::scrollSmoothly(double offset, bool aimed) {
    offset = snapped(offset);
    if (offset == smooth_.target(offset_))
        return;
    const UiContext* ui = context();
    smooth_.scrollTo(scheduler(), offset_, offset, ui ? Duration(ui->theme.motion.scroll) : Duration::zero(), aimed);
}

void ListView::show(double offset) {
    offset = std::clamp(offset, 0.0, maximumOffset());
    if (offset == offset_)
        return;
    offset_ = offset;
    layRows();          // the rows that show now: nothing else changes
    invalidatePaint();
}

void ListView::scrollTo(std::size_t index) {
    if (const std::optional<std::size_t> at = position(index))
        scrollToPosition(*at);
}

void ListView::scrollToPosition(std::size_t index) {
    if (index >= heights_->count() || viewport_.height <= 0)
        return;
    const double top = heights_->top(index), bottom = top + heights_->height(index);
    if (top < offset_)
        setOffset(top);
    else if (bottom > offset_ + viewport_.height)
        setOffset(bottom - viewport_.height);
}

std::pair<std::size_t, std::size_t> ListView::shownItems() const {
    if (rows_.empty())
        return {0, 0};
    return {rows_.front().index, rows_.back().index + 1};
}

Widget* ListView::rowWidget(std::size_t index) const {
    const std::optional<std::size_t> at = position(index);
    for (const Row& row : rows_) {
        if (at && row.index == *at)
            return row.widget;
    }
    return nullptr;
}

std::optional<std::size_t> ListView::itemAt(Point point) const {
    if (!viewport_.contains(point) || heights_->count() == 0)
        return std::nullopt;
    const double y = offset_ + (point.y - viewport_.y);
    if (y >= heights_->total())
        return std::nullopt;
    return modelIndex(heights_->indexAt(y));
}

std::optional<Rect> ListView::itemRect(std::size_t item) const {
    const std::optional<std::size_t> at = position(item);
    if (!at)
        return std::nullopt;
    const std::size_t index = *at;
    if (index >= heights_->count())
        return std::nullopt;
    const double y = heights_->top(index) - offset_ + shiftOf(index);
    const double h = heights_->height(index);
    if (y + h <= 0 || y >= viewport_.height)
        return std::nullopt;
    return Rect{viewport_.x, viewport_.y + static_cast<float>(y), viewport_.width, static_cast<float>(h)};
}

// ---- layout ----

Size ListView::measureContent(Size available) {
    // all the room there is; on its own, a few rows' worth
    const float h = static_cast<float>(std::min(heights_->total(), 320.0));
    return {std::isfinite(available.width) ? available.width : 240.0f, std::isfinite(available.height) ? available.height : h};
}

void ListView::arrangeContent(const Rect& content) {
    area_ = content;
    headerRect_ = {};
    if (headerShown()) {
        const float h = style().rowHeight;
        headerRect_ = {content.x, content.y, content.width, std::min(h, content.height)};
        area_ = {content.x, content.y + headerRect_.height, content.width, std::max(0.0f, content.height - headerRect_.height)};
        // its title after the rows' padding, the arrow's room at its end
        const bool rtl = rightToLeft();
        const Rect title{headerRect_.x + (rtl ? 28.0f : 12.0f), headerRect_.y, std::max(0.0f, headerRect_.width - 40), headerRect_.height};
        headerLabel_->measure({title.width, title.height});
        arrangeInCell(*headerLabel_, title, rtl);
    }
    const Rect rows = area_;
    const ScrollBarStyle* bar = context() ? &context()->theme.scrollBar : nullptr;
    const float barWidth = bar ? bar->width : 8.0f;
    viewport_ = rows;
    showBar_ = heights_->total() > rows.height + 0.5;
    if (showBar_) {
        viewport_.width = std::max(0.0f, viewport_.width - barWidth);
        if (rightToLeft())
            viewport_.x += barWidth;
    }
    offset_ = std::clamp(offset_, 0.0, maximumOffset());
    layRows();
}

void ListView::layRows() {
    std::vector<Row> before = std::move(rows_);
    rows_.clear();
    const std::size_t count = heights_->count();
    if (rebindAll_) {
        // what they show is out of date: all of them for others
        for (Row& row : before)
            spare_.push_back(std::exchange(row.widget, nullptr));
        rebindAll_ = false;
    }
    // a widget for row `i`: the one that showed it, or one put aside or made, bound to it
    const auto acquire = [&](std::size_t i) {
        Widget* widget = nullptr;
        for (Row& row : before) {
            if (row.widget && row.index == i) {
                widget = std::exchange(row.widget, nullptr);
                break;
            }
        }
        if (!widget) {
            if (!spare_.empty()) {
                widget = spare_.back();
                spare_.pop_back();
            } else {
                widget = &addChild(delegate_->makeRow());
            }
            delegate_->bindRow(*widget, modelIndex(i));
            if (!heights_->uniform())
                heights_->set(i, widget->measure({viewport_.width, unbounded}).height);
        }
        return widget;
    };
    // the rows from the one at the offset down, as long as they show; measured as they are bound, which may move the end
    // (and so the offset, which the row at it keeps, or the end, scrolled to it): chosen again then
    for (int pass = 0; pass < 4 && delegate_ && count > 0 && viewport_.height > 0; ++pass) {
        const bool atEnd = offset_ > 0 && offset_ >= maximumOffset() - 0.5;
        const std::size_t first = heights_->indexAt(offset_);
        const double within = offset_ - heights_->top(first);
        // those of before that will not show, for the others (as far as the heights known say)
        std::size_t expected = first;
        for (double y = -within; expected < count && y < viewport_.height; ++expected)
            y += heights_->height(expected);
        for (Row& row : rows_)
            before.push_back(row);
        rows_.clear();
        for (Row& row : before) {
            if (row.widget && (row.index < first || row.index >= expected))
                spare_.push_back(std::exchange(row.widget, nullptr));
        }
        double y = -within;
        for (std::size_t i = first; i < count && y < viewport_.height; ++i) {
            rows_.push_back({acquire(i), i});
            y += heights_->height(i);
        }
        if (heights_->uniform())
            break;
        const double anchored = atEnd ? maximumOffset() : std::clamp(heights_->top(first) + within, 0.0, maximumOffset());
        if (std::abs(anchored - offset_) < 0.01)
            break;
        offset_ = anchored;
    }
    // rows coming in: those after them show above their places, the rows there too
    if (change_ && change_->inserted && delegate_) {
        for (std::size_t i = change_->first + change_->count; i < count; ++i) {
            const double y = heights_->top(i) - offset_ + shiftOf(i);
            if (y >= viewport_.height)
                break;
            const bool shown = std::any_of(rows_.begin(), rows_.end(), [&](const Row& row) { return row.index == i; });
            if (!shown && y + heights_->height(i) > 0)
                rows_.push_back({acquire(i), i});
        }
        std::sort(rows_.begin(), rows_.end(), [](const Row& a, const Row& b) { return a.index < b.index; });
    }
    // those that showed and do not now: put aside, out of sight
    for (Row& row : before) {
        if (row.widget)
            spare_.push_back(row.widget);
    }
    for (Widget* widget : spare_)
        widget->arrange({viewport_.x, parkedAt, viewport_.width, 0});
    const bool rtl = rightToLeft();
    revealed_.clear();
    for (const Row& row : rows_) {
        const float top = viewport_.y + static_cast<float>(heights_->top(row.index) - offset_ + shiftOf(row.index));
        arrangeInCell(*row.widget, {viewport_.x, top, viewport_.width, static_cast<float>(heights_->height(row.index))}, rtl);
        if (change_ && change_->inserted && row.index >= change_->first && row.index < change_->first + change_->count)
            revealed_.push_back(row.widget);
    }
    placeEditor();
    // rows going: where they were, cut to what is left of them
    if (change_ && !change_->inserted) {
        const double top = heights_->top(std::min(change_->first, count)) - offset_;
        for (const auto& [widget, below] : change_->leaving) {
            const Size size = widget->size();
            widget->arrange({viewport_.x, viewport_.y + static_cast<float>(top + below), viewport_.width, size.height});
            revealed_.push_back(widget);
        }
    }
}

// ---- rows inserted and removed, eased ----

double ListView::gap() const {
    if (!change_)
        return 0;
    // no more than it takes to fill the view: a thousand rows come in as fast as ten
    const double height = std::min(change_->height, static_cast<double>(viewport_.height));
    const double shown = changeMotion_.value();
    return height * (change_->inserted ? shown : 1 - shown);
}

double ListView::shiftOf(std::size_t index) const {
    if (!change_)
        return 0;
    if (change_->inserted)
        return index >= change_->first + change_->count ? gap() - change_->height : 0;
    return index >= change_->first ? gap() : 0;
}

Rect ListView::gapRect() const {
    const std::size_t at = change_ ? std::min(change_->first, heights_->count()) : 0;
    const double top = (at < heights_->count() ? heights_->top(at) : heights_->total()) - offset_;
    return {viewport_.x, viewport_.y + static_cast<float>(top), viewport_.width, static_cast<float>(gap())};
}

std::optional<Rect> ListView::childClip(const Widget& child) const {
    if (&child == headerLabel_)
        return std::nullopt;
    if (change_ && std::find(revealed_.begin(), revealed_.end(), &child) != revealed_.end())
        return gapRect().intersected(viewport_);
    // under a header: the rows where they show, not over it
    return headerShown() ? std::optional<Rect>(viewport_) : std::nullopt;
}

void ListView::changeStepped() {
    if (!changeMotion_.running()) {
        endChange();
        return;
    }
    layRows();
    invalidatePaint();
}

void ListView::endChange(bool lay) {
    if (!change_)
        return;
    for (const auto& [widget, below] : change_->leaving)
        spare_.push_back(widget);
    change_.reset();
    revealed_.clear();
    changeMotion_.set(1);
    if (lay)
        layRows();
    invalidatePaint();
}

// ---- painting ----

void ListView::highlight(paint::Painter& p, const Rect& r, float radius, Color color, bool pressed) const {
    // a row's highlight: flat, or raised as a control (a style's)
    const ItemViewStyle& s = style();
    if (s.rowBevel.relief > 0)
        paintRaised(p, r, radius, color, {0, 0, 0, 0}, s.rowBevel, pressed);
    else
        p.fillRoundedRect(r, radius, color);
}

std::optional<paint::Lifted> ListView::headerFlipped() const {
    if (!flipFrom_ || !flip_.running())
        return std::nullopt;
    // over and back up (as a table's header: TableView::flipped)
    const float t = flip_.value();
    const float e = t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.0f) / 2;
    constexpr float quarter = 1.5707963f;
    const float angle = e < 0.5f ? quarter * e * 2 : -quarter * (1 - e) * 2;
    return paint::Lifted{.lift = headerRect_.height * 0.5f * std::abs(std::sin(angle)) + 1, .turn = paint::Quaternion::axisAngle({1, 0, 0}, angle)};
}

std::optional<paint::Lifted> ListView::childLifted(const Widget& child) const {
    return &child == headerLabel_ ? headerFlipped() : std::nullopt;
}

void ListView::paint(PaintContext& context) {
    // behind the rows: under the mouse, pressed, selected (a pill of the accent's tint)
    const ItemViewStyle& s = style();
    paint::Painter& p = context.painter;
    if (headerShown()) {
        // its header: a pill under the mouse, the arrow of the order, a line under it
        const Rect& h = headerRect_;
        // turning over (its title with it: childLifted), the arrow it had until the middle of the turn
        const std::optional<paint::Lifted> turning = headerFlipped();
        const paint::Painter::Mark mark = turning ? p.markHere() : paint::Painter::Mark{};
        const std::optional<SortOrder> shownOrder = turning && flip_.value() < 0.5f ? *flipFrom_ : sortOrder_;
        if (s.headerBackground.a > 0)
            paintRaised(p, {h.x, h.y, h.width, h.height - 1}, s.cornerRadius, s.headerBackground, {0, 0, 0, 0}, s.headerBevel);
        if (headerHovered_) {
            const VisualState state{.hovered = true, .pressed = headerPressed_};
            highlight(p, {h.x + s.inset, h.y + 2, h.width - 2 * s.inset, h.height - 4}, s.cornerRadius, s.row.resolve(state), state.pressed);
        }
        if (shownOrder) {
            const bool rtl = rightToLeft();
            const float ax = rtl ? h.x + 14 : h.right() - 14, ay = h.y + h.height / 2;
            const float d = *shownOrder == SortOrder::Ascending ? -1.0f : 1.0f;
            const Point arrow[] = {{ax - 4, ay - 2 * d}, {ax, ay + 2 * d}, {ax + 4, ay - 2 * d}};
            p.drawPolyline(arrow, 1.4f, s.headerText);
        }
        if (turning)
            p.place(mark, p.transform().map(h.center()), *turning);
        p.fillRect({h.x, h.bottom() - 1, h.width, 1}, s.headerLine);
        headerLabel_->setColor(s.headerText);
    }
    p.pushClip(viewport_);
    const bool grid = s.rowLines || s.columnLines;
    const bool sliding = slide_.running() && selectionMode_ == SelectionMode::Single && current_;
    if (sliding) {
        // the selection on its way from the row it was on
        const auto [top, height] = selectionPill();
        const Rect full{viewport_.x, viewport_.y + static_cast<float>(top - offset_), viewport_.width, static_cast<float>(height)};
        const Rect pill = grid ? full : Rect{full.x + s.inset, full.y + 1, full.width - 2 * s.inset, full.height - 2};
        highlight(p, pill, grid ? 0.0f : s.cornerRadius, s.selected, false);
    }
    for (const Row& row : rows_) {
        const std::optional<Rect> rect = itemRect(modelIndex(row.index));
        if (!rect)
            continue;
        // a row coming in: cut as it is
        const bool cut = change_ && change_->inserted && row.index >= change_->first && row.index < change_->first + change_->count;
        if (cut)
            p.pushClip(gapRect());
        struct Uncut {
            paint::Painter& p;
            bool cut;
            ~Uncut() {
                if (cut)
                    p.popClip();
            }
        } uncut{p, cut};
        // a pill; with the cells' borders, all of its cell
        const Rect r = grid ? *rect : Rect{rect->x + s.inset, rect->y + 1, rect->width - 2 * s.inset, rect->height - 2};
        const float radius = grid ? 0.0f : s.cornerRadius;
        const VisualState state{.hovered = hovered_ == row.index, .pressed = pressed_ == row.index && hovered_ == row.index};
        const bool selected = selectedAt(row.index);
        if (selected && !sliding)
            highlight(p, r, radius, s.selected, false);
        else if (state.hovered && !selected)
            highlight(p, r, radius, s.row.resolve(state), state.pressed);
        if (rippled_ == row.index)
            ripple_.paint(p, r, radius, s.ripple);
        // the current row ringed, unless the selection already shows it
        if (hasFocus() && current_ == row.index && !selected)
            p.strokeRoundedRect(grid ? r.inflated(-1) : r, radius, 1.5f, s.focusRing);
        // the line under it
        if (s.rowLines)
            p.fillRect({rect->x, rect->bottom() - s.gridLineWidth, rect->width, s.gridLineWidth}, s.gridLine);
    }
    p.popClip();
}

void ListView::placeBar() {
    const UiContext* ui = context();
    if (!showBar_ || !ui) {
        bar_.hide();
        return;
    }
    const ScrollBarStyle& s = ui->theme.scrollBar;
    const bool rtl = rightToLeft();
    // as long as its rows show: rows coming in (a node opened) or going out (closed) eased with them, so that the thumb
    // shrinks and grows as they do rather than at once
    double total = heights_->total();
    if (change_) {
        const double still = change_->height * (1.0 - changeMotion_.value());
        total += change_->inserted ? -still : still;
    }
    bar_.place({rtl ? area_.x : area_.right() - s.width, area_.y, s.width, area_.height}, std::max(total, 0.0), viewport_.height, offset_,
               smooth_.target(offset_), rtl, s.minimumThumb);
}

void ListView::paintOver(PaintContext& context) {
    placeBar();
    bar_.paint(context, context.ui.theme.scrollBar);
}

// ---- events ----

void ListView::onMouse(MouseEvent& event) {
    const bool left = event.button == platform::MouseButton::Left;
    switch (event.type) {
    case MouseEventType::Wheel: {
        // a step from where it is going (further as the wheel turns faster), snapped to rows; a touchpad's fractions as
        // they come, not snapped
        const WheelScroll w = wheelScroll(smooth_, scheduler(), offset_, -event.wheelDelta.y * wheelRows * style().rowHeight,
                                          event.wheelPrecise, maximumOffset());
        if (w.moved && w.aimed) {
            scrollSmoothly(w.target, true);
        } else if (w.moved) {
            const UiContext* ui = context();
            smooth_.scrollTo(scheduler(), offset_, w.target, ui ? Duration(ui->theme.motion.scroll) : Duration::zero());
        }
        event.handled = w.moved;
        break;
    }
    case MouseEventType::Move: {
        if (const bool over = headerShown() && headerRect_.contains(event.position); over != headerHovered_) {
            headerHovered_ = over;
            invalidatePaint();
        }
        placeBar();
        if (bar_.mouse(event)) {        // its thumb dragged
            event.handled = true;
            return;
        }
        const std::optional<std::size_t> item = bar_.hovered() ? std::nullopt : itemAt(event.position);
        const std::optional<std::size_t> over = item ? position(*item) : std::nullopt;
        if (over != hovered_) {
            hovered_ = over;
            invalidatePaint();
        }
        break;
    }
    case MouseEventType::Press: {
        if (!left)
            return;
        event.handled = true;
        focus();
        if (headerShown() && headerRect_.contains(event.position)) {
            headerPressed_ = true;
            invalidatePaint();
            return;
        }
        placeBar();
        if (bar_.mouse(event))          // its thumb pressed, or its track: a page
            return;
        const std::optional<std::size_t> item = itemAt(event.position);
        const std::optional<std::size_t> at = item ? position(*item) : std::nullopt;
        pressed_ = at;
        if (at && style().ripple.a > 0 && context()) {
            rippled_ = at;
            ripple_.start(scheduler(), event.position, context()->theme.motion.ripple);
        }
        if (at) {
            if (selectionMode_ == SelectionMode::Multiple && event.clicks == 1) {
                chose(*at, platform::hasAny(event.modifiers, platform::KeyModifiers::Shift),
                      platform::hasAny(event.modifiers, platform::shortcutModifier));
            }
            setCurrentAt(at);
            // twice: edited (if it can be), activated otherwise
            if (event.clicks == 2 && !(hasTrigger(editTriggers_, EditTriggers::DoubleClick) && editAt(*at)))
                activate(*at);
        }
        invalidatePaint();
        break;
    }
    case MouseEventType::Release:
        if (!left)
            return;
        if (std::exchange(headerPressed_, false)) {
            event.handled = true;
            invalidatePaint();
            // ascending, then descending, then not: as it compares (its own comparison kept)
            if (sortingEnabled_ && headerRect_.contains(event.position)) {
                if (!sortOrder_)
                    setSortOrder(SortOrder::Ascending, compare_);
                else if (*sortOrder_ == SortOrder::Ascending)
                    setSortOrder(SortOrder::Descending, compare_);
                else
                    setSortOrder(std::nullopt, compare_);
            }
            return;
        }
        if (bar_.mouse(event)) {
            // its thumb let go: the row at the top whole
            event.handled = true;
            scrollSmoothly(offset_);
        }
        if (pressed_)
            event.handled = true;
        pressed_.reset();
        invalidatePaint();
        break;
    }
}

void ListView::onPreviewKey(KeyEvent& event) {
    // its editor's: Enter commits it, Escape cancels it
    if (editor_->active() && editor_->key(event))
        event.handled = true;
}

void ListView::onText(TextEvent& event) {
    // typed on the current item: edited, what was typed into its editor (it has the focus now)
    if (event.composing || event.text.empty() || editor_->active() || !current_ || !hasTrigger(editTriggers_, EditTriggers::Typing))
        return;
    if (static_cast<unsigned char>(event.text[0]) < 0x20 || !editAt(*current_))
        return;
    event.handled = true;
    if (UiRoot* r = root())
        r->dispatch(platform::TextInput{0, event.text});
}

void ListView::onKey(KeyEvent& event) {
    if (!event.pressed || !model_ || shownCount() == 0)
        return;
    if (event.key == platform::Key::F2 && current_ && hasTrigger(editTriggers_, EditTriggers::F2)) {
        if (editAt(*current_))
            event.handled = true;
        return;
    }
    const std::size_t count = shownCount();
    const std::size_t at = current_.value_or(0);
    const bool control = platform::hasAny(event.modifiers, platform::shortcutModifier);     // Cmd on macOS
    const bool shift = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
    if (selectionMode_ == SelectionMode::Multiple && control && !shift) {
        // all; the current one taken or left
        if (event.key == platform::Key::A) {
            selectAll();
            event.handled = true;
            return;
        }
        if (event.key == platform::Key::Space && current_) {
            chose(*current_, false, true);
            event.handled = true;
            return;
        }
    }
    // a page: the rows that show, less one
    const std::size_t page = std::max<std::size_t>(1, rows_.size() > 1 ? rows_.size() - 1 : 1);
    std::optional<std::size_t> to;
    switch (event.key) {
    case platform::Key::Up: to = current_ ? (at > 0 ? at - 1 : 0) : 0; break;
    case platform::Key::Down: to = current_ ? std::min(at + 1, count - 1) : 0; break;
    case platform::Key::PageUp: to = at > page ? at - page : 0; break;
    case platform::Key::PageDown: to = std::min(at + page, count - 1); break;
    case platform::Key::Home: to = 0; break;
    case platform::Key::End: to = count - 1; break;
    case platform::Key::Enter:
        if (current_) {
            activate(*current_);
            event.handled = true;
        }
        return;
    default: return;
    }
    event.handled = true;
    // Multiple: it alone selected, or from the anchor with Shift; with Ctrl only, the current item moves
    if (selectionMode_ == SelectionMode::Multiple && to && !(control && !shift))
        chose(*to, shift, false);
    setCurrentAt(to);
}

void ListView::onHoverChanged(bool hovered) {
    if (hovered)
        return;
    bar_.leave();
    if (hovered_ || headerHovered_) {
        hovered_.reset();
        headerHovered_ = false;
        invalidatePaint();
    }
}

void ListView::onFocusChanged(bool) {
    invalidatePaint();
}

void ListView::onRootChanged() {
    onStyleChanged();
}

void ListView::onStyleChanged() {
    if (!rowHeight_)
        resetHeights();         // the theme's row height
    rebindAll_ = true;
    invalidateLayout();
}

// ---- the model's changes ----

void ListView::changesBegun() {
    if (batch_++ > 0 || !ordered())
        return;
    // what shows is kept by the items' handles, which the changes do not move
    cancelEdit();
    endChange(false);
    smooth_.stop();
    const Kept kept = keep();
    held_ = {};
    if (kept.current)
        held_.current = order_->handle(*kept.current);
    held_.currentAt = kept.currentAt;
    if (kept.top)
        held_.top = order_->handle(*kept.top);
    held_.within = kept.within;
    order_->hold();
}

void ListView::changesEnded() {
    if (batch_ == 0 || --batch_ > 0 || !order_->holding())
        return;
    // what it held, by the model's indices now (settling may make the handles again)
    Kept kept;
    kept.currentAt = held_.currentAt;
    if (held_.current) {
        kept.current = order_->indexOfHandle(*held_.current);
        kept.lost = !kept.current;
    }
    if (held_.top)
        kept.top = order_->indexOfHandle(*held_.top);
    kept.within = held_.within;
    // each item inserted or changed placed once, where it sorts now
    const ItemOrder::Span span = order_->settle(filter_, [this](std::size_t a, std::size_t b) { return before(a, b); });
    reordered(kept, span.from, span.to, true);
}

void ListView::itemsInserted(std::size_t first, std::size_t count) {
    selection_->inserted(first, count);
    if (anchor_ && *anchor_ >= first)
        *anchor_ += count;
    if (ordered() && order_->holding()) {      // in a batch: placed at its end
        order_->inserted(first, count, filter_, {});
        return;
    }
    if (ordered()) {
        // the items after them move on; those accepted go where they sort
        cancelEdit();
        endChange(false);
        smooth_.stop();
        Kept kept = keep();
        for (std::optional<std::size_t>* index : {&kept.current, &kept.top}) {
            if (*index && **index >= first)
                **index += count;
        }
        ItemOrder::Span span;
        if (count > 4096 && count > order_->shown() / 4) {
            // many: sorted again, all of it
            buildOrder();
            span = {0, order_->shown(), true};
        } else {
            span = order_->inserted(first, count, filter_, [this](std::size_t a, std::size_t b) { return before(a, b); });
        }
        reordered(kept, span.from, span.to, span.counted);
        return;
    }
    cancelEdit();
    // what shows stays where it is: items before it push the offset down as much as they take
    const bool before = !rows_.empty() && first <= rows_.front().index;
    smooth_.stop();         // what shows is kept in place: not where a scroll was going
    endChange(false);
    // among the rows that show: they come out from under the row before them
    const bool among = animatesChanges_ && !rows_.empty() && first > rows_.front().index && first <= rows_.back().index + 1;
    heights_->insert(first, count);
    if (among && scheduler() && scheduler()->animationsEnabled() && count > 0 && context() && context()->theme.motion.normal.count() > 0) {
        change_ = Change{first, count, heights_->top(first + count) - heights_->top(first), true, {}};
        changeMotion_.set(0);
        changeMotion_.animateTo(scheduler(), 1, context() ? Duration(context()->theme.motion.normal) : Duration::zero(), Easing::OutCubic);
    }
    if (before)
        offset_ += heights_->top(first + count) - heights_->top(first);
    if (current_ && *current_ >= first)
        *current_ += count;
    hovered_.reset();
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::itemsRemoved(std::size_t first, std::size_t count) {
    if (const std::size_t had = selection_->count(); had > 0) {
        selection_->removed(first, count);
        if (selection_->count() != had && onSelectionChanged_)
            onSelectionChanged_();
    }
    if (anchor_ && *anchor_ >= first + count)
        *anchor_ -= count;
    else if (anchor_ && *anchor_ >= first)
        anchor_.reset();
    if (ordered() && order_->holding()) {
        order_->removed(first, count);
        return;
    }
    if (ordered()) {
        // those that showed go; the items after them move back
        cancelEdit();
        endChange(false);
        smooth_.stop();
        Kept kept = keep();
        for (std::optional<std::size_t>* index : {&kept.current, &kept.top}) {
            if (*index && **index >= first + count) {
                **index -= count;
            } else if (*index && **index >= first) {
                kept.lost = kept.lost || index == &kept.current;
                index->reset();
            }
        }
        const ItemOrder::Span span = order_->removed(first, count);
        reordered(kept, span.from, span.to, span.counted);
        return;
    }
    cancelEdit();
    const bool before = !rows_.empty() && first < rows_.front().index;
    const double removed = heights_->top(first + count) - heights_->top(first);
    smooth_.stop();
    endChange(false);
    // among the rows that show: they go back under the row before them, showing what they showed meanwhile
    if (animatesChanges_ && !rows_.empty() && first > rows_.front().index && first <= rows_.back().index && count > 0 &&
        scheduler() && scheduler()->animationsEnabled() && context() && context()->theme.motion.normal.count() > 0) {
        Change change{first, count, removed, false, {}};
        std::erase_if(rows_, [&](Row& row) {
            if (row.index < first || row.index >= first + count)
                return false;
            change.leaving.emplace_back(row.widget, heights_->top(row.index) - heights_->top(first));
            return true;
        });
        change_ = std::move(change);
        changeMotion_.set(0);
        changeMotion_.animateTo(scheduler(), 1, context() ? Duration(context()->theme.motion.normal) : Duration::zero(), Easing::OutCubic);
    }
    heights_->remove(first, count);
    if (before)
        offset_ = std::max(0.0, offset_ - removed);
    if (current_) {
        const std::size_t left = heights_->count();
        if (*current_ >= first + count)
            *current_ -= count;
        else if (*current_ >= first)        // it went: the one in its place now, if there is one
            current_ = left == 0 ? std::nullopt : std::optional<std::size_t>(std::min(first, left - 1));
    }
    hovered_.reset();
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::itemsChanged(std::size_t first, std::size_t count) {
    if (ordered() && order_->holding()) {
        order_->changed(first, count, filter_, {});
        return;
    }
    if (ordered()) {
        // many at once: all of it again
        if (count > std::max<std::size_t>(4096, order_->shown() / 4)) {
            reorder();
            return;
        }
        // each: where it is when it still sorts there, elsewhere if not, in or out as the filter says now
        cancelEdit();
        endChange(false);
        const Kept kept = keep();
        const ItemOrder::Span span = order_->changed(first, count, filter_, [this](std::size_t a, std::size_t b) { return before(a, b); });
        reordered(kept, span.from, span.to, span.counted);
        return;
    }
    // those that show, shown again (measured again)
    bool shows = false;
    for (const Row& row : rows_)
        shows = shows || (row.index >= first && row.index < first + count);
    if (!shows)
        return;
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::modelReset() {
    cancelEdit();
    endChange(false);
    smooth_.stop();
    offset_ = 0;
    current_.reset();
    anchor_.reset();
    const bool had = !selection_->empty();
    selection_->clear();
    reorder();
    if (had && onSelectionChanged_)
        onSelectionChanged_();
}

} // namespace glw
