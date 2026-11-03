// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/LineEdit.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace glw {

namespace {

bool isSpaceAt(const std::string& s, std::size_t i) {
    return i < s.size() && (s[i] == ' ' || s[i] == '\t');
}

// The bytes of the first `count` code points of `s`.
std::size_t codePointBytes(const std::string& s, int count) {
    std::size_t i = 0;
    for (int k = 0; k < count && i < s.size(); ++k) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
            ++i;
    }
    return i;
}

// One line: line feeds of pasted text become spaces.
std::string oneLine(std::string text) {
    std::erase(text, '\r');
    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

UiHost* hostOf(const Widget& widget) {
    const UiRoot* root = widget.root();
    return root ? root->host() : nullptr;
}

} // namespace

LineEdit::LineEdit() : LineEdit(Props{}) {}

LineEdit::LineEdit(Props props)
    : text_(oneLine(std::move(props.text))),
      placeholder_(std::move(props.placeholder)),
      onChanged_(std::move(props.onChanged)),
      onSubmit_(std::move(props.onSubmit)),
      style_(std::move(props.style)) {
    setFocusable(true);
    setAlignment(Alignment::Stretch, Alignment::Center);
    caret_ = anchor_ = text_.size();
    setPadding(style().padding);
}

// ---- state ----

void LineEdit::setText(std::string text) {
    text_ = oneLine(std::move(text));
    caret_ = anchor_ = text_.size();
    composition_.clear();
    changed();
}

void LineEdit::setPlaceholder(LocalizedText placeholder) {
    placeholder_ = std::move(placeholder);
    placeholderLayout_.reset();
    invalidatePaint();
}

void LineEdit::setStyle(std::optional<LineEditStyle> style) {
    style_ = std::move(style);
    onStyleChanged();
}

const LineEditStyle& LineEdit::style() const {
    if (style_)
        return *style_;
    if (const UiContext* ui = context())
        return ui->theme.lineEdit;
    static const LineEditStyle unstyled = Theme::dark().lineEdit;
    return unstyled;
}

std::pair<std::size_t, std::size_t> LineEdit::selection() const {
    return {std::min(anchor_, caret_), std::max(anchor_, caret_)};
}

void LineEdit::select(std::size_t anchor, std::size_t caret) {
    anchor_ = std::min(anchor, text_.size());
    caret_ = std::min(caret, text_.size());
    scrollToCaret();
    updateInputArea();
    invalidatePaint();
}

void LineEdit::onRootChanged() {
    onStyleChanged();
}

void LineEdit::onStyleChanged() {
    setPadding(style().padding);
    layout_.reset();
    placeholderLayout_.reset();
    invalidateLayout();
}

void LineEdit::onLanguageChanged() {
    placeholderLayout_.reset();
}

// ---- layout ----

std::string LineEdit::shown() const {
    if (composition_.empty())
        return text_;
    return text_.substr(0, caret_) + composition_ + text_.substr(caret_);
}

std::size_t LineEdit::shownOffset(std::size_t offset) const {
    return offset <= caret_ ? offset : offset + composition_.size();
}

const text::TextLayout* LineEdit::textLayout() {
    const UiContext* ui = context();
    if (!ui)
        return nullptr;
    if (!layout_) {
        // aligned here (textOrigin), not by the layout
        layout_.emplace(shown(), text::TextStyle{{&ui->fonts.defaultFont()}, ui->theme.fontSize},
                        text::TextOptions{.alignment = text::TextAlignment::Left});
    }
    return &*layout_;
}

const text::TextLayout* LineEdit::placeholderLayout() {
    const UiContext* ui = context();
    if (!ui)
        return nullptr;
    if (!placeholderLayout_) {
        placeholderLayout_.emplace(translate(placeholder_), text::TextStyle{{&ui->fonts.defaultFont()}, ui->theme.fontSize},
                                   text::TextOptions{.alignment = text::TextAlignment::Left});
    }
    return &*placeholderLayout_;
}

Size LineEdit::measureContent(Size) {
    const text::TextLayout* layout = textLayout();
    return {style().width, layout ? layout->size().height : 18.0f};
}

void LineEdit::arrangeContent(const Rect& content) {
    content_ = content;
    scrollToCaret();
}

Point LineEdit::textOrigin() {
    const text::TextLayout* layout = textLayout();
    if (!layout)
        return {content_.x, content_.y};
    const Size size = layout->size();
    const float y = content_.y + (content_.height - size.height) / 2;
    // a text that fits is at the start side; a longer one is scrolled
    if (size.width <= content_.width)
        return {rightToLeft() ? content_.right() - size.width : content_.x, y};
    return {content_.x - scroll_, y};
}

void LineEdit::scrollToCaret() {
    const text::TextLayout* layout = textLayout();
    if (!layout)
        return;
    const float width = layout->size().width, room = content_.width;
    if (width <= room) {
        scroll_ = 0;
        return;
    }
    const float x = layout->caret(shownOffset(caret_) + codePointBytes(composition_, compositionCursor_)).top.x;
    if (x - scroll_ < 0)
        scroll_ = x;
    else if (x - scroll_ > room - 1)
        scroll_ = x - room + 1;
    scroll_ = std::clamp(scroll_, 0.0f, width - room + 1);
}

