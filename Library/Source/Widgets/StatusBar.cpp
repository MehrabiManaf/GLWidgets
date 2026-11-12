// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/StatusBar.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

namespace glw {

StatusBar::StatusBar() {
    setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 12.0f));
    setPadding({10, 3, 10, 3});
    setMinimumSize({0, 24});
    setAlignment(Alignment::Stretch, Alignment::End);
    label_ = &add(std::make_unique<Label>(Label::Props{.fontSize = 12, .muted = true, .elide = true}));
    label_->setAlignment(Alignment::Stretch, Alignment::Center);
    label_->setStretch(1);
}

void StatusBar::showMessage(LocalizedText text, std::chrono::milliseconds timeout) {
    message_ = std::move(text);
    timeout_.stop();
    if (timeout > std::chrono::milliseconds::zero()) {
        if (Scheduler* s = scheduler())
            timeout_.start(*s, timeout, [this] { clearMessage(); });
    }
    shown();
}

void StatusBar::clearMessage() {
    timeout_.stop();
    message_.reset();
    shown();
}

void StatusBar::setText(LocalizedText text) {
    text_ = std::move(text);
    shown();
}

void StatusBar::shown() {
    label_->setText(message_ ? *message_ : text_);
}

void StatusBar::addPermanentWidget(std::unique_ptr<Widget> widget) {
    widget->setAlignment(widget->horizontalAlignment(), Alignment::Center);
    addChild(std::move(widget));
}

void StatusBar::paint(PaintContext& context) {
    const Theme& t = context.ui.theme;
    const StatusBarStyle& s = t.statusBar;
    if (s.bevel.relief > 0) {
        // a groove along the bottom of the window
        const Rect groove = Rect{0, 0, size().width, size().height}.inflated(-s.inset);
        paintSunken(context.painter, groove, t.cornerRadius, t.toolBar.background, {0, 0, 0, 0}, s.bevel);
        return;
    }
    context.painter.fillRect({0, 0, size().width, size().height}, t.toolBar.background);
    context.painter.fillRect({0, 0, size().width, 1}, t.palette.border);
}

} // namespace glw
