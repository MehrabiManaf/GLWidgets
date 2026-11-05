// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/TextEdit.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float gutterPad = 12;         // on each side of the line numbers
constexpr float textPad = 6;            // between the gutter and the text
constexpr int wheelLines = 3;
constexpr std::size_t keptAround = 200; // lines laid out kept past those that show, when there are many

bool wordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           static_cast<unsigned char>(c) >= 0x80;
}

bool space(char c) {
    return c == ' ' || c == '\t';
}

// The lines of a text, at \n, \r\n and \r: a line at a time, not a character.
std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1);
    std::size_t at = 0;
    for (;;) {
        const std::size_t end = text.find_first_of("\r\n", at);
        if (end == std::string_view::npos) {
            lines.emplace_back(text.substr(at));
            return lines;
        }
        lines.emplace_back(text.substr(at, end - at));
        at = end + (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' ? 2 : 1);
    }
}

std::string joined(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out += '\n';
        out += lines[i];
    }
    return out;
}

// The text with its line ends as \n only.
std::string normalized(std::string_view text) {
    return joined(splitLines(text));
}

std::size_t leadingSpaces(const std::string& line) {
    std::size_t n = 0;
    while (n < line.size() && space(line[n]))
        ++n;
    return n;
}

} // namespace

TextEdit::TextEdit() {
    setFocusable(true);
    setClipsChildren(true);
    setLayoutDirection(LayoutDirection::LeftToRight);       // code reads so, whatever the language around it
}

TextEdit::~TextEdit() = default;

const TextEditStyle& TextEdit::style() const {
    if (const UiContext* ui = context())
        return ui->theme.textEdit;
    static const TextEditStyle unstyled = Theme::dark().textEdit;
    return unstyled;
}

// ---- the text ----

void TextEdit::setText(std::string_view text) {
    lines_ = splitLines(text);
    caret_ = anchor_ = {};
    column_.reset();
    undo_.clear();
    redo_.clear();
    offsetY_ = 0;
    offsetX_ = 0;
    changed(0, 1);          // every line again
    invalidateLayout();
}

std::string TextEdit::text() const {
    return joined(lines_);
}

std::string TextEdit::text(const TextRange& range) const {
    const TextPosition from = clamped(std::min(range.from, range.to)), to = clamped(std::max(range.from, range.to));
    if (from.line == to.line)
        return lines_[from.line].substr(from.column, to.column - from.column);
    std::string out = lines_[from.line].substr(from.column);
    for (std::size_t l = from.line + 1; l < to.line; ++l)
        out += '\n' + lines_[l];
    out += '\n' + lines_[to.line].substr(0, to.column);
    return out;
}

TextPosition TextEdit::endOf(TextPosition at, std::string_view text) const {
    const std::size_t breaks = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
    if (breaks == 0)
        return {at.line, at.column + text.size()};
    return {at.line + breaks, text.size() - text.rfind('\n') - 1};
}

TextPosition TextEdit::apply(const Edit& edit) {
    const TextPosition end = endOf(edit.at, edit.removed);
    std::vector<std::string> put = splitLines(edit.inserted);
    const std::string prefix = lines_[edit.at.line].substr(0, edit.at.column);
    const std::string suffix = lines_[end.line].substr(end.column);
    const TextPosition after{edit.at.line + put.size() - 1, (put.size() == 1 ? edit.at.column : 0) + put.back().size()};
    put.front() = prefix + put.front();
    put.back() += suffix;
    const std::size_t removedLines = end.line - edit.at.line + 1;
    const std::ptrdiff_t added = static_cast<std::ptrdiff_t>(put.size()) - static_cast<std::ptrdiff_t>(removedLines);
    // the lines in place: as many as there were, then those more inserted or those less erased
    const std::size_t common = std::min(put.size(), removedLines);
    for (std::size_t i = 0; i < common; ++i)
        lines_[edit.at.line + i] = std::move(put[i]);
    if (added > 0)
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(edit.at.line + common), std::make_move_iterator(put.begin() + static_cast<std::ptrdiff_t>(common)),
                      std::make_move_iterator(put.end()));
    else if (added < 0)
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(edit.at.line + common),
                     lines_.begin() + static_cast<std::ptrdiff_t>(edit.at.line + removedLines));
    changed(edit.at.line, added);
    return after;
}

