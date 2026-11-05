// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Theme.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

#include <algorithm>
#include <tuple>

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

Theme Theme::fromPalette(const Palette& p, ThemeStyle style) {
    Theme t;
    t.style = style;
    t.palette = p;
    const bool dark = p.window.r + p.window.g + p.window.b < 1.5f;
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
    t.dock.scrollThumb = withAlpha(p.mutedText, 0.55f);
    t.dock.cornerRadius = t.cornerRadius;

    t.majorTab.background = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.majorTab.active = p.window;
    t.majorTab.text = p.text;
    t.majorTab.mutedText = p.mutedText;
    t.majorTab.insertion = p.accent;
    t.majorTab.cornerRadius = t.cornerRadius;

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
        return Bevel{number(x.gradient, y.gradient), color(x.highlight, y.highlight), color(x.shadow, y.shadow)};
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
    return r;
}

// ---- the other styles: Studio's, changed ----

void Theme::fluid(Theme& t, bool dark) {
    const Palette& p = t.palette;
    const Color clear{0, 0, 0, 0};
    const Color white{1, 1, 1, 1};
    const Color black{0, 0, 0, 1};
    // lower, and round: buttons, fields, combo boxes, tool buttons pills (a half circle at each end: a radius past half
    // their height is half of it); panels' headers, rows, menus round; docks and cards a little
    constexpr float pill = 1000;
    t.cornerRadius = 6;
    t.button.padding = {14, 3, 14, 3};
    t.lineEdit.padding = {14, 3, 14, 3};
    t.button.cornerRadius = t.lineEdit.cornerRadius = t.toolBar.cornerRadius = pill;
    t.menuBar.cornerRadius = 6;
    t.toolBar.buttonSize = 28;
    t.checkBox.cornerRadius = 5;
    t.itemView.rowHeight = t.table.rowHeight = 26;
    t.itemView.cornerRadius = t.table.cornerRadius = 8;
    t.collapsiblePanel.headerHeight = 30;
    t.collapsiblePanel.cornerRadius = 10;
    t.menu.cornerRadius = 6;               // its items; its window square (a menu's, in every style)
    t.dock.cornerRadius = t.card.cornerRadius = 10;
    t.majorTab.cornerRadius = 8;
    t.scrollBar.cornerRadius = pill;
    t.frame.popupRadius = 8;                // tooltips, dialogs, search popups (not menus): as the system rounds them
    // flat, a soft shadow under what stands out; a field without its inner shadow
    // raised a little: lighter at its top, a fine light line inside along it, a soft shadow under it (buttons, combo
    // boxes, tool buttons under the mouse, panels' headers)
    const Bevel raised{.gradient = dark ? 0.05f : 0.04f, .highlight = withAlpha(white, dark ? 0.07f : 0.8f),
                       .shadow = withAlpha(black, dark ? 0.30f : 0.08f)};
    t.button.bevel = raised;
    t.button.background = {.normal = p.surface, .hovered = lit(p.surface, dark ? 0.06f : -0.03f), .pressed = lit(p.surface, dark ? 0.10f : -0.07f),
                           .disabled = mix(p.surface, p.layer, 0.5f)};
    t.button.border = {.normal = withAlpha(p.text, dark ? 0.08f : 0.12f), .focused = p.accent, .disabled = withAlpha(p.text, 0.05f)};
    t.lineEdit.bevel = {};
    t.checkBox.bevel = {};
    t.checkBox.checkedBevel = {};
    t.toolBar.bevel = raised;
    t.collapsiblePanel.bevel = {};
    // panels: no frame; the header raised a little (lighter at its top, a fine light line, a soft shadow) over the body,
    // lighter under the mouse or with the focus; a soft shadow under all of it
    const Color strip = dark ? mix(p.layer, p.surface, 0.40f) : lit(p.window, -0.035f);
    const Color lighter = dark ? mix(p.layer, p.surface, 0.75f) : lit(p.window, -0.06f);
    t.collapsiblePanel.header = {.normal = strip, .hovered = lighter, .pressed = dark ? mix(p.layer, p.surface, 0.25f) : lit(p.window, -0.08f),
                                 .focused = lighter};
    t.collapsiblePanel.content = dark ? mix(p.layer, p.surface, 0.15f) : lit(p.window, -0.015f);
    t.collapsiblePanel.bevel = raised;
    t.collapsiblePanel.border = clear;
    t.collapsiblePanel.separator = withAlpha(black, dark ? 0.22f : 0.05f);
    t.collapsiblePanel.shadow = withAlpha(black, dark ? 0.30f : 0.08f);
    t.collapsiblePanel.arrow = p.text;
    // lists and trees: pills, no lines between them; the selection sliding to where it goes
    t.itemView.rowLines = false;
    t.itemView.inset = 4;
    t.itemView.slideSelection = true;
    t.table.slideSelection = true;
    // a press ripples
    t.button.ripple = withAlpha(dark ? white : p.accent, dark ? 0.16f : 0.14f);
    t.itemView.ripple = withAlpha(p.accent, dark ? 0.22f : 0.16f);
    t.table.ripple = t.itemView.ripple;
    // longer motions, popups from further
    t.motion.fast = std::chrono::milliseconds(150);
    t.motion.normal = std::chrono::milliseconds(260);
    t.motion.slow = std::chrono::milliseconds(380);
    t.motion.scroll = std::chrono::milliseconds(240);
    t.motion.ripple = std::chrono::milliseconds(520);
    t.motion.popupSlide = 12;
}

void Theme::plain(Theme& t) {
    const Color clear{0, 0, 0, 0};
    // flat: no light from above, no shadows, no glow of the focus (its edge says it)
    for (Bevel* bevel : {&t.button.bevel, &t.lineEdit.bevel, &t.checkBox.bevel, &t.checkBox.checkedBevel, &t.toolBar.bevel,
                         &t.collapsiblePanel.bevel})
        *bevel = {};
    t.button.focusRing = t.lineEdit.focusRing = t.checkBox.focusRing = clear;
    t.collapsiblePanel.shadow = clear;
    t.collapsiblePanel.content = t.palette.layer;
    t.collapsiblePanel.border = t.collapsiblePanel.separator = t.palette.outline;
    t.dock.selectedTabHighlight = clear;
    // nothing moves: every motion at once
    t.motion.fast = t.motion.normal = t.motion.slow = t.motion.scroll = t.motion.ripple = std::chrono::milliseconds(0);
    t.motion.popupSlide = 0;
}

} // namespace glw
