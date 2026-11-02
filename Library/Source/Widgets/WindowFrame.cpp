// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace glw {

namespace {

UiHost* hostOf(const Widget& widget) {
    const UiRoot* root = widget.root();
    return root ? root->host() : nullptr;
}

} // namespace

// ---- window buttons ----

WindowButton::WindowButton(WindowButtonKind kind) : kind_(kind) {}

Size WindowButton::measureContent(Size) {
    const UiContext* ui = context();
    return ui ? Size{ui->theme.windowButton.width, ui->theme.titleBar.height} : Size{46, 40};
}

void WindowButton::onHoverChanged(bool) {
    invalidatePaint();
}

void WindowButton::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Move || event.type == MouseEventType::Wheel)
        return;
    event.handled = true;
    if (event.type == MouseEventType::Press) {
        pressed_ = true;
        invalidatePaint();
        return;
    }
    const bool wasPressed = std::exchange(pressed_, false);
    invalidatePaint();
    UiHost* host = hostOf(*this);
    if (!wasPressed || !host || !Rect{0, 0, size().width, size().height}.contains(event.position))
        return;
    switch (kind_) {
    case WindowButtonKind::Minimize: host->minimizeWindow(); break;
    case WindowButtonKind::Maximize: host->toggleMaximizeWindow(); break;
    case WindowButtonKind::Close: host->closeWindow(); break;
    }
}

void WindowButton::paint(PaintContext& context) {
    const WindowButtonStyle& style = context.ui.theme.windowButton;
    paint::Painter& p = context.painter;
    const Rect r{0, 0, size().width, size().height};
    const VisualState state{.hovered = hovered(), .pressed = pressed_ && hovered()};
    const Color background = (kind_ == WindowButtonKind::Close ? style.closeBackground : style.background).resolve(state);
    p.fillRect(r, background);
    const Point c = r.center();
    const Color glyph = style.glyph;
    // what is behind the glyph, to hide the back window of the restore glyph with
    const Color behind = background.a >= 1 ? background : context.ui.theme.titleBar.background;
    switch (kind_) {
    case WindowButtonKind::Minimize:
        p.drawLine({c.x - 5, c.y}, {c.x + 5, c.y}, 1, glyph);
        break;
    case WindowButtonKind::Maximize: {
        const UiHost* host = hostOf(*this);
        if (host && host->windowState() == platform::WindowState::Maximized) {
            // two windows, one behind the other: restore
            p.strokeRoundedRect({c.x - 3, c.y - 5, 8, 8}, 1, 1, glyph);
            p.fillRect({c.x - 5, c.y - 3, 8, 8}, behind);
            p.strokeRoundedRect({c.x - 5, c.y - 3, 8, 8}, 1, 1, glyph);
        } else {
            p.strokeRoundedRect({c.x - 5, c.y - 5, 10, 10}, 1, 1, glyph);
        }
        break;
    }
    case WindowButtonKind::Close:
        p.drawLine({c.x - 5, c.y - 5}, {c.x + 5, c.y + 5}, 1, glyph);
        p.drawLine({c.x - 5, c.y + 5}, {c.x + 5, c.y - 5}, 1, glyph);
        break;
    }
}

// ---- title bar ----

TitleBar::TitleBar() {
    setWindowArea(platform::HitArea::Caption);
    auto title = std::make_unique<Label>();
    title->setMargin({12, 0, 12, 0});
    title_ = title.get();
    center_ = &addChild(std::move(title));

    auto buttons = std::make_unique<Widget>();
    buttons->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal));
    buttons->setHitTestVisible(false);      // between the buttons, the caption
    for (WindowButtonKind kind : {WindowButtonKind::Minimize, WindowButtonKind::Maximize, WindowButtonKind::Close})
        buttons->add(std::make_unique<WindowButton>(kind));
    buttons_ = &addChild(std::move(buttons));
}

void TitleBar::setTitle(LocalizedText title) {
    if (title_)
        title_->setText(std::move(title));
}

void TitleBar::setCenter(std::unique_ptr<Widget> center) {
    removeChild(*center_);
    title_ = nullptr;
    center_ = center ? &addChild(std::move(center), 0) : &addChild(std::make_unique<Widget>(), 0);
}

