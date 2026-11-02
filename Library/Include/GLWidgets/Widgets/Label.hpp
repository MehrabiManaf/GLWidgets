// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Text that is shown: one line, or wrapped to the width it gets. Start and End follow the layout direction; the text
// itself has its own direction (Persian is right to left in a left to right interface too). Its text is a LocalizedText:
// shown as it is, or translated (glw::tr) into the language in effect. Not under the mouse by default: a click on a label
// is one on what holds it.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <optional>
#include <string>

namespace glw {

class GLW_EXPORT Label : public Widget {
public:
    // What a label is made with (ui::Label): designated initializers, in this order.
    struct Props {
        LocalizedText text;
        float fontSize = 0;                             // dips; 0: the theme's
        std::optional<Color> color;                     // none: the theme's text color
        bool muted = false;                             // with no color: the theme's muted text color
        text::TextAlignment alignment = text::TextAlignment::Start;
        bool wrap = false;
    };

    explicit Label(Props props);
    explicit Label(LocalizedText text = {});

    void setText(LocalizedText text);
    const LocalizedText& text() const { return text_; }
    // What it shows: its text in the language in effect.
    std::string shownText() const { return translate(text_); }
    void setFontSize(float size);                       // dips; 0: the theme's
    void setColor(std::optional<Color> color);          // none: the theme's text color
    void setTextAlignment(text::TextAlignment alignment);
    void setWrap(bool wrap);

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override;
    void onStyleChanged() override;
    void onLanguageChanged() override;

private:
    // The text laid out for `width` (unbounded: not wrapped), made again when what it depends on changed.
    const text::TextLayout* textLayout(float width);
    void textChanged();

    LocalizedText text_;
    float fontSize_ = 0;
    std::optional<Color> color_;
    bool muted_ = false;
    text::TextAlignment alignment_ = text::TextAlignment::Start;
    bool wrap_ = false;

    std::optional<text::TextLayout> layout_;
    float layoutWidth_ = -1;
    bool layoutRightToLeft_ = false;
    Rect content_;
};

} // namespace glw
