// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What a Font is made of: its file, read in place by FreeType (rasterization) and HarfBuzz (shaping).
#include <GLWidgets/Text/Font.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

#include <cstddef>
#include <string>
#include <vector>

namespace glw::text {

struct Font::Impl {
    std::vector<std::byte> data;
    FT_Face face = nullptr;
    hb_face_t* hbFace = nullptr;
    hb_font_t* hbFont = nullptr;    // at the font's units per em: positions are scaled by size / unitsPerEm
    float unitsPerEm = 1000;
    std::string family;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl();
};

} // namespace glw::text
