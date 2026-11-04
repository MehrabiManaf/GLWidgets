// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Theme.hpp>

#include <GLWidgets/Widgets/Bevel.hpp>

namespace glw {

namespace {

constexpr Color hex(unsigned v, float alpha = 1) {
    return {((v >> 16) & 0xff) / 255.0f, ((v >> 8) & 0xff) / 255.0f, (v & 0xff) / 255.0f, alpha};
}

} // namespace

// GLWidgets' own: neutral grays with a little blue in them, as Fusion's are gray; controls raised a step above the panels
// they are on and lit from above, fields sunk into them, the edges of controls darker than what is around them. The accent:
// in the dark, amber (a warm orange, a little toward mustard) with dark marks on it; in the light, azure with white ones.
Palette Palette::dark() {
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

Palette Palette::light() {
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

Theme Theme::fromPalette(const Palette& p) {
    Theme t;
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

    t.scrollBar.track = clear;
    t.scrollBar.thumb = {.normal = withAlpha(p.text, 0.22f), .hovered = withAlpha(p.text, 0.38f), .pressed = withAlpha(p.text, 0.52f)};

    t.itemView.row = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.itemView.selected = withAlpha(p.accent, dark ? 0.40f : 0.16f);
    t.itemView.indicator = p.accent;
    t.itemView.focusRing = withAlpha(p.accent, 0.75f);
    t.itemView.expander = p.mutedText;
    t.itemView.headerText = p.mutedText;
    t.itemView.headerLine = withAlpha(p.text, 0.10f);

    t.splitter.handle = p.border;
    t.splitter.active = p.accent;

    t.dock.background = p.layer;
    t.dock.border = dark ? withAlpha(black, 0.35f) : withAlpha(black, 0.10f);
    t.dock.separator = dark ? withAlpha(black, 0.30f) : withAlpha(black, 0.07f);
    t.dock.tab = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.dock.activeTab = dark ? mix(p.layer, p.surface, 0.8f) : lit(p.surface, 0.6f);
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
    t.dock.cornerRadius = 6;

    t.majorTab.background = {.normal = clear, .hovered = p.hover, .pressed = p.pressed};
    t.majorTab.active = p.window;
    t.majorTab.text = p.text;
    t.majorTab.mutedText = p.mutedText;
    t.majorTab.insertion = p.accent;

    // a raised header over its content, sunk a little into the panel
    t.collapsiblePanel.header = {.normal = mix(p.layer, p.surface, 0.45f),
                                 .hovered = dark ? mix(p.layer, p.surface, 0.75f) : lit(p.surface, 0.5f),
                                 .pressed = mix(p.layer, p.surface, 0.25f)};
    t.collapsiblePanel.bevel = {.gradient = raised.gradient, .highlight = raised.highlight};
    t.collapsiblePanel.content = dark ? lit(p.window, -0.04f) : p.layer;
    t.collapsiblePanel.border = p.outline;
    t.collapsiblePanel.title = p.text;
    t.collapsiblePanel.arrow = p.mutedText;
    t.collapsiblePanel.focusRing = withAlpha(p.accent, 0.9f);
    t.collapsiblePanel.cornerRadius = t.cornerRadius + 1;

    t.menuBar.background = p.titleBar;
    t.menuBar.text = p.text;
    t.menuBar.title = {.normal = clear, .hovered = p.hover, .pressed = withAlpha(p.text, dark ? 0.10f : 0.08f)};
    t.menuBar.cornerRadius = t.cornerRadius;

    // tool buttons: flat until the mouse is over them, then raised
    t.toolBar.background = p.window;
    t.toolBar.separator = withAlpha(p.text, 0.12f);
    t.toolBar.button = {.normal = clear, .hovered = withAlpha(lit(p.surface, dark ? 0.0f : 0.5f), dark ? 0.85f : 0.9f),
                        .pressed = withAlpha(lit(p.surface, -0.06f), 0.85f)};
    t.toolBar.checked = withAlpha(p.accent, dark ? 0.30f : 0.20f);
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
    t.menu.cornerRadius = t.cornerRadius - 1;
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
    t.card.cornerRadius = t.cornerRadius + 2;

    t.titleBar.background = p.titleBar;
    t.titleBar.title = p.text;

    t.windowButton.background = {.normal = {0, 0, 0, 0}, .hovered = p.hover, .pressed = p.pressed};
    t.windowButton.closeBackground = {.normal = {0, 0, 0, 0}, .hovered = p.closeHover, .pressed = p.closeHover};
    t.windowButton.glyph = p.text;

    t.frame.background = p.window;
    return t;
}

} // namespace glw
