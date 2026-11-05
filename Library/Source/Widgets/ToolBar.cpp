// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/ToolBar.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>

namespace glw {

namespace {

constexpr float separatorWidth = 9;
constexpr float parkedAt = -100000;     // where what does not fit waits: out of sight, out of the mouse's way
constexpr float textGap = 6;            // between a button's icon and its text

const ToolBarStyle& toolStyle(const Widget& widget) {
    if (const UiRoot* r = widget.root())
        return r->context().theme.toolBar;
    static const ToolBarStyle unstyled = Theme::dark().toolBar;
    return unstyled;
}

// Three dots: what does not fit.
Icon moreIcon() {
    return Icon::fromPainter([](paint::Painter& p, const Rect& r, Color color) {
        const Point c = r.center();
        for (int i = -1; i <= 1; ++i)
            p.fillCircle({c.x + static_cast<float>(i) * r.width * 0.3f, c.y}, r.width * 0.09f, color);
    });
}

} // namespace

// ---- a button ----

ToolButton::ToolButton(std::shared_ptr<Action> action, bool showText) : action_(std::move(action)), showText_(showText) {
    if (showText_ || action_->icon().empty()) {
        label_ = &add(std::make_unique<Label>());
        label_->setHitTestVisible(false);
        label_->setAlignment(Alignment::Start, Alignment::Center);
    }
    observing_ = action_->observe([this] { update(); });
    update();
}

ToolButton::~ToolButton() {
    action_->unobserve(observing_);
}

const ToolBarStyle& ToolButton::style() const {
    return toolStyle(*this);
}

void ToolButton::update() {
    // its text and tooltip: the action's, translated, without the mnemonic's &; its shortcut said in the tooltip
    const std::string text = withoutMnemonic(context() ? translate(action_->text()) : action_->text().source);
    if (label_)
        label_->setText(LocalizedText(text));
    if (!action_->toolTip().source.empty()) {
        setToolTip(action_->toolTip());
    } else {
        const std::string shortcut = action_->shortcut().text();
        setToolTip(LocalizedText(shortcut.empty() ? text : text + " (" + shortcut + ")"));
    }
    setEnabled(action_->enabled());
    invalidateLayout();
}

Size ToolButton::measureContent(Size) {
    const ToolBarStyle& s = style();
    if (!label_)
        return {s.buttonSize, s.buttonSize};
    const Size text = label_->measure({unbounded, unbounded});
    const float icon = action_->icon().empty() ? 0.0f : s.iconSize + textGap;
    return {std::max(s.buttonSize, (s.buttonSize - s.iconSize) / 2 * 2 + icon + text.width), s.buttonSize};
}

void ToolButton::arrangeContent(const Rect& content) {
    if (!label_)
        return;
    const ToolBarStyle& s = style();
    const float start = (s.buttonSize - s.iconSize) / 2 + (action_->icon().empty() ? 0.0f : s.iconSize + textGap);
    const float w = std::max(0.0f, content.width - start);
    const bool rtl = rightToLeft();
    arrangeInCell(*label_, {rtl ? content.x : content.x + start, content.y, w, content.height}, rtl);
}

void ToolButton::paint(PaintContext& context) {
    const ToolBarStyle& s = style();
    paint::Painter& p = context.painter;
    const bool enabled = enabledInTree();
    const VisualState state{.hovered = hovered() && enabled, .pressed = pressed_ && hovered(), .disabled = !enabled};
    Color background = s.button.resolve(state);
    const bool checked = action_->checked() && !state.pressed;
    if (checked)
        background = state.hovered ? withAlpha(s.checked, std::min(1.0f, s.checked.a * 1.5f)) : s.checked;
    // flat until the mouse is over it, then raised (pressed or checked, sunk)
    const Duration fast = context.ui.theme.motion.fast;
    background_.to(scheduler(), background, fast);
    outline_.to(scheduler(), state.hovered || state.pressed || checked ? s.outline : withAlpha(s.outline, 0), fast);
    const Rect r{0, 0, size().width, size().height};
    paintRaised(p, r, s.cornerRadius, background_.value(), outline_.value(), s.bevel, state.pressed || checked);
    ripple_.paint(p, r, s.cornerRadius, s.ripple);
    const Color ink = enabled ? s.icon : s.disabledIcon;
    if (!action_->icon().empty()) {
        // its icon, in the middle (or at the start, before its text)
        const float x = label_ ? (rightToLeft() ? r.width - (s.buttonSize + s.iconSize) / 2 : (s.buttonSize - s.iconSize) / 2)
                               : (r.width - s.iconSize) / 2;
        action_->icon().draw(context, {x, (r.height - s.iconSize) / 2, s.iconSize, s.iconSize}, ink);
    }
    if (label_)
        label_->setColor(ink);
}

void ToolButton::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Wheel || event.type == MouseEventType::Move)
        return;
    event.handled = true;
    if (event.type == MouseEventType::Press) {
        pressed_ = true;
        if (const UiContext* ui = context(); ui && style().ripple.a > 0)
            ripple_.start(scheduler(), event.position, ui->theme.motion.ripple);
    } else {
        const bool over = Rect{0, 0, size().width, size().height}.contains(event.position);
        if (std::exchange(pressed_, false) && over)
            action_->trigger();
    }
    invalidatePaint();
}

