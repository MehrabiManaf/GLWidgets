// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The colors and measures widgets are drawn with, until styles exist (M4).
#include <GLWidgets/Core/Color.hpp>

namespace glw {

struct Theme {
    Color window{0.15f, 0.16f, 0.19f, 1};
    Color surface{0.21f, 0.22f, 0.26f, 1};          // what stands on the window: buttons, cards
    Color titleBar{0.09f, 0.10f, 0.12f, 1};
    Color text{0.88f, 0.89f, 0.92f, 1};
    Color mutedText{0.62f, 0.64f, 0.70f, 1};
    Color accent{0.35f, 0.65f, 1.00f, 1};
    Color border{0.30f, 0.32f, 0.38f, 1};
    Color hover{1, 1, 1, 0.07f};                     // over a surface the mouse is on
    Color pressed{1, 1, 1, 0.14f};
    Color closeHover{0.80f, 0.16f, 0.18f, 1};

    float fontSize = 14;
    float cornerRadius = 6;
    float titleBarHeight = 40;
    float windowButtonWidth = 46;
    float resizeBorder = 6;
    // The window buttons at the start of the title bar (the system's convention on macOS) or at its end (elsewhere).
#ifdef __APPLE__
    bool windowButtonsAtStart = true;
#else
    bool windowButtonsAtStart = false;
#endif
};

} // namespace glw
