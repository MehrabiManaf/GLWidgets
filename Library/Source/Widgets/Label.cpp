// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Label.hpp>

#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <string>
#include <utility>
#include <vector>

namespace glw {

Label::Label(Props props)
    : text_(std::move(props.text)),
      fontSize_(props.fontSize),
      color_(props.color),
      muted_(props.muted),
      alignment_(props.alignment),
      wrap_(props.wrap),
      elide_(props.elide) {
    setHitTestVisible(false);
}

Label::Label(LocalizedText text) : Label(Props{.text = std::move(text)}) {}

void Label::setText(LocalizedText text) {
    if (text != text_) {
        text_ = std::move(text);
        textChanged();
    }
}

void Label::textChanged() {
    layout_.reset();
    invalidateLayout();
}

void Label::setFontSize(float size) {
    if (size != fontSize_) {
        fontSize_ = size;
        layout_.reset();
        invalidateLayout();
    }
}

void Label::setColor(std::optional<Color> color) {
    if (color != color_) {
        color_ = color;
        invalidatePaint();
    }
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

void Label::setElide(bool elide) {
    if (elide != elide_) {
        elide_ = elide;
        invalidateLayout();
    }
}

void Label::onRootChanged() {
    layout_.reset();
}

void Label::onStyleChanged() {
    layout_.reset();        // the theme's font size
}

void Label::onLanguageChanged() {
    layout_.reset();
}

const text::TextLayout* Label::textLayout(float width) {
    const UiContext* ui = context();
    if (!ui)
        return nullptr;
    // wrapped in its width, and half a dip more: a width rounded to pixels may be a little less than the text measured
    const float maxWidth = wrap_ ? width + 0.5f : unbounded;
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
        layout_.emplace(ui->translator.resolve(text_), text::TextStyle{{&ui->fonts.defaultFont()}, size}, options);
        layoutWidth_ = maxWidth;
        layoutRightToLeft_ = rtl;
        elided_.reset();
    }
    return &*layout_;
}

const text::TextLayout* Label::elidedLayout(float width) {
    const text::TextLayout* full = textLayout(unbounded);
    if (!full)
        return nullptr;
    if (elided_ && elidedWidth_ == width)
        return &*elided_;
    // the starts that end between graphemes, the longest that fits with … (found by halves)
    const std::string& text = full->text();
    std::vector<std::size_t> ends;
    for (std::size_t at = full->nextOffset(0); at > 0 && at <= text.size(); at = full->nextOffset(at)) {
        ends.push_back(at);
        if (at == text.size())
            break;
    }
    auto laidOut = [&](std::size_t end) {
        std::string start = text.substr(0, end);
        while (!start.empty() && start.back() == ' ')
            start.pop_back();
        return text::TextLayout(start + "\xE2\x80\xA6", full->style());      // \u2026, in UTF-8
    };
    std::size_t low = 0, high = ends.size();       // how many graphemes: `low` fit, `high` may not
    while (low < high) {
        const std::size_t middle = (low + high + 1) / 2;
        if (laidOut(ends[middle - 1]).size().width <= width)
            low = middle;
        else
            high = middle - 1;
    }
    elided_.emplace(laidOut(low > 0 ? ends[low - 1] : 0));
    elidedWidth_ = width;
    return &*elided_;
}

Size Label::measureContent(Size available) {
    const text::TextLayout* layout = textLayout(available.width);
    if (!layout)
        return {};
    // cut to the width it gets: it wants all of its text, and takes what there is
    if (elide_ && !wrap_)
        return {std::min(layout->size().width, available.width), layout->size().height};
    return layout->size();
}

void Label::arrangeContent(const Rect& content) {
    content_ = content;
}

void Label::paint(PaintContext& context) {
    const text::TextLayout* layout = textLayout(content_.width);
    if (!layout)
        return;
    if (elide_ && !wrap_ && layout->size().width > content_.width + 0.5f)
        layout = elidedLayout(content_.width);
    else
        elided_.reset();
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
    const float y = content_.y + (content_.height - size.height) / 2 + layout->opticalOffset();
    const Palette& palette = context.ui.theme.palette;
    context.drawText(*layout, {x, y}, color_ ? *color_ : muted_ ? palette.mutedText : palette.text);
}

} // namespace glw
