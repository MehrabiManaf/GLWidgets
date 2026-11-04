// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ListView.hpp>

#include "Widgets/ItemEditor.hpp"
#include "Widgets/RowHeights.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace glw {

namespace {

constexpr float wheelRows = 3;          // rows a step of the wheel scrolls
constexpr float parkedAt = -100000;     // where a row widget showing nothing waits: out of sight, out of the mouse's way
constexpr std::size_t hidden = static_cast<std::size_t>(-1);      // a position: filtered out

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

ListView::ListView() : heights_(std::make_unique<RowHeights>()), editor_(std::make_unique<ItemEditor>(*this)) {
    setFocusable(true);
    setClipsChildren(true);
}

ListView::~ListView() {
    if (model_)
        model_->removeObserver(*this);
}

const ItemViewStyle& ListView::style() const {
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
    selectionMode_ = mode;
    invalidatePaint();
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
    return selectionMode_ == SelectionMode::Single && current_ && modelIndex(*current_) == index;
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
    current_ = at;
    invalidatePaint();
    if (onCurrentChanged_)
        onCurrentChanged_(current());
}

void ListView::activate(std::size_t at) {
    if (onActivated_)
        onActivated_(modelIndex(at));
}

// ---- sorting and filtering ----

std::size_t ListView::shownCount() const {
    if (!model_)
        return 0;
    return ordered() ? order_.size() : model_->count();
}

std::size_t ListView::modelIndex(std::size_t at) const {
    return ordered() && at < order_.size() ? order_[at] : at;
}

std::optional<std::size_t> ListView::position(std::size_t index) const {
    if (!ordered())
        return index;
    if (index >= positions_.size() || positions_[index] == hidden)
        return std::nullopt;
    return positions_[index];
}

void ListView::setSortOrder(std::optional<SortOrder> order, std::function<std::partial_ordering(std::size_t, std::size_t)> compare) {
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
    order_.clear();
    positions_.clear();
    const std::size_t count = model_ ? model_->count() : 0;
    if (ordered()) {
        order_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            if (!filter_ || filter_(i))
                order_.push_back(i);
        }
        if (sortOrder_) {
            const auto compare = compare_ ? compare_ : [this](std::size_t a, std::size_t b) { return model_->compare(a, b); };
            // equals in the model's order (a merge sort: fewer of the model's comparisons than a quick sort)
            const bool descending = *sortOrder_ == SortOrder::Descending;
            std::stable_sort(order_.begin(), order_.end(), [&](std::size_t a, std::size_t b) {
                const std::partial_ordering o = compare(a, b);
                return descending ? o > 0 : o < 0;
            });
        }
        positions_.assign(count, hidden);
        for (std::size_t at = 0; at < order_.size(); ++at)
            positions_[order_[at]] = at;
        order_.shrink_to_fit();
    }
    resetHeights();
    offset_ = std::clamp(offset_, 0.0, maximumOffset());
    hovered_.reset();
    pressed_.reset();
    const std::optional<std::size_t> now = was ? position(*was) : std::nullopt;
    const bool told = now != current_ || (current_ && was != current());
    current_ = now;
    if (told && onCurrentChanged_)
        onCurrentChanged_(current());
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

void ListView::scrollSmoothly(double offset) {
    offset = snapped(offset);
    if (offset == smooth_.target(offset_))
        return;
    const UiContext* ui = context();
    smooth_.scrollTo(scheduler(), offset_, offset, ui ? Duration(ui->theme.motion.scroll) : Duration::zero());
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
    const ScrollBarStyle* bar = context() ? &context()->theme.scrollBar : nullptr;
    const float barWidth = bar ? bar->width : 8.0f;
    viewport_ = content;
    showBar_ = heights_->total() > content.height + 0.5;
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
    if (!change_ || std::find(revealed_.begin(), revealed_.end(), &child) == revealed_.end())
        return std::nullopt;
    return gapRect();
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

void ListView::paint(PaintContext& context) {
    // behind the rows: under the mouse, pressed, selected (a pill of the accent's tint)
    const ItemViewStyle& s = style();
    paint::Painter& p = context.painter;
    p.pushClip(viewport_);
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
        const Rect r{rect->x + s.inset, rect->y + 1, rect->width - 2 * s.inset, rect->height - 2};
        const VisualState state{.hovered = hovered_ == row.index, .pressed = pressed_ == row.index && hovered_ == row.index};
        const bool selected = selectionMode_ == SelectionMode::Single && current_ == row.index;
        if (selected)
            p.fillRoundedRect(r, s.cornerRadius, s.selected);
        else if (state.hovered)
            p.fillRoundedRect(r, s.cornerRadius, s.row.resolve(state));
        // the current row ringed, unless the selection already shows it
        if (hasFocus() && current_ == row.index && !selected)
            p.strokeRoundedRect(r, s.cornerRadius, 1.5f, s.focusRing);
    }
    p.popClip();
}

Rect ListView::scrollTrack() const {
    const float width = context() ? context()->theme.scrollBar.width : 8.0f;
    return {rightToLeft() ? area_.x : area_.right() - width, area_.y, width, area_.height};
}

Rect ListView::scrollThumb() const {
    const Rect track = scrollTrack();
    const double total = std::max(1.0, heights_->total());
    const float minimum = context() ? context()->theme.scrollBar.minimumThumb : 24.0f;
    const float length = std::min(track.height, std::max(minimum, static_cast<float>(track.height * viewport_.height / total)));
    const double room = maximumOffset();
    const float at = room > 0 ? static_cast<float>((track.height - length) * offset_ / room) : 0.0f;
    return {track.x, track.y + at, track.width, length};
}

void ListView::paintOver(PaintContext& context) {
    if (!showBar_)
        return;
    const ScrollBarStyle& s = context.ui.theme.scrollBar;
    context.painter.fillRect(scrollTrack(), s.track);
    const VisualState state{.hovered = barHovered_ || thumbGrab_, .pressed = thumbGrab_.has_value()};
    barWide_.animateTo(scheduler(), state.hovered ? 1.0f : 0.0f, context.ui.theme.motion.fast);
    paintScrollThumb(context.painter, scrollThumb(), true, !rightToLeft(), barWide_.value(), s.thumb.resolve(state));
}

// ---- events ----

void ListView::onMouse(MouseEvent& event) {
    const bool left = event.button == platform::MouseButton::Left;
    switch (event.type) {
    case MouseEventType::Wheel: {
        // from where it is going: turned again, it goes further
        const double before = smooth_.target(offset_);
        scrollSmoothly(before + smooth_.boosted(-event.wheelDelta.y * wheelRows * style().rowHeight));
        event.handled = smooth_.target(offset_) != before;
        break;
    }
    case MouseEventType::Move: {
        if (thumbGrab_) {
            // the thumb follows the mouse; the offset, the thumb
            const Rect track = scrollTrack();
            const float length = scrollThumb().height;
            const double room = std::max(1.0f, track.height - length);
            setOffset((event.position.y - *thumbGrab_ - track.y) / room * maximumOffset());
            event.handled = true;
            return;
        }
        const bool overBar = showBar_ && scrollTrack().contains(event.position);
        const std::optional<std::size_t> item = overBar ? std::nullopt : itemAt(event.position);
        const std::optional<std::size_t> over = item ? position(*item) : std::nullopt;
        if (over != hovered_ || overBar != barHovered_) {
            hovered_ = over;
            barHovered_ = overBar;
            invalidatePaint();
        }
        break;
    }
    case MouseEventType::Press: {
        if (!left)
            return;
        event.handled = true;
        focus();
        if (showBar_ && scrollTrack().contains(event.position)) {
            const Rect thumb = scrollThumb();
            if (thumb.contains(event.position))
                thumbGrab_ = event.position.y - thumb.y;
            else        // a page toward where it was pressed
                scrollSmoothly(smooth_.target(offset_) + (event.position.y < thumb.y ? -1.0 : 1.0) * viewport_.height);
            invalidatePaint();
            return;
        }
        const std::optional<std::size_t> item = itemAt(event.position);
        const std::optional<std::size_t> at = item ? position(*item) : std::nullopt;
        pressed_ = at;
        if (at) {
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
        if (thumbGrab_ || pressed_)
            event.handled = true;
        if (thumbGrab_)
            scrollSmoothly(offset_);    // the rows at the top whole
        thumbGrab_.reset();
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
    setCurrentAt(to);
}

void ListView::onHoverChanged(bool hovered) {
    if (!hovered && (hovered_ || barHovered_)) {
        hovered_.reset();
        barHovered_ = false;
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

void ListView::itemsInserted(std::size_t first, std::size_t count) {
    if (ordered()) {        // where they go is the order's: sorted and filtered again
        reorder();
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
    if (among && scheduler() && scheduler()->animationsEnabled() && count > 0) {
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
    if (ordered()) {
        reorder();
        return;
    }
    cancelEdit();
    const bool before = !rows_.empty() && first < rows_.front().index;
    const double removed = heights_->top(first + count) - heights_->top(first);
    smooth_.stop();
    endChange(false);
    // among the rows that show: they go back under the row before them, showing what they showed meanwhile
    if (animatesChanges_ && !rows_.empty() && first > rows_.front().index && first <= rows_.back().index && count > 0 &&
        scheduler() && scheduler()->animationsEnabled()) {
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
    if (ordered()) {        // they may go elsewhere, or not show
        reorder();
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
    reorder();
}

} // namespace glw
