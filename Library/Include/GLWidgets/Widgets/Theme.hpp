// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How widgets look: a palette, and a style for each kind of widget made from it. A style is a plain struct of colors and
// measures; colors that change with the widget's state (hovered, pressed, focused, disabled) are StateColors. A widget
// takes its style from the theme of its root, unless it was given one of its own (setStyle, its Props' style).
//
// Styles are typed structs on purpose: the compiler checks them, a widget reads them without lookups, and they are
// aggregates C++26 reflection will load from files. Selectors in the manner of style sheets can come on top of them.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <optional>

namespace glw {

// The state of an interactive widget, for its StateColors.
struct VisualState {
    bool hovered = false;
    bool pressed = false;           // pressed while the mouse is over it
    bool focused = false;
    bool disabled = false;
};

// A color for each state; a state without one shows the normal color. When several states hold, disabled wins, then
// pressed, hovered and focused.
struct StateColor {
    Color normal;
    std::optional<Color> hovered;
    std::optional<Color> pressed;
    std::optional<Color> focused;
    std::optional<Color> disabled;

    Color resolve(const VisualState& state) const {
        if (state.disabled && disabled)
            return *disabled;
        if (state.pressed && pressed)
            return *pressed;
        if (state.hovered && hovered)
            return *hovered;
        if (state.focused && focused)
            return *focused;
        return normal;
    }
};

// The colors everything is made of.
struct Palette {
    Color window;           // the background of windows
    Color surface;          // what stands on it: buttons, fields, cards
    Color titleBar;
    Color text;
    Color mutedText;
    Color disabledText;
    Color accent;
    Color border;
    Color hover;            // laid over a surface the mouse is on
    Color pressed;
    Color selection;        // behind selected text
    Color closeHover;       // the close button of a window, under the mouse

    GLW_EXPORT static Palette dark();
    GLW_EXPORT static Palette light();
};

struct ButtonStyle {
    StateColor background;
    StateColor border;
    StateColor text;
    float borderWidth = 1;
    float cornerRadius = 6;
    Insets padding{14, 6, 14, 6};
    Color focusRing;
};

struct LineEditStyle {
    StateColor background;
    StateColor border;
    Color text;
    Color placeholder;
    Color selection;
    Color caret;
    float borderWidth = 1;
    float cornerRadius = 6;
    Insets padding{10, 6, 10, 6};
    float width = 220;              // what it asks for: fields are usually stretched
};

struct CheckBoxStyle {
    StateColor background;          // of the box, unchecked
    StateColor border;
    StateColor checkedBackground;
    Color check;                    // the mark
    float size = 18;
    float cornerRadius = 4;
    float spacing = 8;              // between the box and the text
    Color focusRing;
};

struct ScrollBarStyle {
    Color track;
    StateColor thumb;
    float width = 10;
    float minimumThumb = 24;        // the thumb's length, however long the content
};

struct SplitterStyle {
    Color handle;                   // the line between the panes
    Color active;                   // under the mouse, or dragged
    float thickness = 6;            // what can be dragged
};

// Docked panels: the tabs of a stack, and where a dragged tab would go.
struct DockStyle {
    Color background;               // behind the panels' content
    Color tabBar;                   // behind a stack's tabs
    StateColor tab;                 // a tab not shown
    Color activeTab;                // the tab shown
    Color activeLine;               // along the top of the tab shown
    Color text;
    Color mutedText;                // the titles of the tabs not shown
    Color dropZone;
    Color scrollThumb;              // the thin bar that scrolls the tabs, when they do not fit
    float tabHeight = 30;
    float tabMinimumWidth = 64;     // a tab is as wide as its title, between these (a longer title is cut with …)
    float tabMaximumWidth = 200;
};

// The major tabs in a window's title bar, as a browser's.
struct MajorTabStyle {
    StateColor background;          // a tab not shown
    Color active;                   // the tab shown: the color of what it shows, under it
    Color text;
    Color mutedText;
    Color insertion;                // where a dragged tab would go
    float minimumWidth = 90;
    float maximumWidth = 220;
    float cornerRadius = 8;
    float topGap = 6;               // above the tabs: the caption, to drag the window by
};

struct CardStyle {
    Color background;
    Color border;
    float borderWidth = 1;
    float cornerRadius = 6;
};

struct TitleBarStyle {
    Color background;
    Color title;
    float height = 40;
};

struct WindowButtonStyle {
    StateColor background;
    StateColor closeBackground;     // the close button's
    Color glyph;
    float width = 46;
};

struct WindowFrameStyle {
    Color background;
    float resizeBorder = 6;         // dips along the edges that resize the window
    // The window buttons at the start of the title bar (the system's convention on macOS) or at its end (elsewhere).
#ifdef __APPLE__
    bool buttonsAtStart = true;
#else
    bool buttonsAtStart = false;
#endif
};

struct Theme {
    Palette palette;
    float fontSize = 14;
    float cornerRadius = 6;
    ButtonStyle button;
    LineEditStyle lineEdit;
    CheckBoxStyle checkBox;
    ScrollBarStyle scrollBar;
    SplitterStyle splitter;
    DockStyle dock;
    MajorTabStyle majorTab;
    CardStyle card;
    TitleBarStyle titleBar;
    WindowButtonStyle windowButton;
    WindowFrameStyle frame;

    // The styles of every widget made from `palette`.
    GLW_EXPORT static Theme fromPalette(const Palette& palette);
    static Theme dark() { return fromPalette(Palette::dark()); }
    static Theme light() { return fromPalette(Palette::light()); }
};

} // namespace glw
