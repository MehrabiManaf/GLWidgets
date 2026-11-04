// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Highlighter.hpp>

#include <algorithm>

namespace glw {

namespace {

constexpr int inComment = 1;            // a /* */ comment goes on to the next line

bool wordStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || static_cast<unsigned char>(c) >= 0x80;
}

bool wordPart(char c) {
    return wordStart(c) || (c >= '0' && c <= '9');
}

bool digit(char c) {
    return c >= '0' && c <= '9';
}

} // namespace

Highlighter::~Highlighter() = default;

CppHighlighter::CppHighlighter()
    : CppHighlighter({"alignas",   "alignof",   "and",       "asm",          "auto",      "break",    "case",     "catch",
                      "class",     "co_await",  "co_return", "co_yield",     "concept",   "const",    "consteval", "constexpr",
                      "constinit", "const_cast", "continue", "decltype",     "default",   "delete",   "do",       "dynamic_cast",
                      "else",      "enum",      "explicit",  "export",       "extern",    "false",    "final",    "for",
                      "friend",    "goto",      "if",        "import",       "inline",    "module",   "mutable",  "namespace",
                      "new",       "noexcept",  "not",       "nullptr",      "operator",  "or",       "override", "private",
                      "protected", "public",    "register",  "reinterpret_cast", "requires", "return", "sizeof", "static",
                      "static_assert", "static_cast", "struct", "switch",    "template",  "this",     "thread_local", "throw",
                      "true",      "try",       "typedef",   "typeid",       "typename",  "union",    "using",    "virtual",
                      "volatile",  "while"},
                     {"bool", "char", "char8_t", "char16_t", "char32_t", "double", "float", "int", "long", "short", "signed",
                      "unsigned", "void", "wchar_t", "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
                      "uint32_t", "uint64_t", "std", "string", "string_view", "vector", "optional", "span", "unique_ptr",
                      "shared_ptr", "function"}) {}

CppHighlighter::CppHighlighter(std::vector<std::string> keywords, std::vector<std::string> types)
    : keywords_(keywords.begin(), keywords.end()), types_(types.begin(), types.end()) {}

int CppHighlighter::highlight(std::string_view line, int state, std::vector<TokenSpan>& spans) const {
    const std::size_t n = line.size();
    const auto span = [&](std::size_t begin, std::size_t end, TokenKind kind) {
        if (end > begin)
            spans.push_back({static_cast<std::uint32_t>(begin), static_cast<std::uint32_t>(end), kind});
    };
    std::size_t i = 0;
    // a comment from the line before
    if (state == inComment) {
        const std::size_t close = line.find("*/");
        if (close == std::string_view::npos) {
            span(0, n, TokenKind::Comment);
            return inComment;
        }
        span(0, close + 2, TokenKind::Comment);
        i = close + 2;
    }
    // a preprocessor line: all of it, but for a comment at its end
    {
        std::size_t first = i;
        while (first < n && (line[first] == ' ' || line[first] == '\t'))
            ++first;
        if (first < n && line[first] == '#' && i == 0) {
            std::size_t end = n;
            const std::size_t comment = line.find("//", first);
            if (comment != std::string_view::npos)
                end = comment;
            span(first, end, TokenKind::Preprocessor);
            if (comment != std::string_view::npos)
                span(comment, n, TokenKind::Comment);
            return 0;
        }
    }
    while (i < n) {
        const char c = line[i];
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            span(i, n, TokenKind::Comment);
            return 0;
        }
        if (c == '/' && i + 1 < n && line[i + 1] == '*') {
            const std::size_t close = line.find("*/", i + 2);
            if (close == std::string_view::npos) {
                span(i, n, TokenKind::Comment);
                return inComment;
            }
            span(i, close + 2, TokenKind::Comment);
            i = close + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            // to its closing quote (not one escaped), or the end of the line
            std::size_t j = i + 1;
            while (j < n && line[j] != c)
                j += line[j] == '\\' ? 2 : 1;
            j = std::min(j + 1, n);
            span(i, j, TokenKind::String);
            i = j;
            continue;
        }
        if (digit(c) || (c == '.' && i + 1 < n && digit(line[i + 1]))) {
            std::size_t j = i + 1;
            while (j < n && (wordPart(line[j]) || line[j] == '.' || line[j] == '\'' ||
                             ((line[j] == '+' || line[j] == '-') && (line[j - 1] == 'e' || line[j - 1] == 'E'))))
                ++j;
            span(i, j, TokenKind::Number);
            i = j;
            continue;
        }
        if (wordStart(c)) {
            std::size_t j = i + 1;
            while (j < n && wordPart(line[j]))
                ++j;
            const std::string word(line.substr(i, j - i));
            std::size_t after = j;
            while (after < n && line[after] == ' ')
                ++after;
            if (keywords_.contains(word))
                span(i, j, TokenKind::Keyword);
            else if (types_.contains(word))
                span(i, j, TokenKind::Type);
            else if (after < n && line[after] == '(')
                span(i, j, TokenKind::Function);
            i = j;
            continue;
        }
        ++i;
    }
    return 0;
}

} // namespace glw