std::size_t LineEdit::offsetAt(Point local) {
    const text::TextLayout* layout = textLayout();
    if (!layout)
        return caret_;
    std::size_t hit = layout->hitTest(local - textOrigin());
    // an offset of the shown text in the text: the composition is one place, the caret
    if (!composition_.empty() && hit > caret_)
        hit = hit <= caret_ + composition_.size() ? caret_ : hit - composition_.size();
    return std::min(hit, text_.size());
}

// ---- editing ----

void LineEdit::changed() {
    layout_.reset();
    scrollToCaret();
    updateInputArea();
    invalidatePaint();
}

void LineEdit::edited() {
    changed();
    if (const auto handler = onChanged_)
        handler(text_);
}

void LineEdit::replaceSelection(std::string_view with) {
    const auto [from, to] = selection();
    text_.replace(from, to - from, with);
    caret_ = anchor_ = from + with.size();
    edited();
}

void LineEdit::moveTo(std::size_t offset, bool extend) {
    caret_ = offset;
    if (!extend)
        anchor_ = offset;
    scrollToCaret();
    updateInputArea();
    invalidatePaint();
}

std::size_t LineEdit::visualNeighbour(bool right) {
    const text::TextLayout* layout = textLayout();
    if (!layout)
        return caret_;
    // the caret position nearest on that side of the screen, wherever it is in the text (directions mix)
    const float x = layout->caret(caret_).top.x;
    std::size_t best = caret_;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (std::size_t o = 0;; o = layout->nextOffset(o)) {
        const float dx = layout->caret(o).top.x - x;
        if ((right ? dx > 0.01f : dx < -0.01f) && std::abs(dx) < bestDistance) {
            best = o;
            bestDistance = std::abs(dx);
        }
        if (o >= text_.size())
            break;
    }
    return best;
}

std::size_t LineEdit::wordBoundary(bool forward) const {
    if (!layout_)
        return caret_;
    const text::TextLayout& layout = *layout_;
    std::size_t o = caret_;
    if (forward) {
        while (o < text_.size() && isSpaceAt(text_, o))
            o = layout.nextOffset(o);
        while (o < text_.size() && !isSpaceAt(text_, o))
            o = layout.nextOffset(o);
    } else {
        while (o > 0 && isSpaceAt(text_, layout.previousOffset(o)))
            o = layout.previousOffset(o);
        while (o > 0 && !isSpaceAt(text_, layout.previousOffset(o)))
            o = layout.previousOffset(o);
    }
    return o;
}

void LineEdit::updateInputArea() {
    UiHost* host = hostOf(*this);
    const text::TextLayout* layout = host && hasFocus() ? textLayout() : nullptr;
    if (!layout)
        return;
    const text::TextLayout::Caret caret = layout->caret(shownOffset(caret_));
    const Point origin = textOrigin();
    const Point top = mapToRoot({origin.x + caret.top.x, origin.y + caret.top.y});
    host->setTextInputArea({top.x, top.y, 1, caret.height});
}

// ---- events ----

void LineEdit::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left && event.type != MouseEventType::Move)
        return;
    switch (event.type) {
    case MouseEventType::Press: {
        const std::size_t offset = offsetAt(event.position);
        if (event.clicks == 2) {    // a word
            caret_ = offset;
            textLayout();
            const std::size_t start = wordBoundary(false), end = wordBoundary(true);
            select(start, end);
        } else {
            moveTo(offset, platform::hasAny(event.modifiers, platform::KeyModifiers::Shift));
            selecting_ = true;
        }
        event.handled = true;
        break;
    }
    case MouseEventType::Move:
        if (selecting_) {
            moveTo(offsetAt(event.position), true);
            event.handled = true;
        }
        break;
    case MouseEventType::Release:
        selecting_ = false;
        event.handled = true;
        break;
    case MouseEventType::Wheel:
        break;
    }
}

