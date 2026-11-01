// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Positions and sizes. Point, Size and Rect are in device independent units (dips): what the user interface is laid out
// in, the same physical size on every screen. A window with a scale of 2 (Window::scale) has 2 pixels per dip. PixelSize
// and PixelRect are in pixels: what the GPU draws into.
#include <algorithm>

namespace glw {

struct Point {
    float x = 0;
    float y = 0;

    friend bool operator==(const Point&, const Point&) = default;
    friend Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
    friend Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
    friend Point operator*(Point p, float s) { return {p.x * s, p.y * s}; }
};

struct Size {
    float width = 0;
    float height = 0;

    friend bool operator==(const Size&, const Size&) = default;
};

struct Rect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + width; }
    float bottom() const { return y + height; }
    Point center() const { return {x + width / 2, y + height / 2}; }
    Size size() const { return {width, height}; }
    bool empty() const { return width <= 0 || height <= 0; }
    bool contains(Point p) const { return p.x >= x && p.y >= y && p.x < right() && p.y < bottom(); }

    // Grown by `d` on every side (shrunk when negative).
    Rect inflated(float d) const { return {x - d, y - d, width + 2 * d, height + 2 * d}; }
    // The part of both; empty (zero size) when they do not overlap.
    Rect intersected(const Rect& o) const {
        const float l = std::max(x, o.x), t = std::max(y, o.y);
        const float r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        return {l, t, std::max(0.0f, r - l), std::max(0.0f, b - t)};
    }

    friend bool operator==(const Rect&, const Rect&) = default;
};

struct PixelSize {
    int width = 0;
    int height = 0;

    bool empty() const { return width <= 0 || height <= 0; }
    friend bool operator==(const PixelSize&, const PixelSize&) = default;
};

struct PixelRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const { return width <= 0 || height <= 0; }
    friend bool operator==(const PixelRect&, const PixelRect&) = default;
};

} // namespace glw