void TextEdit::changed(std::size_t line, std::ptrdiff_t linesAdded) {
    // laid out again: that line, every one after it when lines came or went (they moved)
    if (linesAdded != 0) {
        cache_.clear();
        widest_ = 0;
    } else {
        cache_.erase(line);
        // colored from the state the line before leaves: those after it, maybe otherwise now
        if (highlighter_) {
            for (auto it = cache_.begin(); it != cache_.end();) {
                if (it->first > line)
                    it = cache_.erase(it);
                else
                    ++it;
            }
        }
    }
    statesKnown_ = std::min(statesKnown_, line + 1);
    if (states_.size() != lines_.size())
        states_.resize(lines_.size(), 0);
    invalidateLayout();
}

void TextEdit::record(Step step) {
    redo_.clear();
    // characters typed one after the other on a line: one step
    if (step.typing && !undo_.empty() && undo_.back().typing && undo_.back().edits.size() == 1 && step.edits.size() == 1) {
        Edit& last = undo_.back().edits.front();
        const Edit& now = step.edits.front();
        if (now.removed.empty() && last.removed.empty() && endOf(last.at, last.inserted) == now.at) {
            last.inserted += now.inserted;
            undo_.back().anchorAfter = step.anchorAfter;
            undo_.back().caretAfter = step.caretAfter;
            return;
        }
    }
    undo_.push_back(std::move(step));
}

void TextEdit::edit(const TextRange& range, std::string_view text, bool typing) {
    const TextPosition from = clamped(std::min(range.from, range.to)), to = clamped(std::max(range.from, range.to));
    std::string inserted = normalized(text);
    if (from == to && inserted.empty())
        return;
    Step step{.edits = {{from, this->text({from, to}), std::move(inserted)}}, .anchorBefore = anchor_, .caretBefore = caret_, .typing = typing};
    const TextPosition end = apply(step.edits.front());
    caret_ = anchor_ = end;
    column_.reset();
    step.anchorAfter = step.caretAfter = end;
    record(std::move(step));
    scrollTo(caret_);
    restartBlink();
    if (onChanged_)
        onChanged_();
}

TextPosition TextEdit::replace(const TextRange& range, std::string_view text) {
    edit(range, text, false);
    return caret_;
}

void TextEdit::append(std::string_view text) {
    // following the end: still at it after
    const bool atEnd = offsetY_ >= maximumOffsetY() - 0.5;
    const TextPosition end{lines_.size() - 1, lines_.back().size()};
    const std::string put = (lines_.size() == 1 && lines_.back().empty() ? "" : "\n") + normalized(text);
    Step step{.edits = {{end, std::string(), put}}, .anchorBefore = anchor_, .caretBefore = caret_, .anchorAfter = anchor_, .caretAfter = caret_};
    apply(step.edits.front());
    record(std::move(step));
    if (atEnd) {
        offsetY_ = std::max(0.0, static_cast<double>(lines_.size()) * lineHeight_ - text_.height);
        invalidatePaint();
    }
    if (onChanged_)
        onChanged_();
}

void TextEdit::typed(std::string_view text) {
    if (readOnly_)
        return;
    const bool typing = text.size() <= 4 && text.find('\n') == std::string_view::npos && text != " ";
    edit(selection(), text, typing && !hasSelection());
}

// ---- undo ----

void TextEdit::undo() {
    if (undo_.empty())
        return;
    Step step = std::move(undo_.back());
    undo_.pop_back();
    for (auto it = step.edits.rbegin(); it != step.edits.rend(); ++it)
        apply({it->at, it->inserted, it->removed});
    anchor_ = clamped(step.anchorBefore);
    caret_ = clamped(step.caretBefore);
    redo_.push_back(std::move(step));
    scrollTo(caret_);
    if (onChanged_)
        onChanged_();
}

void TextEdit::redo() {
    if (redo_.empty())
        return;
    Step step = std::move(redo_.back());
    redo_.pop_back();
    for (const Edit& e : step.edits)
        apply(e);
    anchor_ = clamped(step.anchorAfter);
    caret_ = clamped(step.caretAfter);
    undo_.push_back(std::move(step));
    scrollTo(caret_);
    if (onChanged_)
        onChanged_();
}

// ---- the caret and the selection ----

TextRange TextEdit::selection() const {
    return {std::min(anchor_, caret_), std::max(anchor_, caret_)};
}

TextPosition TextEdit::clamped(TextPosition p) const {
    p.line = std::min(p.line, lines_.size() - 1);
    p.column = std::min(p.column, lines_[p.line].size());
    return p;
}

void TextEdit::select(TextPosition anchor, TextPosition caret) {
    anchor_ = clamped(anchor);
    caret_ = clamped(caret);
    column_.reset();
    scrollTo(caret_);
    restartBlink();
    invalidatePaint();
}

