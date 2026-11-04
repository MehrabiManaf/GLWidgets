// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Text/TextLayout.hpp>

#include "Text/FontImpl.hpp"
#include "Text/Utf8.hpp"

#include <GLWidgets/Core/Assert.hpp>

#include <SheenBidi/SheenBidi.h>
#include <graphemebreak.h>
#include <linebreak.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string_view>

namespace glw::text {

namespace {

// A glyph as HarfBuzz gives it, in dips.
struct ShapedGlyph {
    std::uint32_t glyph = 0;
    std::uint32_t cluster = 0;
    float advance = 0;
    float xOffset = 0;
    float yOffset = 0;
};

// Characters of one paragraph with one bidi level, one script and one font: what is shaped at once.
bool isArabicScript(hb_script_t script) {
    return script == HB_SCRIPT_ARABIC || script == HB_SCRIPT_SYRIAC || script == HB_SCRIPT_THAANA || script == HB_SCRIPT_NKO;
}

struct Item {
    std::size_t begin = 0;
    std::size_t end = 0;
    SBLevel level = 0;
    hb_script_t script = HB_SCRIPT_COMMON;
    const Font* font = nullptr;
    std::vector<ShapedGlyph> glyphs;    // in visual order within the item (right to left for an odd level)
};

// The characters [begin, end) of a line shown between x0 and x1: the glyphs of one cluster, for the caret.
struct ClusterBox {
    std::size_t begin = 0;
    std::size_t end = 0;
    float x0 = 0;
    float x1 = 0;
    bool rightToLeft = false;
};

bool isSpace(char32_t c) {
    return c == 0x20 || c == 0x09 || c == 0x3000 || (c >= 0x2000 && c <= 0x200A);
}

template <class T, void (*Release)(T)>
struct Releaser {
    void operator()(T object) const {
        if (object)
            Release(object);
    }
};

} // namespace

struct TextLayout::Data {
    std::string text;
    TextStyle style;
    TextOptions options;
    Size size;
    std::vector<PositionedGlyph> glyphs;
    std::vector<TextLine> lines;
    std::vector<ClusterBox> boxes;                  // the lines' boxes one after the other, in visual order
    std::vector<std::pair<std::size_t, std::size_t>> lineBoxes;     // [first, count) of each line's boxes
    std::vector<bool> graphemeBoundary;             // text.size() + 1 entries

