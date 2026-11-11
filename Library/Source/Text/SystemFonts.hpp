// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The system's own fonts, for what a style's fonts do not have: the font file the system would fall back to for a
// character (DirectWrite's font fallback on Windows, Core Text's on macOS, fontconfig's on Linux and the BSDs).
#include <optional>
#include <string>
#include <string_view>

namespace glw::text::detail {

struct SystemFontFile {
    std::string path;               // UTF-8
    int index = 0;                  // its face in a collection (.ttc)
    std::string postScriptName;     // when the system names the face rather than its index (Core Text): matched by it
};

// The file of the font the system shows `c` with (for text in `language`, a BCP 47 tag, when given); none: it knows of
// none, or has no fallback of fonts (a build without fontconfig).
std::optional<SystemFontFile> systemFontFor(char32_t c, std::string_view language);

} // namespace glw::text::detail
