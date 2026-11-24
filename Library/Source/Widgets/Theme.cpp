// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Theme.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <utility>

namespace glw {

namespace {

constexpr Color hex(unsigned v, float alpha = 1) {
    return {((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f, alpha};
}

// The luminance at which black and white have equal contrast is sqrt(0.05 * 1.05) - 0.05.
bool darkWindow(Color window) {
    return relativeLuminance(window) < 0.1791288f;
}

} // namespace

// Studio's: neutral grays with a little blue in them, as Fusion's are gray; controls raised a step above the panels they
// are on and lit from above, fields sunk into them, the edges of controls darker than what is around them. The accent: in
// the dark, amber (a warm orange, a little toward mustard) with dark marks on it; in the light, azure with white ones.
// Fluid's: deeper and smoother, the layers further apart, the accent brighter. Plain's: grays without a tint. Their dark
// ones deep: the window near black, the layers and controls a step apart above it.
Palette Palette::dark(ThemeStyle style) {
    if (style == ThemeStyle::Solid) {
        return {
            .window = hex(0x17181B),
            .layer = hex(0x1E2024),
            .surface = hex(0x30343A),
            .base = hex(0x111315),
            .titleBar = hex(0x111215),
            .text = hex(0xF1F3F5),
            .mutedText = hex(0xA5ABB5),
            .disabledText = hex(0x626873),
            .accent = hex(0xF3A64A),
            .onAccent = hex(0x211508),
            .border = {1, 1, 1, 0.075f},
            .outline = {0, 0, 0, 0.48f},
            .hover = {1, 1, 1, 0.065f},
            .pressed = {1, 1, 1, 0.105f},
            .selection = hex(0xF3A64A, 0.34f),
            .closeHover = hex(0xC43C32),
        };
    }
    if (style == ThemeStyle::Fluid) {
        return {
            .window = hex(0x121315),
            .layer = hex(0x1A1B1E),
            .surface = hex(0x28292E),
            .base = hex(0x0F1012),
            .titleBar = hex(0x0E0E10),
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
            .window = hex(0x151719),
            .layer = hex(0x1E2124),
            .surface = hex(0x41464D),
            .base = hex(0x111214),
            .titleBar = hex(0x111113),
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
            .window = hex(0x202020),
            .layer = hex(0x252525),
            .surface = hex(0x343434),
            .base = hex(0x1B1B1B),
            .titleBar = hex(0x1B1B1B),
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
        .window = hex(0x1B1C1F),
        .layer = hex(0x212327),
        .surface = hex(0x33373D),
        .base = hex(0x15171A),
        .titleBar = hex(0x141518),
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
    if (style == ThemeStyle::Solid) {
        return {
            .window = hex(0xEEF0F2),
            .layer = hex(0xF5F6F7),
            .surface = hex(0xFCFCFD),
            .base = hex(0xE7EAED),
            .titleBar = hex(0xE1E4E7),
            .text = hex(0x20242A),
            .mutedText = hex(0x59616E),
            .disabledText = hex(0xA0A7AF),
            .accent = hex(0x326FD1),
            .onAccent = hex(0xFFFFFF),
            .border = {0, 0, 0, 0.085f},
            .outline = {0, 0, 0, 0.20f},
            .hover = {0, 0, 0, 0.045f},
            .pressed = {0, 0, 0, 0.085f},
            .selection = hex(0x3978D4, 0.24f),
            .closeHover = hex(0xC43C32),
        };
    }
    if (style == ThemeStyle::Fluid) {
        return {
            .window = hex(0xE1E3E5),
            .layer = hex(0xEBEDEF),
            .surface = hex(0xF3F4F5),
            .base = hex(0xF8F9FA),
            .titleBar = hex(0xD5D7DA),
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
            .window = hex(0xCBCED2),
            .layer = hex(0xDBDEE1),
            .surface = hex(0xBCC1C7),
            .base = hex(0xEBECEE),
            .titleBar = hex(0xBCC0C5),
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
            .window = hex(0xDDDDDD),
            .layer = hex(0xE8E8E8),
            .surface = hex(0xF1F1F1),
            .base = hex(0xF8F8F8),
            .titleBar = hex(0xCFCFCF),
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
        .window = hex(0xDADCE0),
        .layer = hex(0xE6E7E9),
        .surface = hex(0xEFF0F1),
        .base = hex(0xF7F8F9),
        .titleBar = hex(0xCED1D6),
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
                                                                 {"metal", ThemeStyle::Metal}, {"solid", ThemeStyle::Solid},
                                                                 {"glass", ThemeStyle::Glass}};
    for (const auto& [n, style] : names)
        if (n == name)
            return style;
    return std::nullopt;
}

std::optional<ColorScheme> colorSchemeNamed(std::string_view name) {
    constexpr std::pair<std::string_view, ColorScheme> names[] = {{"dark", ColorScheme::Dark}, {"light", ColorScheme::Light},
                                                                  {"dusk", ColorScheme::Dusk}, {"sand", ColorScheme::Sand},
                                                                  {"turquoise", ColorScheme::Turquoise}, {"celadon", ColorScheme::Celadon},
                                                                  {"midnight", ColorScheme::Midnight}};
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
            .window = hex(0xDDD6C8),
            .layer = hex(0xEAE5DB),
            .surface = hex(0xF3F0E9),
            .base = hex(0xF9F7F2),
            .titleBar = hex(0xD2C8B6),
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
    case ColorScheme::Celadon:
        p = {
            .window = hex(0xD8E1D9),
            .layer = hex(0xE5ECE5),
            .surface = hex(0xF0F3ED),
            .base = hex(0xF6F8F2),
            .titleBar = hex(0xCBD6CD),
            .text = hex(0x20352E),
            .mutedText = hex(0x506458),
            .disabledText = hex(0x88988C),
            .accent = hex(0x28664F),
            .onAccent = hex(0xF8FBF5),
            .border = hex(0x193C2A, 0.12f),
            .outline = hex(0x193C2A, 0.28f),
            .hover = hex(0x35664D, 0.06f),
            .pressed = hex(0x264C39, 0.12f),
            .selection = hex(0x28664F, 0.22f),
            .closeHover = hex(0xB64647),
        };
        break;
    case ColorScheme::Midnight:
        p = {
            .window = hex(0x171E2B),
            .layer = hex(0x202A39),
            .surface = hex(0x303D50),
            .base = hex(0x121925),
            .titleBar = hex(0x101621),
            .text = hex(0xDFE7F2),
            .mutedText = hex(0xA2B2C7),
            .disabledText = hex(0x65748B),
            .accent = hex(0xF2AF82),
            .onAccent = hex(0x302013),
            .border = hex(0xC1D5EC, 0.12f),
            .outline = hex(0x070E1B, 0.60f),
            .hover = hex(0xBDD8F4, 0.08f),
            .pressed = hex(0x070D17, 0.24f),
            .selection = hex(0xF2AF82, 0.30f),
            .closeHover = hex(0xAD4754),
        };
        break;
    }
    if (style == ThemeStyle::Metal) {
        // metal: the controls further from what they are on (lighter in the dark, darker in the light), as Metal's own
        const bool dark = darkWindow(p.window);
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
        t.chart.rise = hex(0x8FD3A0);
        t.chart.fall = hex(0xF28B82);
        e.keyword = hex(0xB99DFF);
        e.type = hex(0x7FD3C4);
        e.string = hex(0xE8B98C);
        e.number = hex(0xF2A5C2);
        e.comment = hex(0x7D7591);
        e.preprocessor = hex(0xE18FDB);
        e.function = hex(0xEADFA6);
        e.success = hex(0x8FD3A0);
        e.warning = hex(0xE8B98C);
        e.error = hex(0xF28B82);
        break;
    case ColorScheme::Sand:
        t.chart.series = {hex(0xB5532F), hex(0x24766E), hex(0xC99A2E), hex(0x5F7418), hex(0x7A4C9A), hex(0x3E6FA8),
                          hex(0x8C3B5A), hex(0x6E8B3D), hex(0x9A7B5B), hex(0x4F5D73)};
        t.chart.rise = hex(0x3F7F3A);
        t.chart.fall = hex(0xB5432F);
        e.keyword = hex(0x9C3B1E);
        e.type = hex(0x24766E);
        e.string = hex(0x5F7418);
        e.number = hex(0x9A5A00);
        e.comment = hex(0x968975);
        e.preprocessor = hex(0x7A4C9A);
        e.function = hex(0x5A4512);
        e.success = hex(0x3F7F3A);
        e.warning = hex(0x9A5A00);
        e.error = hex(0xB5432F);
        break;
    case ColorScheme::Turquoise:
        t.chart.series = {hex(0xE3B04B), hex(0x4FD1D9), hex(0xF2A08F), hex(0x86ABFF), hex(0x7FE6B0), hex(0xC99CE8),
                          hex(0xF4DBA6), hex(0xE86A7A), hex(0x5FA8D3), hex(0xA9B4C8)};
        t.chart.rise = hex(0x7FE6B0);
        t.chart.fall = hex(0xE86A7A);
        e.keyword = hex(0x86ABFF);
        e.type = hex(0x7FD9EE);
        e.string = hex(0xE9C27A);
        e.number = hex(0xF2A08F);
        e.comment = hex(0x6C97A6);
        e.preprocessor = hex(0xC99CE8);
        e.function = hex(0xF4DBA6);
        e.success = hex(0x7FE6B0);
        e.warning = hex(0xE9C27A);
        e.error = hex(0xE86A7A);
        break;
    case ColorScheme::Celadon:
        t.chart.series = {hex(0x28664F), hex(0x416BB0), hex(0xA15B32), hex(0x8060A3), hex(0x397B84),
                          hex(0x997422), hex(0x99536F), hex(0x627941), hex(0x546B89), hex(0x8A655F)};
        t.chart.rise = hex(0x315E36);
        t.chart.fall = hex(0xA03E4D);
        e.keyword = hex(0x6C4987);
        e.type = hex(0x226759);
        e.string = hex(0x526622);
        e.number = hex(0x91552C);
        e.comment = hex(0x5E7164);
        e.preprocessor = hex(0x8B455E);
        e.function = hex(0x395E89);
        e.success = t.chart.rise;
        e.error = t.chart.fall;
        e.warning = hex(0x815A13);
        break;
    case ColorScheme::Midnight:
        t.chart.series = {hex(0xF2AF82), hex(0x83ACEE), hex(0x78C3AC), hex(0xC3A1DE), hex(0xDCC57C),
                          hex(0x77C2D1), hex(0xDF96A9), hex(0xA6C183), hex(0x9CA8DB), hex(0xC1A693)};
        t.chart.rise = hex(0x91CAA2);
        t.chart.fall = hex(0xF09EAA);
        e.keyword = hex(0xC3A1DE);
        e.type = hex(0x78C3AC);
        e.string = hex(0xDCC57C);
        e.number = hex(0xF2AF82);
        e.comment = hex(0x8E9EB5);
        e.preprocessor = hex(0xDF96A9);
        e.function = hex(0x9DBCF1);
        e.success = t.chart.rise;
        e.error = t.chart.fall;
        e.warning = hex(0xE0BE78);
        break;
    }
    return t;
}

bool Theme::isDark() const {
    if (palette.window == windowTone.window)
        return windowTone.dark;
    // Also support directly constructed themes and callers replacing the public window color.
    thread_local WindowTone last;
    if (last.window != palette.window)
        last = {palette.window, darkWindow(palette.window)};
    return last.dark;
}

Theme Theme::fromPalette(const Palette& p, ThemeStyle style) {
    Theme t;
    t.style = style;
    t.palette = p;
    const bool dark = darkWindow(p.window);
    t.windowTone = {p.window, dark};
    t.scheme = dark ? ColorScheme::Dark : ColorScheme::Light;
    const Color clear{0, 0, 0, 0};
    const Color white{1, 1, 1, 1};
    const Color black{0, 0, 0, 1};
    const Color glow = withAlpha(p.accent, dark ? 0.45f : 0.35f);
    // lit from above: raised, a light line inside along the top and a soft shadow; sunken, a shadow inside along the top
    const Bevel raised{.gradient = dark ? 0.05f : 0.07f, .highlight = withAlpha(white, dark ? 0.08f : 0.58f),
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
    t.dock.selectedTab = dark ? lit(p.layer, 0.09f) : lit(p.layer, 0.36f);
    t.dock.selectedTabBorder = dark ? withAlpha(black, 0.45f) : withAlpha(black, 0.16f);
    t.dock.selectedTabHighlight = withAlpha(white, dark ? 0.08f : 0.65f);
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
    t.textEdit.muted = p.mutedText;
    t.textEdit.success = hex(dark ? 0x4EC98A : 0x2E7D32);
    t.textEdit.warning = hex(dark ? 0xE5B550 : 0xA86400);
    t.textEdit.error = hex(dark ? 0xF26D6D : 0xC62828);
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
    t.chart.rise = dark ? hex(0x3FB984) : hex(0x1E9A5A);
    t.chart.fall = dark ? hex(0xE5574F) : hex(0xD03C3C);
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
    else if (style == ThemeStyle::Solid)
        solid(t, dark);
    else if (style == ThemeStyle::Glass)
        glass(t, dark);
    return t;
}

// ---- between two themes ----

Theme Theme::blend(const Theme& a, const Theme& b, float t) {
    if (t <= 0)
        return a;
    if (t >= 1)
        return b;
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
                     .roundness = number(x.roundness, y.roundness),
                     .edge = number(x.edge, y.edge)};
    };
    Theme r = b;
    const auto states = [&](const StateColor& x, const StateColor& y) {
        const auto optional = [&](const std::optional<Color>& left, const std::optional<Color>& right) -> std::optional<Color> {
            if (!left && !right)
                return std::nullopt;
            return color(left.value_or(x.normal), right.value_or(y.normal));
        };
        return StateColor{color(x.normal, y.normal), optional(x.hovered, y.hovered), optional(x.pressed, y.pressed),
                          optional(x.focused, y.focused), optional(x.disabled, y.disabled)};
    };
    const auto fields = [&]<class Style, class Field>(Style Theme::*style, auto interpolate, std::initializer_list<Field Style::*> members) {
        for (auto member : members)
            (r.*style).*member = interpolate((a.*style).*member, (b.*style).*member);
    };
    fields(&Theme::palette, color, {&Palette::window, &Palette::layer, &Palette::surface, &Palette::base, &Palette::titleBar, &Palette::text,
                                    &Palette::mutedText, &Palette::disabledText, &Palette::accent, &Palette::onAccent, &Palette::border,
                                    &Palette::outline, &Palette::hover, &Palette::pressed, &Palette::selection, &Palette::closeHover});
    fields(&Theme::button, color, {&ButtonStyle::focusRing, &ButtonStyle::ripple});
    fields(&Theme::button, states, {&ButtonStyle::background, &ButtonStyle::border, &ButtonStyle::text});
    fields(&Theme::button, bevel, {&ButtonStyle::bevel});
    fields(&Theme::lineEdit, color, {&LineEditStyle::text, &LineEditStyle::placeholder, &LineEditStyle::selection, &LineEditStyle::caret,
                                     &LineEditStyle::focusLine, &LineEditStyle::focusRing});
    fields(&Theme::lineEdit, states, {&LineEditStyle::background, &LineEditStyle::border});
    fields(&Theme::lineEdit, bevel, {&LineEditStyle::bevel, &LineEditStyle::stepperBevel});
    fields(&Theme::checkBox, color, {&CheckBoxStyle::checkedBorder, &CheckBoxStyle::check, &CheckBoxStyle::focusRing});
    fields(&Theme::checkBox, states, {&CheckBoxStyle::background, &CheckBoxStyle::border, &CheckBoxStyle::checkedBackground});
    fields(&Theme::checkBox, bevel, {&CheckBoxStyle::bevel, &CheckBoxStyle::checkedBevel});
    fields(&Theme::scrollBar, color, {&ScrollBarStyle::track});
    fields(&Theme::scrollBar, states, {&ScrollBarStyle::thumb});
    fields(&Theme::scrollBar, bevel, {&ScrollBarStyle::trackBevel, &ScrollBarStyle::thumbBevel});
    fields(&Theme::itemView, color, {&ItemViewStyle::selected, &ItemViewStyle::indicator, &ItemViewStyle::focusRing, &ItemViewStyle::expander,
                                     &ItemViewStyle::headerText, &ItemViewStyle::headerLine, &ItemViewStyle::gridLine, &ItemViewStyle::ripple,
                                     &ItemViewStyle::headerBackground});
    fields(&Theme::itemView, states, {&ItemViewStyle::row});
    fields(&Theme::itemView, bevel, {&ItemViewStyle::headerBevel, &ItemViewStyle::rowBevel});
    fields(&Theme::table, color, {&ItemViewStyle::selected, &ItemViewStyle::indicator, &ItemViewStyle::focusRing, &ItemViewStyle::expander,
                                  &ItemViewStyle::headerText, &ItemViewStyle::headerLine, &ItemViewStyle::gridLine, &ItemViewStyle::ripple,
                                  &ItemViewStyle::headerBackground});
    fields(&Theme::table, states, {&ItemViewStyle::row});
    fields(&Theme::table, bevel, {&ItemViewStyle::headerBevel, &ItemViewStyle::rowBevel});
    fields(&Theme::splitter, color, {&SplitterStyle::handle, &SplitterStyle::active});
    fields(&Theme::splitter, bevel, {&SplitterStyle::groove});
    fields(&Theme::dock, color, {&DockStyle::background, &DockStyle::border, &DockStyle::separator, &DockStyle::selectedTab,
                                 &DockStyle::selectedTabBorder, &DockStyle::selectedTabHighlight, &DockStyle::activeLine, &DockStyle::text,
                                 &DockStyle::mutedText, &DockStyle::dropZone});
    fields(&Theme::dock, states, {&DockStyle::tab});
    fields(&Theme::dock, bevel, {&DockStyle::tabBevel, &DockStyle::islandBevel});
    fields(&Theme::majorTab, color, {&MajorTabStyle::active, &MajorTabStyle::text, &MajorTabStyle::mutedText, &MajorTabStyle::insertion,
                                     &MajorTabStyle::raised});
    fields(&Theme::majorTab, states, {&MajorTabStyle::background});
    fields(&Theme::majorTab, bevel, {&MajorTabStyle::bevel});
    fields(&Theme::card, color, {&CardStyle::background, &CardStyle::border});
    fields(&Theme::card, bevel, {&CardStyle::bevel});
    fields(&Theme::canvas, color, {&CanvasStyle::background, &CanvasStyle::grid, &CanvasStyle::majorGrid});
    fields(&Theme::chart, color, {&ChartStyle::background, &ChartStyle::plot, &ChartStyle::grid, &ChartStyle::axis, &ChartStyle::text,
                                  &ChartStyle::title, &ChartStyle::crosshair, &ChartStyle::selection, &ChartStyle::rise, &ChartStyle::fall});
    fields(&Theme::chart, bevel, {&ChartStyle::barBevel});
    fields(&Theme::node, color, {&NodeStyle::body, &NodeStyle::border, &NodeStyle::shadow, &NodeStyle::selection});
    fields(&Theme::node, bevel, {&NodeStyle::bevel, &NodeStyle::bodyBevel});
    fields(&Theme::menu, color, {&MenuStyle::background, &MenuStyle::border, &MenuStyle::highlight, &MenuStyle::highlightText,
                                 &MenuStyle::text, &MenuStyle::shortcut, &MenuStyle::disabledText, &MenuStyle::mark, &MenuStyle::separator});
    fields(&Theme::menu, bevel, {&MenuStyle::bevel, &MenuStyle::highlightBevel});
    fields(&Theme::collapsiblePanel, color, {&CollapsiblePanelStyle::content, &CollapsiblePanelStyle::border,
                                             &CollapsiblePanelStyle::separator, &CollapsiblePanelStyle::shadow, &CollapsiblePanelStyle::title,
                                             &CollapsiblePanelStyle::arrow});
    fields(&Theme::collapsiblePanel, states, {&CollapsiblePanelStyle::header});
    fields(&Theme::collapsiblePanel, bevel, {&CollapsiblePanelStyle::bevel});
    fields(&Theme::menuBar, color, {&MenuBarStyle::background, &MenuBarStyle::text});
    fields(&Theme::menuBar, states, {&MenuBarStyle::title});
    fields(&Theme::toolBar, color, {&ToolBarStyle::background, &ToolBarStyle::separator, &ToolBarStyle::checked, &ToolBarStyle::outline,
                                    &ToolBarStyle::icon, &ToolBarStyle::disabledIcon, &ToolBarStyle::ripple});
    fields(&Theme::toolBar, states, {&ToolBarStyle::button});
    fields(&Theme::toolBar, bevel, {&ToolBarStyle::bevel});
    fields(&Theme::statusBar, bevel, {&StatusBarStyle::bevel});
    fields(&Theme::textEdit, color, {&TextEditStyle::background, &TextEditStyle::text, &TextEditStyle::gutter, &TextEditStyle::lineNumber,
                                     &TextEditStyle::currentLineNumber, &TextEditStyle::currentLine, &TextEditStyle::selection,
                                     &TextEditStyle::caret, &TextEditStyle::keyword, &TextEditStyle::type, &TextEditStyle::string,
                                     &TextEditStyle::number, &TextEditStyle::comment, &TextEditStyle::preprocessor, &TextEditStyle::function,
                                     &TextEditStyle::muted, &TextEditStyle::success, &TextEditStyle::warning, &TextEditStyle::error});
    fields(&Theme::toolTip, color, {&ToolTipStyle::background, &ToolTipStyle::border, &ToolTipStyle::text});
    fields(&Theme::toolTip, bevel, {&ToolTipStyle::bevel});
    fields(&Theme::dialog, color, {&DialogStyle::background, &DialogStyle::border, &DialogStyle::scrim});
    fields(&Theme::dialog, bevel, {&DialogStyle::bevel});
    fields(&Theme::titleBar, color, {&TitleBarStyle::background, &TitleBarStyle::title});
    fields(&Theme::windowButton, color, {&WindowButtonStyle::glyph});
    fields(&Theme::windowButton, states, {&WindowButtonStyle::background, &WindowButtonStyle::closeBackground});
    fields(&Theme::frame, color, {&WindowFrameStyle::background});
    r.windowTone = {r.palette.window, t < 0.5f ? a.isDark() : b.isDark()};
    if (r.backdrop) {
        r.backdrop->base = color(a.backdrop ? a.backdrop->base : a.frame.background, b.backdrop->base);
        if (a.backdrop) {
            const float delta = std::remainder(b.backdrop->hue - a.backdrop->hue, 1.0f);
            r.backdrop->hue = std::fmod(a.backdrop->hue + t * delta + 1.0f, 1.0f);
            r.backdrop->vivid = number(a.backdrop->vivid, b.backdrop->vivid);
            r.backdrop->tile = number(a.backdrop->tile, b.backdrop->tile);
        }
    }
    // the shapes
    r.cornerRadius = radius(a.cornerRadius, b.cornerRadius);
    r.button.cornerRadius = radius(a.button.cornerRadius, b.button.cornerRadius);
    r.button.hoverLift = number(a.button.hoverLift, b.button.hoverLift);
    r.button.padding = insets(a.button.padding, b.button.padding);
    r.lineEdit.cornerRadius = radius(a.lineEdit.cornerRadius, b.lineEdit.cornerRadius);
    r.lineEdit.padding = insets(a.lineEdit.padding, b.lineEdit.padding);
    r.checkBox.cornerRadius = radius(a.checkBox.cornerRadius, b.checkBox.cornerRadius);
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
    auto& panel = r.collapsiblePanel;
    const auto& from = a.collapsiblePanel;
    const auto& to = b.collapsiblePanel;
    panel.cornerRadius = radius(from.cornerRadius, to.cornerRadius);
    panel.headerHeight = number(from.headerHeight, to.headerHeight);
    r.node.cornerRadius = radius(a.node.cornerRadius, b.node.cornerRadius);
    for (std::size_t i = 0; i < r.chart.series.size(); ++i)
        r.chart.series[i] = color(a.chart.series[i], b.chart.series[i]);
    r.chart.barRadius = radius(a.chart.barRadius, b.chart.barRadius);
    r.dock.tabRadius = radius(a.dock.tabRadius, b.dock.tabRadius);
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
    t.frame.windowRadius = 8;               // windows too, where the system rounds them
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
    // what comes one of its ways at random each time (in its plane: a chart's pieces growing, sliding, settling)
    t.motion.varied = true;
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
    t.frame.windowRadius = 8;
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
    t.dock.transition = DockTransition::Soft;
    t.dock.landing = std::chrono::milliseconds(360);
    t.dock.rising = std::chrono::milliseconds(320);
    t.dock.variedArrivals = false;
    t.motion.popupSlide = 10;
    // glossy: a strong, tight, white highlight where a surface turns toward the light, from the upper left
    t.lighting = {.x = -0.42f, .y = -0.62f, .z = 0.66f,
                  .ambient = dark ? 0.58f : 0.64f,
                  .diffuse = dark ? 0.42f : 0.36f,
                  .specular = dark ? 0.42f : 0.38f,
                  .shininess = 42,
                  .rimShadow = dark ? 0.45f : 0.30f};
}

void Theme::metal(Theme& t, bool dark) {
    reliefs(t, dark);
    t.dock.transition = DockTransition::Mechanical;
    t.dock.landing = std::chrono::milliseconds(240);
    t.dock.rising = std::chrono::milliseconds(210);
    t.dock.variedArrivals = false;
    t.motion.popupSlide = 5;
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