void TextEdit::selectAll() {
    anchor_ = {};
    caret_ = {lines_.size() - 1, lines_.back().size()};
    invalidatePaint();
}

void TextEdit::copy() {
    UiHost* host = root() ? root()->host() : nullptr;
    if (host && hasSelection())
        host->setClipboardText(text(selection()));
}

void TextEdit::cut() {
    copy();
    if (!readOnly_ && hasSelection())
        edit(selection(), "", false);
}

void TextEdit::paste() {
    UiHost* host = root() ? root()->host() : nullptr;
    if (host && !readOnly_)
        typed(host->clipboardText());
}

void TextEdit::moveTo(TextPosition p, bool extend, bool keepColumn) {
    caret_ = clamped(p);
    if (!extend)
        anchor_ = caret_;
    if (!keepColumn)
        column_.reset();
    scrollTo(caret_);
    restartBlink();
    updateInputArea();
    invalidatePaint();
}

TextPosition TextEdit::nextPosition(TextPosition p, bool byWord) {
    const std::string& line = lines_[p.line];
    if (p.column >= line.size())
        return p.line + 1 < lines_.size() ? TextPosition{p.line + 1, 0} : p;
    if (byWord) {
        // over the spaces, then over a word (or over the other characters)
        std::size_t c = p.column;
        while (c < line.size() && space(line[c]))
            ++c;
        if (c < line.size() && wordChar(line[c])) {
            while (c < line.size() && wordChar(line[c]))
                ++c;
        } else {
            while (c < line.size() && !wordChar(line[c]) && !space(line[c]))
                ++c;
        }
        return {p.line, c};
    }
    return {p.line, laid(p.line).layout->nextOffset(p.column)};
}

TextPosition TextEdit::previousPosition(TextPosition p, bool byWord) {
    if (p.column == 0)
        return p.line > 0 ? TextPosition{p.line - 1, lines_[p.line - 1].size()} : p;
    const std::string& line = lines_[p.line];
    if (byWord) {
        std::size_t c = p.column;
        while (c > 0 && space(line[c - 1]))
            --c;
        if (c > 0 && wordChar(line[c - 1])) {
            while (c > 0 && wordChar(line[c - 1]))
                --c;
        } else {
            while (c > 0 && !wordChar(line[c - 1]) && !space(line[c - 1]))
                --c;
        }
        return {p.line, c};
    }
    return {p.line, laid(p.line).layout->previousOffset(p.column)};
}

TextPosition TextEdit::vertical(TextPosition p, std::ptrdiff_t lines) {
    if (!column_)
        column_ = laid(p.line).layout->caret(p.column).top.x;
    const std::ptrdiff_t target = std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(p.line) + lines, 0, static_cast<std::ptrdiff_t>(lines_.size()) - 1);
    if (lines < 0 && p.line == 0)
        return {0, 0};
    if (lines > 0 && p.line + 1 == lines_.size())
        return {p.line, lines_[p.line].size()};
    const std::size_t line = static_cast<std::size_t>(target);
    return {line, laid(line).layout->hitTest({*column_, lineHeight_ / 2})};
}

TextPosition TextEdit::home(TextPosition p) const {
    // the first character that is not a space, then the line's start
    const std::size_t first = leadingSpaces(lines_[p.line]);
    return {p.line, p.column == first ? 0 : first};
}

TextRange TextEdit::wordAt(TextPosition p) const {
    const std::string& line = lines_[p.line];
    std::size_t from = p.column, to = p.column;
    const auto same = [&](char c) {
        if (from < line.size())
            return wordChar(line[from]) == wordChar(c) && space(line[from]) == space(c);
        return wordChar(c);
    };
    if (from >= line.size() && from > 0)
        --from, to = from;
    while (from > 0 && same(line[from - 1]))
        --from;
    while (to < line.size() && same(line[to]))
        ++to;
    return {{p.line, from}, {p.line, to}};
}

void TextEdit::indent(bool out) {
    if (readOnly_)
        return;
    const TextRange sel = selection();
    std::size_t last = sel.to.line;
    if (sel.to.column == 0 && last > sel.from.line)
        --last;                                 // a selection ending at a line's start leaves that line
    const std::size_t width = static_cast<std::size_t>(style().tabWidth);
    Step step{.anchorBefore = anchor_, .caretBefore = caret_};
    for (std::size_t l = sel.from.line; l <= last; ++l) {
        if (out) {
            const std::size_t n = std::min(leadingSpaces(lines_[l]), width);
            if (n > 0)
                step.edits.push_back({{l, 0}, lines_[l].substr(0, n), std::string()});
        } else {
            step.edits.push_back({{l, 0}, std::string(), std::string(width, ' ')});
        }
    }
    if (step.edits.empty())
        return;
    for (const Edit& e : step.edits)
        apply(e);
    anchor_ = {sel.from.line, 0};
    caret_ = {last, lines_[last].size()};
    step.anchorAfter = anchor_;
    step.caretAfter = caret_;
    record(std::move(step));
    invalidatePaint();
    if (onChanged_)
        onChanged_();
}

