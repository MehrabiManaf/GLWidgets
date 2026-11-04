// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The text layout with the default font: shaping, the order of mixed directions, line breaking, carets and selections.
#include "Check.hpp"

#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/TextLayout.hpp>

#include <cmath>
#include <string>

using namespace glw;
using namespace glw::text;

namespace {

const FontLibrary& fonts() {
    static FontLibrary library;
    return library;
}

TextLayout layout(std::string s, TextOptions options = {}) {
    return TextLayout(std::move(s), TextStyle{{&fonts().defaultFont()}, 16}, options);
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.01f;
}

// Every grapheme boundary: the caret there, hit in its middle, gives it back.
void checkCaretsRoundTrip(const TextLayout& t) {
    for (std::size_t o = 0;; o = t.nextOffset(o)) {
        const TextLayout::Caret c = t.caret(o);
        const std::size_t hit = t.hitTest({c.top.x, c.top.y + c.height / 2});
        if (!CHECK(hit == o))
            std::printf("      offset %zu came back as %zu\n", o, hit);
        if (o == t.text().size())
            break;
    }
}

} // namespace

TEST(latinLine) {
    const TextLayout t = layout("Hello world");
    CHECK(t.lines().size() == 1);
    CHECK(t.glyphs().size() == 11);
    CHECK(t.size().width > 0 && t.size().height > 0);
    CHECK(!t.lines()[0].rightToLeft);
    for (std::size_t i = 1; i < t.glyphs().size(); ++i) {
        CHECK(t.glyphs()[i].position.x > t.glyphs()[i - 1].position.x);
        CHECK(t.glyphs()[i].cluster > t.glyphs()[i - 1].cluster);
    }
}

TEST(persianIsRightToLeft) {
    const TextLayout t = layout("سلام دنیا");
    CHECK(t.lines().size() == 1);
    CHECK(t.lines()[0].rightToLeft);
    // the leftmost glyph is the last letter
    CHECK(t.glyphs().front().cluster > t.glyphs().back().cluster);
}

TEST(persianLettersJoin) {
    // سین alone and at the start of a word are different glyphs: the shaper chose the joining form
    const TextLayout alone = layout("س");
    const TextLayout word = layout("سلام");
    std::uint32_t initial = 0;
    for (const PositionedGlyph& g : word.glyphs()) {
        if (g.cluster == 0)
            initial = g.glyph;
    }
    CHECK(alone.glyphs().size() == 1);
    CHECK(initial != 0 && initial != alone.glyphs()[0].glyph);
}

TEST(mixedDirections) {
    // "سلام" bytes 0-7, a space, "hello" bytes 9-13, a space, "دنیا"
    const TextLayout t = layout("سلام hello دنیا");
    CHECK(t.lines()[0].rightToLeft);
    float helloRight = 0, salamLeft = 1e9f;
    std::uint32_t previousHello = 0;
    bool helloInOrder = true;
    for (const PositionedGlyph& g : t.glyphs()) {
        if (g.cluster >= 9 && g.cluster < 14) {
            helloInOrder = helloInOrder && g.cluster >= previousHello;
            previousHello = g.cluster;
            helloRight = std::max(helloRight, g.position.x);
        }
        if (g.cluster < 8)
            salamLeft = std::min(salamLeft, g.position.x);
    }
    CHECK(helloInOrder);                // left to right inside the right to left line
    CHECK(helloRight < salamLeft);      // the first word is on the right
}

TEST(wrappedLineStartingWithLatin) {
    // the second line starts with "hello": in a right to left paragraph, its first word is on the right
    const std::string s = "سلام دنیا دنیا hello سلام";
    const float limit = layout("سلام دنیا دنیا").size().width + 2;
    const TextLayout t = layout(s, {.maxWidth = limit});
    if (!CHECK(t.lines().size() == 2)) {
        for (const TextLine& l : t.lines())
            std::printf("      line [%zu, %zu) width %g of %g\n", l.begin, l.end, l.width, limit);
        return;
    }
    const TextLine& line = t.lines()[1];
    CHECK(line.rightToLeft);
    const std::size_t hello = s.find("hello");
    float helloLeft = 1e9f, persianRight = -1e9f;
    for (std::size_t g = line.firstGlyph; g < line.firstGlyph + line.glyphCount; ++g) {
        const PositionedGlyph& glyph = t.glyphs()[g];
        if (glyph.cluster >= hello && glyph.cluster < hello + 5)
            helloLeft = std::min(helloLeft, glyph.position.x);
        else if (glyph.cluster > hello + 5)
            persianRight = std::max(persianRight, glyph.position.x);
    }
    if (!CHECK(helloLeft > persianRight))
        std::printf("      hello starts at %g, the Persian word ends at %g\n", helloLeft, persianRight);
}

TEST(rightToLeftLinesStartOnTheRight) {
    // a paragraph of the demo, broken into several lines: on each, the words after the first are left of it
    const std::string s =
        "زبان فارسی از راست به چپ نوشته می‌شود و حروفش به هم می‌چسبند؛ شکل هر حرف به جایش در کلمه بستگی دارد. "
        "GLWidgets متن را با HarfBuzz شکل می‌دهد و با الگوریتم دوجهتهٔ یونیکد مرتب می‌کند، پس عددهایی مثل ۱۴۰۵ و 2026 "
        "و کلمه‌های انگلیسی مثل FlowGraph در جای درست خود می‌نشینند.";
    for (float width : {250.0f, 330.0f, 474.0f}) {
        const TextLayout t = layout(s, {.maxWidth = width});
        CHECK(t.lines().size() > 1);
        for (const TextLine& line : t.lines()) {
            CHECK(line.rightToLeft);
            const std::size_t space = s.find(' ', line.begin);
            float first = 0;
            for (std::size_t g = line.firstGlyph; g < line.firstGlyph + line.glyphCount; ++g) {
                if (t.glyphs()[g].cluster == line.begin)
                    first = t.glyphs()[g].position.x;
            }
            for (std::size_t g = line.firstGlyph; g < line.firstGlyph + line.glyphCount; ++g) {
                const PositionedGlyph& glyph = t.glyphs()[g];
                if (glyph.cluster > space && glyph.cluster < line.end && !CHECK(glyph.position.x < first)) {
                    std::printf("      line [%zu, %zu): %zu is at %g, the first character at %g\n", line.begin, line.end,
                                std::size_t(glyph.cluster), glyph.position.x, first);
                    break;
                }
            }
        }
    }
}

