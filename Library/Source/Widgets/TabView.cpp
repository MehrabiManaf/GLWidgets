// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/TabView.hpp>

#include "Widgets/TabPaint.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float dragThreshold = 6;      // dips the mouse moves with a tab before it is dragged
constexpr float wheelStep = 40;         // dips the tabs scroll by a step of the wheel
constexpr float closeRoom = 28;         // at a tab's end side, for its ×
constexpr float edge = 1;               // the stack's edge, around its page

// A tab: its title, drawn as the bar says (shown, under the mouse, pressed). The bar has the mouse for all of them.
class TabFace final : public Widget {
public:
    TabFace(const TabView& view, const LocalizedText& title, bool closable) : view_(view), closable_(closable) {
        setHitTestVisible(false);
        setClipsChildren(true);
        setPadding({10, 0, closable ? closeRoom : 10.0f, 0});
        setMinimumSize({view.style().tabMinimumWidth, 0});
        setMaximumSize({view.style().tabMaximumWidth, unbounded});
        label_ = &add(std::make_unique<Label>(Label::Props{.text = title, .elide = true}));
        label_->setAlignment(Alignment::Start, Alignment::Center);
        label_->setHitTestVisible(false);
    }

    bool active = false, hot = false, pressed = false, closeHot = false;

    // Its × at the end side.
    Rect closeRect() const {
        const float y = size().height / 2 - 8;
        return rightToLeft() ? Rect{6, y, 16, 16} : Rect{size().width - 22, y, 16, 16};
    }
    bool closable() const { return closable_; }
    bool cut() const { return label_->size().width + 0.5f < label_->measure({unbounded, unbounded}).width; }

protected:
    Size measureContent(Size available) override {
        const Size text = Widget::measureContent(available);
        return {text.width, std::max(text.height, view_.style().tabHeight)};
    }
    void paint(PaintContext& context) override {
        const DockStyle& s = view_.style();
        tabs::paintTab(context, s, size(), active, {.hovered = hot, .pressed = pressed && hot});
        label_->setColor(active || hot ? s.text : s.mutedText);
        if (closable_ && (hot || active))
            tabs::paintClose(context, s, closeRect(), closeHot);
    }

private:
    const TabView& view_;
    bool closable_;
    Label* label_ = nullptr;
};

} // namespace

// The tabs side by side, scrolled when they do not fit. It has the mouse for them: a press shows a tab (or on its ×, asks
// to close it), a drag moves it along.
class TabView::Bar final : public Widget {
public:
    explicit Bar(TabView& view) : view_(view) { setClipsChildren(true); }

    TabFace& face(std::size_t i) const { return static_cast<TabFace&>(*children()[i]); }
    void reveal() {
        reveal_ = true;
        invalidateLayout();
    }
    // The faces made again keep what the mouse does on them.
    void restore() {
        for (std::size_t i = 0; i < children().size(); ++i) {
            TabFace& f = face(i);
            f.active = view_.current_ == i;
            f.hot = hot_ == i;
            f.pressed = pressed_ == i;
        }
        invalidatePaint();
    }

protected:
    Size measureContent(Size available) override {
        Size size;
        for (const auto& tab : children()) {
            const Size s = tab->measure({unbounded, available.height});
            size.width += s.width;
            size.height = std::max(size.height, s.height);
        }
        return size;
    }

    void arrangeContent(const Rect& content) override {
        area_ = content;
        widths_.clear();
        total_ = 0;
        for (const auto& tab : children()) {
            widths_.push_back(tab->measure({unbounded, content.height}).width);
            total_ += widths_.back();
        }
        if (reveal_ && view_.current_ && *view_.current_ < widths_.size()) {
            float start = 0;
            for (std::size_t i = 0; i < *view_.current_; ++i)
                start += widths_[i];
            const float end = start + widths_[*view_.current_];
            if (start < offset_)
                offset_ = start;
            else if (end > offset_ + content.width)
                offset_ = end - content.width;
        }
        reveal_ = false;
        offset_ = std::clamp(offset_, 0.0f, std::max(0.0f, total_ - content.width));
        const bool rtl = rightToLeft();
        float at = -offset_;
        for (std::size_t i = 0; i < widths_.size(); ++i) {
            const float x = rtl ? content.right() - at - widths_[i] : content.x + at;
            children()[i]->arrange({x, content.y, widths_[i], content.height});
            at += widths_[i];
        }
    }

    std::optional<LocalizedText> toolTipAt(Point point) const override {
        const auto i = tabAt(point);
        if (!i)
            return std::nullopt;
        if (const auto& tip = view_.tabs_[*i].toolTip)
            return tip;
        return face(*i).cut() ? std::optional<LocalizedText>(view_.tabs_[*i].title) : std::nullopt;
    }

