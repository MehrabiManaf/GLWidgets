// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A field of one line of text the user types: Persian, English or both, in any order.
//
// The caret moves by graphemes (what a user sees as one character). The arrow keys move it to the left or the right on the
// screen, also where Persian and English meet; Home and End go to the start and the end of the text. With Shift they
// select; with Control (Option on macOS) they go by words. Selecting is done with the mouse too (drag, double click for a
// word). Control (Command on macOS) with A, C, X, V selects all, copies, cuts and pastes. An input method's composition
// shows at the caret until it is typed. A text longer than the field scrolls to keep the caret in view.
//
// onChanged is called when the user changes the text (not by setText); onSubmit when they press Enter.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace glw {

class GLW_EXPORT LineEdit : public Widget {
public:
    // What a field is made with (ui::LineEdit): designated initializers, in this order.
    struct Props {
        std::string text;
        LocalizedText placeholder;                      // shown while it is empty
        std::function<void(const std::string&)> onChanged;
        std::function<void(const std::string&)> onSubmit;
        std::optional<LineEditStyle> style;             // none: the theme's
    };

    LineEdit();
    explicit LineEdit(Props props);

    const std::string& text() const { return text_; }
    void setText(std::string text);
    void setPlaceholder(LocalizedText placeholder);
    void setOnChanged(std::function<void(const std::string&)> onChanged) { onChanged_ = std::move(onChanged); }
    void setOnSubmit(std::function<void(const std::string&)> onSubmit) { onSubmit_ = std::move(onSubmit); }
    void setStyle(std::optional<LineEditStyle> style);
    const LineEditStyle& style() const;

    // UTF-8 offsets: where the caret is, and where the selection started (the same when nothing is selected).
    std::size_t caret() const { return caret_; }
    std::size_t anchor() const { return anchor_; }
    std::pair<std::size_t, std::size_t> selection() const;       // in order
    void select(std::size_t anchor, std::size_t caret);
    void selectAll() { select(0, text_.size()); }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onText(TextEvent& event) override;
    void onFocusChanged(bool focused) override;
    void onHoverChanged(bool hovered) override;
    void onRootChanged() override;
    void onStyleChanged() override;
    void onLanguageChanged() override;

private:
    // The text laid out as shown: with the input method's composition at the caret.
    const text::TextLayout* textLayout();
    const text::TextLayout* placeholderLayout();
    std::string shown() const;
    std::size_t shownOffset(std::size_t offset) const;      // an offset of the text in the shown text
    Point textOrigin();                                     // where the shown text is drawn, in its own coordinates
    std::size_t offsetAt(Point local);

    void replaceSelection(std::string_view with);
    void edited();
    void moveTo(std::size_t offset, bool extend);
    std::size_t visualNeighbour(bool right);
    std::size_t wordBoundary(bool forward) const;
    void scrollToCaret();
    void updateInputArea();
    void changed();                                         // the text, the caret or the composition: shown again

    std::string text_;
    LocalizedText placeholder_;
    std::function<void(const std::string&)> onChanged_;
    std::function<void(const std::string&)> onSubmit_;
    std::optional<LineEditStyle> style_;

    std::size_t anchor_ = 0;
    std::size_t caret_ = 0;
    std::string composition_;
    int compositionCursor_ = 0;                             // code points into the composition
    bool selecting_ = false;

    std::optional<text::TextLayout> layout_;
    std::optional<text::TextLayout> placeholderLayout_;
    Rect content_;
    float scroll_ = 0;                                      // dips of a long text scrolled out on the left
    // The caret shown again and blinking from now (it moved, the field got the focus); it rests, shown, after a while.
    void restartBlink();
    Timer blink_;
    ColorTransition border_{[this] { invalidatePaint(); }};
    ColorTransition glow_{[this] { invalidatePaint(); }};
    bool caretShown_ = true;
    int blinks_ = 0;
};

} // namespace glw