void TitleBar::onRootChanged() {
    onStyleChanged();
}

void TitleBar::onStyleChanged() {
    const UiContext* ui = context();
    if (!ui)
        return;
    if (title_)
        title_->setColor(ui->theme.titleBar.title);
    // the order of the system: close first at the start (macOS), close last at the end (elsewhere)
    const std::vector<WindowButtonKind> order =
        ui->theme.frame.buttonsAtStart
            ? std::vector{WindowButtonKind::Close, WindowButtonKind::Minimize, WindowButtonKind::Maximize}
            : std::vector{WindowButtonKind::Minimize, WindowButtonKind::Maximize, WindowButtonKind::Close};
    std::vector<WindowButtonKind> current;
    for (const auto& child : buttons_->children())
        current.push_back(static_cast<const WindowButton&>(*child).kind());
    if (current == order)
        return;
    while (!buttons_->children().empty())
        buttons_->removeChild(*buttons_->children().front());
    for (WindowButtonKind kind : order)
        buttons_->add(std::make_unique<WindowButton>(kind));
}

Size TitleBar::measureContent(Size available) {
    const UiContext* ui = context();
    const Size buttons = buttons_->measure(available);
    const Insets m = center_->margin();
    const Size center = center_->measure({std::max(0.0f, available.width - buttons.width - m.horizontal()), available.height});
    const float height = std::max({ui ? ui->theme.titleBar.height : 40.0f, buttons.height, center.height + m.vertical()});
    return {buttons.width + center.width + m.horizontal(), height};
}

void TitleBar::arrangeContent(const Rect& content) {
    const UiContext* ui = context();
    const bool rtl = rightToLeft();
    // at the start or at the end, as the theme says; the start is on the right in right to left
    const bool atStart = ui && ui->theme.frame.buttonsAtStart;
    const bool left = atStart != rtl;
    const float width = std::min(buttons_->desiredSize().width, content.width);
    const Rect buttons{left ? content.x : content.right() - width, content.y, width, content.height};
    arrangeInCell(*buttons_, buttons, rtl);
    const Rect rest{left ? content.x + width : content.x, content.y, content.width - width, content.height};
    arrangeInCell(*center_, rest, rtl);
}

void TitleBar::paint(PaintContext& context) {
    context.painter.fillRect({0, 0, size().width, size().height}, context.ui.theme.titleBar.background);
}

// ---- frame ----

WindowFrame::WindowFrame(std::unique_ptr<Widget> content) {
    setLayout(std::make_unique<StackLayout>(Orientation::Vertical));
    titleBar_ = &add(std::make_unique<TitleBar>());
    setContent(std::move(content));
}

void WindowFrame::setContent(std::unique_ptr<Widget> content) {
    if (content_)
        removeChild(*content_);
    if (!content)
        content = std::make_unique<Widget>();
    content->setStretch(1);     // all that the title bar leaves
    content_ = &addChild(std::move(content));
}

void WindowFrame::paint(PaintContext& context) {
    context.painter.fillRect({0, 0, size().width, size().height}, context.ui.theme.frame.background);
}

std::optional<platform::HitArea> WindowFrame::frameHitArea(Point p) const {
    using platform::HitArea;
    const UiHost* host = hostOf(*this);
    const UiContext* ui = context();
    if (!host || !ui || host->windowState() != platform::WindowState::Normal)
        return std::nullopt;
    const float border = ui->theme.frame.resizeBorder;
    const Size s = size();
    const bool left = p.x < border, right = p.x >= s.width - border;
    const bool top = p.y < border, bottom = p.y >= s.height - border;
    if (top && left) return HitArea::ResizeTopLeft;
    if (top && right) return HitArea::ResizeTopRight;
    if (bottom && left) return HitArea::ResizeBottomLeft;
    if (bottom && right) return HitArea::ResizeBottomRight;
    if (top) return HitArea::ResizeTop;
    if (bottom) return HitArea::ResizeBottom;
    if (left) return HitArea::ResizeLeft;
    if (right) return HitArea::ResizeRight;
    return std::nullopt;
}

} // namespace glw
