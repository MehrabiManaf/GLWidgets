// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/CollapsiblePanel.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float arrowRoom = 34;         // the arrow's, at the header's start
constexpr float actionsGap = 6;         // between the title and the buttons at the end

bool inside(const Widget* widget, const Widget& subtree) {
    for (; widget; widget = widget->parent()) {
        if (widget == &subtree)
            return true;
    }
    return false;
}

} // namespace

CollapsiblePanel::CollapsiblePanel() : CollapsiblePanel(Props{}) {}

CollapsiblePanel::CollapsiblePanel(Props props) : expanded_(props.expanded), onToggled_(std::move(props.onToggled)) {
    setFocusable(true);
    setClipsChildren(true);
    const CollapsiblePanelStyle s = Theme::dark().collapsiblePanel;     // its sizes: the same in every theme
    title_ = &add(std::make_unique<Label>(Label::Props{.text = std::move(props.title), .elide = true}));
    title_->setHitTestVisible(false);
    title_->setAlignment(Alignment::Stretch, Alignment::Center);
    auto actions = std::make_unique<Widget>();
    actions->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, s.actionMargin));
    actions_ = &add(std::move(actions));
    auto content = std::make_unique<Widget>();
    content->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, s.spacing));
    content->setPadding(s.padding);
    content_ = &add(std::move(content));
    progress_.set(expanded_ ? 1.0f : 0.0f);
    content_->setVisible(expanded_);
}

const CollapsiblePanelStyle& CollapsiblePanel::style() const {
    if (const UiContext* ui = context())
        return ui->theme.collapsiblePanel;
    static const CollapsiblePanelStyle unstyled = Theme::dark().collapsiblePanel;
    return unstyled;
}

void CollapsiblePanel::setTitle(LocalizedText title) {
    title_->setText(std::move(title));
}

ToolButton& CollapsiblePanel::addAction(std::shared_ptr<Action> action) {
    auto& button = static_cast<ToolButton&>(actions_->addChild(std::make_unique<ToolButton>(std::move(action))));
    const CollapsiblePanelStyle& s = style();
    button.setSizes(s.actionSize, s.actionIconSize);
    return button;
}

void CollapsiblePanel::setExpanded(bool expanded, bool animated) {
    if (expanded == expanded_ && (progress_.running() || progress_.value() == (expanded ? 1.0f : 0.0f)))
        return;
    expanded_ = expanded;
    if (expanded)
        content_->setVisible(true);
    else if (root() && inside(root()->focusWidget(), *content_))
        focus();                            // what had the focus goes: the header has it
    if (animated)
        progress_.animateTo(scheduler(), expanded ? 1.0f : 0.0f,
                            context() ? Duration(context()->theme.motion.slow) : Duration(std::chrono::milliseconds(300)), Easing::InOutCubic);
    else
        progress_.set(expanded ? 1.0f : 0.0f);
    stepped();
}

void CollapsiblePanel::toggle() {
    setExpanded(!expanded_);
    if (onToggled_)
        onToggled_(expanded_);
}

void CollapsiblePanel::stepped() {
    // closed, all the way: its content out of the way
    if (!expanded_ && !progress_.running() && progress_.value() <= 0)
        content_->setVisible(false);
    invalidateLayout();
    invalidatePaint();
}

float CollapsiblePanel::headerHeight() const {
    return style().headerHeight;
}

Rect CollapsiblePanel::header() const {
    return {0, 0, size().width, std::min(headerHeight(), size().height)};
}

// ---- layout ----

Size CollapsiblePanel::measureContent(Size available) {
    const float width = std::isfinite(available.width) ? available.width : unbounded;
    const Size title = title_->measure({unbounded, unbounded});
    const Size actions = actions_->measure({unbounded, unbounded});
    // (the room at its end as arrangeContent leaves it)
    const float margin = actions_->children().empty() ? 4.0f : style().actionMargin;
    float wanted = arrowRoom + title.width + actionsGap + actions.width + margin;
    float height = headerHeight();
    if (content_->visible()) {
        // as much of its content as it is open
        const Size content = content_->measure({width, unbounded});
        wanted = std::max(wanted, content.width);
        height += content.height * progress_.value();
    }
    return {std::isfinite(available.width) ? available.width : wanted, height};
}

void CollapsiblePanel::arrangeContent(const Rect& content) {
    const bool rtl = rightToLeft();
    const float h = headerHeight();
    const float width = content.width;
    const auto mirrored = [&](Rect r) { return rtl ? Rect{content.x + width - (r.x - content.x) - r.width, r.y, r.width, r.height} : r; };
    const Size actions = actions_->desiredSize();
    const float actionsWidth = actions.width;
    // the buttons at its end, the style's room from it
    const float margin = actions_->children().empty() ? 4.0f : style().actionMargin;
    actions_->arrange(mirrored({content.x + width - actionsWidth - margin, content.y + (h - actions.height) / 2, actionsWidth, actions.height}));
    const float titleWidth = std::max(0.0f, width - arrowRoom - actionsWidth - actionsGap - margin);
    arrangeInCell(*title_, mirrored({content.x + arrowRoom, content.y, titleWidth, h}), rtl);
    // all of its content, at its height: what does not show yet is cut
    if (content_->visible()) {
        const float full = content_->measure({width, unbounded}).height;
        content_->arrange({content.x, content.y + h, width, full});
    }
}