// ---- how it shows ----

void TextEdit::setReadOnly(bool readOnly) {
    readOnly_ = readOnly;
    restartBlink();
    invalidatePaint();
}

void TextEdit::setLineNumbers(bool shown) {
    lineNumbers_ = shown;
    invalidateLayout();
}

void TextEdit::setHighlighter(std::unique_ptr<Highlighter> highlighter) {
    highlighter_ = std::move(highlighter);
    statesKnown_ = 0;
    states_.assign(lines_.size(), 0);
    cache_.clear();
    invalidatePaint();
}

void TextEdit::setFonts(std::vector<const text::Font*> fonts) {
    fonts_ = std::move(fonts);
    onStyleChanged();
}

std::vector<const text::Font*> TextEdit::fonts() const {
    std::vector<const text::Font*> result = fonts_;
    if (const UiContext* ui = context()) {
        const text::Font* fallback = &ui->fonts.defaultFont();
        if (std::find(result.begin(), result.end(), fallback) == result.end())
            result.push_back(fallback);
    }
    return result;
}

void TextEdit::measureLine() {
    if (!context())
        return;
    // a line's height and a digit's width, from the fonts
    const text::TextLayout sample("0", text::TextStyle{fonts(), style().fontSize});
    if (!sample.lines().empty()) {
        lineHeight_ = std::ceil(sample.lines()[0].height);
        baseline_ = sample.lines()[0].baseline;
    }
    digitWidth_ = sample.size().width;
}

void TextEdit::highlightTo(std::size_t line) {
    if (states_.size() != lines_.size())
        states_.resize(lines_.size(), 0);
    if (statesKnown_ == 0) {
        states_[0] = 0;
        statesKnown_ = 1;
    }
    std::vector<TokenSpan> scratch;
    for (std::size_t l = statesKnown_; l <= line && l < lines_.size(); ++l) {
        scratch.clear();
        states_[l] = highlighter_->highlight(lines_[l - 1], states_[l - 1], scratch);
        statesKnown_ = l + 1;
    }
}

const TextEdit::Laid& TextEdit::laid(std::size_t line) {
    if (const auto it = cache_.find(line); it != cache_.end())
        return it->second;
    Laid laid;
    text::TextOptions options;
    options.direction = text::TextDirection::LeftToRight;
    laid.layout = std::make_unique<text::TextLayout>(lines_[line], text::TextStyle{fonts(), style().fontSize}, options);
    widest_ = std::max(widest_, laid.layout->size().width);
    if (highlighter_) {
        highlightTo(line);
        std::vector<TokenSpan> tokens;
        highlighter_->highlight(lines_[line], states_[line], tokens);
        const TextEditStyle& s = style();
        for (const TokenSpan& t : tokens) {
            Color color = s.text;
            switch (t.kind) {
            case TokenKind::Text: continue;
            case TokenKind::Keyword: color = s.keyword; break;
            case TokenKind::Type: color = s.type; break;
            case TokenKind::String: color = s.string; break;
            case TokenKind::Number: color = s.number; break;
            case TokenKind::Comment: color = s.comment; break;
            case TokenKind::Preprocessor: color = s.preprocessor; break;
            case TokenKind::Function: color = s.function; break;
            }
            laid.colors.push_back({t.begin, t.end, color});
        }
    }
    return cache_.emplace(line, std::move(laid)).first->second;
}

void TextEdit::trimCache() {
    // many laid out: those far from what shows go
    const auto [first, last] = shownLines();
    if (cache_.size() <= (last - first) + 2 * keptAround)
        return;
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->first + keptAround < first || it->first > last + keptAround)
            it = cache_.erase(it);
        else
            ++it;
    }
}

// ---- scrolling ----

float TextEdit::gutterWidth() const {
    if (!lineNumbers_)
        return 0;
    std::size_t digits = 1;
    for (std::size_t n = lines_.size(); n >= 10; n /= 10)
        ++digits;
    return static_cast<float>(std::max<std::size_t>(digits, 3)) * digitWidth_ + 2 * gutterPad;
}