    void layout();
    void layoutParagraph(std::size_t begin, std::size_t end, SBAlgorithmRef algorithm, const std::vector<hb_script_t>& scripts,
                         const std::vector<char>& lineBreaks, hb_buffer_t* buffer, float& y);
    const Font* fontFor(char32_t c, const Font* current) const;
    std::size_t graphemesIn(std::size_t begin, std::size_t end) const;
    std::size_t graphemeAt(std::size_t begin, std::size_t index) const;
    float xAt(const ClusterBox& box, std::size_t offset) const;
    std::span<const ClusterBox> boxesOf(std::size_t line) const {
        return std::span(boxes).subspan(lineBoxes[line].first, lineBoxes[line].second);
    }
    std::size_t lineOf(std::size_t offset) const;
};

// ---- layout ----

void TextLayout::Data::layout() {
    GLW_EXPECTS(!style.fonts.empty() && style.fonts.front() != nullptr && style.size > 0);
    static std::once_flag unibreakInitialized;
    std::call_once(unibreakInitialized, [] {
        init_linebreak();
        init_graphemebreak();
    });

    const std::size_t n = text.size();
    const auto* utf8 = reinterpret_cast<const utf8_t*>(text.data());
    const char* language = options.language.empty() ? nullptr : options.language.c_str();

    std::vector<char> lineBreaks(n), graphemeBreaks(n);
    graphemeBoundary.assign(n + 1, false);
    graphemeBoundary[0] = graphemeBoundary[n] = true;
    std::vector<hb_script_t> scripts(n, HB_SCRIPT_COMMON);
    std::unique_ptr<std::remove_pointer_t<SBAlgorithmRef>, Releaser<SBAlgorithmRef, SBAlgorithmRelease>> algorithm;
    if (n > 0) {
        set_linebreaks_utf8(utf8, n, language, lineBreaks.data());
        set_graphemebreaks_utf8(utf8, n, language, graphemeBreaks.data());
        for (std::size_t i = 0; i < n; ++i) {
            if (graphemeBreaks[i] == GRAPHEMEBREAK_BREAK)
                graphemeBoundary[i + 1] = true;
        }

        const SBCodepointSequence sequence{SBStringEncodingUTF8, text.data(), n};
        // scripts, the common characters (spaces, digits, punctuation) given to the script around them
        SBScriptLocatorRef locator = SBScriptLocatorCreate();
        SBScriptLocatorLoadCodepoints(locator, &sequence);
        while (SBScriptLocatorMoveNext(locator)) {
            const SBScriptAgent* agent = SBScriptLocatorGetAgent(locator);
            const hb_script_t script = hb_script_from_iso15924_tag(SBScriptGetUnicodeTag(agent->script));
            std::fill_n(scripts.begin() + static_cast<std::ptrdiff_t>(agent->offset), agent->length, script);
        }
        SBScriptLocatorRelease(locator);
        algorithm.reset(SBAlgorithmCreate(&sequence));
    }

    hb_buffer_t* buffer = hb_buffer_create();
    float y = 0;
    // paragraphs, ended by the separators of Unicode (line feed, carriage return and both, U+2029...); after a last
    // separator, an empty one
    std::size_t begin = 0;
    SBUInteger separator = 0;
    while (begin < n) {
        SBUInteger length = 0;
        SBAlgorithmGetParagraphBoundary(algorithm.get(), begin, n - begin, &length, &separator);
        layoutParagraph(begin, begin + length - separator, algorithm.get(), scripts, lineBreaks, buffer, y);
        begin += length;
    }
    if (n == 0 || separator > 0)
        layoutParagraph(n, n, algorithm.get(), scripts, lineBreaks, buffer, y);
    hb_buffer_destroy(buffer);

    // alignment, now that the widest line is known
    float widest = 0;
    for (const TextLine& line : lines)
        widest = std::max(widest, line.width);
    const float box = std::isfinite(options.maxWidth) ? options.maxWidth : widest;
    for (std::size_t l = 0; l < lines.size(); ++l) {
        TextLine& line = lines[l];
        TextAlignment alignment = options.alignment;
        if (alignment == TextAlignment::Start)
            alignment = line.rightToLeft ? TextAlignment::Right : TextAlignment::Left;
        else if (alignment == TextAlignment::End)
            alignment = line.rightToLeft ? TextAlignment::Left : TextAlignment::Right;
        const float left = alignment == TextAlignment::Left    ? 0.0f
                           : alignment == TextAlignment::Right ? box - line.width
                                                               : (box - line.width) / 2;
        const float shift = left - line.left;       // line.left: where its content is before alignment
        for (std::size_t g = line.firstGlyph; g < line.firstGlyph + line.glyphCount; ++g)
            glyphs[g].position.x += shift;
        for (std::size_t b = lineBoxes[l].first; b < lineBoxes[l].first + lineBoxes[l].second; ++b) {
            boxes[b].x0 += shift;
            boxes[b].x1 += shift;
        }
        line.left = left;
    }
    size = Size{widest, y};
}

void TextLayout::Data::layoutParagraph(std::size_t begin, std::size_t end, SBAlgorithmRef algorithm,
                                       const std::vector<hb_script_t>& scripts, const std::vector<char>& lineBreaks,
                                       hb_buffer_t* buffer, float& y) {
    const std::string_view view(text);
    const SBLevel requested = options.direction == TextDirection::LeftToRight   ? 0
                              : options.direction == TextDirection::RightToLeft ? 1
                                                                                : SBLevelDefaultLTR;
    std::unique_ptr<std::remove_pointer_t<SBParagraphRef>, Releaser<SBParagraphRef, SBParagraphRelease>> paragraph;
    const SBLevel* levels = nullptr;
    SBLevel baseLevel = requested == 1 ? 1 : 0;
    if (end > begin) {
        paragraph.reset(SBAlgorithmCreateParagraph(algorithm, begin, end - begin, requested));
        levels = SBParagraphGetLevelsPtr(paragraph.get());
        baseLevel = SBParagraphGetBaseLevel(paragraph.get());
    }

    // ---- items ----
    std::vector<Item> items;
    const Font* current = nullptr;
    for (std::size_t i = begin; i < end;) {
        const auto [c, length] = decodeUtf8(view, i);
        const SBLevel level = levels[i - begin];
        const Font* font = fontFor(c, current);
        if (items.empty() || items.back().level != level || items.back().script != scripts[i] || items.back().font != font)
            items.push_back(Item{i, i + length, level, scripts[i], font, {}});
        else
            items.back().end = i + length;
        current = font;
        i += length;
    }

    // ---- shaping ----
    std::vector<float> advance(text.size() + 1, 0.0f);     // per cluster, at its first byte
    const hb_language_t language =
        options.language.empty() ? hb_language_get_default() : hb_language_from_string(options.language.c_str(), -1);
    for (Item& item : items) {
        hb_buffer_clear_contents(buffer);
        // the whole text, so that shaping sees the characters around the item (Persian letters join across a font change)
        hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), static_cast<unsigned>(item.begin),
                           static_cast<int>(item.end - item.begin));
        hb_buffer_set_direction(buffer, item.level & 1 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, item.script);
        hb_buffer_set_language(buffer, language);
        hb_shape(item.font->impl().hbFont, buffer, nullptr, 0);
        unsigned count = 0;
        const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
        const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &count);
        const float scale = style.size / item.font->impl().unitsPerEm;
        item.glyphs.reserve(count);
        for (unsigned k = 0; k < count; ++k) {
            const ShapedGlyph g{infos[k].codepoint, infos[k].cluster, positions[k].x_advance * scale,
                                positions[k].x_offset * scale, positions[k].y_offset * scale};
            item.glyphs.push_back(g);
            advance[g.cluster] += g.advance;
        }
    }
    const auto widthOf = [&](std::size_t from, std::size_t to) {
        float w = 0;
        for (std::size_t i = from; i < to; ++i)
            w += advance[i];
        return w;
    };

    // ---- lines: greedy, at the break opportunities of UAX #14 ----
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    std::size_t lineStart = begin;
    std::size_t lastBreak = std::string::npos;
    float width = 0;
    for (std::size_t i = begin; i < end;) {
        const auto [c, length] = decodeUtf8(view, i);
        const float w = widthOf(i, i + length);
        // a space may hang past the end of a line; anything else that does not fit starts a new one
        if (!isSpace(c) && width + w > options.maxWidth && i > lineStart) {
            std::size_t cut = lastBreak != std::string::npos && lastBreak > lineStart ? lastBreak : i;
            if (cut == i) {     // a word longer than the line: broken between graphemes
                while (cut > lineStart && !graphemeBoundary[cut])
                    --cut;
                if (cut == lineStart)
                    cut = i;
            }
            ranges.emplace_back(lineStart, cut);
            lineStart = cut;
            lastBreak = std::string::npos;
            width = widthOf(lineStart, i);
        }
        width += w;
        if (lineBreaks[i + length - 1] == LINEBREAK_ALLOWBREAK)
            lastBreak = i + length;
        i += length;
    }
    ranges.emplace_back(lineStart, end);

    // ---- each line: bidi runs in visual order, their glyphs left to right ----
    const FontMetrics primary = style.fonts.front()->metrics(style.size);
    for (const auto& [lineBegin, lineEnd] : ranges) {
        TextLine line;
        line.begin = lineBegin;
        line.end = lineEnd;
        line.rightToLeft = baseLevel & 1;
        line.firstGlyph = glyphs.size();
        const std::size_t firstBox = boxes.size();

        FontMetrics metrics = primary;
        for (const Item& item : items) {
            if (item.end > lineBegin && item.begin < lineEnd) {
                const FontMetrics m = item.font->metrics(style.size);
                metrics.ascent = std::max(metrics.ascent, m.ascent);
                metrics.descent = std::max(metrics.descent, m.descent);
                metrics.lineGap = std::max(metrics.lineGap, m.lineGap);
            }
        }
        line.top = y;
        line.height = metrics.lineHeight() * style.lineSpacing;
        line.baseline = y + (line.height - metrics.ascent - metrics.descent) / 2 + metrics.ascent;
        // looking centered: the middle of its letters where its height's middle is (Arabic script is the font's own)
        bool arabic = false;
        for (const Item& item : items) {
            if (item.end > lineBegin && item.begin < lineEnd)
                arabic = arabic || isArabicScript(item.script);
        }
        if (!arabic) {
            const float letters = (primary.xHeight + primary.capHeight) / 4;    // the middle of them, above the baseline
            const float middle = (metrics.ascent - metrics.descent) / 2;         // the height's, above it
            line.opticalShift = letters - middle;
        }

        float x = 0;
        if (lineEnd > lineBegin) {
            std::unique_ptr<std::remove_pointer_t<SBLineRef>, Releaser<SBLineRef, SBLineRelease>> sbLine(
                SBParagraphCreateLine(paragraph.get(), lineBegin, lineEnd - lineBegin));
            const SBRun* runs = SBLineGetRunsPtr(sbLine.get());
            const SBUInteger runCount = SBLineGetRunCount(sbLine.get());
            for (SBUInteger r = 0; r < runCount; ++r) {
                const std::size_t runBegin = runs[r].offset, runEnd = runs[r].offset + runs[r].length;
                const bool runRtl = runs[r].level & 1;
                const auto emit = [&](const Item& item, const ShapedGlyph& g) {
                    if (g.cluster < runBegin || g.cluster >= runEnd)
                        return;
                    glyphs.push_back(PositionedGlyph{item.font, g.glyph, {x + g.xOffset, line.baseline - g.yOffset}, g.cluster});
                    if (boxes.size() > firstBox && boxes.back().begin == g.cluster)
                        boxes.back().x1 = x + g.advance;
                    else
                        boxes.push_back(ClusterBox{g.cluster, 0, x, x + g.advance, runRtl});
                    x += g.advance;
                };
                // the items of the run in its direction; an item of the other direction (spaces moved to the line's end)
                // has its glyphs reversed
                const auto emitItem = [&](const Item& item) {
                    if (item.end <= runBegin || item.begin >= runEnd)
                        return;
                    if (static_cast<bool>(item.level & 1) == runRtl)
                        std::for_each(item.glyphs.begin(), item.glyphs.end(), [&](const ShapedGlyph& g) { emit(item, g); });
                    else
                        std::for_each(item.glyphs.rbegin(), item.glyphs.rend(), [&](const ShapedGlyph& g) { emit(item, g); });
                };
                if (runRtl)
                    std::for_each(items.rbegin(), items.rend(), emitItem);
                else
                    std::for_each(items.begin(), items.end(), emitItem);
            }
        }
        line.glyphCount = glyphs.size() - line.firstGlyph;

        // a box ends where the next cluster of the line begins
        std::vector<std::size_t> starts;
        for (std::size_t b = firstBox; b < boxes.size(); ++b)
            starts.push_back(boxes[b].begin);
        std::sort(starts.begin(), starts.end());
        for (std::size_t b = firstBox; b < boxes.size(); ++b) {
            const auto next = std::upper_bound(starts.begin(), starts.end(), boxes[b].begin);
            boxes[b].end = next == starts.end() ? lineEnd : *next;
        }
        lineBoxes.emplace_back(firstBox, boxes.size() - firstBox);

        // the spaces the line ends with do not count: they hang past its end (on the left in right to left)
        std::size_t contentEnd = lineEnd;
        while (contentEnd > lineBegin && isSpace(static_cast<unsigned char>(text[contentEnd - 1])))
            --contentEnd;
        const float trailing = widthOf(contentEnd, lineEnd);
        line.width = x - trailing;
        line.left = line.rightToLeft ? trailing : 0;
        lines.push_back(line);
        y += line.height;
    }
}

