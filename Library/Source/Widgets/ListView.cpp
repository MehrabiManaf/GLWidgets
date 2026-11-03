// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ListView.hpp>

#include "Widgets/RowHeights.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

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

// ---- the view ----

ListView::ListView() : heights_(std::make_unique<RowHeights>()) {
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
    // texts: shown as they are, unless told otherwise
    if (auto* texts = dynamic_cast<StringListModel*>(model_.get()); texts && !delegate_)
        delegate_ = std::make_unique<TextDelegate>([texts](std::size_t i) { return texts->text(i); });
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
    const std::size_t count = model_ ? model_->count() : 0;
    const float height = rowHeight_.value_or(style().rowHeight);
    // measured: estimated as the theme's row until then
    heights_->reset(count, height > 0 ? height : style().rowHeight, height > 0);
}

void ListView::setCurrent(std::optional<std::size_t> index) {
    const std::size_t count = model_ ? model_->count() : 0;
    if (index && *index >= count)
        index = count > 0 ? std::optional<std::size_t>(count - 1) : std::nullopt;
    if (index)
        scrollTo(*index);
    if (index == current_)
        return;
    current_ = index;
    invalidatePaint();
    if (onCurrentChanged_)
        onCurrentChanged_(current_);
}

void ListView::activate(std::size_t index) {
    if (onActivated_)
        onActivated_(index);
}

// ---- scrolling ----

double ListView::maximumOffset() const {
    return std::max(0.0, heights_->total() - viewport_.height);
}

void ListView::setOffset(double offset) {
    offset = std::clamp(offset, 0.0, maximumOffset());
    if (offset == offset_)
        return;
    offset_ = offset;
    layRows();          // the rows that show now: nothing else changes
    invalidatePaint();
}

void ListView::scrollTo(std::size_t index) {
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
    for (const Row& row : rows_) {
        if (row.index == index)
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
    return heights_->indexAt(y);
}

std::optional<Rect> ListView::itemRect(std::size_t index) const {
    if (index >= heights_->count())
        return std::nullopt;
    const double y = heights_->top(index) - offset_;
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
                delegate_->bindRow(*widget, i);
                if (!heights_->uniform())
                    heights_->set(i, widget->measure({viewport_.width, unbounded}).height);
            }
            rows_.push_back({widget, i});
            y += heights_->height(i);
        }
        if (heights_->uniform())
            break;
        const double anchored = atEnd ? maximumOffset() : std::clamp(heights_->top(first) + within, 0.0, maximumOffset());
        if (std::abs(anchored - offset_) < 0.01)
            break;
        offset_ = anchored;
    }
    // those that showed and do not now: put aside, out of sight
    for (Row& row : before) {
        if (row.widget)
            spare_.push_back(row.widget);
    }
    for (Widget* widget : spare_)
        widget->arrange({viewport_.x, parkedAt, viewport_.width, 0});
    const bool rtl = rightToLeft();
    for (const Row& row : rows_) {
        const float top = viewport_.y + static_cast<float>(heights_->top(row.index) - offset_);
        arrangeInCell(*row.widget, {viewport_.x, top, viewport_.width, static_cast<float>(heights_->height(row.index))}, rtl);
    }
}

// ---- painting ----

void ListView::paint(PaintContext& context) {
    // behind the rows: under the mouse, pressed, selected (a soft background, a short bar at its start)
    const ItemViewStyle& s = style();
    paint::Painter& p = context.painter;
    const bool rtl = rightToLeft();
    p.pushClip(viewport_);
    for (const Row& row : rows_) {
        const std::optional<Rect> rect = itemRect(row.index);
        if (!rect)
            continue;
        const Rect r{rect->x + s.inset, rect->y + 1, rect->width - 2 * s.inset, rect->height - 2};
        const VisualState state{.hovered = hovered_ == row.index, .pressed = pressed_ == row.index && hovered_ == row.index};
        if (isSelected(row.index)) {
            p.fillRoundedRect(r, s.cornerRadius, s.selected);
            const float bar = std::min(16.0f, r.height - 8);
            p.fillRoundedRect({rtl ? r.right() - 4 : r.x + 1, r.center().y - bar / 2, 3, bar}, 1.5f, s.indicator);
        } else if (state.hovered) {
            p.fillRoundedRect(r, s.cornerRadius, s.row.resolve(state));
        }
        if (hasFocus() && current_ == row.index)
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
    const Rect thumb = scrollThumb().inflated(-2);
    const VisualState state{.hovered = barHovered_ || thumbGrab_, .pressed = thumbGrab_.has_value()};
    context.painter.fillRoundedRect(thumb, std::min(thumb.width, thumb.height) / 2, s.thumb.resolve(state));
}

// ---- events ----

void ListView::onMouse(MouseEvent& event) {
    const bool left = event.button == platform::MouseButton::Left;
    switch (event.type) {
    case MouseEventType::Wheel: {
        const double before = offset_;
        setOffset(offset_ - event.wheelDelta.y * wheelRows * style().rowHeight);
        event.handled = offset_ != before;
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
        const std::optional<std::size_t> over = overBar ? std::nullopt : itemAt(event.position);
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
                setOffset(offset_ + (event.position.y < thumb.y ? -1.0 : 1.0) * viewport_.height);
            invalidatePaint();
            return;
        }
        const std::optional<std::size_t> item = itemAt(event.position);
        pressed_ = item;
        if (item) {
            setCurrent(item);
            if (event.clicks == 2)
                activate(*item);
        }
        invalidatePaint();
        break;
    }
    case MouseEventType::Release:
        if (!left)
            return;
        if (thumbGrab_ || pressed_)
            event.handled = true;
        thumbGrab_.reset();
        pressed_.reset();
        invalidatePaint();
        break;
    }
}

void ListView::onKey(KeyEvent& event) {
    if (!event.pressed || !model_ || model_->count() == 0)
        return;
    const std::size_t count = model_->count();
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
    setCurrent(to);
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
    // what shows stays where it is: items before it push the offset down as much as they take
    const bool before = !rows_.empty() && first <= rows_.front().index;
    heights_->insert(first, count);
    if (before)
        offset_ += heights_->top(first + count) - heights_->top(first);
    if (current_ && *current_ >= first)
        *current_ += count;
    hovered_.reset();
    rebindAll_ = true;
    invalidateLayout();
}

void ListView::itemsRemoved(std::size_t first, std::size_t count) {
    const bool before = !rows_.empty() && first < rows_.front().index;
    const double removed = heights_->top(first + count) - heights_->top(first);
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
    resetHeights();
    offset_ = 0;
    current_.reset();
    hovered_.reset();
    rebindAll_ = true;
    invalidateLayout();
}

} // namespace glw
