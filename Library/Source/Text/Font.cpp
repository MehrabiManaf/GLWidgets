// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Text/FontImpl.hpp"
#include "Text/SystemFonts.hpp"

#include <GLWidgets/Core/Error.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <unordered_map>
#include <utility>

namespace glw::text {

namespace detail {
// Library/Fonts/Vazirmatn-Regular.ttf, embedded by cmake/EmbedFile.cmake
extern const unsigned char defaultFont[];
extern const std::size_t defaultFontSize;
} // namespace detail

// ---- font ----

Font::Impl::~Impl() {
    if (hbFont)
        hb_font_destroy(hbFont);
    if (hbFace)
        hb_face_destroy(hbFace);
    if (face)
        FT_Done_Face(face);
}

Font::Font(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Font::~Font() = default;

std::string_view Font::family() const {
    return impl_->family;
}

bool Font::hasCharacter(char32_t c) const {
    return FT_Get_Char_Index(impl_->face, c) != 0;
}

const FontLibrary& Font::library() const {
    return *impl_->library;
}

FontMetrics Font::metrics(float size) const {
    const FT_Face face = impl_->face;
    const float scale = size / impl_->unitsPerEm;
    FontMetrics m;
    m.ascent = face->ascender * scale;
    m.descent = -face->descender * scale;
    m.lineGap = std::max(0.0f, face->height * scale - m.ascent - m.descent);
    // the OS/2 table's (version 2 and later); a font without them: what most Latin fonts have
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    if (os2 && os2->version >= 2 && os2->sCapHeight > 0 && os2->sxHeight > 0) {
        m.capHeight = os2->sCapHeight * scale;
        m.xHeight = os2->sxHeight * scale;
    } else {
        m.capHeight = size * 0.70f;
        m.xHeight = size * 0.50f;
    }
    return m;
}

// ---- library ----

struct FontLibrary::Impl {
    FT_Library freetype = nullptr;
    std::vector<std::unique_ptr<Font>> fonts;       // the first is the default font
    bool systemFallback = true;
    std::unordered_map<char32_t, const Font*> fallbacks;            // each character looked for: its font, or none
    std::map<std::pair<std::string, int>, const Font*> systemFiles; // the system's fonts loaded: by file and face
};

FontLibrary::FontLibrary() : impl_(std::make_unique<Impl>()) {
    if (FT_Init_FreeType(&impl_->freetype))
        throw Error("FreeType could not start");
    addFont(std::as_bytes(std::span(detail::defaultFont, detail::defaultFontSize)));
}

FontLibrary::~FontLibrary() {
    impl_->fonts.clear();       // their faces before the library
    FT_Done_FreeType(impl_->freetype);
}

const Font& FontLibrary::defaultFont() const {
    return *impl_->fonts.front();
}

const Font& FontLibrary::addFont(std::span<const std::byte> data, int face) {
    auto font = std::make_unique<Font::Impl>();
    font->data.assign(data.begin(), data.end());
    font->library = this;
    const auto* bytes = reinterpret_cast<const FT_Byte*>(font->data.data());
    if (FT_New_Memory_Face(impl_->freetype, bytes, static_cast<FT_Long>(font->data.size()), face, &font->face))
        throw Error("not a font that FreeType can read");

    hb_blob_t* blob = hb_blob_create(reinterpret_cast<const char*>(font->data.data()),
                                     static_cast<unsigned>(font->data.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    font->hbFace = hb_face_create(blob, static_cast<unsigned>(face));
    hb_blob_destroy(blob);      // the face keeps it
    font->hbFont = hb_font_create(font->hbFace);
    font->unitsPerEm = static_cast<float>(hb_face_get_upem(font->hbFace));
    font->family = font->face->family_name ? font->face->family_name : "";

    impl_->fonts.push_back(std::unique_ptr<Font>(new Font(std::move(font))));
    return *impl_->fonts.back();
}

const Font& FontLibrary::addFontFile(const std::string& path, int face) {
    // (its path UTF-8, whatever the system's code page)
    std::ifstream file(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size())), std::ios::binary);
    if (!file)
        throw Error("cannot open the font " + path);
    const std::vector<char> data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return addFont(std::as_bytes(std::span(data)), face);
}

void FontLibrary::setSystemFallback(bool enabled) {
    impl_->systemFallback = enabled;
    impl_->fallbacks.clear();
}

bool FontLibrary::systemFallback() const {
    return impl_->systemFallback;
}

const Font* FontLibrary::fallbackFor(char32_t c, std::string_view language) const {
    Impl& d = *impl_;
    if (const auto known = d.fallbacks.find(c); known != d.fallbacks.end())
        return known->second;
    const Font* found = nullptr;
    // one of the system's already loaded (a font of a script has many of its characters)
    for (const auto& [file, font] : d.systemFiles) {
        if (font->hasCharacter(c)) {
            found = font;
            break;
        }
    }
    if (!found && d.systemFallback) {
        if (std::optional<detail::SystemFontFile> file = detail::systemFontFor(c, language)) {
            try {
                // of a collection named by its face's PostScript name (Core Text's): the face of that name
                int index = file->index;
                if (!file->postScriptName.empty()) {
                    FT_Face probe = nullptr;
                    if (!FT_New_Face(d.freetype, file->path.c_str(), -1, &probe)) {
                        const FT_Long faces = probe->num_faces;
                        FT_Done_Face(probe);
                        for (FT_Long i = 0; i < faces; ++i) {
                            if (FT_New_Face(d.freetype, file->path.c_str(), i, &probe))
                                continue;
                            const char* name = FT_Get_Postscript_Name(probe);
                            const bool same = name && file->postScriptName == name;
                            FT_Done_Face(probe);
                            if (same) {
                                index = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }
                const auto key = std::pair(file->path, index);
                if (const auto loaded = d.systemFiles.find(key); loaded != d.systemFiles.end()) {
                    found = loaded->second;
                } else {
                    const Font& font = const_cast<FontLibrary*>(this)->addFontFile(file->path, index);
                    d.systemFiles.emplace(key, &font);
                    found = &font;
                }
                if (!found->hasCharacter(c))
                    found = nullptr;
            } catch (const Error&) {
                found = nullptr;        // (a file it cannot read: as none)
            }
        }
    }
    d.fallbacks.emplace(c, found);
    return found;
}

} // namespace glw::text
