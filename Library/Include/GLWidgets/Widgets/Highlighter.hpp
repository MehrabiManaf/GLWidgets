// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What colors code in a TextEdit: a line at a time, from the state the line before left it in (a comment or a string not
// closed yet), so that an edit is highlighted again from its line down only as far as the states change, and only as far
// as the lines that show.
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace glw {

enum class TokenKind : std::uint8_t {
    Text,                   // the text's own color
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Preprocessor,
    Function,
};

struct TokenSpan {
    std::uint32_t begin = 0;        // bytes of the line
    std::uint32_t end = 0;
    TokenKind kind = TokenKind::Text;
};

class GLW_EXPORT Highlighter {
public:
    virtual ~Highlighter();
    // The spans of `line` (sorted, not overlapping; what is in none is text), from `state` (0 for the first line): the
    // state it leaves for the next line.
    virtual int highlight(std::string_view line, int state, std::vector<TokenSpan>& spans) const = 0;
};

// C, C++ and what looks like them (C#, Java, JavaScript, shaders): comments (// and /* */, over lines), strings and
// characters, numbers, preprocessor lines, keywords and types (its own lists, or C++'s), what is called (a name before
// an opening parenthesis).
class GLW_EXPORT CppHighlighter : public Highlighter {
public:
    CppHighlighter();
    CppHighlighter(std::vector<std::string> keywords, std::vector<std::string> types);
    int highlight(std::string_view line, int state, std::vector<TokenSpan>& spans) const override;

private:
    std::unordered_set<std::string> keywords_;
    std::unordered_set<std::string> types_;
};

} // namespace glw
