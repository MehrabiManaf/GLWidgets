// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Dialogs: a modal popup (PopupRequest::modal) in the middle of the window it opens from, which shows dimmed and takes no
// input until it closes. A dialog has a title, content of any widgets and a row of buttons at its end; a button closes it
// and says which it was, Enter is its default button (in the accent color), Escape closes it without a choice. The focus
// starts on the first widget of its content that takes it (a field), or on the default button.
//
//   glw::showDialog(*this, {.title = "Rename", .content = std::move(form), .buttons = {"Rename", "Cancel"},
//                           .onClosed = [this](std::optional<std::size_t> button) { if (button == 0u) rename(); }});
//   glw::showMessage(*this, "Close the window?", "What is not saved is lost.", {"Close", "Cancel"}, ...);
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

class Button;

struct DialogOptions {
    LocalizedText title;
    std::unique_ptr<Widget> content;
    std::vector<LocalizedText> buttons;             // at its end, in order (from the end side)
    std::size_t defaultButton = 0;                  // Enter's; none past the last
    // Once it closed: the button chosen, none for Escape (or its window closing).
    std::function<void(std::optional<std::size_t>)> onClosed;
};

// The content of a dialog's popup.
class GLW_EXPORT Dialog : public Widget {
public:
    explicit Dialog(DialogOptions options);

    // Closes it, as if `button` was chosen (none: as Escape).
    void close(std::optional<std::size_t> button);
    Button* button(std::size_t index) const { return index < buttons_.size() ? buttons_[index] : nullptr; }
    // The focus on the first widget of its content that takes it, or on the default button (showDialog does, once open).
    void focusFirst();

protected:
    Size measureContent(Size available) override;
    void paint(PaintContext& context) override;
    void onKey(KeyEvent& event) override;
    void onRootChanged() override;
    void onStyleChanged() override;
    void onWindowClosing() override;

private:
    void applyStyle();
    std::size_t defaultButton_;
    std::vector<Button*> buttons_;
    std::function<void(std::optional<std::size_t>)> onClosed_;
    std::optional<std::size_t> chosen_;
    bool closing_ = false;
};

// Opens a dialog over the window of `from`. False where that window has no popups.
GLW_EXPORT bool showDialog(Widget& from, DialogOptions options);
// A dialog of a text (wrapped).
GLW_EXPORT bool showMessage(Widget& from, LocalizedText title, LocalizedText text, std::vector<LocalizedText> buttons,
                            std::function<void(std::optional<std::size_t>)> onClosed = {});

} // namespace glw
