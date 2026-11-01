// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Text/FontImpl.hpp"

#include <GLWidgets/Core/Error.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <span>
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

FontMetrics Font::metrics(float size) const {
    const FT_Face face = impl_->face;
    const float scale = size / impl_->unitsPerEm;
    FontMetrics m;
    m.ascent = face->ascender * scale;
    m.descent = -face->descender * scale;
    m.lineGap = std::max(0.0f, face->height * scale - m.ascent - m.descent);
    return m;
}

// ---- library ----

struct FontLibrary::Impl {
    FT_Library freetype = nullptr;
    std::vector<std::unique_ptr<Font>> fonts;       // the first is the default font
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

const Font& FontLibrary::addFont(std::span<const std::byte> data) {
    auto font = std::make_unique<Font::Impl>();
    font->data.assign(data.begin(), data.end());
    const auto* bytes = reinterpret_cast<const FT_Byte*>(font->data.data());
    if (FT_New_Memory_Face(impl_->freetype, bytes, static_cast<FT_Long>(font->data.size()), 0, &font->face))
        throw Error("not a font that FreeType can read");

    hb_blob_t* blob = hb_blob_create(reinterpret_cast<const char*>(font->data.data()),
                                     static_cast<unsigned>(font->data.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    font->hbFace = hb_face_create(blob, 0);
    hb_blob_destroy(blob);      // the face keeps it
    font->hbFont = hb_font_create(font->hbFace);
    font->unitsPerEm = static_cast<float>(hb_face_get_upem(font->hbFace));
    font->family = font->face->family_name ? font->face->family_name : "";

    impl_->fonts.push_back(std::unique_ptr<Font>(new Font(std::move(font))));
    return *impl_->fonts.back();
}

const Font& FontLibrary::addFontFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw Error("cannot open the font " + path);
    const std::vector<char> data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return addFont(std::as_bytes(std::span(data)));
}

} // namespace glw::text
