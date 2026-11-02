// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Label.hpp>

#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <utility>

namespace glw {

Label::Label(std::string text) : text_(std::move(text)) {
    setHitTestVisible(false);
}

void Label::setText(std::string text) {
    if (text != text_) {
        text_ = std::move(text);
        layout_.reset();
        invalidateLayout();
    }
}

void Label::setFontSize(float size) {
    if (size != fontSize_) {
        fontSize_ = size;
        layout_.reset();
        invalidateLayout();
    }
}

void Label::setColor(std::optional<Color> color) {
    color_ = color;
    invalidatePaint();
}

void Label::setTextAlignment(text::TextAlignment alignment) {
    if (alignment != alignment_) {
        alignment_ = alignment;
        layout_.reset();
        invalidateLayout();
    }
}

void Label::setWrap(bool wrap) {
    if (wrap != wrap_) {
        wrap_ = wrap;
        layout_.reset();
        invalidateLayout();
    }
}

void Label::onRootChanged() {
    layout_.reset();
}

const text::TextLayout* Label::textLayout(float width) {
    const UiContext* ui = context();
    if (!ui)
        return nullptr;
    const float maxWidth = wrap_ ? width : unbounded;
    const bool rtl = rightToLeft();
    if (!layout_ || maxWidth != layoutWidth_ || rtl != layoutRightToLeft_) {
        text::TextOptions options;
        options.maxWidth = maxWidth;
        // Start and End are the layout direction's, whatever the direction of the text
        options.alignment = alignment_;
        if (alignment_ == text::TextAlignment::Start)
            options.alignment = rtl ? text::TextAlignment::Right : text::TextAlignment::Left;
        else if (alignment_ == text::TextAlignment::End)
            options.alignment = rtl ? text::TextAlignment::Left : text::TextAlignment::Right;
        const float size = fontSize_ > 0 ? fontSize_ : ui->theme.fontSize;
        layout_.emplace(text_, text::TextStyle{{&ui->fonts.defaultFont()}, size}, options);
        layoutWidth_ = maxWidth;
        layoutRightToLeft_ = rtl;
    }
    return &*layout_;
}

Size Label::measureContent(Size available) {
    const text::TextLayout* layout = textLayout(available.width);
    return layout ? layout->size() : Size{};
}

void Label::arrangeContent(const Rect& content) {
    content_ = content;
}

void Label::paint(PaintContext& context) {
    const text::TextLayout* layout = textLayout(content_.width);
    if (!layout)
        return;
    const Size size = layout->size();
    // a wrapped text is aligned in its width by the layout; one line is placed here
    float x = content_.x;
    if (!wrap_) {
        const bool rtl = rightToLeft();
        const bool right = alignment_ == text::TextAlignment::Right || (alignment_ == text::TextAlignment::Start && rtl) ||
                           (alignment_ == text::TextAlignment::End && !rtl);
        if (alignment_ == text::TextAlignment::Center)
            x += (content_.width - size.width) / 2;
        else if (right)
            x += content_.width - size.width;
    }
    const float y = content_.y + (content_.height - size.height) / 2;
    context.drawText(*layout, {x, y}, color_ ? *color_ : context.ui.theme.text);
}

} // namespace glw
