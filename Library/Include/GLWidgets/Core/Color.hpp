// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
namespace glw {

// An sRGB color, every channel in [0, 1], not premultiplied. The surfaces are not sRGB formats: the values are written as they are.
struct Color {
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 1;

    friend bool operator==(const Color&, const Color&) = default;
};

} // namespace glw
