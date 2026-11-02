// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/Json.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>

namespace glw::json {

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Value> document(std::string* error) {
        std::optional<Value> value = parseValue();
        skipSpace();
        if (value && pos_ != text_.size())
            fail("text after the value");
        if (!error_.empty()) {
            if (error)
                *error = error_ + " (at byte " + std::to_string(pos_) + ")";
            return std::nullopt;
        }
        return value;
    }

private:
    std::optional<Value> parseValue() {
        skipSpace();
        if (pos_ >= text_.size())
            return fail("a value expected");
        const char c = text_[pos_];
        if (c == '{')
            return parseObject();
        if (c == '[')
            return parseArray();
        if (c == '"') {
            std::optional<std::string> s = parseString();
            return s ? std::optional<Value>(Value{std::move(*s)}) : std::nullopt;
        }
        if (literal("true"))
            return Value{true};
        if (literal("false"))
            return Value{false};
        if (literal("null"))
            return Value{nullptr};
        return parseNumber();
    }

    std::optional<Value> parseObject() {
        ++pos_;
        Object object;
        skipSpace();
        if (peek('}'))
            return Value{std::move(object)};
        while (true) {
            skipSpace();
            if (!peek('"', false))
                return fail("a key expected");
            std::optional<std::string> key = parseString();
            if (!key)
                return std::nullopt;
            skipSpace();
            if (!peek(':'))
                return fail("':' expected");
            std::optional<Value> value = parseValue();
            if (!value)
                return std::nullopt;
            // a key given again: the last value counts, as most readers do
            const auto same = std::find_if(object.begin(), object.end(), [&](const auto& m) { return m.first == *key; });
            if (same != object.end())
                same->second = std::move(*value);
            else
                object.emplace_back(std::move(*key), std::move(*value));
            skipSpace();
            if (peek('}'))
                return Value{std::move(object)};
            if (!peek(','))
                return fail("',' or '}' expected");
        }
    }

    std::optional<Value> parseArray() {
        ++pos_;
        Array array;
        skipSpace();
        if (peek(']'))
            return Value{std::move(array)};
        while (true) {
            std::optional<Value> value = parseValue();
            if (!value)
                return std::nullopt;
            array.push_back(std::move(*value));
            skipSpace();
            if (peek(']'))
                return Value{std::move(array)};
            if (!peek(','))
                return fail("',' or ']' expected");
        }
    }

    std::optional<std::string> parseString() {
        ++pos_;     // the opening quote
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"')
                return out;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size())
                break;
            const char e = text_[pos_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t c32 = 0;
                if (!hex4(c32))
                    return failString("a \\u escape needs four hex digits");
                // a surrogate pair: two escapes for one code point
                if (c32 >= 0xD800 && c32 <= 0xDBFF && text_.substr(pos_, 2) == "\\u") {
                    pos_ += 2;
                    std::uint32_t low = 0;
                    if (!hex4(low) || low < 0xDC00 || low > 0xDFFF)
                        return failString("a high surrogate without its low one");
                    c32 = 0x10000 + ((c32 - 0xD800) << 10) + (low - 0xDC00);
                }
                appendUtf8(out, c32);
                break;
            }
            default: return failString("an unknown escape");
            }
        }
        return failString("a string without its closing quote");
    }

    std::optional<Value> parseNumber() {
        const std::size_t start = pos_;
        while (pos_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '-' ||
                                       text_[pos_] == '+' || text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E'))
            ++pos_;
        double value = 0;
        const auto result = std::from_chars(text_.data() + start, text_.data() + pos_, value);
        if (start == pos_ || result.ec != std::errc())
            return fail("a value expected");
        return Value{value};
    }

    bool hex4(std::uint32_t& value) {
        if (pos_ + 4 > text_.size())
            return false;
        const auto result = std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, value, 16);
        if (result.ptr != text_.data() + pos_ + 4)
            return false;
        pos_ += 4;
        return true;
    }

    static void appendUtf8(std::string& out, std::uint32_t c) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    void skipSpace() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
            ++pos_;
    }

    // `c` is next: taken (unless `take` is false).
    bool peek(char c, bool take = true) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            pos_ += take ? 1 : 0;
            return true;
        }
        return false;
    }

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word)
            return false;
        pos_ += word.size();
        return true;
    }

    std::nullopt_t fail(const char* message) {
        if (error_.empty())
            error_ = message;
        return std::nullopt;
    }
    std::optional<std::string> failString(const char* message) {
        fail(message);
        return std::nullopt;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string error_;
};

} // namespace

std::optional<Value> parse(std::string_view text, std::string* error) {
    return Parser(text).document(error);
}

} // namespace glw::json
