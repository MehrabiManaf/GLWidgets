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

// `a` taken toward `b` by `t` (0: `a`, 1: `b`), every channel and alpha.
constexpr Color mix(const Color& a, const Color& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

// `c` with an alpha of `alpha`.
constexpr Color withAlpha(Color c, float alpha) {
    c.a = alpha;
    return c;
}

} // namespace glw
