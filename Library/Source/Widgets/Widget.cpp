// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Widget.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Text/DrawText.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

namespace glw {

void PaintContext::drawText(const text::TextLayout& layout, Point origin, Color color) const {
    text::drawText(painter, ui.atlas, layout, origin, color);
}

Widget::Widget() = default;
Widget::~Widget() = default;

// ---- the tree ----

Widget& Widget::addChild(std::unique_ptr<Widget> child, std::size_t index) {
    GLW_EXPECTS(child && !child->parent_ && !child->uiRoot_);
    Widget& added = *child;
    child->parent_ = this;
    index = std::min(index, children_.size());
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
    if (root())
        added.notifyRootChanged();
    invalidateLayout();
    return added;
}

std::unique_ptr<Widget> Widget::removeChild(Widget& child) {
    GLW_EXPECTS(child.parent_ == this);
    const auto it = std::find_if(children_.begin(), children_.end(), [&](const auto& c) { return c.get() == &child; });
    UiRoot* shownBy = root();
    if (shownBy)
        shownBy->forget(child);     // while its ancestors are still known
    std::unique_ptr<Widget> removed = std::move(*it);
    children_.erase(it);
    removed->parent_ = nullptr;
    if (shownBy)
        removed->notifyRootChanged();
    invalidateLayout();
    return removed;
}

UiRoot* Widget::root() const {
    const Widget* top = this;
    while (top->parent_)
        top = top->parent_;
    return top->uiRoot_;
}

void Widget::setRootOfTree(UiRoot* root) {
    uiRoot_ = root;
    notifyRootChanged();
}

void Widget::notifyRootChanged() {
    onRootChanged();
    for (const auto& child : children_)
        child->notifyRootChanged();
}

// ---- geometry ----

Point Widget::mapToRoot(Point local) const {
    for (const Widget* w = this; w; w = w->parent_)
        local = local + Point{w->bounds_.x, w->bounds_.y};
    return local;
}

Point Widget::mapFromRoot(Point point) const {
    return point - mapToRoot({});
}

Rect Widget::contentRect() const {
    return padding_.deflate({0, 0, bounds_.width, bounds_.height}, rightToLeft());
}

const UiContext* Widget::context() const {
    const UiRoot* r = root();
    return r ? &r->context() : nullptr;
}

// ---- layout ----

void Widget::setLayout(std::unique_ptr<Layout> layout) {
    layout_ = std::move(layout);
    invalidateLayout();
}

void Widget::setMargin(Insets margin) {
    if (margin != margin_) {
        margin_ = margin;
        invalidateLayout();
    }
}

void Widget::setPadding(Insets padding) {
    if (padding != padding_) {
        padding_ = padding;
        invalidateLayout();
    }
}

void Widget::setMinimumSize(Size size) {
    if (size != minimumSize_) {
        minimumSize_ = size;
        invalidateLayout();
    }
}

void Widget::setMaximumSize(Size size) {
    if (size != maximumSize_) {
        maximumSize_ = size;
        invalidateLayout();
    }
}

void Widget::setStretch(float stretch) {
    if (stretch != stretch_) {
        stretch_ = stretch;
        invalidateLayout();
    }
}

void Widget::setAlignment(Alignment horizontal, Alignment vertical) {
    if (horizontal != horizontalAlignment_ || vertical != verticalAlignment_) {
        horizontalAlignment_ = horizontal;
        verticalAlignment_ = vertical;
        invalidateLayout();
    }
}

void Widget::setLayoutDirection(LayoutDirection direction) {
    if (direction != direction_) {
        direction_ = direction;
        invalidateLayout();
    }
}

bool Widget::rightToLeft() const {
    for (const Widget* w = this; w; w = w->parent_) {
        if (w->direction_ != LayoutDirection::Inherit)
            return w->direction_ == LayoutDirection::RightToLeft;
    }
    const UiRoot* r = root();
    return r && r->rightToLeft();
}

Size Widget::measure(Size available) {
    if (!visible_)
        return desiredSize_ = {};
    const Size inner{std::max(0.0f, available.width - padding_.horizontal()), std::max(0.0f, available.height - padding_.vertical())};
    const Size content = measureContent(inner);
    const Size size{content.width + padding_.horizontal(), content.height + padding_.vertical()};
    desiredSize_ = {std::max(minimumSize_.width, std::min(size.width, maximumSize_.width)),
                    std::max(minimumSize_.height, std::min(size.height, maximumSize_.height))};
    return desiredSize_;
}

void Widget::arrange(const Rect& rect) {
    bounds_ = rect;
    arrangeContent(contentRect());
}

Size Widget::measureContent(Size available) {
    if (layout_)
        return layout_->measure(*this, available);
    Size result;
    for (const auto& child : children_) {
        if (!child->visible_)
            continue;
        const Insets m = child->margin_;
        const Size s = child->measure({std::max(0.0f, available.width - m.horizontal()), std::max(0.0f, available.height - m.vertical())});
        result.width = std::max(result.width, s.width + m.horizontal());
        result.height = std::max(result.height, s.height + m.vertical());
    }
    return result;
}

void Widget::arrangeContent(const Rect& content) {
    if (layout_) {
        layout_->arrange(*this, content);
        return;
    }
    const bool rtl = rightToLeft();
    for (const auto& child : children_) {
        if (child->visible_)
            arrangeInCell(*child, content, rtl);
    }
}

void Widget::invalidateLayout() {
    if (UiRoot* r = root())
        r->invalidateLayout();
}

void Widget::invalidatePaint() {
    if (UiRoot* r = root())
        r->invalidatePaint();
}

// ---- state ----

void Widget::setVisible(bool visible) {
    if (visible == visible_)
        return;
    visible_ = visible;
    if (UiRoot* r = root(); r && !visible)
        r->forget(*this);
    invalidateLayout();
}

void Widget::setEnabled(bool enabled) {
    if (enabled == enabled_)
        return;
    enabled_ = enabled;
    if (UiRoot* r = root(); r && !enabled)
        r->forget(*this);
    invalidatePaint();
}

bool Widget::enabledInTree() const {
    for (const Widget* w = this; w; w = w->parent_) {
        if (!w->enabled_)
            return false;
    }
    return true;
}

void Widget::setClipsChildren(bool clips) {
    if (clips != clipsChildren_) {
        clipsChildren_ = clips;
        invalidatePaint();
    }
}

bool Widget::hasFocus() const {
    const UiRoot* r = root();
    return r && r->focusWidget() == this;
}

void Widget::focus() {
    if (UiRoot* r = root())
        r->setFocus(this);
}

} // namespace glw
