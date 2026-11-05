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
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

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

// The looks a theme is made in, each in the dark and in the light (Theme::dark, Theme::light; Application::setStyle):
//   Studio  (the default) as a tool's (Qt's Fusion, made finer): square, controls raised and lit from above, fields sunk; quick motions.
//   Fluid   modern: rounded, flat with soft shadows; more motion and longer: a press ripples out of where it was, the
//           selection slides from row to row, popups come in from further.
//   Plain   simple: square and flat, no shadows, no glows, and nothing animated (every motion at once).
//   Plastic real: its controls reliefs lit by a light (the theme's lighting): buttons rise out of the panel, rounding
//           down to it, fields are sunk into it, their rims shadowing their floors; pressed, a button goes down.
//           Glossy: sharp white highlights where they turn toward the light.
//   Metal   ... of metal: its highlights of its own color, its surfaces brushed; gunmetal in the dark, aluminum in the
//           light.
enum class ThemeStyle : std::uint8_t {
    Studio,
    Fluid,
    Plain,
    Plastic,
    Metal,
};

// The colors a theme is made in, in any of the looks (Theme::of; Application::setColorScheme):
//   Dark     the style's own dark colors: grays with a little blue in them, an amber accent.
//   Light    the style's own light ones: an azure accent.
//   Dusk     dark too, of other colors: a plum gray, as the sky after the sun has set, a violet accent.
//   Sand     light too, of other colors: warm, as paper and sand are, a terracotta accent.
//   Turquoise  dark, of the colors of glazed tiles: a deep, blue turquoise, its hairlines and the mouse's light lapis
//              lazuli, an accent of saffron gold, its text ivory.
// The code in a text edit is in the scheme's colors too.
enum class ColorScheme : std::uint8_t {
    Dark,
    Light,
    Dusk,
    Sand,
    Turquoise,
};

// A style, a scheme by its name, in lower case ("studio", "fluid", "plain", "plastic", "metal"; "dark", "light", "dusk",
// "sand", "turquoise"), as a command line or a settings file says it; none for another name.
GLW_EXPORT std::optional<ThemeStyle> themeStyleNamed(std::string_view name);
GLW_EXPORT std::optional<ColorScheme> colorSchemeNamed(std::string_view name);

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

    GLW_EXPORT static Palette dark(ThemeStyle style = ThemeStyle::Studio);
    GLW_EXPORT static Palette light(ThemeStyle style = ThemeStyle::Studio);
    // A scheme's, in a style: Dark's and Light's the style's own (dark, light); the others the same in every style, but
    // Metal's controls a little lighter (or darker), as metal stands out of what it is on.
    GLW_EXPORT static Palette of(ColorScheme scheme, ThemeStyle style = ThemeStyle::Studio);
};

// How a control stands out of what it is on, lit from above. Raised (a button): its background lighter at its top, a light
// line inside along its top that fades down its sides, a soft shadow under it; pressed, it sinks. Sunken (a field): a
// shadow inside along its top, a light line along its bottom edge. paintRaised and paintSunken (Widgets/Bevel.hpp) draw it.
struct Bevel {
    float gradient = 0;             // the background mixed toward white by this at its top, toward black by half of it at its bottom
    Color highlight{0, 0, 0, 0};    // raised: the line inside, at the top; sunken: the line under its bottom edge (none: none)
    Color shadow{0, 0, 0, 0};       // raised: under it; sunken: inside, along its top
    // A relief instead (Plastic, Metal): raised (sunken) this many dips, lit by the theme's light, its edge rounding over
    // `roundness` dips; the gradient and the highlight then left to the light. 0: painted as above.
    float relief = 0;
    float roundness = 4;
};

struct ButtonStyle {
    StateColor background;
    StateColor border;              // focused: the accent
    StateColor text;
    Bevel bevel;
    float borderWidth = 1;
    float cornerRadius = 0;
    Insets padding{12, 3, 12, 3};
    Color focusRing;                // a glow around it while it has the focus
    Color ripple{0, 0, 0, 0};                   // a press's: a circle out of where it was pressed, fading (transparent: none)
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
    Insets padding{10, 3, 10, 3};
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
    float rowHeight = 26;
    float cornerRadius = 0;
    float inset = 3;                // rows drawn this much in from the view's sides
    float indent = 16;              // a tree's, for each level
    float columnWidth = 140;        // a table's, until it is set
    // the cells' borders
    bool rowLines = false;          // a line under each row: lists, trees, tables
    bool columnLines = false;       // a line between a table's columns, its header's too
    Color gridLine;
    float gridLineWidth = 1;
    Color ripple{0, 0, 0, 0};                   // a press on a row: a circle out of where it was pressed (transparent: none)
    bool slideSelection = false;    // the selection slides from the row it was on to the one it goes to
    Color headerBackground{0, 0, 0, 0};         // behind a header's titles (transparent: none)
    Bevel headerBevel;              // ... raised as a control (Plastic, Metal)
    Bevel rowBevel;                 // the rows' highlights (under the mouse, pressed, selected) raised as controls (Plastic, Metal)
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
    // The tab shown: joined to its content under the bar (Studio's, Plain's; a relief of the content's color in Plastic
    // and Metal, its face the content's), or standing on the bar as a control does, raised by `tabBevel` (a pill in
    // Fluid); a tab under the mouse, the same.
    bool tabsJoined = true;
    Bevel tabBevel;
    float tabRadius = 4;            // a tab's corners (a standing one's: at most half its height, a pill)
    // A stack raised as a control (Plastic, Metal): under its tab bar, a slab of its background, the tab shown a relief
    // of it as high, joined to it as a folder's tab is (its face the slab's, no seam), the other tabs on the window.
    Bevel islandBevel;
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
    float topGap = 4;               // above the tabs: the caption, to drag the window by
    // The tab shown: joined to the window's content under it (as a browser's; a relief of its color in Plastic and Metal),
    // or standing on the strip as a control does, raised by `bevel` in `raised` (a pill in Fluid).
    bool joined = true;
    Bevel bevel;
    Color raised{0, 0, 0, 0};
};