const Font* TextLayout::Data::fontFor(char32_t c, const Font* current) const {
    // spaces, punctuation and marks stay in the font of what is around them when it has them
    const hb_script_t script = hb_unicode_script(hb_unicode_funcs_get_default(), c);
    if (current && (script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED) && current->hasCharacter(c))
        return current;
    for (const Font* font : style.fonts) {
        if (font->hasCharacter(c))
            return font;
    }
    return current ? current : style.fonts.front();
}

// ---- editing ----

std::size_t TextLayout::Data::graphemesIn(std::size_t begin, std::size_t end) const {
    std::size_t count = 0;
    for (std::size_t i = begin + 1; i <= end; ++i)
        count += graphemeBoundary[i] ? 1 : 0;
    return std::max<std::size_t>(count, 1);
}

std::size_t TextLayout::Data::graphemeAt(std::size_t begin, std::size_t index) const {
    std::size_t i = begin;
    for (std::size_t k = 0; k < index && i < text.size(); ++k) {
        ++i;
        while (i < text.size() && !graphemeBoundary[i])
            ++i;
    }
    return i;
}

float TextLayout::Data::xAt(const ClusterBox& box, std::size_t offset) const {
    // a cluster of several graphemes (a ligature) is shared evenly between them
    std::size_t before = 0;
    for (std::size_t i = box.begin + 1; i <= offset; ++i)
        before += graphemeBoundary[i] ? 1 : 0;
    const float fraction = static_cast<float>(before) / static_cast<float>(graphemesIn(box.begin, box.end));
    return box.rightToLeft ? box.x1 - fraction * (box.x1 - box.x0) : box.x0 + fraction * (box.x1 - box.x0);
}

