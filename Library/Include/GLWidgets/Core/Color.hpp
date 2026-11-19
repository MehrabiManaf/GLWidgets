// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>

namespace glw {

// An sRGB color, every channel in [0, 1], not premultiplied. The surfaces are not sRGB formats: the values are written as they are.
struct Color {
    float r = 0;
    float g = 0;
    float b = 0;
    float a = 1;

    friend bool operator==(const Color&, const Color&) = default;
};

// Relative luminance of an opaque sRGB color, in linear light (black = 0, white = 1).
inline float relativeLuminance(Color color) {
    const auto linear = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * linear(color.r) + 0.7152f * linear(color.g) + 0.0722f * linear(color.b);
}

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