double TextEdit::maximumOffsetY() const {
    return std::max(0.0, static_cast<double>(lines_.size()) * lineHeight_ - text_.height);
}

float TextEdit::maximumOffsetX() const {
    return std::max(0.0f, widest_ + textPad + 4 * digitWidth_ - text_.width);
}

void TextEdit::setVerticalOffset(double offset) {
    smoothY_.stop();
    showY(offset);
}

void TextEdit::setHorizontalOffset(float offset) {
    smoothX_.stop();
    showX(offset);
}

void TextEdit::scrollSmoothly(double vertical, float horizontal) {
    const UiContext* ui = context();
    const Duration duration = ui ? Duration(ui->theme.motion.scroll) : Duration::zero();
    // to the nearest line's top (or the end): the line at the top shows whole
    const double limit = maximumOffsetY();
    vertical = std::clamp(vertical, 0.0, limit);
    if (vertical < limit - 0.5 && lineHeight_ > 0)
        vertical = std::min(limit, std::round(vertical / lineHeight_) * lineHeight_);
    horizontal = std::clamp(horizontal, 0.0f, maximumOffsetX());
    if (vertical != smoothY_.target(offsetY_))
        smoothY_.scrollTo(scheduler(), offsetY_, vertical, duration);
    if (horizontal != static_cast<float>(smoothX_.target(offsetX_)))
        smoothX_.scrollTo(scheduler(), offsetX_, horizontal, duration);
}

void TextEdit::showY(double offset) {
    offset = std::clamp(offset, 0.0, maximumOffsetY());
    if (offset == offsetY_)
        return;
    offsetY_ = offset;
    updateInputArea();
    invalidatePaint();
}

void TextEdit::showX(float offset) {
    offset = std::clamp(offset, 0.0f, maximumOffsetX());
    if (offset == offsetX_)
        return;
    offsetX_ = offset;
    invalidatePaint();
}

std::pair<std::size_t, std::size_t> TextEdit::shownLines() const {
    if (lineHeight_ <= 0 || text_.height <= 0)
        return {0, 0};
    const std::size_t first = std::min(static_cast<std::size_t>(offsetY_ / lineHeight_), lines_.size());
    const std::size_t last = std::min(static_cast<std::size_t>(std::ceil((offsetY_ + text_.height) / lineHeight_)), lines_.size());
    return {first, last};
}

std::size_t TextEdit::pageLines() const {
    return std::max<std::size_t>(1, static_cast<std::size_t>(text_.height / lineHeight_) - 1);
}

void TextEdit::scrollTo(TextPosition position) {
    if (text_.height <= 0)
        return;
    const double top = static_cast<double>(position.line) * lineHeight_;
    if (top < offsetY_)
        setVerticalOffset(top);
    else if (top + lineHeight_ > offsetY_ + text_.height)
        setVerticalOffset(top + lineHeight_ - text_.height);
    // across, with a few characters' room past the caret
    const float x = laid(position.line).layout->caret(position.column).top.x;
    widest_ = std::max(widest_, x);
    const float room = 4 * digitWidth_;
    if (x < offsetX_ + room)
        setHorizontalOffset(x - room);
    else if (x + room > offsetX_ + text_.width - textPad)
        setHorizontalOffset(x + room - text_.width + textPad);
}

Point TextEdit::pointOf(TextPosition position) {
    position = clamped(position);
    const float x = laid(position.line).layout->caret(position.column).top.x;
    return {text_.x + textPad - offsetX_ + x, text_.y + static_cast<float>(static_cast<double>(position.line) * lineHeight_ - offsetY_)};
}

TextPosition TextEdit::positionAt(Point point) {
    const double y = point.y - text_.y + offsetY_;
    const std::size_t line = y <= 0 ? 0 : std::min(static_cast<std::size_t>(y / lineHeight_), lines_.size() - 1);
    const float x = point.x - (text_.x + textPad) + offsetX_;
    return {line, laid(line).layout->hitTest({x, lineHeight_ / 2})};
}

// ---- layout ----

Size TextEdit::measureContent(Size available) {
    return {std::isfinite(available.width) ? available.width : 480.0f, std::isfinite(available.height) ? available.height : 320.0f};
}

void TextEdit::arrangeContent(const Rect& content) {
    area_ = content;
    const float gutter = gutterWidth();
    const float barWidth = context() ? context()->theme.scrollBar.width : 8.0f;
    text_ = {content.x + gutter, content.y, std::max(0.0f, content.width - gutter), content.height};
    showBar_ = static_cast<double>(lines_.size()) * lineHeight_ > content.height + 0.5;
    if (showBar_)
        text_.width = std::max(0.0f, text_.width - barWidth);
    offsetY_ = std::clamp(offsetY_, 0.0, maximumOffsetY());
    offsetX_ = std::clamp(offsetX_, 0.0f, maximumOffsetX());
}