std::size_t TextLayout::Data::lineOf(std::size_t offset) const {
    std::size_t line = 0;
    while (line + 1 < lines.size() && lines[line + 1].begin <= offset)
        ++line;
    return line;
}

TextLayout::TextLayout() : TextLayout(std::string(), TextStyle{}, {}) {}

TextLayout::TextLayout(std::string text, const TextStyle& style, const TextOptions& options) : data_(std::make_unique<Data>()) {
    data_->text = std::move(text);
    data_->style = style;
    data_->options = options;
    if (!data_->style.fonts.empty())
        data_->layout();
    else
        data_->graphemeBoundary.assign(data_->text.size() + 1, true);
}

TextLayout::~TextLayout() = default;
TextLayout::TextLayout(TextLayout&&) noexcept = default;
TextLayout& TextLayout::operator=(TextLayout&&) noexcept = default;

const std::string& TextLayout::text() const {
    return data_->text;
}

const TextStyle& TextLayout::style() const {
    return data_->style;
}

Size TextLayout::size() const {
    return data_->size;
}

float TextLayout::opticalOffset() const {
    return data_->lines.size() == 1 ? data_->lines.front().opticalShift : 0.0f;
}

std::span<const PositionedGlyph> TextLayout::glyphs() const {
    return data_->glyphs;
}