void LineEdit::onKey(KeyEvent& event) {
    // while an input method composes, the keys are its own
    if (!event.pressed || !composition_.empty())
        return;
    using platform::Key;
    const bool shift = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
    const bool byWord = platform::hasAny(event.modifiers, platform::wordModifier);
    const bool shortcut = platform::hasAny(event.modifiers, platform::shortcutModifier);
    const auto [from, to] = selection();
    const text::TextLayout* layout = textLayout();
    if (!layout)
        return;

    switch (event.key) {
    case Key::Left:
    case Key::Right: {
        const bool right = event.key == Key::Right;
        if (byWord) {
            // a word forward is to the right in left to right text, to the left in right to left text
            const bool rtlText = !layout->lines().empty() && layout->lines()[0].rightToLeft;
            moveTo(wordBoundary(right != rtlText), shift);
        } else if (from != to && !shift) {
            // a selection collapses to its side on the screen
            const float a = layout->caret(from).top.x, b = layout->caret(to).top.x;
            moveTo(right == (a > b) ? from : to, false);
        } else {
            moveTo(visualNeighbour(right), shift);
        }
        break;
    }
    case Key::Home: moveTo(0, shift); break;
    case Key::End: moveTo(text_.size(), shift); break;
    case Key::Backspace:
    case Key::Delete:
        if (from != to) {
            replaceSelection({});
        } else {
            const bool back = event.key == Key::Backspace;
            const std::size_t other = byWord ? wordBoundary(!back) : back ? layout->previousOffset(caret_) : layout->nextOffset(caret_);
            if (other == caret_)
                return;
            anchor_ = other;
            replaceSelection({});
        }
        break;
    case Key::Enter:
        if (const auto handler = onSubmit_)
            handler(text_);
        break;
    case Key::A:
    case Key::C:
    case Key::X:
    case Key::V: {
        if (!shortcut)
            return;
        UiHost* host = hostOf(*this);
        if (event.key == Key::A)
            selectAll();
        else if (event.key == Key::V && host)
            replaceSelection(oneLine(host->clipboardText()));
        else if (from != to && host) {
            host->setClipboardText(text_.substr(from, to - from));
            if (event.key == Key::X)
                replaceSelection({});
        }
        break;
    }
    default:
        return;
    }
    event.handled = true;
}

void LineEdit::onText(TextEvent& event) {
    event.handled = true;
    if (event.composing) {
        composition_ = oneLine(event.text);
        compositionCursor_ = event.cursor;
        changed();
        return;
    }
    composition_.clear();
    replaceSelection(oneLine(event.text));
}

void LineEdit::onFocusChanged(bool focused) {
    if (UiHost* host = hostOf(*this)) {
        if (focused) {
            host->startTextInput();
            updateInputArea();
        } else {
            host->stopTextInput();
        }
    }
    if (!focused && !composition_.empty()) {
        composition_.clear();
        changed();
    }
    selecting_ = false;
    invalidatePaint();
}

void LineEdit::onHoverChanged(bool) {
    invalidatePaint();
}

// ---- painting ----

void LineEdit::paint(PaintContext& context) {
    const LineEditStyle& s = style();
    paint::Painter& p = context.painter;
    const VisualState state{.hovered = hovered(), .focused = hasFocus(), .disabled = !enabledInTree()};
    const Rect r{0, 0, size().width, size().height};
    p.fillRoundedRect(r, s.cornerRadius, s.background.resolve(state));
    if (s.borderWidth > 0)
        p.strokeRoundedRect(r, s.cornerRadius, s.borderWidth, s.border.resolve(state));
    if (state.focused && s.focusLineWidth > 0) {
        // along the bottom, within the rounded corners
        p.pushClip(r);
        p.fillRoundedRect({r.x, r.bottom() - s.focusLineWidth - s.cornerRadius, r.width, s.focusLineWidth + s.cornerRadius},
                          s.cornerRadius, s.focusLine);
        p.fillRect({r.x, r.bottom() - s.focusLineWidth - s.cornerRadius, r.width, s.cornerRadius}, s.background.resolve(state));
        p.popClip();
    }

    const text::TextLayout* layout = textLayout();
    if (!layout)
        return;
    p.pushClip({content_.x, 0, content_.width, size().height});
    if (text_.empty() && composition_.empty()) {
        if (const text::TextLayout* placeholder = placeholderLayout(); placeholder && !placeholder->text().empty()) {
            const float w = placeholder->size().width;
            const Point at{rightToLeft() ? content_.right() - w : content_.x, content_.y + (content_.height - placeholder->size().height) / 2};
            context.drawText(*placeholder, at, s.placeholder);
        }
    }
    const Point origin = textOrigin();
    const auto [from, to] = selection();
    if (hasFocus() && from != to) {
        for (const Rect& sel : layout->selectionRects(shownOffset(from), shownOffset(to)))
            p.fillRect({origin.x + sel.x, origin.y + sel.y, sel.width, sel.height}, s.selection);
    }
    const Color textColor = state.disabled ? context.ui.theme.palette.disabledText : s.text;
    context.drawText(*layout, origin, textColor);
    // the composition, not typed yet: underlined
    if (!composition_.empty()) {
        for (const Rect& c : layout->selectionRects(caret_, caret_ + composition_.size()))
            p.drawLine({origin.x + c.x, origin.y + c.bottom() - 2}, {origin.x + c.right(), origin.y + c.bottom() - 2}, 1, textColor);
    }
    if (hasFocus()) {
        const text::TextLayout::Caret caret = layout->caret(shownOffset(caret_) + codePointBytes(composition_, compositionCursor_));
        p.fillRect({origin.x + caret.top.x - 0.75f, origin.y + caret.top.y, 1.5f, caret.height}, s.caret);
    }
    p.popClip();
}

} // namespace glw