// ---- painting ----

void TextEdit::paint(PaintContext& context) {
    const TextEditStyle& s = style();
    paint::Painter& p = context.painter;
    p.fillRect(area_, s.background);
    if (lineNumbers_)
        p.fillRect({area_.x, area_.y, text_.x - area_.x, area_.height}, s.gutter);
    trimCache();
    const auto [first, last] = shownLines();
    const TextRange sel = selection();
    const bool focused = hasFocus();
    const std::vector<const text::Font*> numberFonts = fonts();

    p.pushClip(text_);
    for (std::size_t line = first; line < last; ++line) {
        const float y = text_.y + static_cast<float>(static_cast<double>(line) * lineHeight_ - offsetY_);
        const Laid& l = laid(line);
        const Point origin{text_.x + textPad - offsetX_, y};
        if (line == caret_.line && !hasSelection() && !readOnly_)
            p.fillRect({text_.x, y, text_.width, lineHeight_}, s.currentLine);
        // the selection: its part of the line, and the line's end when it goes on past it
        if (!sel.empty() && line >= sel.from.line && line <= sel.to.line) {
            const std::size_t from = line == sel.from.line ? sel.from.column : 0;
            const std::size_t to = line == sel.to.line ? sel.to.column : lines_[line].size();
            for (const Rect& r : l.layout->selectionRects(from, to))
                p.fillRect({origin.x + r.x, y, r.width, lineHeight_}, s.selection);
            if (line < sel.to.line)
                p.fillRect({origin.x + l.layout->size().width, y, digitWidth_ / 2, lineHeight_}, s.selection);
        }
        context.drawText(*l.layout, origin, s.text, l.colors);
    }
    // the caret: gliding to where it went (the theme's fast motion), in the text's coordinates (scrolling moves it with
    // the text)
    if (focused && !readOnly_) {
        const Point scrolled{text_.x + textPad - offsetX_, text_.y - static_cast<float>(offsetY_)};
        const Point at = pointOf(caret_) - scrolled;
        if (!caretPlaced_) {
            caretPlaced_ = true;
            caretFrom_ = caretTo_ = at;
            caretGlide_.set(1);
        } else if (at != caretTo_) {
            caretFrom_ = caretFrom_ + (caretTo_ - caretFrom_) * caretGlide_.value();
            caretTo_ = at;
            caretGlide_.set(0);
            caretGlide_.animateTo(scheduler(), 1, context.ui.theme.motion.fast, Easing::OutCubic);
        }
        const bool gliding = caretGlide_.running();
        const Point shown = scrolled + caretFrom_ + (caretTo_ - caretFrom_) * caretGlide_.value();
        if ((caretShown_ || gliding) && caret_.line + 1 >= first && caret_.line <= last)
            p.fillRect({shown.x - 0.75f, shown.y, 1.5f, lineHeight_}, s.caret);
    } else {
        caretPlaced_ = false;       // focused again: where it is, at once
    }
    p.popClip();

    // the line numbers, at the end of the gutter
    if (lineNumbers_) {
        p.pushClip({area_.x, area_.y, text_.x - area_.x, area_.height});
        for (std::size_t line = first; line < last; ++line) {
            const float y = text_.y + static_cast<float>(static_cast<double>(line) * lineHeight_ - offsetY_);
            const text::TextLayout number(std::to_string(line + 1), text::TextStyle{numberFonts, s.fontSize});
            const float x = text_.x - gutterPad - number.size().width;
            context.drawText(number, {x, y}, line == caret_.line ? s.currentLineNumber : s.lineNumber);
        }
        p.popClip();
    }
}

Rect TextEdit::verticalTrack() const {
    const float width = context() ? context()->theme.scrollBar.width : 8.0f;
    return {area_.right() - width, area_.y, width, area_.height};
}

Rect TextEdit::verticalThumb() const {
    const Rect track = verticalTrack();
    const double total = std::max(1.0, static_cast<double>(lines_.size()) * lineHeight_);
    const float minimum = context() ? context()->theme.scrollBar.minimumThumb : 24.0f;
    const float length = std::min(track.height, std::max(minimum, static_cast<float>(track.height * text_.height / total)));
    const double room = maximumOffsetY();
    const float at = room > 0 ? static_cast<float>((track.height - length) * offsetY_ / room) : 0.0f;
    return {track.x, track.y + at, track.width, length};
}