    void onMouse(MouseEvent& event) override {
        switch (event.type) {
        case MouseEventType::Press: {
            const auto i = tabAt(event.position);
            if (!i)
                return;
            event.handled = true;
            if (event.button == platform::MouseButton::Middle) {
                view_.requestClose(*i);
                return;
            }
            if (event.button != platform::MouseButton::Left)
                return;
            if (face(*i).closable() && onClose(*i, event.position)) {
                pressedClose_ = *i;
                return;
            }
            view_.setCurrent(*i);
            pressed_ = dragStart_ = *i;
            pressX_ = event.position.x;
            dragging_ = false;
            restore();
            break;
        }
        case MouseEventType::Move: {
            hover(event.position);
            if (!pressed_)
                return;
            event.handled = true;
            if (!dragging_ && std::abs(event.position.x - pressX_) > dragThreshold && view_.props_.movable && view_.count() > 1)
                dragging_ = true;
            if (dragging_)
                dragTo(event.position);
            break;
        }
        case MouseEventType::Release:
            if (event.button != platform::MouseButton::Left)
                return;
            event.handled = true;
            if (const auto closing = std::exchange(pressedClose_, std::nullopt)) {
                if (tabAt(event.position) == closing && onClose(*closing, event.position))
                    view_.requestClose(*closing);
                return;
            }
            if (pressed_ && dragging_ && view_.props_.onMoved && *pressed_ != dragStart_)
                view_.props_.onMoved(*dragStart_, *pressed_);
            pressed_.reset();
            dragging_ = false;
            restore();
            break;
        case MouseEventType::Wheel: {
            const float most = std::max(0.0f, total_ - area_.width);
            if (most <= 0)
                return;
            const float step = (-event.wheelDelta.y + (rightToLeft() ? -event.wheelDelta.x : event.wheelDelta.x)) * wheelStep;
            offset_ = std::clamp(offset_ + step, 0.0f, most);
            rearrange();
            invalidatePaint();
            event.handled = true;
            break;
        }
        }
    }

    void onHoverChanged(bool hovered) override {
        if (!hovered)
            hover({-1e9f, -1e9f});
    }

private:
    std::optional<std::size_t> tabAt(Point point) const {
        for (std::size_t i = 0; i < children().size(); ++i) {
            const Rect b = children()[i]->bounds();
            if (b.contains(point) && area_.contains(point))
                return i;
        }
        return std::nullopt;
    }
    bool onClose(std::size_t i, Point point) const {
        const TabFace& f = face(i);
        const Point local{point.x - f.bounds().x, point.y - f.bounds().y};
        return f.closeRect().contains(local);
    }
    void hover(Point point) {
        const auto i = tabAt(point);
        bool changed = i != hot_;
        hot_ = i;
        for (std::size_t k = 0; k < children().size(); ++k) {
            TabFace& f = face(k);
            const bool closeHot = i == k && f.closable() && onClose(k, point);
            changed = changed || closeHot != f.closeHot;
            f.closeHot = closeHot;
            f.hot = hot_ == k;
        }
        if (changed)
            invalidatePaint();
    }
    // The tab pressed goes where the mouse is: past the middle of a neighbour, it takes its place.
    void dragTo(Point point) {
        const std::size_t at = *pressed_;
        const bool rtl = rightToLeft();
        std::size_t to = 0;
        for (std::size_t i = 0; i < children().size(); ++i) {
            const float middle = children()[i]->bounds().center().x;
            if (i != at && (rtl ? point.x < middle : point.x > middle))
                ++to;       // the mouse is past it: the tab goes after it
        }
        if (to == at)
            return;
        pressed_ = to;
        view_.moveTab(at, to);      // the faces made again (the bar keeps the mouse)
    }

    TabView& view_;
    Rect area_;
    std::vector<float> widths_;
    float total_ = 0;
    float offset_ = 0;
    bool reveal_ = false;
    std::optional<std::size_t> hot_, pressed_, pressedClose_, dragStart_;
    float pressX_ = 0;
    bool dragging_ = false;
};

TabView::TabView() : TabView(Props{}) {}

TabView::TabView(Props props) : props_(std::move(props)) {
    bar_ = &add(std::make_unique<Bar>(*this));
}

TabView::~TabView() = default;

const DockStyle& TabView::style() const {
    if (const UiContext* ui = context())
        return ui->theme.dock;
    static const DockStyle unstyled = Theme::dark().dock;
    return unstyled;
}

std::size_t TabView::addTab(LocalizedText title, std::unique_ptr<Widget> content) {
    insertTab(tabs_.size(), std::move(title), std::move(content));
    return tabs_.size() - 1;
}

