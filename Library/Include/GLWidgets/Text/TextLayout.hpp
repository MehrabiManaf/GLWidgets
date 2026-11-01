// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A text laid out: shaped, broken into lines, placed. Everything a text field needs to show text and edit it.
//
// Each paragraph (the text between line feeds, or other paragraph separators of Unicode) has its direction, given or found from its first strong character, and is
// laid out by the Unicode bidirectional algorithm (SheenBidi): Persian and English mix on a line in the right order. Its
// characters are split into runs of one direction, one script and one font (the first font of the style that has the
// character), shaped by HarfBuzz, and broken into lines where Unicode allows (libunibreak), at most maxWidth wide.
//
// Positions are in dips from the layout's top left corner. Offsets are UTF-8 byte offsets into the text; the ones given
// to caret, selectionRects... are at grapheme boundaries (what a user sees as one character: a letter with its marks).
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace glw::text {

class Font;

enum class TextDirection : std::uint8_t {
    Auto,                   // from the first strong character of each paragraph (left to right when there is none)
    LeftToRight,
    RightToLeft,
};

enum class TextAlignment : std::uint8_t {
    Start,                  // the side a paragraph starts on: left for left to right, right for right to left
    End,
    Left,
    Right,
    Center,
};

struct TextStyle {
    std::vector<const Font*> fonts;     // tried in order for each character; at least one
    float size = 14;                    // dips
    float lineSpacing = 1;              // times the fonts' line height
};

struct TextOptions {
    float maxWidth = std::numeric_limits<float>::infinity();   // lines are broken to fit (a word longer is broken too)
    TextDirection direction = TextDirection::Auto;
    // Lines are aligned in maxWidth, or in the widest line when there is no maxWidth.
    TextAlignment alignment = TextAlignment::Start;
    std::string language;               // BCP 47 ("fa", "en"...), for shaping and line breaking; empty: the system's
};

// A glyph to draw: the pen position on its baseline.
struct PositionedGlyph {
    const Font* font = nullptr;
    std::uint32_t glyph = 0;            // index in the font
    Point position;
    std::uint32_t cluster = 0;          // offset of the first character it shows
};

struct TextLine {
    std::size_t begin = 0;              // offsets of its characters, without the paragraph separator
    std::size_t end = 0;
    float top = 0;
    float height = 0;
    float baseline = 0;                 // from the layout's top
    float left = 0;                     // of its content, after alignment
    float width = 0;                    // of its content, without the spaces it ends with
    std::size_t firstGlyph = 0;         // its glyphs in glyphs(), in visual order (left to right)
    std::size_t glyphCount = 0;
    bool rightToLeft = false;           // the direction of its paragraph
};

class GLW_EXPORT TextLayout {
public:
    TextLayout();
    TextLayout(std::string text, const TextStyle& style, const TextOptions& options = {});
    ~TextLayout();
    TextLayout(TextLayout&&) noexcept;
    TextLayout& operator=(TextLayout&&) noexcept;

    const std::string& text() const;
    const TextStyle& style() const;
    // The widest line's width (without trailing spaces) and the height of all lines.
    Size size() const;
    std::span<const PositionedGlyph> glyphs() const;
    std::span<const TextLine> lines() const;

    // ---- editing ----

    struct Caret {
        Point top;                      // the top of the caret line
        float height = 0;
        std::size_t line = 0;
    };
    // Where the caret goes before the character at `offset` (after the last one at text().size()). At a boundary between
    // runs of two directions, it is where the character at `offset` is.
    Caret caret(std::size_t offset) const;
    // The offset of the caret position nearest to `point`.
    std::size_t hitTest(Point point) const;
    // Rectangles covering the characters in [from, to): one per piece of the selection, several on a line where directions mix.
    std::vector<Rect> selectionRects(std::size_t from, std::size_t to) const;
    // The grapheme boundary after / before `offset` (the text's end / 0 at the ends).
    std::size_t nextOffset(std::size_t offset) const;
    std::size_t previousOffset(std::size_t offset) const;

private:
    struct Data;
    std::unique_ptr<Data> data_;
};

} // namespace glw::text
