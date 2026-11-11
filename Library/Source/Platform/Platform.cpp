// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Platform/Platform.hpp>

#include <algorithm>
#include <cmath>

namespace glw::platform {

Window::~Window() = default;
Platform::~Platform() = default;

Rect keptOnScreens(const Rect& window, std::span<const Rect> screens, float grip) {
    if (screens.empty())
        return window;
    // its title bar on one of them: where it is
    const Rect top{window.x, window.y, window.width, std::min(grip, window.height)};
    for (const Rect& screen : screens) {
        const Rect shows = top.intersected(screen);
        if (shows.width >= std::min(grip, window.width) && shows.height >= std::min(grip, top.height) * 0.5f)
            return window;
    }
    // the screen most of it is over; none: the one nearest its middle
    const Rect* best = nullptr;
    float most = 0;
    for (const Rect& screen : screens) {
        const Rect over = window.intersected(screen);
        if (const float area = over.width * over.height; area > most) {
            most = area;
            best = &screen;
        }
    }
    if (!best) {
        const Point middle = window.center();
        float nearest = 0;
        for (const Rect& screen : screens) {
            const float dx = std::max({screen.x - middle.x, 0.0f, middle.x - screen.right()});
            const float dy = std::max({screen.y - middle.y, 0.0f, middle.y - screen.bottom()});
            if (const float d = dx * dx + dy * dy; !best || d < nearest) {
                nearest = d;
                best = &screen;
            }
        }
    }
    // on it, as large as fits
    Rect placed = window;
    placed.width = std::min(window.width, best->width);
    placed.height = std::min(window.height, best->height);
    placed.x = std::clamp(window.x, best->x, best->right() - placed.width);
    placed.y = std::clamp(window.y, best->y, best->bottom() - placed.height);
    return placed;
}

} // namespace glw::platform