struct CardStyle {
    Color background;
    Color border;
    float borderWidth = 1;
    float cornerRadius = 0;
    Bevel bevel;                    // raised as a control (a slab in Plastic and Metal, as a dock's stacks); none: flat
};

// A header over content that opens and closes (CollapsiblePanel), as Fluent's expander: a card.
struct CollapsiblePanelStyle {
    StateColor header;              // under the mouse, pressed, with the focus (which shows so: no ring)
    Bevel bevel;                    // of the header
    Color content;                  // behind the content, under the header
    Color border;                   // around all of it
    Color separator{0, 0, 0, 0};    // between the header and the content
    Color shadow{0, 0, 0, 0};       // a soft one under all of it (transparent: none)
    Color title;
    Color arrow;
    float headerHeight = 30;
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
    float buttonSize = 28;          // its buttons are square
    float iconSize = 16;
    float padding = 4;              // around its buttons
    float spacing = 2;              // between them
    float cornerRadius = 0;
    Color ripple{0, 0, 0, 0};                   // a press's (transparent: none)
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
    std::chrono::milliseconds ripple{450};      // a press's ripple, out and gone
    float popupSlide = 6;                       // dips a popup slides in from, out of its anchor
    // None of them: every motion at once (Plain).
    bool still() const { return fast.count() == 0 && normal.count() == 0 && slow.count() == 0 && scroll.count() == 0; }
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

// A node of a node editor (a graph on a Canvas): a body with a header of the node's own color (its kind's) over it, as the
// theme's controls are (raised, flat, a relief), a shadow under it, an edge around it.
struct NodeStyle {
    Color body;
    Color border;
    Color shadow{0, 0, 0, 0};       // under it (transparent: none)
    Color selection;                // around it, selected
    Bevel bevel;                    // its header's
    Bevel bodyBevel;                // its body's
    float cornerRadius = 8;
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
    float height = 32;              // as the system's caption (its buttons as high)
};

struct WindowButtonStyle {
    StateColor background;
    StateColor closeBackground;     // the close button's
    Color glyph;
    float width = 46;
};

struct WindowFrameStyle {
    Color background;
    float popupRadius = 0;          // popups' windows (not menus') rounded by this, where the system rounds windows
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
    ThemeStyle style = ThemeStyle::Studio;
    ColorScheme scheme = ColorScheme::Dark;     // what its palette is (one made otherwise: Dark or Light, as dark as it is)
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
    NodeStyle node;
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
    paint::Lighting lighting;       // what lights reliefs (Bevel::relief): the renderer's, as the theme changes

    // The styles of every widget made from `palette`, in `style`.
    GLW_EXPORT static Theme fromPalette(const Palette& palette, ThemeStyle style = ThemeStyle::Studio);
    // The style's own colors.
    static Theme dark(ThemeStyle style = ThemeStyle::Studio) { return fromPalette(Palette::dark(style), style); }
    static Theme light(ThemeStyle style = ThemeStyle::Studio) { return fromPalette(Palette::light(style), style); }
    // A scheme's, in a style: its palette's, its code in its own colors.
    GLW_EXPORT static Theme of(ColorScheme scheme, ThemeStyle style = ThemeStyle::Studio);
    // Between two themes, `t` of the way from `from` to `to` (an application changing theme, or style, eased): the
    // palettes' colors mixed, and the shapes (corners rounding, controls growing lower, bevels fading); what neither
    // mixes (what is on or off, the motions) is `to`'s.
    GLW_EXPORT static Theme blend(const Theme& from, const Theme& to, float t);
    // Whether its palette is the dark one's (a dark window).
    bool isDark() const { return palette.window.r + palette.window.g + palette.window.b < 1.5f; }

private:
    static void fluid(Theme& theme, bool dark);
    static void plain(Theme& theme);
    static void reliefs(Theme& theme, bool dark);       // Plastic's and Metal's shapes
    static void plastic(Theme& theme, bool dark);
    static void metal(Theme& theme, bool dark);
};

} // namespace glw
