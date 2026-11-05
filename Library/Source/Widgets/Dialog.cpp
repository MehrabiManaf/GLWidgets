// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Dialog.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>

namespace glw {

namespace {

const DialogStyle& dialogStyle(const Widget& widget) {
    if (const UiRoot* r = widget.root())
        return r->context().theme.dialog;
    static const DialogStyle unstyled = Theme::dark().dialog;
    return unstyled;
}

// The first widget in `widget` (itself included) that takes the focus.
Widget* firstFocusable(Widget& widget) {
    if (widget.focusable() && widget.visible() && widget.enabled())
        return &widget;
    for (const auto& child : widget.children()) {
        if (Widget* found = firstFocusable(*child))
            return found;
    }
    return nullptr;
}

} // namespace

Dialog::Dialog(DialogOptions options) : defaultButton_(options.defaultButton), onClosed_(std::move(options.onClosed)) {
    const DialogStyle s = Theme::dark().dialog;        // its sizes: the same in every theme
    setLayout(std::make_unique<StackLayout>(Orientation::Vertical, s.spacing));
    setPadding(Insets::all(s.padding));
    if (!options.title.source.empty()) {
        auto& title = add(std::make_unique<Label>(Label::Props{.text = std::move(options.title), .fontSize = s.titleSize, .wrap = true}));
        title.setAlignment(Alignment::Stretch, Alignment::Start);
    }
    if (options.content)
        addChild(std::move(options.content));
    if (!options.buttons.empty()) {
        auto row = std::make_unique<Widget>();
        row->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 8.0f));
        row->setAlignment(Alignment::End, Alignment::Start);
        for (std::size_t i = 0; i < options.buttons.size(); ++i) {
            auto& button = row->add(std::make_unique<Button>(std::move(options.buttons[i]), [this, i] { close(i); }));
            button.setMinimumSize({80, 0});
            buttons_.push_back(&button);
        }
        addChild(std::move(row));
    }
}

void Dialog::close(std::optional<std::size_t> button) {
    if (closing_)
        return;
    closing_ = true;
    chosen_ = button;
    if (UiHost* host = root() ? root()->host() : nullptr)
        host->closeWindow();
    else
        onWindowClosing();
}

Size Dialog::measureContent(Size available) {
    // not wider than the style says: its text wraps
    available.width = std::min(available.width, dialogStyle(*this).maximumWidth - padding().horizontal());
    return Widget::measureContent(available);
}

void Dialog::paint(PaintContext& context) {
    const DialogStyle& s = dialogStyle(*this);
    const Rect all{0, 0, size().width, size().height};
    context.painter.fillRect(all, s.background);
    context.painter.strokeRoundedRect(all, root() ? root()->windowRadius() : 0.0f, 1, s.border);
}

void Dialog::onKey(KeyEvent& event) {
    if (!event.pressed)
        return;
    if (event.key == platform::Key::Escape) {
        close(std::nullopt);
        event.handled = true;
    } else if (event.key == platform::Key::Enter && defaultButton_ < buttons_.size()) {
        close(defaultButton_);
        event.handled = true;
    }
}

void Dialog::onRootChanged() {
    const UiRoot* r = root();
    if (!r)
        return;
    applyStyle();
}

void Dialog::onStyleChanged() {
    applyStyle();
}

void Dialog::applyStyle() {
    const UiRoot* r = root();
    if (!r)
        return;
    // the default button in the accent color
    if (defaultButton_ < buttons_.size()) {
        const Palette& p = r->context().theme.palette;
        ButtonStyle accent = r->context().theme.button;
        accent.background = {.normal = p.accent, .hovered = Color{p.accent.r, p.accent.g, p.accent.b, 0.9f},
                             .pressed = Color{p.accent.r, p.accent.g, p.accent.b, 0.8f}, .disabled = p.disabledText};
        accent.border = {.normal = p.accent};
        accent.text = {.normal = p.onAccent, .disabled = p.window};
        buttons_[defaultButton_]->setStyle(accent);
    }
}

void Dialog::focusFirst() {
    // its first field, or the default button
    Widget* first = nullptr;
    for (const auto& child : children()) {
        if (!buttons_.empty() && child.get() == buttons_.front()->parent())
            break;
        if ((first = firstFocusable(*child)))
            break;
    }
    if (!first && defaultButton_ < buttons_.size())
        first = buttons_[defaultButton_];
    if (first)
        first->focus();
}

void Dialog::onWindowClosing() {
    if (onClosed_)
        std::exchange(onClosed_, nullptr)(chosen_);
}

bool showDialog(Widget& from, DialogOptions options) {
    UiRoot* root = from.root();
    UiHost* host = root ? root->host() : nullptr;
    if (!host)
        return false;
    UiRoot* opened = host->openPopup({.content = std::make_unique<Dialog>(std::move(options)),
                                      .anchor = {0, 0, root->size().width, root->size().height},
                                      .placement = PopupPlacement::Center,
                                      .modal = true});
    if (!opened)
        return false;
    static_cast<Dialog*>(opened->content())->focusFirst();
    return true;
}

bool showMessage(Widget& from, LocalizedText title, LocalizedText text, std::vector<LocalizedText> buttons,
                 std::function<void(std::optional<std::size_t>)> onClosed) {
    auto message = std::make_unique<Label>(Label::Props{.text = std::move(text), .muted = true, .wrap = true});
    return showDialog(from, {.title = std::move(title), .content = std::move(message), .buttons = std::move(buttons),
                             .onClosed = std::move(onClosed)});
}

} // namespace glw