void TextEdit::paintOver(PaintContext& context) {
    if (!showBar_)
        return;
    const ScrollBarStyle& s = context.ui.theme.scrollBar;
    context.painter.fillRect(verticalTrack(), s.track);
    const VisualState state{.hovered = barHovered_ || thumbGrab_, .pressed = thumbGrab_.has_value()};
    barWide_.animateTo(scheduler(), state.hovered ? 1.0f : 0.0f, context.ui.theme.motion.fast);
    paintScrollThumb(context.painter, verticalThumb(), true, true, barWide_.value(), s.thumb.resolve(state), s.cornerRadius);
}

// ---- the caret's blink, the input method ----

void TextEdit::restartBlink() {
    caretShown_ = true;
    blinks_ = 0;
    Scheduler* s = scheduler();
    if (!s || !hasFocus() || readOnly_) {
        blink_.stop();
        return;
    }
    blink_.start(*s, std::chrono::milliseconds(530), [this] {
        caretShown_ = !caretShown_;
        invalidatePaint();
        if (++blinks_ >= 20 && caretShown_)
            blink_.stop();
    }, true);
}

void TextEdit::updateInputArea() {
    UiRoot* r = root();
    if (!r || !r->host() || !hasFocus() || readOnly_)
        return;
    const Point at = mapToRoot(pointOf(caret_));
    r->host()->setTextInputArea({at.x, at.y, 1, lineHeight_});
}

// ---- events ----

std::optional<platform::Cursor> TextEdit::cursorAt(Point point) const {
    // over its lines, and while they are selected by a drag; the arrow over its bar
    const bool overBar = showBar_ && verticalTrack().contains(point);
    if (selecting_ || (!thumbGrab_ && !overBar && text_.contains(point)))
        return platform::Cursor::Text;
    return Widget::cursorAt(point);
}

void TextEdit::onHoverChanged(bool hovered) {
    if (!hovered && barHovered_) {
        barHovered_ = false;
        invalidatePaint();
    }
}

void TextEdit::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Wheel: {
        const bool across = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift) || event.wheelDelta.x != 0;
        // from where it is going: turned again, it goes further
        const double y = smoothY_.target(offsetY_);
        const float x = static_cast<float>(smoothX_.target(offsetX_));
        if (across) {
            const float by = event.wheelDelta.x != 0 ? event.wheelDelta.x : -event.wheelDelta.y;
            scrollSmoothly(y, x + static_cast<float>(smoothX_.boosted(by * wheelLines * 2 * digitWidth_)));
        } else {
            scrollSmoothly(y + smoothY_.boosted(-event.wheelDelta.y * wheelLines * lineHeight_), x);
        }
        event.handled = true;
        break;
    }
    case MouseEventType::Press: {
        if (event.button != platform::MouseButton::Left)
            return;
        event.handled = true;
        focus();
        if (showBar_ && verticalTrack().contains(event.position)) {
            const Rect thumb = verticalThumb();
            if (thumb.contains(event.position))
                thumbGrab_ = event.position.y - thumb.y;
            else
                scrollSmoothly(smoothY_.target(offsetY_) + (event.position.y < thumb.y ? -1.0 : 1.0) * text_.height,
                               static_cast<float>(smoothX_.target(offsetX_)));
            invalidatePaint();
            return;
        }
        const TextPosition at = positionAt(event.position);
        clicks_ = std::min(event.clicks, 3);
        if (clicks_ == 2) {
            const TextRange word = wordAt(at);
            anchor_ = word.from;
            moveTo(word.to, true);
        } else if (clicks_ == 3) {
            anchor_ = {at.line, 0};
            moveTo(at.line + 1 < lines_.size() ? TextPosition{at.line + 1, 0} : TextPosition{at.line, lines_[at.line].size()}, true);
        } else {
            moveTo(at, platform::hasAny(event.modifiers, platform::KeyModifiers::Shift));
        }
        selecting_ = true;
        break;
    }
    case MouseEventType::Move:
        if (const bool over = showBar_ && verticalTrack().contains(event.position); over != barHovered_) {
            barHovered_ = over;
            invalidatePaint();
        }
        if (thumbGrab_) {
            const Rect track = verticalTrack();
            const double room = std::max(1.0f, track.height - verticalThumb().height);
            setVerticalOffset((event.position.y - *thumbGrab_ - track.y) / room * maximumOffsetY());
            event.handled = true;
        } else if (selecting_) {
            moveTo(positionAt(event.position), true);
            event.handled = true;
        }
        break;
    case MouseEventType::Release:
        if (event.button != platform::MouseButton::Left)
            return;
        selecting_ = false;
        thumbGrab_.reset();
        invalidatePaint();
        event.handled = true;
        break;
    }
}

