// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A text of many lines, edited or only read: code (with line numbers, a highlighter, a monospace font), a log appended to,
// a field of several lines. Its lines are kept apart, and only those that show are laid out and painted (laid out once,
// kept while they do not change): a text of a million lines costs what shows. Lines are not wrapped; it scrolls both
// ways.
//
// Each line is laid out left to right whatever is in it (code reads so), the Persian or Arabic in its strings and comments
// shaped and ordered as it is written. Positions are lines and bytes in them (UTF-8); the caret moves by graphemes.
//
// The keys: arrows (with the word modifier: by words; Up and Down keep the column they started from), Home (to the first
// character that is not a space, then the line's start), End, Page Up / Down, Ctrl+Home / End; Shift extends the
// selection. Enter keeps the line's indent; Tab indents the lines selected (or types spaces), Shift+Tab unindents them;
// Backspace and Delete (by words with the modifier); Ctrl+A, C, X, V, Z, Y (and Shift+Ctrl+Z). The mouse: a click puts the
// caret, a drag selects, a double click selects a word, a triple one a line; the wheel scrolls (across with Shift).
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Text/DrawText.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Highlighter.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <compare>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glw::text {
class Font;
class TextLayout;
} // namespace glw::text

namespace glw {

// A place in a text: a line and a byte offset in it.
struct TextPosition {
    std::size_t line = 0;
    std::size_t column = 0;
    friend auto operator<=>(const TextPosition&, const TextPosition&) = default;
};

struct TextRange {
    TextPosition from;
    TextPosition to;
    bool empty() const { return from == to; }
};

class GLW_EXPORT TextEdit : public Widget {
public:
    TextEdit();
    ~TextEdit() override;

    // ---- the text ----

    // Its text, lines separated by \n (\r\n and \r read as \n). Undo forgets.
    void setText(std::string_view text);
    std::string text() const;
    std::size_t lineCount() const { return lines_.size(); }
    const std::string& line(std::size_t index) const { return lines_[index]; }
    std::string text(const TextRange& range) const;
    // `range` replaced by `text` (undone as one edit): where the text put ends.
    TextPosition replace(const TextRange& range, std::string_view text);
    // At its end, on lines of their own after what is there (a log); it stays scrolled to its end when it was there.
    void append(std::string_view text);
    // Called after it changed (by the user or by the calls above but setText).
    void setOnChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }

    // ---- the caret and the selection ----

    TextPosition caret() const { return caret_; }
    TextRange selection() const;            // in order
    bool hasSelection() const { return anchor_ != caret_; }
    // The caret at `caret`, the selection from `anchor` (the caret's place: none), scrolled to it.
    void select(TextPosition anchor, TextPosition caret);
    void selectAll();
    // The selection to the clipboard (cut: and out of the text, unless read only); the clipboard's text over it.
    void cut();
    void copy();
    void paste();

    // ---- undo ----

    void undo();
    void redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    // ---- how it shows ----

    // Read only: no caret, no edits by the user (selected and copied still); what calls replace and append still edits it.
    void setReadOnly(bool readOnly);
    bool readOnly() const { return readOnly_; }
    // Line numbers in a gutter at its start (on by default).
    void setLineNumbers(bool shown);
    // What colors it (none by default).
    void setHighlighter(std::unique_ptr<Highlighter> highlighter);
    // The fonts of its text, tried in order for each character (a monospace font for code); the default font is tried
    // after them. None: the default font.
    void setFonts(std::vector<const text::Font*> fonts);

    // ---- scrolling ----

    // Dips scrolled from the top / from the start.
    double verticalOffset() const { return offsetY_; }
    float horizontalOffset() const { return offsetX_; }
    void setVerticalOffset(double offset);
    void setHorizontalOffset(float offset);
    // Scrolled as little as it takes for `position` to show.
    void scrollTo(TextPosition position);
    float lineHeight() const { return lineHeight_; }
    // The lines that show now: [first, last).
    std::pair<std::size_t, std::size_t> shownLines() const;
    // The lines laid out and kept (those that showed lately).
    std::size_t laidOutLines() const { return cache_.size(); }
    // Where the text of a position is (its coordinates: the caret's top), and what position is at a point.
    Point pointOf(TextPosition position);
    TextPosition positionAt(Point point);

    const TextEditStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    Rect childrenClip() const override { return text_; }
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onText(TextEvent& event) override;
    void onFocusChanged(bool focused) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    // A line laid out, with its colors.
    struct Laid {
        std::unique_ptr<text::TextLayout> layout;
        std::vector<text::ColorSpan> colors;
    };
    // An edit, as undo does it again: `inserted` put at `at` where `removed` was.
    struct Edit {
        TextPosition at;
        std::string removed;
        std::string inserted;
    };
    struct Step {                           // what one undo takes back
        std::vector<Edit> edits;
        TextPosition anchorBefore, caretBefore, anchorAfter, caretAfter;
        bool typing = false;                // characters typed one after the other: one step
    };

    // The text changed: the lines from `line` on are laid out and highlighted again, `count` lines more (less) than
    // before.
    TextPosition apply(const Edit& edit);
    TextPosition endOf(TextPosition at, std::string_view text) const;
    void changed(std::size_t line, std::ptrdiff_t linesAdded);
    void record(Step step);
    void edit(const TextRange& range, std::string_view text, bool typing);
    void typed(std::string_view text);

    const Laid& laid(std::size_t line);
    void highlightTo(std::size_t line);     // the states of the lines before it known
    void trimCache();
    std::vector<const text::Font*> fonts() const;
    void measureLine();

    // moving
    TextPosition clamped(TextPosition p) const;
    TextPosition nextPosition(TextPosition p, bool byWord);
    TextPosition previousPosition(TextPosition p, bool byWord);
    TextPosition vertical(TextPosition p, std::ptrdiff_t lines);
    TextPosition home(TextPosition p) const;
    void moveTo(TextPosition p, bool extend, bool keepColumn = false);
    TextRange wordAt(TextPosition p) const;
    void indent(bool out);

    // geometry
    float gutterWidth() const;
    double maximumOffsetY() const;
    float maximumOffsetX() const;
    std::size_t pageLines() const;
    Rect verticalTrack() const;
    Rect verticalThumb() const;
    void restartBlink();
    void updateInputArea();

    std::vector<std::string> lines_{std::string()};
    TextPosition caret_, anchor_;
    std::optional<float> column_;           // where Up and Down keep the caret (dips from the text's start)
    std::vector<Step> undo_, redo_;
    std::function<void()> onChanged_;

    bool readOnly_ = false;
    bool lineNumbers_ = true;
    std::unique_ptr<Highlighter> highlighter_;
    std::vector<int> states_;               // the highlighter's state at each line's start, known up to statesKnown_
    std::size_t statesKnown_ = 0;
    std::vector<const text::Font*> fonts_;

    std::unordered_map<std::size_t, Laid> cache_;
    float lineHeight_ = 18;
    float baseline_ = 14;
    float digitWidth_ = 8;
    float widest_ = 0;                      // of the lines laid out so far

    Rect area_;                             // its content rectangle
    Rect text_;                             // where the lines show: less the gutter and the bar
    double offsetY_ = 0;
    float offsetX_ = 0;
    bool showBar_ = false;
    std::optional<double> thumbGrab_;
    bool selecting_ = false;
    int clicks_ = 1;                        // how the drag selects: characters, words, lines

    Timer blink_;
    bool caretShown_ = true;
    int blinks_ = 0;
};

} // namespace glw
