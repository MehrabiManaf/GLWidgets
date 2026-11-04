// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Fonts: TrueType / OpenType files loaded once and shared by everything that shows text in them. A FontLibrary owns its
// fonts; a Font must not outlive its library. Sizes are in dips (the em size).
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

class GLW_EXPORT Font {
public:
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    std::string_view family() const;
    bool hasCharacter(char32_t c) const;
    FontMetrics metrics(float size) const;

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

    // Loads a font from the bytes of a TrueType / OpenType file (copied) or from a file. Throws Error when it is not a font.
    const Font& addFont(std::span<const std::byte> data);
    const Font& addFontFile(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw::text