void TextEdit::onKey(KeyEvent& event) {
    if (!event.pressed)
        return;
    using platform::Key;
    const bool shift = platform::hasAny(event.modifiers, platform::KeyModifiers::Shift);
    const bool byWord = platform::hasAny(event.modifiers, platform::wordModifier);
    const bool shortcut = platform::hasAny(event.modifiers, platform::shortcutModifier);
    const TextRange sel = selection();
    switch (event.key) {
    case Key::Left:
        if (hasSelection() && !shift)
            moveTo(sel.from, false);
        else
            moveTo(previousPosition(caret_, byWord), shift);
        break;
    case Key::Right:
        if (hasSelection() && !shift)
            moveTo(sel.to, false);
        else
            moveTo(nextPosition(caret_, byWord), shift);
        break;
    case Key::Up: moveTo(vertical(caret_, -1), shift, true); break;
    case Key::Down: moveTo(vertical(caret_, 1), shift, true); break;
    case Key::PageUp: moveTo(vertical(caret_, -static_cast<std::ptrdiff_t>(pageLines())), shift, true); break;
    case Key::PageDown: moveTo(vertical(caret_, static_cast<std::ptrdiff_t>(pageLines())), shift, true); break;
    case Key::Home: moveTo(shortcut ? TextPosition{} : home(caret_), shift); break;
    case Key::End:
        moveTo(shortcut ? TextPosition{lines_.size() - 1, lines_.back().size()} : TextPosition{caret_.line, lines_[caret_.line].size()}, shift);
        break;
    case Key::Backspace:
        if (readOnly_)
            return;
        if (hasSelection()) {
            edit(sel, "", false);
        } else if (!byWord && caret_.column > 0 && leadingSpaces(lines_[caret_.line]) >= caret_.column) {
            // in the indent: back to the tab stop before
            const std::size_t width = static_cast<std::size_t>(style().tabWidth);
            const std::size_t to = (caret_.column - 1) / width * width;
            edit({{caret_.line, to}, caret_}, "", false);
        } else {
            edit({previousPosition(caret_, byWord), caret_}, "", false);
        }
        break;
    case Key::Delete:
        if (readOnly_)
            return;
        edit(hasSelection() ? sel : TextRange{caret_, nextPosition(caret_, byWord)}, "", false);
        break;
    case Key::Enter: {
        if (readOnly_)
            return;
        // the indent of the line, as far as the caret
        const std::string& line = lines_[sel.from.line];
        typed("\n" + line.substr(0, std::min(leadingSpaces(line), sel.from.column)));
        break;
    }
    case Key::Tab:
        if (readOnly_)
            return;
        if (shift || (hasSelection() && sel.from.line != sel.to.line)) {
            indent(shift);
        } else {
            const std::size_t width = static_cast<std::size_t>(style().tabWidth);
            typed(std::string(width - sel.from.column % width, ' '));
        }
        break;
    case Key::A:
        if (!shortcut)
            return;
        selectAll();
        break;
    case Key::C:
    case Key::X:
        if (!shortcut)
            return;
        if (event.key == Key::X)
            cut();
        else
            copy();
        break;
    case Key::V:
        if (!shortcut || readOnly_)
            return;
        paste();
        break;
    case Key::Z:
        if (!shortcut || readOnly_)
            return;
        if (shift)
            redo();
        else
            undo();
        break;
    case Key::Y:
        if (!shortcut || readOnly_)
            return;
        redo();
        break;
    default: return;
    }
    event.handled = true;
}

void TextEdit::onText(TextEvent& event) {
    // what an input method composes is typed when it is done
    if (event.composing || readOnly_)
        return;
    typed(event.text);
    event.handled = true;
}

void TextEdit::onFocusChanged(bool focused) {
    if (UiHost* host = root() ? root()->host() : nullptr) {
        if (focused && !readOnly_) {
            host->startTextInput();
            updateInputArea();
        } else if (!focused) {
            host->stopTextInput();
        }
    }
    restartBlink();
    invalidatePaint();
}

void TextEdit::onRootChanged() {
    onStyleChanged();
}

void TextEdit::onStyleChanged() {
    measureLine();
    cache_.clear();
    widest_ = 0;
    invalidateLayout();
}

} // namespace glw