void TabView::insertTab(std::size_t index, LocalizedText title, std::unique_ptr<Widget> content) {
    index = std::min(index, tabs_.size());
    Widget& page = add(std::move(content));
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(index), Tab{std::move(title), &page, std::nullopt});
    if (current_ && *current_ >= index)
        ++*current_;
    current(index, true);
}

std::unique_ptr<Widget> TabView::removeTab(std::size_t index) {
    if (index >= tabs_.size())
        return nullptr;
    Widget* page = tabs_[index].content;
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(index));
    std::unique_ptr<Widget> taken = removeChild(*page);
    taken->setVisible(true);
    // the one shown stays; if it went, the one in its place (or before it, at the end)
    const std::optional<std::size_t> old = current_;
    std::optional<std::size_t> next;
    if (!tabs_.empty() && old)
        next = *old > index ? *old - 1 : *old == index ? std::min(index, tabs_.size() - 1) : *old;
    current(next, next != old || old == index);
    return taken;
}

void TabView::moveTab(std::size_t from, std::size_t to) {
    if (from >= tabs_.size() || to >= tabs_.size() || from == to)
        return;
    Tab tab = std::move(tabs_[from]);
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(from));
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(to), std::move(tab));
    std::optional<std::size_t> shown = current_;
    if (shown) {
        if (*shown == from)
            shown = to;
        else if (from < *shown && to >= *shown)
            --*shown;
        else if (from > *shown && to <= *shown)
            ++*shown;
    }
    const bool changed = shown != current_;
    current_ = shown;
    rebuild();
    if (changed && props_.onCurrentChanged)
        props_.onCurrentChanged(current_);
}

void TabView::setCurrent(std::size_t index) {
    if (index < tabs_.size() && current_ != index)
        current(index, true);
}

void TabView::current(std::optional<std::size_t> index, bool tell) {
    current_ = index;
    for (std::size_t i = 0; i < tabs_.size(); ++i)
        tabs_[i].content->setVisible(index == i);
    rebuild();
    bar_->reveal();
    if (tell && props_.onCurrentChanged)
        props_.onCurrentChanged(current_);
}

void TabView::setTitle(std::size_t index, LocalizedText title) {
    if (index >= tabs_.size())
        return;
    tabs_[index].title = std::move(title);
    rebuild();
}

void TabView::setTabToolTip(std::size_t index, LocalizedText text) {
    if (index < tabs_.size())
        tabs_[index].toolTip = std::move(text);
}

std::optional<std::size_t> TabView::indexOf(const Widget& content) const {
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].content == &content)
            return i;
    }
    return std::nullopt;
}

void TabView::setClosable(bool closable) {
    props_.closable = closable;
    rebuild();
}

void TabView::rebuild() {
    while (!bar_->children().empty())
        bar_->removeChild(*bar_->children().back());
    for (const Tab& tab : tabs_)
        bar_->add(std::make_unique<TabFace>(*this, tab.title, props_.closable));
    bar_->restore();
    invalidateLayout();
}

void TabView::requestClose(std::size_t index) {
    const auto close = [this, index] {
        if (index >= tabs_.size())
            return;
        if (props_.onCloseRequested)
            props_.onCloseRequested(index);
        else
            removeTab(index);
    };
    if (UiRoot* r = root())
        r->defer(close);
    else
        close();
}

Rect TabView::tabRect(std::size_t index) const {
    if (index >= bar_->children().size())
        return {};
    const Rect b = bar_->children()[index]->bounds();
    const Rect bar = bar_->bounds();
    return {bar.x + b.x, bar.y + b.y, b.width, b.height};
}

void TabView::onStyleChanged() {
    rebuild();      // the tabs' widths are the style's
}

Size TabView::measureContent(Size available) {
    const float bar = style().tabHeight;
    bar_->measure({available.width, bar});
    Size page;
    if (current_)
        page = tabs_[*current_].content->measure({available.width, std::max(0.0f, available.height - bar)});
    return {page.width + 2 * edge, bar + page.height + edge};
}

void TabView::arrangeContent(const Rect& content) {
    const float bar = style().tabHeight;
    const bool rtl = rightToLeft();
    bar_->arrange({content.x, content.y, content.width, bar});
    const Rect below{content.x + edge, content.y + bar, std::max(0.0f, content.width - 2 * edge),
                     std::max(0.0f, content.height - bar - edge)};
    if (current_)
        arrangeInCell(*tabs_[*current_].content, below, rtl);
}

void TabView::paint(PaintContext& context) {
    std::optional<Rect> active;
    if (current_)
        active = tabRect(*current_);
    tabs::paintStack(context, style(), size(), style().tabHeight, active, bar_->bounds());
}

} // namespace glw
