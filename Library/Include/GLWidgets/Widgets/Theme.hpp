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

// The colors everything is made of, in layers: the title bar the darkest, the window's background over it, panels (layers,
// the islands of docked panels) over that, raised controls on them; fields sunk into them.
struct Palette {
    Color window;           // the background of windows
    Color layer;            // a panel on it: a dock stack, a card
    Color surface;          // what stands on that: buttons
    Color base;             // what is sunk into it: fields, the box of a check box
    Color titleBar;
    Color text;
    Color mutedText;
    Color disabledText;
    Color accent;
    Color onAccent;         // text and marks on the accent
    Color border;           // a hairline: low contrast, so things are told apart by their layers more than by lines
    Color outline;          // the edge of a control: darker than what it is on, so it stands out of it
    Color hover;            // laid over a surface the mouse is on
    Color pressed;
    Color selection;        // behind selected text
    Color closeHover;       // the close button of a window, under the mouse

    GLW_EXPORT static Palette dark();
    GLW_EXPORT static Palette light();
};

// How a control stands out of what it is on, lit from above. Raised (a button): its background lighter at its top, a light
// line inside along its top that fades down its sides, a soft shadow under it; pressed, it sinks. Sunken (a field): a
// shadow inside along its top, a light line along its bottom edge. paintRaised and paintSunken (Widgets/Bevel.hpp) draw it.
struct Bevel {
    float gradient = 0;             // the background mixed toward white by this at its top, toward black by half of it at its bottom
    Color highlight;                // raised: the line inside, at the top; sunken: the line under its bottom edge
    Color shadow;                   // raised: under it; sunken: inside, along its top
};

struct ButtonStyle {
    StateColor background;
    StateColor border;              // focused: the accent
    StateColor text;
    Bevel bevel;
    float borderWidth = 1;
    float cornerRadius = 0;
    Insets padding{12, 5, 12, 5};
    Color focusRing;                // a glow around it while it has the focus
};

struct LineEditStyle {
    StateColor background;
    StateColor border;
    Color text;
    Color placeholder;
    Color selection;
    Color caret;
    Color focusLine;                // along its bottom while it has the focus (if focusLineWidth)
    Color focusRing;                // a glow around it while it has the focus
    Bevel bevel;                    // sunken
    float borderWidth = 1;
    float cornerRadius = 0;
    float focusLineWidth = 0;
    Insets padding{10, 5, 10, 5};
    float width = 220;              // what it asks for: fields are usually stretched
};

struct CheckBoxStyle {
    StateColor background;          // of the box, unchecked
    StateColor border;
    StateColor checkedBackground;
    Color checkedBorder;
    Color check;                    // the mark
    Bevel bevel;                    // unchecked: sunken
    Bevel checkedBevel;             // checked: raised
    float size = 16;
    float cornerRadius = 0;
    float spacing = 8;              // between the box and the text
    Color focusRing;                // a glow around the box while it has the focus
};

struct ScrollBarStyle {
    Color track;
    StateColor thumb;
    float width = 8;
    float minimumThumb = 24;        // the thumb's length, however long the content
    float cornerRadius = 0;         // of the thumb (at most half as much as it is thick: a pill)
};

// The rows of item views (ListView): a box under the mouse, the one selected a box of the accent's tint; the current one,
// when it is not the one selected, ringed while the view has the focus. With the cells' borders (rowLines, columnLines),
// a line between the rows and between a table's columns, and the rows' backgrounds fill their cells.
struct ItemViewStyle {
    StateColor row;                 // behind a row: under the mouse, pressed
    Color selected;                 // behind the one selected
    Color indicator;                // a table header's edge, under the mouse or dragged
    Color focusRing;
    Color expander;                 // a tree's arrow, by a node with children
    Color headerText;               // a table's column headers
    Color headerLine;               // under them, and between them
    float rowHeight = 28;
    float cornerRadius = 0;
    float inset = 3;                // rows drawn this much in from the view's sides
    float indent = 16;              // a tree's, for each level
    float columnWidth = 140;        // a table's, until it is set
    // the cells' borders
    bool rowLines = false;          // a line under each row: lists, trees, tables
    bool columnLines = false;       // a line between a table's columns, its header's too
    Color gridLine;
    float gridLineWidth = 1;
};

