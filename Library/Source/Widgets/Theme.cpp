// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Theme.hpp>

namespace glw {

namespace {

// `top` laid over `base` (alpha blending): a state color that is opaque, whatever is under the widget.
Color over(Color base, Color top) {
    const float a = top.a;
    return {base.r + (top.r - base.r) * a, base.g + (top.g - base.g) * a, base.b + (top.b - base.b) * a, base.a};
}

} // namespace

Palette Palette::dark() {
    return {
        .window = {0.15f, 0.16f, 0.19f, 1},
        .surface = {0.21f, 0.22f, 0.26f, 1},
        .titleBar = {0.09f, 0.10f, 0.12f, 1},
        .text = {0.88f, 0.89f, 0.92f, 1},
        .mutedText = {0.62f, 0.64f, 0.70f, 1},
        .disabledText = {0.45f, 0.47f, 0.52f, 1},
        .accent = {0.35f, 0.65f, 1.00f, 1},
        .border = {0.30f, 0.32f, 0.38f, 1},
        .hover = {1, 1, 1, 0.07f},
        .pressed = {1, 1, 1, 0.14f},
        .selection = {0.35f, 0.65f, 1.00f, 0.35f},
        .closeHover = {0.80f, 0.16f, 0.18f, 1},
    };
}

Palette Palette::light() {
    return {
        .window = {0.96f, 0.96f, 0.97f, 1},
        .surface = {1, 1, 1, 1},
        .titleBar = {0.90f, 0.91f, 0.93f, 1},
        .text = {0.10f, 0.11f, 0.13f, 1},
        .mutedText = {0.38f, 0.40f, 0.45f, 1},
        .disabledText = {0.62f, 0.64f, 0.68f, 1},
        .accent = {0.10f, 0.45f, 0.90f, 1},
        .border = {0.78f, 0.80f, 0.84f, 1},
        .hover = {0, 0, 0, 0.05f},
        .pressed = {0, 0, 0, 0.10f},
        .selection = {0.10f, 0.45f, 0.90f, 0.25f},
        .closeHover = {0.85f, 0.18f, 0.20f, 1},
    };
}

Theme Theme::fromPalette(const Palette& p) {
    Theme t;
    t.palette = p;

    t.button.background = {.normal = p.surface, .hovered = over(p.surface, p.hover), .pressed = over(p.surface, p.pressed)};
    t.button.border = {.normal = p.border, .focused = p.accent};
    t.button.text = {.normal = p.text, .disabled = p.disabledText};
    t.button.cornerRadius = t.cornerRadius;
    t.button.focusRing = p.accent;

    t.lineEdit.background = {.normal = p.surface, .hovered = over(p.surface, p.hover), .focused = p.surface};
    t.lineEdit.border = {.normal = p.border, .focused = p.accent, .disabled = over(p.border, p.hover)};
    t.lineEdit.text = p.text;
    t.lineEdit.placeholder = p.mutedText;
    t.lineEdit.selection = p.selection;
    t.lineEdit.caret = p.accent;
    t.lineEdit.cornerRadius = t.cornerRadius;

    t.card.background = p.surface;
    t.card.border = p.border;
    t.card.cornerRadius = t.cornerRadius;

    t.titleBar.background = p.titleBar;
    t.titleBar.title = p.text;

    t.windowButton.background = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.windowButton.closeBackground = {.normal = {0, 0, 0, 0}, .hovered = p.closeHover, .pressed = p.closeHover};
    t.windowButton.glyph = p.text;

    t.frame.background = p.window;
    return t;
}

} // namespace glw
