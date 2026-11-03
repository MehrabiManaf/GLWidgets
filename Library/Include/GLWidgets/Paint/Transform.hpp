// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A uniform scale then a translation: what a canvas needs to zoom and pan.
#include <GLWidgets/Core/Geometry.hpp>

namespace glw::paint {

// No rotation, so boxes and clips stay aligned with the axes. Widths (of lines, borders) and radii scale with it.
struct Transform {
    float scale = 1;
    Point translation;

    Point map(Point p) const { return {p.x * scale + translation.x, p.y * scale + translation.y}; }
    Rect map(const Rect& r) const {
        return {r.x * scale + translation.x, r.y * scale + translation.y, r.width * scale, r.height * scale};
    }
    // What this one maps to `p`.
    Point unmap(Point p) const { return {(p.x - translation.x) / scale, (p.y - translation.y) / scale}; }
    Rect unmap(const Rect& r) const {
        return {(r.x - translation.x) / scale, (r.y - translation.y) / scale, r.width / scale, r.height / scale};
    }
    // `inner` first, then this one
    Transform operator*(const Transform& inner) const { return {scale * inner.scale, map(inner.translation)}; }
    bool identity() const { return scale == 1 && translation.x == 0 && translation.y == 0; }
};

} // namespace glw::paint
