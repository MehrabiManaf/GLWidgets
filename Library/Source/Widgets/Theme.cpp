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

// Fluent 2's neutrals, as Windows 11 has them: in the dark, the title bar #191919, the window #202020, layers #2B2B2B,
// controls #373737; its accent light blue, with dark marks on it. Pressed is a little less than under the mouse.
Palette Palette::dark() {
    return {
        .window = {0.125f, 0.125f, 0.125f, 1},
        .layer = {0.169f, 0.169f, 0.169f, 1},
        .surface = {0.216f, 0.216f, 0.216f, 1},
        .titleBar = {0.098f, 0.098f, 0.098f, 1},
        .text = {0.96f, 0.96f, 0.96f, 1},
        .mutedText = {0.66f, 0.66f, 0.66f, 1},
        .disabledText = {0.42f, 0.42f, 0.42f, 1},
        .accent = {0.376f, 0.804f, 1.00f, 1},
        .onAccent = {0, 0, 0, 1},
        .border = {1, 1, 1, 0.08f},
        .hover = {1, 1, 1, 0.06f},
        .pressed = {1, 1, 1, 0.035f},
        .selection = {0.376f, 0.804f, 1.00f, 0.35f},
        .closeHover = {0.77f, 0.17f, 0.11f, 1},
    };
}

// In the light: the title bar #EBEBEB, the window #F3F3F3, layers #FBFBFB, controls white; the accent #005FB8.
Palette Palette::light() {
    return {
        .window = {0.953f, 0.953f, 0.953f, 1},
        .layer = {0.984f, 0.984f, 0.984f, 1},
        .surface = {1, 1, 1, 1},
        .titleBar = {0.922f, 0.922f, 0.922f, 1},
        .text = {0.10f, 0.10f, 0.10f, 1},
        .mutedText = {0.38f, 0.38f, 0.38f, 1},
        .disabledText = {0.64f, 0.64f, 0.64f, 1},
        .accent = {0.0f, 0.373f, 0.722f, 1},
        .onAccent = {1, 1, 1, 1},
        .border = {0, 0, 0, 0.09f},
        .hover = {0, 0, 0, 0.045f},
        .pressed = {0, 0, 0, 0.025f},
        .selection = {0.0f, 0.373f, 0.722f, 0.22f},
        .closeHover = {0.77f, 0.17f, 0.11f, 1},
    };
}

Theme Theme::fromPalette(const Palette& p) {
    Theme t;
    t.palette = p;

    t.button.background = {.normal = p.surface, .hovered = over(p.surface, p.hover), .pressed = over(p.surface, p.pressed)};
    t.button.border = {.normal = p.border};
    t.button.text = {.normal = p.text, .disabled = p.disabledText};
    t.button.cornerRadius = t.cornerRadius;
    t.button.focusRing = p.accent;

    // focused: darker (in the dark) and a line of the accent along the bottom, as Fluent's fields
    t.lineEdit.background = {.normal = p.surface, .hovered = over(p.surface, p.hover), .focused = over(p.surface, Color{p.window.r, p.window.g, p.window.b, 0.5f})};
    t.lineEdit.border = {.normal = p.border, .disabled = p.border};
    t.lineEdit.focusLine = p.accent;
    t.lineEdit.text = p.text;
    t.lineEdit.placeholder = p.mutedText;
    t.lineEdit.selection = p.selection;
    t.lineEdit.caret = p.accent;
    t.lineEdit.cornerRadius = t.cornerRadius;

    t.checkBox.background = {.normal = p.surface, .hovered = over(p.surface, p.hover), .pressed = over(p.surface, p.pressed)};
    t.checkBox.border = {.normal = p.mutedText, .hovered = p.text, .disabled = p.disabledText};
    t.checkBox.checkedBackground = {.normal = p.accent, .hovered = over(p.accent, Color{p.window.r, p.window.g, p.window.b, 0.12f}), .disabled = p.disabledText};
    t.checkBox.check = p.onAccent;
    t.checkBox.focusRing = p.accent;

    t.scrollBar.track = {0, 0, 0, 0};
    t.scrollBar.thumb = {.normal = {p.text.r, p.text.g, p.text.b, 0.25f},
                         .hovered = Color{p.text.r, p.text.g, p.text.b, 0.40f},
                         .pressed = Color{p.text.r, p.text.g, p.text.b, 0.55f}};

    t.itemView.row = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.itemView.selected = {p.text.r, p.text.g, p.text.b, 0.09f};
    t.itemView.indicator = p.accent;
    t.itemView.focusRing = {p.text.r, p.text.g, p.text.b, 0.55f};

    t.splitter.handle = p.border;
    t.splitter.active = p.accent;

    t.dock.background = p.layer;
    t.dock.border = p.border;
    t.dock.separator = Color{p.text.r, p.text.g, p.text.b, 0.06f};
    t.dock.tab = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.dock.activeLine = p.accent;
    t.dock.splitter = t.splitter;
    t.dock.splitter.handle = {0, 0, 0, 0};          // the gap shows the window: no line
    t.dock.text = p.text;
    t.dock.mutedText = p.mutedText;
    t.dock.dropZone = {p.accent.r, p.accent.g, p.accent.b, 0.22f};
    t.dock.scrollThumb = {p.mutedText.r, p.mutedText.g, p.mutedText.b, 0.55f};

    t.majorTab.background = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.majorTab.active = p.window;
    t.majorTab.text = p.text;
    t.majorTab.mutedText = p.mutedText;
    t.majorTab.insertion = p.accent;

    t.card.background = p.layer;
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
