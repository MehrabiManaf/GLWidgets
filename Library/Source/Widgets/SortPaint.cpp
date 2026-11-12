// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/SortPaint.hpp"

#include <algorithm>
#include <cmath>

namespace glw::sorting {

namespace {

constexpr float pi = 3.14159265f;

// Fast, past the end by about `over`, back.
float outBack(float u, float over) {
    u = std::clamp(u, 0.0f, 1.0f);
    const float v = u - 1;
    return 1 + (over + 1) * v * v * v + over * v * v;
}

} // namespace

Duration moveTime(std::chrono::milliseconds slow) {
    return std::chrono::duration_cast<Duration>(Duration(slow) * 1.8);
}

paint::Lifted keyMoved(float t, float height, SortOrder to) {
    t = std::clamp(t, 0.0f, 1.0f);
    // up and down again (quick up, slower down), a little larger meanwhile
    const float up = std::sin(pi * std::min(1.0f, t / 0.85f));
    // rocking about its long middle: its top toward the one looking first for ascending, its bottom for descending; twice,
    // less the second time
    const float way = to == SortOrder::Ascending ? 1.0f : -1.0f;
    const float rock = way * 0.5f * std::sin(2 * pi * t) * (1 - t) * (1 - t);
    return {.lift = std::max(4.0f, height * 0.35f) * up,
            .turn = paint::Quaternion::axisAngle({1, 0, 0}, rock) * paint::towardLight(0.12f * up),
            .scale = 1 + 0.05f * up};
}

void paintArrow(paint::Painter& p, Point at, SortOrder order, Color color, float t, std::optional<SortOrder> from) {
    // its points as it is at last, about its middle (up: its tip above)
    const float d = order == SortOrder::Ascending ? -1.0f : 1.0f;
    Point points[] = {{-4, -2 * d}, {0, 2 * d}, {4, -2 * d}};
    float angle = 0, scale = 1;
    if (t < 1) {
        const float u = std::clamp(t / 0.8f, 0.0f, 1.0f);
        if (from && *from != order)
            angle = pi * (1 - outBack(u, 1.8f));        // round from the other way, past it and back
        else if (!from)
            scale = std::max(0.0f, outBack(u, 2.2f));    // out of its middle
    }
    const float c = std::cos(angle), s = std::sin(angle);
    for (Point& point : points)
        point = {at.x + (point.x * c - point.y * s) * scale, at.y + (point.x * s + point.y * c) * scale};
    if (scale > 0.01f)
        p.drawPolyline(points, 1.4f, color);
}

} // namespace glw::sorting
