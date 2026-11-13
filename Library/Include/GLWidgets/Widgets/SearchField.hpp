// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A field to search with: a magnifier at its end side while it is empty, which turns into a button that clears it once
// something is typed (Escape clears it too, as long as it has text). What it searches for is told as it is typed
// (onChanged), and when Enter is pressed (onSubmit).
//
//   ui::SearchField({.onChanged = [this](const std::string& text) { filter(text); }})
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <string>

namespace glw {

class LineEdit;

class GLW_EXPORT SearchField : public Widget {
public:
    // What a search field is made with (ui::SearchField): designated initializers, in this order.
    struct Props {
        std::string text;
        LocalizedText placeholder;                      // shown while it is empty; none: "Search"
        std::function<void(const std::string&)> onChanged;
        std::function<void(const std::string&)> onSubmit;
    };

    SearchField();
    explicit SearchField(Props props);

    const std::string& text() const;
    // Not told (onChanged), as LineEdit::setText.
    void setText(std::string text);
    // Empty, as the clear button makes it: told (onChanged) when it had text.
    void clear();
    void setPlaceholder(LocalizedText placeholder);
    void setOnChanged(std::function<void(const std::string&)> handler) { onChanged_ = std::move(handler); }
    void setOnSubmit(std::function<void(const std::string&)> handler);
    // The field it is typed in (to focus it, to select its text).
    LineEdit& field() const { return *field_; }

protected:
    void paintOver(PaintContext& context) override;
    void onPreviewMouse(MouseEvent& event) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    bool onButton(Point point) const;           // the icon's place, at its end side
    void textChanged();                         // the icon turned, as the text is or is not empty

    LineEdit* field_ = nullptr;
    std::function<void(const std::string&)> onChanged_;
    Animation clearing_{[this] { invalidatePaint(); }};        // 0: the magnifier, 1: the clear button
    bool hovered_ = false;                      // the mouse on the clear button
    bool pressed_ = false;
};

} // namespace glw