std::span<const TextLine> TextLayout::lines() const {
    return data_->lines;
}

TextLayout::Caret TextLayout::caret(std::size_t offset) const {
    const Data& d = *data_;
    if (d.lines.empty())
        return {};
    offset = std::min(offset, d.text.size());
    const std::size_t l = d.lineOf(offset);
    const TextLine& line = d.lines[l];
    const auto boxes = d.boxesOf(l);
    float x = line.left + (line.rightToLeft ? line.width : 0);
    const ClusterBox* last = nullptr;
    for (const ClusterBox& box : boxes) {
        if (box.begin <= offset && offset < box.end) {
            x = d.xAt(box, offset);
            last = nullptr;
            break;
        }
        if (box.begin < offset && (!last || box.begin > last->begin))
            last = &box;
    }
    // after the line's last character: its trailing edge
    if (last)
        x = last->rightToLeft ? last->x0 : last->x1;
    return {Point{x, line.top}, line.height, l};
}

std::size_t TextLayout::hitTest(Point point) const {
    const Data& d = *data_;
    if (d.lines.empty())
        return 0;
    std::size_t l = 0;
    while (l + 1 < d.lines.size() && point.y >= d.lines[l].top + d.lines[l].height)
        ++l;
    const auto boxes = d.boxesOf(l);
    if (boxes.empty())
        return d.lines[l].begin;

    // the box under x (boxes are left to right, one after the other), or the one at the side x is past
    const ClusterBox* box = &boxes.front();
    float fraction = 0;
    if (point.x >= boxes.back().x1) {
        box = &boxes.back();
        fraction = 1;
    } else if (point.x > boxes.front().x0) {
        for (const ClusterBox& b : boxes) {
            if (point.x < b.x1) {
                box = &b;
                fraction = b.x1 > b.x0 ? (point.x - b.x0) / (b.x1 - b.x0) : 0;
                break;
            }
        }
    }
    if (box->rightToLeft)
        fraction = 1 - fraction;
    const std::size_t graphemes = d.graphemesIn(box->begin, box->end);
    const auto index = static_cast<std::size_t>(std::lround(fraction * static_cast<float>(graphemes)));
    return std::min(d.graphemeAt(box->begin, index), box->end);
}