void ToolButton::onHoverChanged(bool) {
    invalidatePaint();
}

void ToolButton::onRootChanged() {
    update();
}

void ToolButton::onLanguageChanged() {
    update();
}

// ---- the bar ----

ToolBar::ToolBar() {
    setAlignment(Alignment::Stretch, Alignment::Start);
    moreAction_ = std::make_shared<Action>("More", [this] { showOverflow(); });
    moreAction_->setIcon(moreIcon());
    more_ = &add(std::make_unique<ToolButton>(moreAction_));
}

ToolBar::~ToolBar() {
    if (registered_)
        registered_->removeShortcuts(this);
}

const ToolBarStyle& ToolBar::style() const {
    return toolStyle(*this);
}

ToolButton& ToolBar::addAction(std::shared_ptr<Action> action, bool showText) {
    auto button = std::make_unique<ToolButton>(action, showText);
    ToolButton& added = *button;
    addItem(std::move(button), std::move(action));
    return added;
}

void ToolBar::addSeparator() {
    items_.push_back({});
    invalidateLayout();
}

void ToolBar::addItem(std::unique_ptr<Widget> widget, std::shared_ptr<Action> action) {
    // before the menu of what does not fit, which stays the last
    const auto at = static_cast<std::size_t>(std::find_if(children().begin(), children().end(), [&](const auto& c) { return c.get() == more_; }) -
                                             children().begin());
    Widget& added = addChild(std::move(widget), at);
    items_.push_back({&added, std::move(action)});
    registerShortcuts();
}

std::vector<std::shared_ptr<Action>> ToolBar::overflow() const {
    std::vector<std::shared_ptr<Action>> result;
    for (const Item& item : items_) {
        if (!item.shown && item.action)
            result.push_back(item.action);
    }
    return result;
}

void ToolBar::showOverflow() {
    std::vector<MenuItem> menu;
    for (const std::shared_ptr<Action>& action : overflow())
        menu.push_back({.action = action});
    if (!menu.empty())
        showMenu(*more_, {0, 0, more_->size().width, more_->size().height}, std::move(menu));
}

void ToolBar::registerShortcuts() {
    UiRoot* now = root();
    if (registered_ && registered_ != now)
        registered_->removeShortcuts(this);
    registered_ = now;
    if (!now)
        return;
    std::vector<std::shared_ptr<Action>> actions;
    for (const Item& item : items_) {
        if (item.action && !item.action->shortcut().empty())
            actions.push_back(item.action);
    }
    now->addShortcuts(this, std::move(actions));
}

Size ToolBar::measureContent(Size) {
    const ToolBarStyle& s = style();
    float width = 0, height = s.buttonSize;
    for (const Item& item : items_) {
        const Size size = item.widget ? item.widget->measure({unbounded, unbounded}) : Size{separatorWidth, 0};
        width += size.width + s.spacing;
        height = std::max(height, size.height);
    }
    return {width + 2 * s.padding, height + 2 * s.padding};
}

void ToolBar::arrangeContent(const Rect& content) {
    const ToolBarStyle& s = style();
    const bool rtl = rightToLeft();
    const float moreWidth = more_->measure({unbounded, unbounded}).width;
    float total = 0;
    for (const Item& item : items_)
        total += (item.widget ? item.widget->desiredSize().width : separatorWidth) + s.spacing;
    // all of them, or as many as fit before the menu of the others
    const bool all = total - s.spacing <= content.width - 2 * s.padding;
    const float limit = all ? content.width - s.padding : content.width - s.padding - moreWidth - s.spacing;
    float x = s.padding;
    bool fits = true;
    for (Item& item : items_) {
        const Size size = item.widget ? item.widget->desiredSize() : Size{separatorWidth, 0};
        fits = fits && x + size.width <= limit;
        item.shown = fits;
        const float h = std::min(size.height > 0 ? size.height : s.buttonSize, content.height - 2 * s.padding);
        const float left = rtl ? content.width - x - size.width : x;
        if (item.widget) {
            if (fits)
                item.widget->arrange({content.x + left, content.y + (content.height - h) / 2, size.width, h});
            else
                item.widget->arrange({parkedAt, 0, size.width, h});
        }
        item.x = content.x + left + size.width / 2;
        if (fits)
            x += size.width + s.spacing;
    }
    if (all) {
        more_->arrange({parkedAt, 0, moreWidth, s.buttonSize});
    } else {
        const float left = rtl ? s.padding : content.width - s.padding - moreWidth;
        more_->arrange({content.x + left, content.y + (content.height - s.buttonSize) / 2, moreWidth, s.buttonSize});
    }
}

void ToolBar::paint(PaintContext& context) {
    const ToolBarStyle& s = style();
    paint::Painter& p = context.painter;
    const Rect all{0, 0, size().width, size().height};
    p.fillRect(all, s.background);
    for (const Item& item : items_) {
        if (!item.widget && item.shown)
            p.fillRect({std::round(item.x), s.padding + 6, 1, size().height - 2 * (s.padding + 6)}, s.separator);
    }
}

void ToolBar::onRootChanged() {
    registerShortcuts();
}

} // namespace glw
