// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Theme.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace glw {

namespace {

constexpr Color hex(unsigned v, float alpha = 1) {
    return {((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f, alpha};
}

} // namespace

// Studio's: neutral grays with a little blue in them, as Fusion's are gray; controls raised a step above the panels they
// are on and lit from above, fields sunk into them, the edges of controls darker than what is around them. The accent: in
// the dark, amber (a warm orange, a little toward mustard) with dark marks on it; in the light, azure with white ones.
// Fluid's: deeper and smoother, the layers further apart, the accent brighter. Plain's: grays without a tint.
Palette Palette::dark(ThemeStyle style) {
    if (style == ThemeStyle::Fluid) {
        return {
            .window = hex(0x18191C),
            .layer = hex(0x222327),
            .surface = hex(0x2D2F34),
            .base = hex(0x141518),
            .titleBar = hex(0x121315),
            .text = hex(0xEDEEF0),
            .mutedText = hex(0x9DA1A8),
            .disabledText = hex(0x585C63),
            .accent = hex(0xFF8A3D),
            .onAccent = hex(0x1C1108),
            .border = {1, 1, 1, 0.06f},
            .outline = {0, 0, 0, 0.35f},
            .hover = {1, 1, 1, 0.06f},
            .pressed = {1, 1, 1, 0.10f},
            .selection = hex(0xFF8A3D, 0.36f),
            .closeHover = hex(0xC42B1C),
        };
    }
    if (style == ThemeStyle::Metal) {
        return {
            .window = hex(0x1C1E21),
            .layer = hex(0x282B2F),
            .surface = hex(0x4A4F57),
            .base = hex(0x16181A),
            .titleBar = hex(0x161719),
            .text = hex(0xE8EAED),
            .mutedText = hex(0x9AA0A8),
            .disabledText = hex(0x5C6168),
            .accent = hex(0xE0913F),
            .onAccent = hex(0x1C1409),
            .border = {1, 1, 1, 0.07f},
            .outline = {0, 0, 0, 0.55f},
            .hover = {1, 1, 1, 0.06f},
            .pressed = {0, 0, 0, 0.12f},
            .selection = hex(0xE0913F, 0.38f),
            .closeHover = hex(0xC42B1C),
        };
    }
    if (style == ThemeStyle::Plain) {
        return {
            .window = hex(0x2A2A2A),
            .layer = hex(0x313131),
            .surface = hex(0x3B3B3B),
            .base = hex(0x242424),
            .titleBar = hex(0x232323),
            .text = hex(0xE6E6E6),
            .mutedText = hex(0x9E9E9E),
            .disabledText = hex(0x636363),
            .accent = hex(0xE8892E),
            .onAccent = hex(0x1A1209),
            .border = {1, 1, 1, 0.08f},
            .outline = {0, 0, 0, 0.50f},
            .hover = {1, 1, 1, 0.06f},
            .pressed = {0, 0, 0, 0.12f},
            .selection = hex(0xE8892E, 0.40f),
            .closeHover = hex(0xC42B1C),
        };
    }
    return {
        .window = hex(0x232529),
        .layer = hex(0x2B2E33),
        .surface = hex(0x3A3E45),
        .base = hex(0x1C1E22),
        .titleBar = hex(0x1A1C1F),
        .text = hex(0xE6E8EB),
        .mutedText = hex(0x9A9FA8),
        .disabledText = hex(0x5D626A),
        .accent = hex(0xF0902A),
        .onAccent = hex(0x1C1409),
        .border = {1, 1, 1, 0.07f},
        .outline = {0, 0, 0, 0.55f},
        .hover = {1, 1, 1, 0.05f},
        .pressed = {0, 0, 0, 0.10f},
        .selection = hex(0xF0902A, 0.36f),
        .closeHover = hex(0xC42B1C),
    };
}

Palette Palette::light(ThemeStyle style) {
    if (style == ThemeStyle::Fluid) {
        return {
            .window = hex(0xF2F3F5),
            .layer = hex(0xFFFFFF),
            .surface = hex(0xFFFFFF),
            .base = hex(0xFFFFFF),
            .titleBar = hex(0xE8EAED),
            .text = hex(0x16181B),
            .mutedText = hex(0x5E6570),
            .disabledText = hex(0xA9AEB6),
            .accent = hex(0x2B6BE8),
            .onAccent = hex(0xFFFFFF),
            .border = {0, 0, 0, 0.07f},
            .outline = {0, 0, 0, 0.13f},
            .hover = {0, 0, 0, 0.04f},
            .pressed = {0, 0, 0, 0.08f},
            .selection = hex(0x2B6BE8, 0.22f),
            .closeHover = hex(0xC42B1C),
        };
    }
    if (style == ThemeStyle::Metal) {
        return {
            .window = hex(0xD3D6DA),
            .layer = hex(0xE0E3E6),
            .surface = hex(0xC6CBD1),
            .base = hex(0xF2F3F5),
            .titleBar = hex(0xC4C8CD),
            .text = hex(0x1C1F23),
            .mutedText = hex(0x565D66),
            .disabledText = hex(0x9AA0A8),
            .accent = hex(0x2F6FDB),
            .onAccent = hex(0xFFFFFF),
            .border = {0, 0, 0, 0.10f},
            .outline = {0, 0, 0, 0.28f},
            .hover = {0, 0, 0, 0.04f},
            .pressed = {0, 0, 0, 0.09f},
            .selection = hex(0x2F6FDB, 0.26f),
            .closeHover = hex(0xC42B1C),
        };
    }
    if (style == ThemeStyle::Plain) {
        return {
            .window = hex(0xECECEC),
            .layer = hex(0xF4F4F4),
            .surface = hex(0xFAFAFA),
            .base = hex(0xFFFFFF),
            .titleBar = hex(0xDFDFDF),
            .text = hex(0x1E1E1E),
            .mutedText = hex(0x606060),
            .disabledText = hex(0xA8A8A8),
            .accent = hex(0x2F6FDB),
            .onAccent = hex(0xFFFFFF),
            .border = {0, 0, 0, 0.10f},
            .outline = {0, 0, 0, 0.30f},
            .hover = {0, 0, 0, 0.04f},
            .pressed = {0, 0, 0, 0.09f},
            .selection = hex(0x2F6FDB, 0.26f),
            .closeHover = hex(0xC42B1C),
        };
    }
    return {
        .window = hex(0xE7E9EC),
        .layer = hex(0xF2F3F5),
        .surface = hex(0xF7F8F9),
        .base = hex(0xFFFFFF),
        .titleBar = hex(0xDBDEE3),
        .text = hex(0x1D2026),
        .mutedText = hex(0x5E6570),
        .disabledText = hex(0xA4AAB2),
        .accent = hex(0x2F6FDB),
        .onAccent = hex(0xFFFFFF),
        .border = {0, 0, 0, 0.09f},
        .outline = {0, 0, 0, 0.26f},
        .hover = {0, 0, 0, 0.035f},
        .pressed = {0, 0, 0, 0.08f},
        .selection = hex(0x2F6FDB, 0.26f),
        .closeHover = hex(0xC42B1C),
    };
}

std::optional<ThemeStyle> themeStyleNamed(std::string_view name) {
    constexpr std::pair<std::string_view, ThemeStyle> names[] = {{"studio", ThemeStyle::Studio}, {"fluid", ThemeStyle::Fluid},
                                                                 {"plain", ThemeStyle::Plain}, {"plastic", ThemeStyle::Plastic},
                                                                 {"metal", ThemeStyle::Metal}};
    for (const auto& [n, style] : names)
        if (n == name)
            return style;
    return std::nullopt;
}

std::optional<ColorScheme> colorSchemeNamed(std::string_view name) {
    constexpr std::pair<std::string_view, ColorScheme> names[] = {{"dark", ColorScheme::Dark}, {"light", ColorScheme::Light},
                                                                  {"dusk", ColorScheme::Dusk}, {"sand", ColorScheme::Sand},
                                                                  {"turquoise", ColorScheme::Turquoise}};
    for (const auto& [n, scheme] : names)
        if (n == name)
            return scheme;
    return std::nullopt;
}

// The other schemes: one palette each, in every style.
//   Dusk: plum grays, warmer and redder than Dark's, layers a step apart; a soft violet accent, dark marks on it.
//   Sand: paper and sand, the hairlines and shades warm (brown, not black); a terracotta accent, white marks on it.
//   Turquoise: the glaze of tiles, a deep turquoise, blue (Persian turquoise); its hairlines and the light under the mouse lapis lazuli, blue;
//   saffron gold the accent and the selection, the text ivory.
Palette Palette::of(ColorScheme scheme, ThemeStyle style) {
    Palette p;
    switch (scheme) {
    case ColorScheme::Dark:
        return dark(style);
    case ColorScheme::Light:
        return light(style);
    case ColorScheme::Dusk:
        p = {
            .window = hex(0x1F1B24),
            .layer = hex(0x29242F),
            .surface = hex(0x38323F),
            .base = hex(0x19161D),
            .titleBar = hex(0x17141B),
            .text = hex(0xEDE8F2),
            .mutedText = hex(0xA59CB1),
            .disabledText = hex(0x605869),
            .accent = hex(0xAE8FFF),
            .onAccent = hex(0x170F2B),
            .border = {1, 1, 1, 0.07f},
            .outline = {0, 0, 0, 0.55f},
            .hover = {1, 1, 1, 0.05f},
            .pressed = {0, 0, 0, 0.10f},
            .selection = hex(0xAE8FFF, 0.34f),
            .closeHover = hex(0xC42B1C),
        };
        break;
    case ColorScheme::Sand:
        p = {
            .window = hex(0xE9E2D4),
            .layer = hex(0xF4EFE5),
            .surface = hex(0xFAF7F0),
            .base = hex(0xFFFDF8),
            .titleBar = hex(0xDDD3C1),
            .text = hex(0x2B251E),
            .mutedText = hex(0x6E6455),
            .disabledText = hex(0xAEA492),
            .accent = hex(0xB5532F),
            .onAccent = hex(0xFFFFFF),
            .border = hex(0x4A3518, 0.12f),
            .outline = hex(0x3A2810, 0.28f),
            .hover = hex(0x5A3C10, 0.05f),
            .pressed = hex(0x5A3C10, 0.10f),
            .selection = hex(0xB5532F, 0.24f),
            .closeHover = hex(0xC42B1C),
        };
        break;
    case ColorScheme::Turquoise:
        p = {
            .window = hex(0x082731),
            .layer = hex(0x0C3440),
            .surface = hex(0x134653),
            .base = hex(0x051B22),
            .titleBar = hex(0x04161C),
            .text = hex(0xF3EBDC),
            .mutedText = hex(0xA2BFCB),
            .disabledText = hex(0x4D6B76),
            .accent = hex(0xE3B04B),
            .onAccent = hex(0x1A1405),
            .border = hex(0x4A78E0, 0.20f),
            .outline = {0, 0, 0, 0.50f},
            .hover = hex(0x4A78E0, 0.12f),
            .pressed = {0, 0, 0, 0.14f},
            .selection = hex(0xE3B04B, 0.34f),
            .closeHover = hex(0xC42B1C),
        };
        break;
    }
    if (style == ThemeStyle::Metal) {
        // metal: the controls further from what they are on (lighter in the dark, darker in the light), as Metal's own
        const bool dark = p.window.r + p.window.g + p.window.b < 1.5f;
        p.surface = dark ? lerp(p.surface, p.text, 0.10f) : lerp(p.surface, p.mutedText, 0.18f);
    }
    return p;
}

Theme Theme::of(ColorScheme scheme, ThemeStyle style) {
    Theme t = fromPalette(Palette::of(scheme, style), style);
    t.scheme = scheme;
    // the code: in the scheme's colors (Dark's and Light's those of the editors people know)
    TextEditStyle& e = t.textEdit;
    switch (scheme) {
    case ColorScheme::Dark:
    case ColorScheme::Light:
        break;
    case ColorScheme::Dusk:
        t.chart.series = {hex(0xAE8FFF), hex(0xF2A5C2), hex(0x7FD3C4), hex(0xE8B98C), hex(0x8FB3FF), hex(0xE18FDB),
                          hex(0xEADFA6), hex(0xF28B82), hex(0x9FD08A), hex(0xB5A9C9)};
        e.keyword = hex(0xB99DFF);
        e.type = hex(0x7FD3C4);
        e.string = hex(0xE8B98C);
        e.number = hex(0xF2A5C2);
        e.comment = hex(0x7D7591);
        e.preprocessor = hex(0xE18FDB);
        e.function = hex(0xEADFA6);
        break;
    case ColorScheme::Sand:
        t.chart.series = {hex(0xB5532F), hex(0x24766E), hex(0xC99A2E), hex(0x5F7418), hex(0x7A4C9A), hex(0x3E6FA8),
                          hex(0x8C3B5A), hex(0x6E8B3D), hex(0x9A7B5B), hex(0x4F5D73)};
        e.keyword = hex(0x9C3B1E);
        e.type = hex(0x24766E);
        e.string = hex(0x5F7418);
        e.number = hex(0x9A5A00);
        e.comment = hex(0x968975);
        e.preprocessor = hex(0x7A4C9A);
        e.function = hex(0x5A4512);
        break;
    case ColorScheme::Turquoise:
        t.chart.series = {hex(0xE3B04B), hex(0x4FD1D9), hex(0xF2A08F), hex(0x86ABFF), hex(0x7FE6B0), hex(0xC99CE8),
                          hex(0xF4DBA6), hex(0xE86A7A), hex(0x5FA8D3), hex(0xA9B4C8)};
        e.keyword = hex(0x86ABFF);
        e.type = hex(0x7FD9EE);
        e.string = hex(0xE9C27A);
        e.number = hex(0xF2A08F);
        e.comment = hex(0x6C97A6);
        e.preprocessor = hex(0xC99CE8);
        e.function = hex(0xF4DBA6);
        break;
    }
    return t;
}

Theme Theme::fromPalette(const Palette& p, ThemeStyle style) {
    Theme t;
    t.style = style;
    t.palette = p;
    const bool dark = p.window.r + p.window.g + p.window.b < 1.5f;
    t.scheme = dark ? ColorScheme::Dark : ColorScheme::Light;
    const Color clear{0, 0, 0, 0};
    const Color white{1, 1, 1, 1};
    const Color black{0, 0, 0, 1};
    const Color glow = withAlpha(p.accent, dark ? 0.45f : 0.35f);
    // lit from above: raised, a light line inside along the top and a soft shadow; sunken, a shadow inside along the top
    const Bevel raised{.gradient = dark ? 0.05f : 0.07f, .highlight = withAlpha(white, dark ? 0.08f : 0.85f),
                       .shadow = withAlpha(black, dark ? 0.30f : 0.10f)};
    const Bevel sunken{.highlight = withAlpha(white, dark ? 0.045f : 0.6f), .shadow = withAlpha(black, dark ? 0.30f : 0.07f)};

    // buttons: raised, their edge the accent while they have the focus (not under the mouse: it would come and go)
    t.button.background = {.normal = p.surface, .hovered = lit(p.surface, dark ? 0.05f : 0.5f), .pressed = lit(p.surface, -0.06f),
                           .disabled = mix(p.surface, p.layer, 0.5f)};
    t.button.border = {.normal = p.outline, .focused = p.accent, .disabled = withAlpha(p.outline, p.outline.a * 0.6f)};
    t.button.text = {.normal = p.text, .disabled = p.disabledText};
    t.button.bevel = raised;
    t.button.cornerRadius = t.cornerRadius;
    t.button.focusRing = glow;

    // fields: sunken, a shade lighter under the mouse; their edge the accent while they have the focus
    t.lineEdit.background = {.normal = p.base, .hovered = lit(p.base, dark ? 0.025f : 0), .disabled = mix(p.base, p.layer, 0.6f)};
    t.lineEdit.border = {.normal = p.outline, .focused = p.accent, .disabled = withAlpha(p.outline, p.outline.a * 0.6f)};
    t.lineEdit.focusLine = p.accent;
    t.lineEdit.focusRing = glow;
    t.lineEdit.bevel = sunken;
    t.lineEdit.text = p.text;
    t.lineEdit.placeholder = p.mutedText;
    t.lineEdit.selection = p.selection;
    t.lineEdit.caret = p.accent;
    t.lineEdit.cornerRadius = t.cornerRadius;

    // check boxes: a sunken box; checked, a raised one of the accent
    t.checkBox.background = {.normal = p.base, .hovered = lit(p.base, dark ? 0.04f : 0), .pressed = lit(p.base, -0.04f)};
    t.checkBox.border = {.normal = p.outline, .hovered = withAlpha(p.accent, 0.8f), .disabled = withAlpha(p.outline, p.outline.a * 0.6f)};
    t.checkBox.checkedBackground = {.normal = p.accent, .hovered = lit(p.accent, 0.08f), .pressed = lit(p.accent, -0.08f), .disabled = p.disabledText};
    t.checkBox.checkedBorder = lit(p.accent, -0.35f);
    t.checkBox.check = p.onAccent;
    t.checkBox.bevel = sunken;
    t.checkBox.checkedBevel = {.gradient = 0.12f, .highlight = withAlpha(white, 0.22f), .shadow = withAlpha(black, dark ? 0.30f : 0.12f)};
    t.checkBox.focusRing = glow;
    t.checkBox.cornerRadius = t.cornerRadius;

    t.scrollBar.track = clear;
    t.scrollBar.cornerRadius = t.cornerRadius;
    t.scrollBar.thumb = {.normal = withAlpha(p.text, 0.22f), .hovered = withAlpha(p.text, 0.38f), .pressed = withAlpha(p.text, 0.52f)};

    t.itemView.row = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    // a tint of the accent strong enough to stay orange on the dark (fainter, it reads as brown)
    t.itemView.selected = withAlpha(p.accent, dark ? 0.58f : 0.16f);
    t.itemView.indicator = p.accent;
    t.itemView.focusRing = withAlpha(p.accent, 0.75f);
    t.itemView.expander = p.mutedText;
    t.itemView.headerText = p.mutedText;
    t.itemView.headerLine = withAlpha(p.text, 0.10f);
    t.itemView.gridLine = withAlpha(p.text, dark ? 0.08f : 0.10f);
    t.itemView.cornerRadius = t.cornerRadius;
    t.itemView.rowLines = true;         // lists and trees: a line between their rows
    // ... their headers raised as controls are (as a panel's header)
    t.itemView.headerBackground = mix(p.layer, p.surface, 0.45f);
    t.itemView.headerBevel = {.gradient = raised.gradient, .highlight = raised.highlight};
    // tables: the cells' borders, as a spreadsheet's
    t.table = t.itemView;
    t.table.columnLines = true;

    t.splitter.handle = p.border;
    t.splitter.active = p.accent;

    t.dock.background = p.layer;
    t.dock.border = dark ? withAlpha(black, 0.35f) : withAlpha(black, 0.10f);
    t.dock.separator = dark ? withAlpha(black, 0.30f) : withAlpha(black, 0.07f);
    t.dock.tab = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.dock.selectedTab = dark ? lit(p.layer, 0.09f) : lit(p.layer, 0.7f);
    t.dock.selectedTabBorder = dark ? withAlpha(black, 0.45f) : withAlpha(black, 0.16f);
    t.dock.selectedTabHighlight = withAlpha(white, dark ? 0.08f : 0.9f);
    t.dock.activeLine = p.accent;
    t.dock.splitter = t.splitter;
    t.dock.splitter.handle = clear;          // the gap shows the window: no line
    t.dock.splitter.thickness = 4;           // a narrow gap: the islands' edges tell them apart
    t.dock.text = p.text;
    t.dock.mutedText = p.mutedText;
    t.dock.dropZone = withAlpha(p.accent, 0.22f);
    t.dock.cornerRadius = t.cornerRadius;
    t.dock.tabRadius = 4;
    t.dock.tabBevel = raised;               // what stands as a tab: the tongue of a hidden bar

    t.majorTab.background = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.majorTab.active = p.window;
    t.majorTab.text = p.text;
    t.majorTab.mutedText = p.mutedText;
    t.majorTab.insertion = p.accent;
    t.majorTab.cornerRadius = t.cornerRadius;
    t.majorTab.raised = p.surface;

    // a raised header over its content, sunk a little into the panel; lighter under the mouse or with the focus
    t.collapsiblePanel.header = {.normal = mix(p.layer, p.surface, 0.45f),
                                 .hovered = dark ? mix(p.layer, p.surface, 0.75f) : lit(p.surface, 0.5f),
                                 .pressed = mix(p.layer, p.surface, 0.25f),
                                 .focused = dark ? mix(p.layer, p.surface, 0.75f) : lit(p.surface, 0.5f)};
    t.collapsiblePanel.bevel = {.gradient = raised.gradient, .highlight = raised.highlight};
    // ... a card: its body a step above the window (not sunk below it), a soft edge around it, a line under the header,
    // a shadow under it all
    t.collapsiblePanel.content = dark ? mix(p.window, p.layer, 0.7f) : p.layer;
    t.collapsiblePanel.border = dark ? withAlpha(black, 0.45f) : withAlpha(black, 0.13f);
    t.collapsiblePanel.separator = dark ? withAlpha(black, 0.35f) : withAlpha(black, 0.08f);
    t.collapsiblePanel.shadow = withAlpha(black, dark ? 0.28f : 0.07f);
    t.collapsiblePanel.title = p.text;
    t.collapsiblePanel.arrow = p.mutedText;
    t.collapsiblePanel.cornerRadius = t.cornerRadius;

    t.menuBar.background = p.titleBar;
    t.menuBar.text = p.text;
    t.menuBar.title = {.normal = clear, .hovered = p.hover, .pressed = withAlpha(p.text, dark ? 0.10f : 0.08f)};
    t.menuBar.cornerRadius = t.cornerRadius;

    // tool buttons: flat until the mouse is over them, then raised
    t.toolBar.background = p.window;
    t.toolBar.separator = withAlpha(p.text, 0.12f);
    t.toolBar.button = {.normal = clear, .hovered = withAlpha(lit(p.surface, dark ? 0.0f : 0.5f), dark ? 0.85f : 0.9f),
                        .pressed = withAlpha(lit(p.surface, -0.06f), 0.85f)};
    t.toolBar.checked = withAlpha(p.accent, dark ? 0.58f : 0.20f);   // as a selected row: orange on the dark, not brown
    t.toolBar.outline = withAlpha(black, dark ? 0.45f : 0.20f);
    t.toolBar.bevel = {.gradient = raised.gradient, .highlight = raised.highlight, .shadow = withAlpha(black, dark ? 0.25f : 0.08f)};
    t.toolBar.icon = p.text;
    t.toolBar.disabledIcon = p.disabledText;
    t.toolBar.cornerRadius = t.cornerRadius;

    // menus: a raised panel, the item chosen in the accent
    t.menu.background = mix(p.layer, p.surface, dark ? 0.25f : 0.6f);
    t.menu.border = p.outline;
    t.menu.highlight = p.accent;
    t.menu.highlightText = p.onAccent;
    t.menu.text = p.text;
    t.menu.shortcut = p.mutedText;
    t.menu.disabledText = p.disabledText;
    t.menu.mark = p.text;
    t.menu.separator = withAlpha(p.text, 0.10f);
    t.menu.cornerRadius = t.cornerRadius;
    // code: the colors of the editors people know (Visual Studio Code's Dark+ and Light+)
    t.textEdit.background = p.window;
    t.textEdit.text = p.text;
    t.textEdit.gutter = p.window;
    t.textEdit.lineNumber = p.disabledText;
    t.textEdit.currentLineNumber = p.text;
    t.textEdit.currentLine = Color{p.text.r, p.text.g, p.text.b, 0.04f};
    t.textEdit.selection = p.selection;
    t.textEdit.caret = p.text;
    t.textEdit.keyword = hex(dark ? 0x569CD6 : 0x0000FF);
    t.textEdit.type = hex(dark ? 0x4EC9B0 : 0x267F99);
    t.textEdit.string = hex(dark ? 0xCE9178 : 0xA31515);
    t.textEdit.number = hex(dark ? 0xB5CEA8 : 0x098658);
    t.textEdit.comment = hex(dark ? 0x6A9955 : 0x008000);
    t.textEdit.preprocessor = hex(dark ? 0xC586C0 : 0xAF00DB);
    t.textEdit.function = hex(dark ? 0xDCDCAA : 0x795E26);
    t.toolTip.background = t.menu.background;
    t.toolTip.border = p.outline;
    t.toolTip.text = p.text;
    t.dialog.background = p.layer;
    t.dialog.border = p.outline;
    t.dialog.scrim = Color{0, 0, 0, 0.40f};
    t.canvas.background = p.window;
    // charts: grids faint, axes and labels muted; the series' colors after the accent, as bright in the dark as in the
    // light
    t.chart.grid = withAlpha(p.text, dark ? 0.075f : 0.085f);
    t.chart.axis = withAlpha(p.text, dark ? 0.28f : 0.32f);
    t.chart.text = p.mutedText;
    t.chart.title = p.text;
    t.chart.crosshair = withAlpha(p.text, 0.38f);
    t.chart.selection = withAlpha(p.accent, 0.20f);
    t.chart.series = dark ? std::array{p.accent, hex(0x4C9BE8), hex(0x5BBF73), hex(0xE8615A), hex(0xA78BFA), hex(0x3FC1C9),
                                       hex(0xE57AB8), hex(0x9CC655), hex(0xC4976B), hex(0x8E9AAF)}
                          : std::array{p.accent, hex(0xE07B26), hex(0x2E9E5B), hex(0xD64545), hex(0x7B5CD6), hex(0x1A9BA3),
                                       hex(0xC2478F), hex(0x7F9A1E), hex(0x9A6B45), hex(0x6B7480)};
    t.chart.barBevel = {.gradient = dark ? 0.05f : 0.06f, .highlight = withAlpha(white, dark ? 0.07f : 0.35f)};
    t.chart.barRadius = 0;
    // nodes: a body a layer above the canvas, a header raised (its kind's color), a shadow under them
    t.node.body = p.layer;
    t.node.border = dark ? withAlpha(black, 0.55f) : withAlpha(black, 0.18f);
    t.node.shadow = withAlpha(black, dark ? 0.45f : 0.15f);
    t.node.selection = p.accent;
    t.node.bevel = {.gradient = 0.08f, .highlight = withAlpha(white, 0.18f)};
    t.node.cornerRadius = 3;
    t.canvas.grid = Color{p.text.r, p.text.g, p.text.b, 0.035f};
    t.canvas.majorGrid = Color{p.text.r, p.text.g, p.text.b, 0.08f};
    t.card.background = p.layer;
    t.card.border = dark ? withAlpha(black, 0.35f) : withAlpha(black, 0.10f);
    t.card.cornerRadius = t.cornerRadius;

    t.titleBar.background = p.titleBar;
    t.titleBar.title = p.text;

    t.windowButton.background = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.windowButton.closeBackground = {.normal = {0, 0, 0, 0}, .hovered = p.closeHover, .pressed = p.closeHover};
    t.windowButton.glyph = p.text;

    t.frame.background = p.window;

    if (style == ThemeStyle::Fluid)
        fluid(t, dark);
    else if (style == ThemeStyle::Plain)
        plain(t);
    else if (style == ThemeStyle::Plastic)
        plastic(t, dark);
    else if (style == ThemeStyle::Metal)
        metal(t, dark);
    return t;
}

// ---- between two themes ----

Theme Theme::blend(const Theme& a, const Theme& b, float t) {
    const auto color = [t](Color x, Color y) { return lerp(x, y, t); };
    const auto number = [t](float x, float y) { return x + (y - x) * t; };
    // a pill's radius (as large as it gets) as round as the control is high: it rounds as it goes, not at once
    const auto radius = [&](float x, float y) {
        constexpr float most = 16;
        return t >= 1 ? y : number(std::min(x, most), std::min(y, most));
    };
    const auto insets = [&](Insets x, Insets y) {
        return Insets{number(x.start, y.start), number(x.top, y.top), number(x.end, y.end), number(x.bottom, y.bottom)};
    };
    const auto bevel = [&](const Bevel& x, const Bevel& y) {
        return Bevel{.gradient = number(x.gradient, y.gradient),
                     .highlight = color(x.highlight, y.highlight),
                     .shadow = color(x.shadow, y.shadow),
                     .relief = number(x.relief, y.relief),
                     .roundness = number(x.roundness, y.roundness)};
    };
    const Palette& p = a.palette;
    const Palette& q = b.palette;
    const Palette palette{
        .window = color(p.window, q.window),
        .layer = color(p.layer, q.layer),
        .surface = color(p.surface, q.surface),
        .base = color(p.base, q.base),
        .titleBar = color(p.titleBar, q.titleBar),
        .text = color(p.text, q.text),
        .mutedText = color(p.mutedText, q.mutedText),
        .disabledText = color(p.disabledText, q.disabledText),
        .accent = color(p.accent, q.accent),
        .onAccent = color(p.onAccent, q.onAccent),
        .border = color(p.border, q.border),
        .outline = color(p.outline, q.outline),
        .hover = color(p.hover, q.hover),
        .pressed = color(p.pressed, q.pressed),
        .selection = color(p.selection, q.selection),
        .closeHover = color(p.closeHover, q.closeHover),
    };
    Theme r = fromPalette(palette, b.style);
    r.scheme = b.scheme;
    // the code's colors (a scheme's own)
    for (auto [out, x, y] : {std::tuple{&r.textEdit.keyword, a.textEdit.keyword, b.textEdit.keyword},
                             std::tuple{&r.textEdit.type, a.textEdit.type, b.textEdit.type},
                             std::tuple{&r.textEdit.string, a.textEdit.string, b.textEdit.string},
                             std::tuple{&r.textEdit.number, a.textEdit.number, b.textEdit.number},
                             std::tuple{&r.textEdit.comment, a.textEdit.comment, b.textEdit.comment},
                             std::tuple{&r.textEdit.preprocessor, a.textEdit.preprocessor, b.textEdit.preprocessor},
                             std::tuple{&r.textEdit.function, a.textEdit.function, b.textEdit.function}})
        *out = color(x, y);
    // the shapes
    r.cornerRadius = radius(a.cornerRadius, b.cornerRadius);
    r.button.cornerRadius = radius(a.button.cornerRadius, b.button.cornerRadius);
    r.button.padding = insets(a.button.padding, b.button.padding);
    r.button.bevel = bevel(a.button.bevel, b.button.bevel);
    r.button.focusRing = color(a.button.focusRing, b.button.focusRing);
    r.lineEdit.cornerRadius = radius(a.lineEdit.cornerRadius, b.lineEdit.cornerRadius);
    r.lineEdit.padding = insets(a.lineEdit.padding, b.lineEdit.padding);
    r.lineEdit.bevel = bevel(a.lineEdit.bevel, b.lineEdit.bevel);
    r.lineEdit.focusRing = color(a.lineEdit.focusRing, b.lineEdit.focusRing);
    r.checkBox.cornerRadius = radius(a.checkBox.cornerRadius, b.checkBox.cornerRadius);
    r.checkBox.bevel = bevel(a.checkBox.bevel, b.checkBox.bevel);
    r.checkBox.checkedBevel = bevel(a.checkBox.checkedBevel, b.checkBox.checkedBevel);
    r.checkBox.focusRing = color(a.checkBox.focusRing, b.checkBox.focusRing);
    r.scrollBar.cornerRadius = radius(a.scrollBar.cornerRadius, b.scrollBar.cornerRadius);
    for (auto [out, x, y] : {std::tuple{&r.itemView, &a.itemView, &b.itemView}, std::tuple{&r.table, &a.table, &b.table}}) {
        out->cornerRadius = radius(x->cornerRadius, y->cornerRadius);
        out->rowHeight = number(x->rowHeight, y->rowHeight);
        out->inset = number(x->inset, y->inset);
    }
    r.dock.cornerRadius = radius(a.dock.cornerRadius, b.dock.cornerRadius);
    r.majorTab.cornerRadius = radius(a.majorTab.cornerRadius, b.majorTab.cornerRadius);
    r.card.cornerRadius = radius(a.card.cornerRadius, b.card.cornerRadius);
    r.menu.cornerRadius = radius(a.menu.cornerRadius, b.menu.cornerRadius);
    r.menuBar.cornerRadius = radius(a.menuBar.cornerRadius, b.menuBar.cornerRadius);
    r.toolBar.cornerRadius = radius(a.toolBar.cornerRadius, b.toolBar.cornerRadius);
    r.toolBar.buttonSize = number(a.toolBar.buttonSize, b.toolBar.buttonSize);
    r.toolBar.bevel = bevel(a.toolBar.bevel, b.toolBar.bevel);
    auto& panel = r.collapsiblePanel;
    const auto& from = a.collapsiblePanel;
    const auto& to = b.collapsiblePanel;
    panel.cornerRadius = radius(from.cornerRadius, to.cornerRadius);
    panel.headerHeight = number(from.headerHeight, to.headerHeight);
    panel.bevel = bevel(from.bevel, to.bevel);
    panel.content = color(from.content, to.content);
    panel.border = color(from.border, to.border);
    panel.separator = color(from.separator, to.separator);
    panel.shadow = color(from.shadow, to.shadow);
    r.dock.selectedTabHighlight = color(a.dock.selectedTabHighlight, b.dock.selectedTabHighlight);
    r.node.cornerRadius = radius(a.node.cornerRadius, b.node.cornerRadius);
    for (std::size_t i = 0; i < r.chart.series.size(); ++i)
        r.chart.series[i] = color(a.chart.series[i], b.chart.series[i]);
    r.chart.barRadius = radius(a.chart.barRadius, b.chart.barRadius);
    r.chart.barBevel = bevel(a.chart.barBevel, b.chart.barBevel);
    r.dock.tabBevel = bevel(a.dock.tabBevel, b.dock.tabBevel);
    r.dock.islandBevel = bevel(a.dock.islandBevel, b.dock.islandBevel);
    r.card.bevel = bevel(a.card.bevel, b.card.bevel);
    r.dock.tabRadius = radius(a.dock.tabRadius, b.dock.tabRadius);
    r.majorTab.bevel = bevel(a.majorTab.bevel, b.majorTab.bevel);
    r.node.bevel = bevel(a.node.bevel, b.node.bevel);
    r.node.bodyBevel = bevel(a.node.bodyBevel, b.node.bodyBevel);
    for (ItemViewStyle* view : {&r.itemView, &r.table}) {
        const ItemViewStyle& x = view == &r.itemView ? a.itemView : a.table;
        const ItemViewStyle& y = view == &r.itemView ? b.itemView : b.table;
        view->headerBackground = color(x.headerBackground, y.headerBackground);
        view->headerBevel = bevel(x.headerBevel, y.headerBevel);
        view->rowBevel = bevel(x.rowBevel, y.rowBevel);
    }
    const paint::Lighting& la = a.lighting;
    const paint::Lighting& lb = b.lighting;
    r.lighting = {number(la.x, lb.x), number(la.y, lb.y), number(la.z, lb.z), number(la.ambient, lb.ambient),
                  number(la.diffuse, lb.diffuse), number(la.specular, lb.specular), number(la.shininess, lb.shininess),
                  number(la.rimShadow, lb.rimShadow), number(la.metal, lb.metal), number(la.brushed, lb.brushed)};
    return r;
}

// ---- the other styles: Studio's, changed ----

void Theme::fluid(Theme& t, bool dark) {
    // Plastic's and Metal's reliefs, lit softly: a broad, gentle white highlight
    reliefs(t, dark);
    t.lighting = {.x = -0.42f, .y = -0.62f, .z = 0.66f,
                  .ambient = dark ? 0.60f : 0.66f,
                  .diffuse = dark ? 0.40f : 0.34f,
                  .specular = dark ? 0.28f : 0.34f,
                  .shininess = 20,
                  .rimShadow = dark ? 0.45f : 0.30f};
    // rounder: buttons, fields, combo boxes, tool buttons and scroll thumbs pills (a half circle at each end: a radius
    // past half their height is half of it), as low as every style's and wider for their ends; the rest round
    constexpr float pill = 1000;
    t.cornerRadius = 6;
    t.button.padding = {14, 3, 14, 3};
    t.lineEdit.padding = {14, 3, 14, 3};
    t.button.cornerRadius = t.lineEdit.cornerRadius = t.toolBar.cornerRadius = t.scrollBar.cornerRadius = pill;
    t.menuBar.cornerRadius = t.menu.cornerRadius = 6;       // its items; its window square (a menu's, in every style)
    t.checkBox.cornerRadius = 5;
    t.itemView.cornerRadius = t.table.cornerRadius = 8;
    t.collapsiblePanel.cornerRadius = 10;
    t.dock.cornerRadius = t.card.cornerRadius = 10;
    t.dock.tabRadius = 8;
    t.majorTab.cornerRadius = 10;
    t.frame.popupRadius = 10;               // tooltips, dialogs, search popups (not menus): as the system rounds them
    t.node.cornerRadius = 12;
    t.chart.barRadius = 6;
    // their edges rounding down over further
    for (Bevel* bevel : {&t.button.bevel, &t.toolBar.bevel, &t.collapsiblePanel.bevel, &t.lineEdit.bevel, &t.dock.tabBevel,
                         &t.dock.islandBevel, &t.card.bevel, &t.majorTab.bevel, &t.itemView.headerBevel, &t.table.headerBevel,
                         &t.itemView.rowBevel, &t.table.rowBevel, &t.chart.barBevel})
        bevel->roundness *= 1.4f;
    // lists and trees: pills, no lines between them; the selection sliding to where it goes
    t.itemView.rowLines = false;
    t.itemView.inset = 4;
    t.itemView.slideSelection = true;
    t.table.slideSelection = true;
    // a press ripples
    const Color white{1, 1, 1, 1};
    t.button.ripple = withAlpha(dark ? white : t.palette.accent, dark ? 0.16f : 0.14f);
    t.itemView.ripple = withAlpha(t.palette.accent, dark ? 0.22f : 0.16f);
    t.table.ripple = t.itemView.ripple;
    // more motion and longer, popups from further
    t.motion.fast = std::chrono::milliseconds(150);
    t.motion.normal = std::chrono::milliseconds(260);
    t.motion.slow = std::chrono::milliseconds(380);
    t.motion.scroll = std::chrono::milliseconds(240);
    t.motion.ripple = std::chrono::milliseconds(520);
    t.motion.popupSlide = 12;
}

void Theme::reliefs(Theme& t, bool dark) {
    const Color black{0, 0, 0, 1};
    // round a little; the controls reliefs: buttons, combo boxes, tool buttons under the mouse, panels' headers and a
    // checked box raised, fields and boxes sunk, a soft shadow under what is raised
    t.cornerRadius = 4;
    t.button.cornerRadius = 5;
    t.lineEdit.cornerRadius = 4;
    t.checkBox.cornerRadius = 3;
    t.toolBar.cornerRadius = t.menuBar.cornerRadius = t.menu.cornerRadius = 4;
    t.collapsiblePanel.cornerRadius = 6;
    t.itemView.cornerRadius = t.table.cornerRadius = 4;
    t.dock.cornerRadius = t.card.cornerRadius = 5;     // stacks and cards a little round only
    t.majorTab.cornerRadius = 10;
    t.scrollBar.cornerRadius = 1000;
    t.frame.popupRadius = 8;
    const Color shadow = withAlpha(black, dark ? 0.50f : 0.22f);
    // low reliefs, their edges rounding over a few dips
    t.button.bevel = {.shadow = shadow, .relief = 1.6f, .roundness = 2.5f};
    t.toolBar.bevel = {.shadow = shadow, .relief = 1.4f, .roundness = 2};
    t.collapsiblePanel.bevel = {.shadow = shadow, .relief = 1.4f, .roundness = 2.5f};
    t.lineEdit.bevel = {.relief = 1.4f, .roundness = 2};
    t.checkBox.bevel = {.relief = 1.2f, .roundness = 1.5f};
    t.checkBox.checkedBevel = {.shadow = shadow, .relief = 1.2f, .roundness = 1.5f};
    // lists', trees' and tables' headers raised as controls, and the rows' highlights
    for (ItemViewStyle* view : {&t.itemView, &t.table}) {
        view->headerBackground = t.collapsiblePanel.header.normal;
        view->headerBevel = {.shadow = withAlpha(black, dark ? 0.35f : 0.12f), .relief = 1.2f, .roundness = 2};
        view->rowBevel = {.relief = 1.2f, .roundness = 2};
    }
    // tabs: the one shown a relief of its stack's color joined to it (its lower edge under the bar, cut: its top and sides
    // lit, its face the stack's), one under the mouse a little raised
    t.dock.tabsJoined = true;
    t.dock.tabBevel = {.shadow = withAlpha(black, dark ? 0.40f : 0.15f), .relief = 1.2f, .roundness = 1.5f};
    // the stacks a little raised, slabs, the tab shown a folder's tab of theirs
    t.dock.islandBevel = {.shadow = withAlpha(black, dark ? 0.45f : 0.14f), .relief = 0.8f, .roundness = 1.5f};
    t.card.bevel = t.dock.islandBevel;      // cards too (a dock's center)
    t.dock.tabRadius = 4;
    t.dock.selectedTab = t.dock.background;
    t.majorTab.joined = true;
    t.majorTab.bevel = t.dock.tabBevel;
    t.majorTab.cornerRadius = 6;
    t.node.cornerRadius = 8;
    t.chart.barRadius = 3;
    t.chart.barBevel = {.relief = 1.0f, .roundness = 1.5f};
    t.node.bevel = {.relief = 2.2f, .roundness = 3.5f};
    t.node.bodyBevel = {.relief = 1.6f, .roundness = 4};
    t.node.border = withAlpha(black, dark ? 0.45f : 0.15f);
}

void Theme::plastic(Theme& t, bool dark) {
    reliefs(t, dark);
    // glossy: a strong, tight, white highlight where a surface turns toward the light, from the upper left
    t.lighting = {.x = -0.42f, .y = -0.62f, .z = 0.66f,
                  .ambient = dark ? 0.58f : 0.64f,
                  .diffuse = dark ? 0.42f : 0.36f,
                  .specular = dark ? 0.42f : 0.50f,
                  .shininess = 42,
                  .rimShadow = dark ? 0.45f : 0.30f};
}

void Theme::metal(Theme& t, bool dark) {
    reliefs(t, dark);
    // a metal's highlight of its own color, broad; its surfaces brushed
    t.lighting = {.x = -0.42f, .y = -0.62f, .z = 0.66f,
                  .ambient = dark ? 0.50f : 0.56f,
                  .diffuse = dark ? 0.50f : 0.44f,
                  .specular = dark ? 0.85f : 0.70f,
                  .shininess = 14,
                  .rimShadow = dark ? 0.50f : 0.35f,
                  .metal = 0.85f,
                  .brushed = dark ? 0.07f : 0.05f};
}

void Theme::plain(Theme& t) {
    const Color clear{0, 0, 0, 0};
    // flat: no light from above, no shadows, no glow of the focus (its edge says it)
    for (Bevel* bevel : {&t.button.bevel, &t.lineEdit.bevel, &t.checkBox.bevel, &t.checkBox.checkedBevel, &t.toolBar.bevel,
                         &t.collapsiblePanel.bevel})
        *bevel = {};
    t.button.focusRing = t.lineEdit.focusRing = t.checkBox.focusRing = clear;
    t.collapsiblePanel.shadow = clear;
    t.itemView.headerBevel = t.table.headerBevel = t.node.bevel = t.node.bodyBevel = {};
    t.node.shadow = clear;
    t.node.cornerRadius = 0;
    t.chart.barBevel = {};
    t.chart.barRadius = 0;
    t.node.border = t.palette.outline;
    t.collapsiblePanel.content = t.palette.layer;
    t.collapsiblePanel.border = t.collapsiblePanel.separator = t.palette.outline;
    t.dock.selectedTabHighlight = clear;
    t.dock.selectedTab = t.dock.background;     // flat: the tab shown of the color of what it shows
    t.dock.tabRadius = 0;
    // nothing moves: every motion at once
    t.motion.fast = t.motion.normal = t.motion.slow = t.motion.scroll = t.motion.ripple = std::chrono::milliseconds(0);
    t.motion.popupSlide = 0;
}

} // namespace glw