std::vector<Rect> TextLayout::selectionRects(std::size_t from, std::size_t to) const {
    const Data& d = *data_;
    if (from > to)
        std::swap(from, to);
    std::vector<Rect> rects;
    for (std::size_t l = 0; l < d.lines.size(); ++l) {
        const TextLine& line = d.lines[l];
        if (line.end < from || line.begin >= to)
            continue;
        const std::size_t first = rects.size();
        for (const ClusterBox& box : d.boxesOf(l)) {
            const std::size_t a = std::max(from, box.begin), b = std::min(to, box.end);
            if (a >= b)
                continue;
            const float x0 = d.xAt(box, a);
            const float x1 = b >= box.end ? (box.rightToLeft ? box.x0 : box.x1) : d.xAt(box, b);
            rects.push_back(Rect{std::min(x0, x1), line.top, std::abs(x1 - x0), line.height});
        }
        // pieces next to each other on the line become one
        std::sort(rects.begin() + static_cast<std::ptrdiff_t>(first), rects.end(),
                  [](const Rect& a, const Rect& b) { return a.x < b.x; });
        std::size_t kept = first;
        for (std::size_t i = first; i < rects.size(); ++i) {
            if (i > first && rects[i].x <= rects[kept - 1].right() + 0.5f)
                rects[kept - 1].width = std::max(rects[kept - 1].right(), rects[i].right()) - rects[kept - 1].x;
            else
                rects[kept++] = rects[i];
        }
        rects.resize(kept);
    }
    return rects;
}

std::size_t TextLayout::nextOffset(std::size_t offset) const {
    const Data& d = *data_;
    for (std::size_t i = offset + 1; i < d.graphemeBoundary.size(); ++i) {
        if (d.graphemeBoundary[i])
            return i;
    }
    return d.text.size();
}

std::size_t TextLayout::previousOffset(std::size_t offset) const {
    const Data& d = *data_;
    offset = std::min(offset, d.text.size());
    while (offset > 0) {
        --offset;
        if (d.graphemeBoundary[offset])
            return offset;
    }
    return 0;
}

} // namespace glw::text