struct SplitterStyle {
    Color handle;                   // the line between the panes
    Color active;                   // under the mouse, or dragged
    float thickness = 6;            // what can be dragged
};

// Docked panels: each stack an island (a layer with rounded corners) on the window, gaps between them; the tab shown
// raised and joined to its content as Fusion's; where a dragged tab would go.
struct DockStyle {
    Color background;               // of a stack: its tabs and the content of the panel shown
    Color border;                   // around a stack
    Color separator;                // between a stack's tabs and its content
    StateColor tab;                 // behind a tab, under the mouse
    Color selectedTab;              // the tab shown: its top (it fades to the background at its bottom)
    Color selectedTabBorder;
    Color selectedTabHighlight;     // the light line inside along its top
    Color activeLine;               // the guides and the outline of where a dragged tab would go
    Color text;
    Color mutedText;                // the titles of the tabs not shown
    Color dropZone;
    Color scrollThumb;              // the thin bar that scrolls the tabs, when they do not fit
    SplitterStyle splitter;         // between stacks: a gap, a line only under the mouse
    float cornerRadius = 0;
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
    float cornerRadius = 0;
    float topGap = 6;               // above the tabs: the caption, to drag the window by
};

struct CardStyle {
    Color background;
    Color border;
    float borderWidth = 1;
    float cornerRadius = 0;
};

// A header over content that opens and closes (CollapsiblePanel), as Fluent's expander: a card.
struct CollapsiblePanelStyle {
    StateColor header;              // under the mouse, pressed
    Bevel bevel;                    // of the header
    Color content;                  // behind the content, under the header
    Color border;
    Color title;
    Color arrow;
    Color focusRing;
    float headerHeight = 36;
    Insets padding{14, 8, 14, 12};  // around the content
    float spacing = 8;              // between the content's widgets
    float cornerRadius = 0;
};

struct MenuBarStyle {
    Color background;               // in a row of its own (in the title bar, the title bar's)
    Color text;
    StateColor title;               // behind a menu's title: under the mouse, open
    float height = 32;              // in a row of its own
    float padding = 10;             // around a title's text
    float cornerRadius = 0;
};

struct ToolBarStyle {
    Color background;
    Color separator;
    StateColor button;              // a button's background: under the mouse, pressed
    Color checked;                  // ... a checked one's
    Color outline;                  // around a button under the mouse, pressed or checked
    Bevel bevel;                    // ... raised (pressed, sunk)
    Color icon;                     // a monochrome icon's color
    Color disabledIcon;
    float buttonSize = 32;          // its buttons are square
    float iconSize = 16;
    float padding = 4;              // around its buttons
    float spacing = 2;              // between them
    float cornerRadius = 0;
};

struct MenuStyle {
    Color background;
    Color border;
    Color highlight;                // the item under the mouse, or chosen by the keys
    Color highlightText;            // ... its text and marks
    Color text;
    Color shortcut;
    Color disabledText;
    Color mark;                     // a check mark, a submenu's arrow
    Color separator;
    float itemHeight = 28;
    float padding = 4;              // around the items
    float cornerRadius = 0;         // of the highlight
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
    std::chrono::milliseconds fast{100};
    std::chrono::milliseconds normal{200};
    std::chrono::milliseconds slow{300};
    std::chrono::milliseconds scroll{180};      // a turn of the wheel, a page: eased to where it goes (a faster wheel
                                                // goes further: SmoothScroll::boosted)
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
    float cornerRadius = 0;
    ButtonStyle button;
    LineEditStyle lineEdit;
    CheckBoxStyle checkBox;
    ScrollBarStyle scrollBar;
    ItemViewStyle itemView;         // lists and trees
    ItemViewStyle table;            // tables: their cells' borders shown
    SplitterStyle splitter;
    DockStyle dock;
    MajorTabStyle majorTab;
    CardStyle card;
    CanvasStyle canvas;
    MenuStyle menu;
    CollapsiblePanelStyle collapsiblePanel;
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
