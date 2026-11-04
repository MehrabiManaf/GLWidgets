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

#include <chrono>
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

// The colors everything is made of, in layers as Fluent 2 (Windows 11) has them: the title bar the darkest, the window's
// background over it, panels (layers, the islands of docked panels) over that, controls on them.
struct Palette {
    Color window;           // the background of windows
    Color layer;            // a panel on it: a dock stack, a card
    Color surface;          // what stands on that: buttons, fields
    Color titleBar;
    Color text;
    Color mutedText;
    Color disabledText;
    Color accent;
    Color onAccent;         // text and marks on the accent
    Color border;           // a hairline: low contrast, so things are told apart by their layers more than by lines
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
    float cornerRadius = 4;
    Insets padding{12, 5, 12, 5};
    Color focusRing;
};

struct LineEditStyle {
    StateColor background;
    StateColor border;
    Color text;
    Color placeholder;
    Color selection;
    Color caret;
    Color focusLine;                // along its bottom while it has the focus
    float borderWidth = 1;
    float cornerRadius = 4;
    float focusLineWidth = 2;
    Insets padding{10, 5, 10, 5};
    float width = 220;              // what it asks for: fields are usually stretched
};

struct CheckBoxStyle {
    StateColor background;          // of the box, unchecked
    StateColor border;
    StateColor checkedBackground;
    Color check;                    // the mark
    float size = 16;
    float cornerRadius = 4;
    float spacing = 8;              // between the box and the text
    Color focusRing;
};

struct ScrollBarStyle {
    Color track;
    StateColor thumb;
    float width = 8;
    float minimumThumb = 24;        // the thumb's length, however long the content
};

// The rows of item views (ListView): a pill under the mouse; the one selected marked by a soft background and a short bar at
// its start, as Fluent's lists; the current one ringed while the view has the focus.
struct ItemViewStyle {
    StateColor row;                 // behind a row: under the mouse, pressed
    Color selected;                 // behind the one selected
    Color indicator;                // the bar at its start
    Color focusRing;
    Color expander;                 // a tree's arrow, by a node with children
    Color headerText;               // a table's column headers
    Color headerLine;               // under them, and between them
    float rowHeight = 28;
    float cornerRadius = 4;
    float inset = 3;                // rows drawn this much in from the view's sides
    float indent = 16;              // a tree's, for each level
    float columnWidth = 140;        // a table's, until it is set
};

struct SplitterStyle {
    Color handle;                   // the line between the panes
    Color active;                   // under the mouse, or dragged
    float thickness = 6;            // what can be dragged
};

// Docked panels: each stack an island (a layer with rounded corners) on the window, gaps between them; the tab shown
// marked by a short bar under its title; where a dragged tab would go.
struct DockStyle {
    Color background;               // of a stack: its tabs and the content of the panel shown
    Color border;                   // around a stack
    Color separator;                // between a stack's tabs and its content
    StateColor tab;                 // behind a tab: a pill, under the mouse
    Color activeLine;               // under the title of the tab shown
    Color text;
    Color mutedText;                // the titles of the tabs not shown
    Color dropZone;
    Color scrollThumb;              // the thin bar that scrolls the tabs, when they do not fit
    SplitterStyle splitter;         // between stacks: a gap, a line only under the mouse
    float cornerRadius = 8;
    float tabHeight = 28;
    float tabMinimumWidth = 56;     // a tab is as wide as its title, between these (a longer title is cut with …)
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
    float cornerRadius = 8;
};

struct MenuBarStyle {
    Color background;               // in a row of its own (in the title bar, the title bar's)
    Color text;
    StateColor title;               // behind a menu's title: under the mouse, open
    float height = 32;              // in a row of its own
    float padding = 10;             // around a title's text
    float cornerRadius = 4;
};

struct ToolBarStyle {
    Color background;
    Color separator;
    StateColor button;              // a button's background: under the mouse, pressed
    Color checked;                  // ... a checked one's
    Color icon;                     // a monochrome icon's color
    Color disabledIcon;
    float buttonSize = 32;          // its buttons are square
    float iconSize = 16;
    float padding = 4;              // around its buttons
    float spacing = 2;              // between them
    float cornerRadius = 4;
};

struct MenuStyle {
    Color background;
    Color border;
    Color highlight;                // the item under the mouse, or chosen by the keys
    Color text;
    Color shortcut;
    Color disabledText;
    Color mark;                     // a check mark, a submenu's arrow
    Color separator;
    float itemHeight = 28;
    float padding = 4;              // around the items
    float cornerRadius = 4;         // of the highlight
    float minimumWidth = 160;
};

// A multi-line text and code (TextEdit): its text, its gutter, and the colors of the kinds of tokens a highlighter finds.
struct TextEditStyle {
    Color background;
    Color text;
    Color gutter;                   // behind the line numbers
    Color lineNumber;
    Color currentLineNumber;
    Color currentLine;              // behind the line the caret is on
    Color selection;
    Color caret;
    // tokens
    Color keyword;
    Color type;
    Color string;
    Color number;
    Color comment;
    Color preprocessor;
    Color function;
    float fontSize = 13;
    float tabWidth = 4;             // in spaces
};

// How long things take to change (Fluent's durations): a hover or a press, what opens, what moves far.
struct MotionStyle {
    std::chrono::milliseconds fast{83};
    std::chrono::milliseconds normal{167};
    std::chrono::milliseconds slow{250};
};

struct ToolTipStyle {
    Color background;
    Color border;
    Color text;
    float maximumWidth = 320;               // its text wraps past it
    std::chrono::milliseconds delay{500};   // the mouse still over a widget before its tooltip shows
    // ... when another tooltip closed a moment ago: going from one to the next is quick
    std::chrono::milliseconds reshowDelay{80};
};

struct DialogStyle {
    Color background;
    Color border;
    Color scrim;                    // over the window it opened from
    float padding = 20;
    float spacing = 16;             // between its title, its content and its buttons
    float titleSize = 16;
    float maximumWidth = 460;       // its text wraps past it
};

struct CanvasStyle {
    Color background;
    Color grid;                     // its lines, every gridSpacing dips of the canvas
    Color majorGrid;                // every majorEvery of them
    float gridSpacing = 16;
    int majorEvery = 8;
};

struct TitleBarStyle {
    Color background;
    Color title;
    float height = 38;
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
    float contentInset = 4;         // around its content: the window's background, between it and the edges
    // The window buttons at the start of the title bar (the system's convention on macOS) or at its end (elsewhere).
#ifdef __APPLE__
    bool buttonsAtStart = true;
#else
    bool buttonsAtStart = false;
#endif
};

struct Theme {
    Palette palette;
    float fontSize = 13;
    float cornerRadius = 4;
    ButtonStyle button;
    LineEditStyle lineEdit;
    CheckBoxStyle checkBox;
    ScrollBarStyle scrollBar;
    ItemViewStyle itemView;
    SplitterStyle splitter;
    DockStyle dock;
    MajorTabStyle majorTab;
    CardStyle card;
    CanvasStyle canvas;
    MenuStyle menu;
    MenuBarStyle menuBar;
    ToolBarStyle toolBar;
    TextEditStyle textEdit;
    ToolTipStyle toolTip;
    MotionStyle motion;
    DialogStyle dialog;
    TitleBarStyle titleBar;
    WindowButtonStyle windowButton;
    WindowFrameStyle frame;

    // The styles of every widget made from `palette`.
    GLW_EXPORT static Theme fromPalette(const Palette& palette);
    static Theme dark() { return fromPalette(Palette::dark()); }
    static Theme light() { return fromPalette(Palette::light()); }
};

} // namespace glw
