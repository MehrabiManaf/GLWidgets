// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Fonts: TrueType / OpenType files loaded once and shared by everything that shows text in them. A FontLibrary owns its
// fonts; a Font must not outlive its library. Sizes are in dips (the em size).
//
// What none of a text's fonts has (Chinese, an emoji, Devanagari...) is shown in a font of the system's: the one the
// system itself falls back to for that character (DirectWrite on Windows, Core Text on macOS, fontconfig on Linux),
// found the first time it is needed, its file loaded once, kept by the library (FontLibrary::fallbackFor; text layouts
// ask it). Without it (setSystemFallback(false), or fontconfig missing from a build), such characters show the default
// font's empty box.
#include <GLWidgets/Export.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace glw::text {

// Vertical measures of a font at a size, in dips.
struct FontMetrics {
    float ascent = 0;       // above the baseline
    float descent = 0;      // below the baseline (positive)
    float lineGap = 0;      // what the font asks for between lines
    float capHeight = 0;    // of capital letters, above the baseline
    float xHeight = 0;      // of small letters without ascenders
    float lineHeight() const { return ascent + descent + lineGap; }
};

class FontLibrary;

class GLW_EXPORT Font {
public:
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    std::string_view family() const;
    bool hasCharacter(char32_t c) const;
    FontMetrics metrics(float size) const;
    // The library that holds it (where its text finds fonts for what it does not have).
    const FontLibrary& library() const;

    // The implementation's (FreeType and HarfBuzz objects), for the rest of the text layer.
    struct Impl;
    Impl& impl() const { return *impl_; }

private:
    friend class FontLibrary;
    explicit Font(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class GLW_EXPORT FontLibrary {
public:
    // With the default font built in: Vazirmatn (Persian, Arabic and Latin; SIL Open Font License).
    FontLibrary();
    ~FontLibrary();
    FontLibrary(const FontLibrary&) = delete;
    FontLibrary& operator=(const FontLibrary&) = delete;

    const Font& defaultFont() const;

    // Loads a font from the bytes of a TrueType / OpenType file (copied) or from a file; of a collection (.ttc), its
    // `face`th font. Throws Error when it is not a font.
    const Font& addFont(std::span<const std::byte> data, int face = 0);
    const Font& addFontFile(const std::string& path, int face = 0);

    // A font of the system's for `c` (in text of `language`, a BCP 47 tag, when given): one it already loaded that has
    // it, or the one the system falls back to for it, loaded now; none when there is none (or no fallback). Each
    // character is looked for once.
    const Font* fallbackFor(char32_t c, std::string_view language = {}) const;
    // Whether it looks among the system's fonts (on by default).
    void setSystemFallback(bool enabled);
    bool systemFallback() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw::text