TEST(lineBreaking) {
    const std::string s = "one two three four five six seven";
    const float limit = layout("one two three").size().width + 1;
    const TextLayout t = layout(s, {.maxWidth = limit});
    CHECK(t.lines().size() >= 3);
    std::size_t next = 0;
    for (const TextLine& line : t.lines()) {
        CHECK(line.begin == next);      // the lines cover the text, one after the other
        CHECK(line.width <= limit + 0.01f);
        next = line.end;
    }
    CHECK(next == s.size());
    CHECK(t.lines()[0].end == 14);      // "one two three " - the space hangs at the end of the line
}

TEST(longWordIsBroken) {
    const TextLayout t = layout("abcdefghijklmnopqrstuvwxyz", {.maxWidth = 40});
    CHECK(t.lines().size() > 2);
    for (const TextLine& line : t.lines())
        CHECK(line.end > line.begin);
}

TEST(paragraphs) {
    const TextLayout t = layout("a\nb\r\nc\n");
    CHECK(t.lines().size() == 4);       // the last one empty, after the last line feed
    CHECK(t.lines()[1].begin == 2 && t.lines()[1].end == 3);
    CHECK(t.lines()[2].begin == 5);
    CHECK(t.lines()[3].begin == 7 && t.lines()[3].end == 7);
    CHECK(t.lines()[3].top > t.lines()[2].top);
}

TEST(caretsLeftToRight) {
    const TextLayout t = layout("Hello world");
    const TextLine& line = t.lines()[0];
    CHECK(near(t.caret(0).top.x, line.left));
    CHECK(near(t.caret(t.text().size()).top.x, line.left + line.width));
    checkCaretsRoundTrip(t);
}

TEST(caretsRightToLeft) {
    const TextLayout t = layout("سلام دنیا");
    const TextLine& line = t.lines()[0];
    CHECK(near(t.caret(0).top.x, line.left + line.width));     // the first letter is on the right
    CHECK(near(t.caret(t.text().size()).top.x, line.left));
    checkCaretsRoundTrip(t);
}

TEST(graphemes) {
    const TextLayout t = layout("e\xCC\x81x");      // e, a combining acute accent (2 bytes), x
    CHECK(t.nextOffset(0) == 3);
    CHECK(t.previousOffset(3) == 0);
    CHECK(t.nextOffset(3) == 4);
    CHECK(t.nextOffset(4) == 4);
    CHECK(t.previousOffset(0) == 0);
}

TEST(selectionOfAWholeLine) {
    for (const char* s : {"Hello world", "سلام hello دنیا"}) {
        const TextLayout t = layout(s);
        const auto rects = t.selectionRects(0, t.text().size());
        CHECK(rects.size() == 1);       // the pieces of the runs, next to each other, are one
        CHECK(near(rects[0].width, t.lines()[0].width));
    }
}

TEST(selectionAcrossDirections) {
    // the end of "hello" and the start of "دنیا": two pieces apart on the line
    const TextLayout t = layout("سلام hello دنیا");
    const auto rects = t.selectionRects(12, 19);
    CHECK(rects.size() == 2);
}

TEST(alignment) {
    const TextLayout center = layout("Hello", {.maxWidth = 500, .alignment = TextAlignment::Center});
    const TextLine& line = center.lines()[0];
    CHECK(near(line.left, (500 - line.width) / 2));
    const TextLayout end = layout("Hello", {.maxWidth = 500, .alignment = TextAlignment::End});
    CHECK(near(end.lines()[0].left + end.lines()[0].width, 500));
    const TextLayout persian = layout("سلام", {.maxWidth = 500});     // Start is on the right
    CHECK(near(persian.lines()[0].left + persian.lines()[0].width, 500));
}

TEST(emptyText) {
    const TextLayout t = layout("");
    CHECK(t.lines().size() == 1);
    CHECK(t.glyphs().empty());
    CHECK(t.hitTest({10, 5}) == 0);
    CHECK(t.caret(0).height > 0);
}

TEST(aLineOfLatinIsCenteredByItsLetters) {
    // the default font (Vazirmatn) has room below for Persian: Latin letters' middle is above its height's
    const FontMetrics m = fonts().defaultFont().metrics(16);
    CHECK(m.capHeight > m.xHeight && m.xHeight > 0 && m.capHeight < m.ascent);
    const TextLayout latin = layout("Open");
    const float expected = (m.xHeight + m.capHeight) / 4 - (m.ascent - m.descent) / 2;
    CHECK(expected > 0.5f && std::abs(latin.opticalOffset() - expected) < 0.01f);
    // a line with Persian: the font's height is centered on it already; and a text of several lines is not centered
    CHECK(layout("سلام").opticalOffset() == 0);
    CHECK(layout("Script ۱ سلام").opticalOffset() == 0);
    CHECK(layout("one\ntwo").opticalOffset() == 0);
}
