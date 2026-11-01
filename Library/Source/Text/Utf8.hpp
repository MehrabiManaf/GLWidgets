// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <string_view>
#include <utility>

namespace glw::text {

// The code point at byte `i` of `text` and its length in bytes. What is not UTF-8 is U+FFFD, one byte long.
inline std::pair<char32_t, std::size_t> decodeUtf8(std::string_view text, std::size_t i) {
    const auto byte = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char b0 = byte(i);
    if (b0 < 0x80)
        return {b0, 1};
    std::size_t length = 0;
    char32_t c = 0;
    if ((b0 & 0xE0) == 0xC0) {
        length = 2;
        c = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
        length = 3;
        c = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
        length = 4;
        c = b0 & 0x07;
    } else {
        return {0xFFFD, 1};
    }
    if (i + length > text.size())
        return {0xFFFD, 1};
    for (std::size_t k = 1; k < length; ++k) {
        if ((byte(i + k) & 0xC0) != 0x80)
            return {0xFFFD, 1};
        c = (c << 6) | (byte(i + k) & 0x3F);
    }
    return {c, length};
}

} // namespace glw::text