// ---- painting ----

void CollapsiblePanel::paint(PaintContext& context) {
    const CollapsiblePanelStyle& s = style();
    paint::Painter& p = context.painter;
    const Rect all{0, 0, size().width, size().height};
    const Rect head = header();
    // a card: a soft shadow under it, the content's background under all of it, the header over it
    if (s.shadow.a > 0)
        p.drawShadow({all.x, all.y + 1, all.width, all.height}, s.cornerRadius, 5, s.shadow);
    p.fillRoundedRect(all, s.cornerRadius, s.content);
    const VisualState state{.hovered = headerHovered_, .pressed = pressed_ && headerHovered_, .focused = hasFocus()};
    headerColor_.to(scheduler(), s.header.resolve(state), context.ui.theme.motion.fast);
    // the header's lower corners squaring as it opens (cut out of a box taller by as much), a line under it
    const float opened = std::clamp(progress_.value(), 0.0f, 1.0f);
    p.pushClip(head);
    paintRaised(p, {0, 0, head.width, head.height + s.cornerRadius * opened}, s.cornerRadius, headerColor_.value(), {0, 0, 0, 0}, s.bevel);
    p.popClip();
    if (opened > 0)
        p.fillRect({0, head.bottom() - 1, head.width, 1}, withAlpha(s.separator, s.separator.a * opened));
    if (s.border.a > 0)
        p.strokeRoundedRect(all, s.cornerRadius, 1, s.border);
    // the arrow: toward the end side closed, down open, turning between
    const bool rtl = rightToLeft();
    const Point c{rtl ? all.width - arrowRoom / 2 : arrowRoom / 2, head.height / 2};
    const float angle = progress_.value() * 1.5707964f * (rtl ? -1.0f : 1.0f);
    const float cs = std::cos(angle), sn = std::sin(angle);
    const float d = rtl ? -1.0f : 1.0f;
    Point points[] = {{-2.0f * d, -4}, {2.0f * d, 0}, {-2.0f * d, 4}};
    for (Point& q : points)
        q = {c.x + q.x * cs - q.y * sn, c.y + q.x * sn + q.y * cs};
    p.drawPolyline(points, 1.5f, s.arrow);
    title_->setColor(s.title);
}

void CollapsiblePanel::paintOver(PaintContext&) {}

// ---- events ----

void CollapsiblePanel::onMouse(MouseEvent& event) {
    if (event.type == MouseEventType::Wheel)
        return;
    const bool over = header().contains(event.position);
    if (over != headerHovered_) {
        headerHovered_ = over;
        invalidatePaint();
    }
    if (event.button != platform::MouseButton::Left)
        return;
    if (event.type == MouseEventType::Press && over) {
        pressed_ = true;
        focus();
        event.handled = true;
        invalidatePaint();
    } else if (event.type == MouseEventType::Release && pressed_) {
        pressed_ = false;
        event.handled = true;
        if (over)
            toggle();
        invalidatePaint();
    }
}

void CollapsiblePanel::onKey(KeyEvent& event) {
    if (!event.pressed || !hasFocus())
        return;
    const bool rtl = rightToLeft();
    const bool open = event.key == (rtl ? platform::Key::Left : platform::Key::Right);
    const bool close = event.key == (rtl ? platform::Key::Right : platform::Key::Left);
    if (event.key == platform::Key::Enter || event.key == platform::Key::Space || (open && !expanded_) || (close && expanded_)) {
        toggle();
        event.handled = true;
    }
}

void CollapsiblePanel::onHoverChanged(bool hovered) {
    if (!hovered && headerHovered_) {
        headerHovered_ = false;
        invalidatePaint();
    }
}

void CollapsiblePanel::onFocusChanged(bool) {
    invalidatePaint();
}

void CollapsiblePanel::onRootChanged() {
    onStyleChanged();
}

void CollapsiblePanel::onStyleChanged() {
    const CollapsiblePanelStyle& s = style();
    content_->setPadding(s.padding);
    // the buttons at its header's end, as the style sizes them and the room between them
    actions_->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, s.actionMargin));
    for (const auto& child : actions_->children()) {
        if (auto* button = dynamic_cast<ToolButton*>(child.get()))
            button->setSizes(s.actionSize, s.actionIconSize);
    }
    invalidateLayout();
}

} // namespace glw
