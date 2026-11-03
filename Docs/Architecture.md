# GLWidgets architecture

GLWidgets is a C++ GUI framework whose widgets are drawn entirely with the GPU. It runs on Windows, Linux and macOS, it
depends only on permissively licensed libraries, and new widgets are made by deriving from the existing ones. Its first
real user is the second version of the [FlowGraph](https://github.com/MehrabiManaf/FlowGraph) script editor, which must
not need Qt. After that it is meant for user interfaces inside 3D applications and games, and for charts and plots.

This document says how it is built and why. Parts marked *planned* are decided but not written yet; the
[milestones](#milestones) say what exists.

## Requirements

- **Platforms:** Windows, Linux (X11 and Wayland), macOS.
- **Licenses:** GLWidgets is MIT; every dependency is permissive (zlib, MIT, BSD, Apache-2.0, FTL, OFL). No LGPL or GPL,
  even as an optional part.
- **Persian and other right-to-left languages:** bidirectional text, shaping, right-to-left layouts.
- **Localization:** every text a user sees is translatable, and the language can change while the application runs.
- **Docking of documents and panels:** major tabs dock only in the tab strip at the top of a window, as in a web
  browser, so whole windows merge and split. Panel tabs dock, split and float inside a major tab, as in Unreal Engine's
  Slate.
- **Custom window chrome:** windows have no title bar or borders of the system. GLWidgets draws them, so the application
  looks and behaves the same everywhere.
- **Embedding:** the user interface can be drawn over a 3D scene, into a texture placed in a 3D world, or inside an engine
  that owns the window and the GPU device.
- **Charts and plots:** drawing many thousands of points, lines and markers interactively, and custom GPU drawing for more.
- **C++20 now, C++26 later:** the design leaves room for reflection, contracts and `std::execution` to replace what is
  written by hand today, without changing the API users see.

Out of scope: native widgets of the system (everything is drawn by GLWidgets), mobile platforms, a UI designer tool.

## Layers

```
Application
   │
Widgets        Widget tree, layout, events, focus, window chrome, Application         Library/Include/GLWidgets/Widgets
   │
Text           Fonts, shaping, bidi, line breaking, glyph atlas, carets               Library/Include/GLWidgets/Text
   │
Paint          Painter → DrawList (portable)    Renderer: DrawList → Rhi              Library/Include/GLWidgets/Paint
   │
Rhi            GPU: devices, surfaces, buffers, textures, shaders, pipelines          Library/Include/GLWidgets/Rhi
   │
Platform       Windows, events, input, hit testing, clipboard, IME, DPI               Library/Include/GLWidgets/Platform
   │
Core           Geometry, color, errors, contract checks                               Library/Include/GLWidgets/Core
```

A layer uses only the layers below it. Three rules matter most:

1. **Widgets never touch the RHI.** A widget paints with a `Painter`, which records a `DrawList`. A draw list holds only
   vertices, indices and commands that refer to textures by id. It is the boundary that makes embedding possible (an
   engine draws it with its own renderer), as well as batching, caching and other GPU backends.
2. **The widget layer does not need the Platform.** A widget tree takes events (plain structs) and gives draw lists. With
   the Platform, GLWidgets makes the windows; without it, an engine feeds the events and draws the lists.
3. **SDL3 stays behind the Platform and Rhi interfaces.** No SDL type is in a public header. Where SDL cannot do
   something, the system's own API is added next to it, inside the implementation (SDL exposes the native handles:
   `HWND`, `NSWindow`, `wl_surface`...).

All of it is one library, `GLWidgets` (shared by default, `GLW_SHARED`), with the public headers in
`Library/Include/GLWidgets/<Layer>/` and the implementation in `Library/Source/<Layer>/`.

## Platform

`glw::platform::Platform` makes windows and reports events; `createPlatform()` returns the SDL3 implementation
(`Source/Platform/Sdl`).

**Units.** Everything is in device independent units (*dips*). A window's `scale()` is its pixels per dip (2 on a Retina
screen, 1.5 on Windows at 150 %). SDL's own window coordinates are pixels on Windows and X11 but points on macOS and
Wayland. The SDL platform converts every position and size, so users of `Platform` never see the difference. What the GPU
draws into is in pixels (`PixelSize`).

**Events.** One plain struct per kind of event, `Event` is a `std::variant` of them. Each carries the `WindowId` of its
window, so one event loop serves every window of a docking layout. Events are plain aggregates on purpose: an engine can
make them from its own input, and reflection will be able to log, record and replay them.

**Threading.** One platform per process, used on the main thread (macOS requires it), as everything of the user
interface. `wakeUp()` is the one call allowed from other threads.

**Quitting.** Closing the last window does not quit (`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE` is off). With several windows
that is the application's decision. `QuitRequested` comes from the system: Command+Q, the session ending.

**Keyboard.** `KeyChanged` is for shortcuts and navigation. Its letters are those of the US layout whatever the active
layout, so Ctrl+C is `Key::C` on a Persian keyboard too (SDL's "latin_letters" key code option). Typed text comes only from
`TextInput`, and the composition of an input method comes from `TextComposition`.

### Window chrome

A window made with `WindowDesc::borderless` has no title bar or borders of the system: its whole area is drawn by
GLWidgets. The application tells the system what each point is with `Window::setHitTest`: the caption (drag to move,
double-click to maximize), one of the eight resize borders, or client area (everything else, the window buttons included).
The system then moves, resizes and snaps the window itself: `WM_NCHITTEST` on Windows (Aero Snap works),
`_NET_WM_MOVERESIZE` on X11, `xdg_toplevel.move/resize` on Wayland, where drawing the decorations in the client is the
norm anyway.

What stays native, behind the interface:

| What | Where | Plan |
|---|---|---|
| Window shadow and rounded corners | Windows 11 | DWM attributes on the borderless window (native code, M3) |
| The snap layouts flyout on the maximize button | Windows 11 | Answer `HTMAXBUTTON` for our maximize button (native code, M3) |
| Shadow | macOS | Given by the system |
| Shadow | X11, Wayland | None, or drawn by GLWidgets in a transparent margin (decided in M3) |

The window buttons look the same everywhere; on which side they are is a style setting, by default the system's
convention (left on macOS, right elsewhere).

### Known gaps

| Gap | Where | Plan |
|---|---|---|
| Live resize: the event loop is blocked while the user drags a window edge | Windows, macOS | Done: `Platform::setExposeHandler` (SDL's event watch on `SDL_EVENT_WINDOW_EXPOSED`) draws from inside the system's loop |
| No z-order of windows | All | Track the activation order of our own windows |
| Global menu bar | macOS | Native `NSMenu` code |
| Accessibility | All | Platform accessibility APIs, possibly through AccessKit (MIT/Apache-2.0) |
| Window positions and global mouse position | Wayland | See [Docking](#docking) |

## RHI

`glw::rhi::Device` is the GPU. The implementation is SDL3's GPU API (`Source/Rhi/SdlGpu`), which picks Vulkan
(Windows, Linux), Direct3D 12 (Windows 10 and later) or Metal (macOS 10.14 and later) at run time. OpenGL or Direct3D 11
would be further implementations of the same interface, written only if a target machine needs them.

The RHI is public. Applications use it for what widgets do not draw: a 3D scene under the user interface, a chart with
millions of points, a texture shown in a viewport widget. It has buffers, textures (sampled and render targets),
samplers, shaders, pipelines and command lists. A command list records uploads outside passes, and draws inside a pass
into a window's surface or into a texture.

- **One device for every window.** Each window gets a `Surface` (a swap chain) from the same device, so textures, the
  glyph atlas and pipelines are shared. A panel that moves to another window uploads nothing again.
- **A frame:** `beginCommands()`, uploads, then for each target a pass (`beginSurfacePass` or `beginTexturePass`, draws,
  `endPass`), then `submit()`, which presents. Windows are drawn only when they need it; between frames the application
  sleeps in `waitForEvents`.
- **Resources** may be destroyed right after the frame that used them is submitted: the GPU API keeps them until the GPU
  is done with them.
- **Shaders** are written once in HLSL (`Library/Shaders`) and compiled ahead of time by `Tools/CompileShaders.py` into
  DXBC (Direct3D 12, with `fxc` from the Windows SDK), SPIR-V (Vulkan, with DXC) and MSL (Metal, SPIR-V through
  SPIRV-Cross). The results are committed as headers, so building GLWidgets needs no shader compiler. To change a shader
  you need the Windows SDK and the Vulkan SDK, which has DXC with SPIR-V and SPIRV-Cross; the script finds both by itself.
  A shader names its resources as SDL's GPU API requires (see the comment at the top of `Library/Shaders/Painter.hlsl`).
  On Windows, setting `SDL_GPU_DRIVER=vulkan` runs the Vulkan path instead of Direct3D 12, to try both on one machine.
- **Colors** are sRGB, written as they are into non-sRGB swap chains. Color management and HDR are deliberately left for
  later.

SDL's GPU API cannot adopt a device that an engine already made. That is why embedding goes through draw lists (see
[Embedding](#embedding)) and not through a shared device.

## Paint

`glw::paint::Painter` records into a `DrawList`. Everything is in dips; a transform (uniform scale and translation, enough
for zooming and panning a canvas) and a stack of clip rectangles apply to what is drawn. It has:

- **Boxes:** rectangles, rounded rectangles, circles, their borders, and blurred shadows. They are drawn as a signed
  distance field in the fragment shader, so their edges are anti-aliased at any scale and a box is four vertices whatever
  its corner radius or blur.
- **Lines and polygons:** polylines of any width with mitered joins, and convex polygons, tessellated on the CPU with a
  one-pixel anti-aliasing fringe. This is what charts and node connections are made of.
- **Images:** a texture (by id) on a rectangle, tinted.
- **Masks:** the red channel of a texture as coverage, in a color. Glyphs are masks of the glyph atlas (see [Text](#text)).

**The vertex format and one shader are the whole contract of a draw list.** A vertex has a position, texture coordinates,
the parameters of a box (half size, corner radius, border width, blur), its kind (textured, box, mask) and an RGBA8
color. A command is a range of indices, a clip rectangle and a texture id. One pipeline with one shader pair draws all of it
(`Library/Shaders/Painter.hlsl`), with premultiplied alpha blending. An engine that draws GLWidgets' lists itself needs
exactly that shader and one pipeline.

`glw::paint::Renderer` draws draw lists with the RHI: it uploads a list's geometry into the buffers of its target, then
draws it inside the target's pass, one draw call per command. Texture ids are mapped to RHI textures by the renderer; 0 is
white.

**For charts:** points are mapped from data to dips by the chart, in double precision, before they reach the painter.
Tens of thousands of points per frame are fine for the tessellated polylines. Beyond that, a draw list will hold *custom
commands* *(planned, M6)*: a callback that draws with the RHI directly (instanced markers, a line shader over a storage
buffer of points) in the same pass, between the commands around it. Custom commands exist only when GLWidgets renders the
list itself.

## Embedding *(planned, M6)*

Three ways to use GLWidgets in a 3D application, from the most integrated to the least:

| Mode | Who owns the window and the device | How the UI is drawn |
|---|---|---|
| **Standalone** | GLWidgets (`Platform`, `rhi::Device`) | The application draws its scene with the RHI into a texture or in the window's pass, and the UI goes on top. A viewport widget shows a texture rendered by the application. |
| **Engine** | The engine | The engine gives GLWidgets its input as `Event` structs and gets draw lists back. It draws them with its own renderer, using GLWidgets' shader and vertex format, the way Dear ImGui is integrated with its backends. |
| **In the world** | Either | A widget tree drawn into a texture (`beginTexturePass`, or the engine's own render target), which the scene puts on a surface. Input is the scene's ray hit turned into positions in the texture. |

What this asks of the widget layer: a UI root that is not tied to a platform window (it has a size, a scale, receives
events, produces a draw list), and textures referred to by id everywhere.

## Text

The editor needs hit testing, cursor movement and selection in text that mixes Persian, English and code, so GLWidgets
has its own text stack instead of SDL_ttf (`glw::text`, `Library/Include/GLWidgets/Text`):

| Step | Library | License |
|---|---|---|
| Bidirectional algorithm (UAX #9), script runs | SheenBidi | Apache-2.0 |
| Line breaking (UAX #14), graphemes (UAX #29) | libunibreak | zlib |
| Shaping | HarfBuzz | MIT |
| Rasterization | FreeType | FTL (BSD-like, credit in the documentation: [ThirdPartyNotices.md](ThirdPartyNotices.md)) |
| Default font, Persian and Latin | Vazirmatn, compiled into the library | OFL |
| Font discovery and fallback *(planned)* | DirectWrite / CoreText / fontconfig | system / MIT |

**`FontLibrary` and `Font`.** A library owns its fonts, loaded from memory or files. It starts with the default font.

**`TextLayout`** lays out a text with a `TextStyle` (fonts in fallback order, size, line spacing) and `TextOptions`
(maximum width, direction, alignment, language). All offsets are UTF-8 byte offsets, the same for every library involved.
The steps:

1. **Paragraphs** at Unicode's paragraph separators. Each has its direction, given or from its first strong character.
2. **Items:** runs of one bidi level, one script (Common characters such as spaces and digits take the script around
   them) and one font (the first of the style that has the character; spaces and marks stay in the font around them).
3. **Shaping** of each item by HarfBuzz, with the whole text as context, so letters join across item boundaries.
4. **Lines:** greedy, at the break opportunities of UAX #14. Spaces may hang past the end. A word longer than the line is
   broken between graphemes.
5. **Visual order** of each line from SheenBidi's line runs; the glyphs of each run placed left to right.
6. **Alignment** in the maximum width, or in the widest line when there is none. Start and End follow each paragraph's
   direction.

A laid-out line keeps its clusters as boxes in visual order. A box is a range of characters and the span of x they cover. A
cluster of several graphemes (a ligature such as لا) is shared evenly between them. `caret`, `hitTest`, `selectionRects`
and `nextOffset` / `previousOffset` (by grapheme) work from them.

**`GlyphAtlas`** rasterizes glyphs on demand into one single-channel texture (2048 × 2048 by default). It is shared by all
text of all windows, and keyed by font, glyph, size in quarter pixels and horizontal position in quarter pixels. Placing
glyphs to a quarter of a pixel matters for Persian: rounded to whole pixels, joined letters can show gaps. Hinting is light
(vertical only), so the shaper's horizontal positions stand. When the atlas is full it starts again empty, and `upload` says
so: what was painted that frame is painted again. The atlas is in memory; an engine takes its pixels and dirty rectangle
itself.

**`drawText`** draws a layout through a painter: each glyph as a mask quad, rasterized at the size it has on the target's
pixels (scale and zoom included). Text on the zoomable node canvas therefore stays sharp, at the cost of more atlas entries
while zooming. Whether MSDF glyphs are needed for the canvas is measured in M5.

Still to come: system fonts and fallback through the system, visual cursor movement (arrow keys across direction
changes), the composition of input methods in text fields, and tab stops.

## Widgets

The widget layer is the face of the library, so it lives in `glw` itself (`glw::Widget`, `glw::Button`,
`glw::Application`), with its headers in `Library/Include/GLWidgets/Widgets`.

- **A tree of widgets.** A parent owns its children (`std::unique_ptr`); everything else holds non-owning pointers. New
  widget types derive from `Widget` or one of its subclasses and override what they need: `measureContent`,
  `arrangeContent`, `paint` / `paintOver`, the event handlers. A widget removed from a shown tree is first forgotten by its
  root (focus, mouse capture, hover), so no pointer into it stays.
- **Layout in two passes:** `measure` (the size wanted for the space offered: padding included, margin not, clamped to
  the minimum and maximum sizes), then `arrange` (the rectangle given, in the parent's coordinates). A `Layout` object
  places a widget's children (`StackLayout`: along an axis, sharing what is left by stretch). Without one, every child gets
  the whole content. When a stack is too short for what its children want, they shrink as in CSS flexbox: those that
  stretch give up length first, then the others, never below their minimum sizes. That is what lets a scroll area in a
  stack take the room that is left, and scroll.
- **Directions:** margin and padding have start and end sides, and alignments have Start and End. A widget's layout
  direction is its parent's unless set, up to the root. Setting it once at the root mirrors the whole interface: stacks,
  alignments, labels, and the window buttons. A label's text keeps its own direction (bidi). Its Start and End alignment
  follow the layout direction.
- **`UiRoot`** shows a tree. It lays it out, records it into a draw list, and takes the events of its window as the plain
  structs of `Platform/Event.hpp`. It knows no platform window and no GPU, which is the embedding requirement. A `UiHost`
  (the window: its state, its buttons' actions, text input) is all it asks of what holds it.
- **Events:** the widget under the mouse is the deepest one that is visible and can be hit; labels and decorations cannot
  (`setHitTestVisible`). An event goes through a preview phase from the root down, then back up from the target, until a
  handler marks it handled. A widget handling a mouse press gets every mouse event until the buttons are released. Hover is
  tracked on the whole path, so a button knows the mouse is over its label. Keys go to the focus. A press gives the focus
  to the nearest focusable widget, and Tab / Shift+Tab move it in tree order.
- **Retained, invalidated:** a widget marks the layout or the paint as needed (`invalidateLayout`, `invalidatePaint`), and
  a window is laid out and drawn only then. For now, a window that needs it is laid out and painted whole. Doing it per
  widget is an optimization for later.
- **Measured for the room it gets:** a parent may measure a child without bounds along its own axis (a stack, a splitter),
  then give it less. So arranging a child measures it again for its real room, and a wrapped text takes the lines it
  needs there. A measure for the same room is kept until the layout is invalidated (`UiRoot::layoutGeneration`), so
  measuring twice costs little.
- **After the event:** a handler that removes or moves widgets, perhaps the one handling the event, does it through
  `UiRoot::defer`. The change runs once the event is done, and at once when no event is being dispatched.
- **Widgets can move between windows** (docking). `onRootChanged` tells a widget when it is shown by another root, or by
  none, so it can drop what depended on it (a label's text layout).
- **Window chrome is widgets:** `WindowFrame` holds a `TitleBar` (its own area drags the window; its center holds the
  title, and later the tab strip) and the window buttons. The frame's edges resize the window. The window's hit test asks
  the frame for its edges first, then the widget under the point for its `windowArea`.
- **`Application`** owns the platform, the device, the fonts and the glyph atlas, and runs the loop. It routes events to the
  windows by id, draws only windows that need it, and keeps a window dirty until a frame really reaches it. `Window::close`
  waits until the event being handled is done, so a button can close its own window. During live resize (Windows, macOS),
  the platform's expose handler draws the window from inside the system's loop, and waits for the GPU before it goes
  back to it, so that what is drawn has the window's new size.
- **What shows follows the input as closely as vsync allows.** The device keeps one frame in flight
  (`DeviceDesc::framesInFlight`; SDL's default is two), and the loop waits for the windows' next images
  (`Surface::waitUntilReady`) before it reads the input, not after: a frame is drawn from the newest input and is not
  queued behind another. Measured on Windows (Direct3D 12 and Vulkan, Release), a dragged node is one refresh behind the
  mouse (the system draws the cursor itself, at once: no windowed application with vsync does better), and a window
  resized by its edge is about a refresh behind where Notepad's is. A Debug build adds about a refresh.

### Declarative construction

Interfaces can be written as the tree they are (`Widgets/Declarative.hpp`, namespace `glw::ui`), in plain C++20: no macro,
no code generator.

```cpp
ui::VStack({.spacing = 16},
    ui::Label({.text = "GLWidgets", .fontSize = 26}),
    ui::HStack({.spacing = 10},
        ui::Button({.text = "Hello", .onClick = [this] { status->setText("Hello"); }}),
        ui::Button({.text = "Close", .onClick = [this] { window->close(); }})),
    ui::Label({.text = "Nothing clicked yet."}).bind(status))
    .padding(glw::Insets::all(28));
```

- **A builder per widget**, named after it, takes the widget's `Props`: a plain aggregate of its own settings, written
  with designated initializers. Containers (`VStack`, `HStack`, `Overlay`, `Panel`) take their children after it, as
  arguments or as a vector made in a loop. `ui::make<W>(...)` builds any widget, an application's own included.
- **What every widget has** (stretch, margin, padding, alignments, sizes, direction, enabled...) is set by calls chained
  after the builder. C++20 designated initializers cannot reach a base's members, so these settings cannot be part of each
  `Props`. `with(f)` gives the widget to a function for anything else.
- **`bind(pointer)`** gives back the widget made, as Slate's `SAssignNew` does. The pointer must live as long as the
  handlers that use it: it is a member of the class that builds the interface, and the handlers capture `this`.
- **A builder becomes a `std::unique_ptr`** to its widget wherever one is expected. Built trees and widgets made one by
  one mix freely; the builders only call the widgets' public API.

Why `Props` are plain aggregates: C++26 reflection can enumerate them. Interfaces loaded from files, serialization and an
inspector will then come from them without code written per widget (see [Metadata and C++26](#metadata-and-c26)). Still
to come, with the property system of M4: binding widgets to values that change (reactive state), and loading interfaces
from markup.

### Styles

How widgets look is in `Theme` (`Widgets/Theme.hpp`): a `Palette` (the colors everything is made of) and a style for each
kind of widget, made from the palette by `Theme::fromPalette` (`Theme::dark()`, `Theme::light()`).

The look is Fluent 2's (Windows 11), compact: neutral grays in layers (the title bar the darkest, the window's background,
panels over it, controls on them) told apart more by their layers than by lines, hairlines of low contrast, one accent
(with dark marks on it in the dark theme, as Windows 11 has), corners of 4 dips on controls and 8 on panels. Docked
stacks are islands on the window with gaps between them; the tab shown is marked by a short rounded bar under its title;
a field with the focus has a line of the accent along its bottom. Text is 13 dips, tabs 28 high.

- **A style is a typed struct** (`ButtonStyle`, `LineEditStyle`, `CardStyle`, `TitleBarStyle`...) of colors and measures.
  Colors that change with the widget's state are `StateColor`s: a normal color, and optional ones for hovered, pressed,
  focused and disabled, which win in that order (disabled first).
- **A widget takes its style from the theme of its root, unless it has its own** (`setStyle`, or `.style` in its Props).
  A widget given explicit colors (a `Panel`) keeps them; a `Card` or a muted `Label` takes the theme's, so they follow a
  change of theme.
- **The theme changes while the application runs** (`Application::setTheme`): every widget is told (`onStyleChanged`) and
  the windows are laid out again, since paddings and sizes are part of the styles.

Typed structs rather than style sheets: the compiler checks them, a widget reads them without lookups, and they are plain
aggregates that C++26 reflection will load from files. Selectors in the manner of CSS (a style for every button in a
toolbar) can come on top of them later.

### Basic widgets

`Label`, `Button`, `CheckBox`, `Panel`, `Card`, and `LineEdit`: a field of one line of text in any mix of directions. In
`LineEdit`:

- Arrow keys move the caret left and right on the screen, also across a change of direction; Home and End go to the
  logical ends.
- With Shift they select. With the word modifier (Control, Option on macOS) they move by words, forward in the text's
  direction.
- Shortcuts use the platform's modifier: Control, Command on macOS (`platform::shortcutModifier`).
- The clipboard and the position of the input method's window go through the `UiHost`. The input method's composition
  shows underlined at the caret until it is typed.
- A text longer than the field scrolls to keep the caret in view.

Containers:

- **`ScrollArea`** shows a content larger than itself. It has scroll bars per policy (Auto, Always, Never). It scrolls
  with the wheel (Shift: across), Page Up and Page Down, a dragged thumb, a pressed track, and to the widget the keyboard
  focus goes to (`Widget::scrollIntoView`).
  - It clips its content to its viewport: what is scrolled out is neither painted nor under the mouse
    (`Widget::childrenClip`).
  - A wheel turn it cannot use goes on to the scroll area around it.
  - In right to left, the vertical bar is on the left, and the content starts on the right.
- **`Splitter`** has panes with handles between them. Each pane takes a share of the room by its stretch, and never goes
  below its minimum size. Dragging a handle sets the stretches to the panes' lengths, so a resized splitter keeps its
  proportions. Side by side panes start on the start side.
- **`CheckBox`** is a box and its text, toggled by a click or Space. The box is on the start side.

**Only what changed is laid out, only what shows is painted.** `invalidateLayout` marks a widget and its ancestors, not
the rest: the others keep their measure (for the same space), and one given a rectangle of the same size is only moved,
what is in it staying where it is in it (`rearrange` arranges a widget again in place, when what its arrangement depends
on changed and its size did not). So scrolling moves the content without arranging anything in it. Painting skips a
widget out of what shows (the window, a clipping ancestor such as a scroll area's view), and everything in it; a little
around each widget is counted as its own, for what paints out of its bounds (focus rings, shadows). `glw_benchmarks`
measures it (Release, by hand): with 100 000 labels in a scroll area, painting went from 90 to 2.7 ms and a step of the
wheel from 104 to 2.9 ms. What stays proportional to the number of widgets (a stack walks its children when one changes,
and every widget is made and measured once) is for virtualized views of models, for data of any size.

**Item views show models of any size** (`Widgets/ItemModel.hpp`, `Widgets/ListView.hpp`, `Widgets/TreeView.hpp`,
`Widgets/TableView.hpp`). A model says how many items it has and tells its views what changed, by ranges (inserted, removed,
changed, reset); what an item is stays the model's own, and a delegate that knows the model makes a row's widget
(`makeRow`) and shows an item in it (`bindRow`): nothing goes through a value of any type, as Qt's `QVariant` would. Only
the rows that show are widgets, used again for other items as the view scrolls (the view scrolls itself: no content of
its whole height). Rows are all of one height (where a row is, is a product: no memory per item) or each as high as its
widget measures, estimated until it shows, in a Fenwick tree of doubles (where a row starts and which row is at a place,
in O(log n)); what shows stays in place as estimates are corrected, and a view scrolled to its end stays at it. With a
million items (or a hundred million), laying out, painting, scrolling a step and jumping to the middle each take less
than a millisecond (`glw_benchmarks`). A ListView has a current item, selected in Single mode (a soft background and a
short bar at its start, as Fluent's lists), chosen by the mouse and the keys, activated by a double click or Enter; a
`StringListModel` shows its texts by itself.

A `TreeView` is a ListView of the nodes that show: the top level's and, in order, the children of those expanded (an
entry each: the node, its depth, whether it has children and is expanded). A `TreeModel` gives a node's children and
whether it has any before they are counted, so a folder is read when it is expanded; its nodes are ids of its own
(`TreeNode`). Expanding inserts a node's subtree in the entries (the list keeps what shows in place), collapsing removes
it; a node expanded stays so when its parent is collapsed and expanded again. The view draws each row's indent and arrow
(a chevron, mirrored in right to left), the delegate the rest; Right and Left go in and out, a double click or Enter
expands, collapses and activates. With a thousand folders of a thousand files, all of them expanded (a million rows),
painting and scrolling take less than a tenth of a millisecond, and a folder collapsed or expanded above them 1.5 ms.

A `TableView` is a ListView of the rows of a `TableModel` (rows and columns, with headers; it tells what changed by
ranges of rows, and when its columns change), under a header that stays as they scroll. Each row is a strip of the cells
of the columns that show, and so is the header: only those cells are widgets, in both directions, put aside and used
again (by a row for another column) as the table scrolls down or across. A delegate makes a cell's widget for a kind
(`cellKind(column)`: cells of one kind show any column of it, so a column of check boxes beside columns of texts is
reused as such) and shows a cell in it (`bindCell(widget, row, column)`); `TableTextDelegate` shows texts. Columns are
as wide as set (the theme's until then), where each starts summed once when one changes; the last takes the room the
others leave. Dragging a header's edge resizes its column (the edge lit in the accent color), a click on a header is
told (to sort by it, say: sorting is the model's), and the table scrolls across with its own bar, the wheel turned with
Shift or tilted, and Left and Right; in right to left the first column is at the right and scrolling goes left. With a
million rows of a hundred columns (or a hundred million of a thousand), some 190 widgets show it; painting and scrolling
down or across take less than half a millisecond, a jump to anywhere 3 ms.

**A canvas is a surface without edges with widgets on it** (`Widgets/Canvas.hpp`): the ground of a node editor, a
diagram, a level map. A widget may give its children coordinates of their own (`childrenTransform`: a uniform scale and a
translation, `paint::Transform`); they are then painted, found under the mouse and mapped to the root
(`mapToRoot`, `mapFromRoot`, `transformToRoot`) through it, and culled in their own coordinates, so a widget in a canvas
is an ordinary widget at any zoom: a field in a node takes the focus and is typed into, its text is rasterized at the size
it shows and stays sharp. A `Canvas` places its items at points of the canvas, as big as they measure; zooming and
panning change its transform only, nothing is laid out again, and moving an item arranges that item alone. The wheel
zooms at the mouse, the middle or the right button drags the view (the right one let go without moving asks for a
context menu); it draws a grid, leaving out the lines that would come closer than 6 dips, and a subclass draws under its
items or over them in the canvas's coordinates (links, a selection rectangle). `Widget::setChildIndex` raises an item
without taking it out of the tree (the mouse it captured, its focus are kept). The painter draws cubic Bezier curves
(`drawBezier`, flattened by Wang's formula to a quarter of a pixel at the scale they show). With ten thousand nodes of
three labels, painting, panning and a step of the wheel take less than half a millisecond, and 6 ms with all of them in
view, zoomed out (text smaller than a pixel is left out); the first layout, shaping 30,000 texts, 0.3 s.
`Examples/NodeEditor` is a node editor on it (nodes with pins and fields, links, selection, comments).

Still to come in the widget core: a blinking caret and animations (they need timers), mouse cursors (the resize cursor
over a splitter's handle and a table header's edge), and the native extras of the chrome (Windows 11 shadow, rounded corners and snap layouts).

## Docking

*Done: in a window and across windows, with global panels and saved workspaces (`Widgets/Docking.hpp`,
`Widgets/MajorTabs.hpp`, `Widgets/DockingHost.hpp`, `Widgets/Workspace.hpp`), in the full mode and Wayland's restricted
one. Wayland's drag-and-drop between windows: later.*

Two kinds of tabs, as in Slate:

- **Major tabs** are documents or workspaces: a script, a level, a chart sheet. They live only in the tab strip at the top
  of a window, which is the window's title bar, as in Chrome or Firefox. Dragging a major tab out of its strip makes a new
  window with it. Dropping it on another window's strip moves it there. Dragging the only tab of a window drags the window,
  so windows merge and split by their tabs.
- **Panel tabs** are tools: a node palette, a property inspector, an output log. They dock inside the content area of a
  major tab, in a tree of splits and tab stacks. Dragged out, a panel tab floats in a panel window: a window with a dock
  area of its own, where more panel tabs can be docked. It has no title bar apart from its tabs: the tab bars along its
  top drag it where they have no tab, and the window buttons are at their end, as in Unreal Engine. A panel tab never
  goes into a major tab strip, and a major tab never goes into a dock area.

**Every panel says who owns it.** A panel tab type declares its scope:

- **Major tab:** the panel belongs to one major tab, as in Slate (the node palette and the inspector of one script). It
  docks only in that major tab's dock area, or floats in a panel window. While its major tab is not the active one, the
  panel and its floating windows are hidden.
- **Global:** the panel belongs to the application (an output log, a search across files). Every window has a dock area of
  its own around the content of its major tabs, as the side bars of an editor are around its documents. Global panels
  dock there, or float, and stay whichever major tab is active.

A panel never moves into a dock area of the other scope, so what owns it never changes while it is dragged.

How it is done: `DockPanel::scope` and `DockArea`'s scope say it, and an area takes panels of its scope only. An area may
have a **center** (`setCenter`, optional): a node of its tree that is not a panel, never moved, floated nor closed, whose
room is kept (a minimum size, so the splits around it cannot take it away, and with no panel it fills the area), which
panels dock around only (a tab dragged over it shows its four side guides; `addPanel` places them beside it). A script's
view in FlowGraph is the center of its major tab's area; a window's major tabs are the center of its global area. Areas
that panels move between are
one **group**: a major tab's area and its floating windows' areas; for Global, every window's area and theirs
(`makeSibling` makes one more, and a major tab torn off into a new window gives it one around its `MajorTabs`). Looking
for where a dragged tab would go, the stacks of other groups are passed over: a script's panel over a global stack goes
nowhere, a global panel over the scripts' panels finds the center they are in. A group's floating windows are above one
of its areas' windows (its home); when that window closes they go above another, and its global panels go into another
window's area. Saved layouts name the center where it is (`{"center": true}`).

**The widgets are the layout.** A `DockArea` holds one node: a `DockTabs` (a stack of panels: their tabs, and the content
of the one shown) or a `DockSplit` (a `Splitter` of nodes). The tree is saved as JSON and restored, with panels known by
their ids (`saveLayout`, `restoreLayout`). Panels a layout does not name go into the first stack, and ids it names that
the area does not have are passed over.

This changes what was planned, a model kept apart from the widgets. The widget tree already holds everything a layout
needs, and a separate model would only have to be kept in step with it. A tab moving between windows is a reparent in the
widget tree.

In a dock area:

- **Dragging a tab**, the stack under the mouse shows its guides: a cross of five in its middle, as in Visual Studio and
  Unreal Engine. Let go on the middle one, or on the stack's tabs, the tab joins them. Let go on one of the four around
  it, it makes a new stack on that side, sharing the stack's room. The guide under the mouse is lit and where the tab
  would go is shaded. Elsewhere on a stack, nothing is dropped: the guides are aimed at, not the stack's edges, so a
  drop is never a surprise.
- **A stack left without tabs goes**, and a split left with one pane gives way to it.
- **A tab's ×** closes its panel (`setOnPanelClosed` gets it).
- **A tab is as wide as its title**, between the style's minimum and maximum widths; a longer title is cut with …
  (`Label`'s `elide`). Tabs that do not fit scroll rather than shrink: by the wheel, and by a thin bar along the bottom
  of the tab bar, shown while the mouse is over the tabs. The tab shown is kept in view.
- **Buttons at the end of a stack's tab bar**: those of the panel shown (`DockPanel::buttons`: an icon drawn by the
  application, and what a click does), then the float button, which floats the panel shown by its stack's place, or,
  in a floating window, docks it back in its area. The float button is there only with a docking host.
- **A stack of one panel can hide its tab bar** (its hide button, `DockTabs::setTabBarHidden`), as Unreal Engine's "hide
  tab well": the panel takes all of the stack, and a little triangle in its start corner shows the bar again. It comes back
  by itself when another panel joins the stack; a floating window keeps its bar, which is its title. Saved layouts keep it.
- **`addPanel(panel, nextTo, side, share)`** builds a layout in code: beside a stack on its Start, End, Top or Bottom
  side, or in it.

`MajorTabs` shows the content of one of its tabs. Its `MajorTabStrip`, set as the title bar's center, shows the tabs as a
browser does: a click shows a tab, its × closes it, dragging moves it along the strip, and + asks for a new one. Where the
strip has no tab, it drags the window.

**Across windows**, what docking needs from the application is a `DockingHost`: where the windows are on the screen,
which one is under the mouse, opening, closing, moving, raising and hiding them. `Application` is one, given to every
root as `UiContext::docking`. Without one (an engine's root, a test of one window), tabs and panels stay in their window.
The tests use a host of their own, with windows that are only roots at places of a pretend screen.

- **A major tab** dragged along its strip (or a little above or below it) moves in it. Let go over another window's
  strip, it goes there and that window comes to the front. Let go anywhere else, it opens in a window of its own where
  it was let go, with a strip and the same handlers (they are given the `MajorTabs` they are for, so + adds to the right
  window). The only tab of a window drags the window instead, and let go over another window's strip, merges it into
  that window. A window left without tabs closes.
- **A panel tab** dragged out of its stack's tab bar (by more than a little above or below it) floats at once, as in
  Visual Studio and Unreal Engine, even over its own dock area: its panel goes into a tool window above its area's
  window, with a `DockArea` of its own, and the drag goes on with that window, which follows the mouse. Let go on a
  guide, it docks there; elsewhere it stays where it is. The areas of a major tab and of its floating windows are one
  group: any of them takes a panel of any other, and the stack it would go to shows it in its own window.
- **A floating window** moves the same way whether it is dragged by its only tab or by its title (the tab bars along its
  top where they have no tab): it docks where it is let go on a guide, all its panels with it. Its title is ours, not
  the system's, so that it docks as it moves; a double click on it maximizes it. A floating window goes when its last
  panel leaves.
  Closed by the user, its panels go back to their area. The floating windows hide while their area does (another major
  tab is shown), follow their area to another window, and close when it is destroyed.

**A workspace is saved and restored** as JSON (`saveWorkspace`, `restoreWorkspace`): the windows (where they are, how
large, maximized or not), the major tabs of each by their ids (`MajorTabs::setTabId`) with the layout of their panels and
their floating windows, and the global panels of each window with theirs. What is saved is the arrangement, not the
content: restoring asks the application for each window's `MajorTabs` (with its handlers), each document by its tab's
id and each global panel by its id (`WorkspaceFactory`), and puts each where it was; a document's own panels are made
with it and arranged by its saved layout. `Application::setOnQuitting` is the moment to save: once, while every window is
still there (a window closed before is not in it, as in a browser). Screen positions are saved as they are, so a window
of a screen no longer there comes back off it: keeping windows on the screens there are needs the screens' list, which
the platform layer does not give yet.

A window closed during the events is told (`onWindowClosing`) while every tree is still whole, and destroyed after the
events, one at a time: what one destroys may close others. Before a window is destroyed, the tool windows above it are
detached from it, since the system would otherwise close them with it.

Tearing off and docking back needs what not every system gives:

| Need | Windows, macOS, X11 | Wayland |
|---|---|---|
| Place a floating window where the mouse is | `SDL_SetWindowPosition` | Not allowed for clients |
| Mouse position outside our windows while dragging | `SDL_GetGlobalMouseState`, `SDL_CaptureMouse` | Not available |
| Move a window with the mouse and dock it back | Our own drag loop | `xdg-toplevel-drag` protocol: KDE Plasma yes, GNOME and sway no (2026) |

So docking has two modes:

- **Full** (Windows, macOS, X11): our own drag loop. Guides and drop zones are drawn inside the target window. A window
  dragged by its only tab is seen through while it moves (`DockingHost::setWindowOpacity`), so the guides under it show.
  Where a major tab would open a window of its own, a translucent outline shows it: an overlay window
  (`DockingHost::showWindowPreview`), above every window and never focused, since the place may be outside every window
  of the application. A panel's window opens at once instead, shown without the focus (`NewWindow::focus`): the window
  dragged from keeps it, and with it the mouse, so the drag goes on (the dock area takes the mouse from the tab, which
  goes with its panel: `UiRoot::captureMouse`).
- **Restricted** (Wayland, where `Platform::placesWindows` is false): a tab is dragged in its window only, with the same
  guides and drops. Let go out of its window, a panel floats in a window the compositor places; a major tab opens in a
  window of its own. No window follows the mouse and nothing is seen through: a floating window is moved by its title as
  any window, by the compositor (its tab bars are its caption), and its float button docks its panel back. Moving a tab
  into another window by the mouse needs Wayland's own drag-and-drop between our windows (and `xdg-toplevel-drag`, where
  the compositor has it, for a window that follows the mouse), through SDL's `wl_display` / `wl_surface`: not done yet.
  `GLW_DOCKING=restricted` gives this mode anywhere, to try it.

An application can also run on XWayland to get the full mode (`SDL_VIDEO_DRIVER=x11`).

## Localization and right-to-left

- **Translatable text is a value of its own** (`Widgets/Localization.hpp`). Widgets hold a `LocalizedText`, not a
  `std::string`. A text made from a string is shown as it is (user data, names, numbers). A text made by `glw::tr("...")`
  is translated: its English source is looked up by its key (the source itself unless one is given). Its arguments
  (`{name}`) are put in after translation, so a translator can move them.
- **Catalogs** are JSON, one per language, read by `Translator`. A text is its translation, or [the English it
  translates, its translation]. When the English changed since, the translation is outdated and the English shows, as in
  FlowGraph's catalogs.
- **The language changes while the application runs** (`Application::setLanguage`). Every widget is told
  (`onLanguageChanged`) and shows its texts again, window titles too. A language written right to left (Persian, Arabic,
  Hebrew, Urdu...) mirrors the windows.
- **Still to come:** plural rules (from the CLDR data, Unicode license); numbers, dates and times through a `Locale`
  interface (Persian digits, the Solar Hijri calendar), with a small implementation first and ICU behind the same interface
  if the need grows; and a tool that writes and updates catalogs from the texts of an application.
- **Layout direction:** every widget has a `LayoutDirection` (inherited, overridable). In right-to-left, layouts, scroll
  bars, tab order, docking and directional icons are mirrored. A text field takes its paragraph direction from its content
  unless told otherwise. A code editor stays left-to-right while still shaping the Persian inside strings and comments.

## Metadata and C++26

Qt needs moc for properties and signals. GLWidgets uses no code generator: the metadata comes from C++ itself.

**A runtime description is the contract.** Whatever needs metadata (the style system, serialization of layouts and
settings, an inspector, data binding) reads a runtime description: a type's name, base, properties (name, type, getter,
setter) and enum values. Today these descriptions are written with macros next to the class. With C++26 reflection
(P2996) and annotations (P3394) a `consteval` function produces exactly the same descriptions from the class itself:

```cpp
// C++20
class Button : public Widget {
    LocalizedText text_;
    bool flat_ = false;
    GLW_PROPERTIES(Button, text_, flat_)
};

// C++26: same descriptions, no list to keep up to date
class Button : public Widget {
    [[=glw::property]] LocalizedText text_;
    [[=glw::property]] bool flat_ = false;
};
```

What uses the descriptions does not change. Rules that keep that true:

- Properties are ordinary data members (or light wrappers), so reflection can enumerate them.
- No code generator, no external description files that duplicate the classes.
- Events, descriptors, vertices and settings are plain aggregates.
- Macros are only allowed where reflection will remove them, and are kept out of the hot paths of the API.

**Contracts.** `GLW_EXPECTS`, `GLW_ENSURES` and `GLW_ASSERT` (`Core/Assert.hpp`) become `pre`, `post` and
`contract_assert` when compilers ship contracts. Their conditions must have no side effects.

**Modules.** Headers for now, one public header per component and no macros needed by users except the export macro, so
that a `import glw;` module can be added once CMake and the three compilers handle modules well.

**`std::execution`.** Background work (file loading, font loading, search) will be designed around senders and receivers,
starting from a small task interface that can be implemented on top of `std::execution` later.

## Dependencies

| Library | Used for | License | Status |
|---|---|---|---|
| SDL3 3.4.18 | Platform, RHI | zlib | In use (pinned tarball, SHA-256 checked) |
| FreeType 2.15.0 | Glyph rasterization | FTL | In use (pinned, SHA-256 checked) |
| HarfBuzz 14.6.0 | Shaping | MIT | In use (pinned, SHA-256 checked) |
| SheenBidi 3.0.0 | Bidirectional text, script runs | Apache-2.0 | In use (pinned, SHA-256 checked) |
| libunibreak 8.0 | Line breaks, graphemes | zlib | In use (pinned, SHA-256 checked) |
| Vazirmatn 33.003 | Default font | OFL | In the repository (`Library/Fonts`), compiled in |
| CLDR data | Plural rules, locale data | Unicode | Planned (M4) |

Shader tools, needed only to change a shader (`Tools/CompileShaders.py`): `fxc` (Windows SDK), DXC (MIT /
University of Illinois license), SPIRV-Cross (Apache-2.0).

The `GLW_USE_SYSTEM_SDL3` option uses an installed SDL3 instead of the pinned release (for Linux distributions).

## Conventions

- C++20, `std::string` holds UTF-8 everywhere.
- Namespace `glw`, one nested namespace per lower layer (`glw::platform`, `glw::rhi`, `glw::paint`, `glw::text`); the
  widget layer, the library's face, is `glw` itself. Types are `PascalCase`; functions and variables are `camelCase`; data
  members end with `_`. The builders of `glw::ui` are `PascalCase` too: each stands for the widget it makes.
- Files are `PascalCase.hpp` / `.cpp`; public headers live in `Include/GLWidgets/<Layer>/` and are included as
  `<GLWidgets/<Layer>/X.hpp>`.
- Every file starts with SPDX headers (copyright and `MIT`).
- Errors of the environment (no GPU, no display) throw `glw::Error`. Bugs of the caller are contract failures, which abort.
- Comments say what and why: a header comment for each file, a comment for each non-obvious function.

## Milestones

| | Content | Status |
|---|---|---|
| M0 | CMake, SDL3, the Platform and Rhi interfaces, windows in dips, events, several windows on one GPU device, CI on the three systems | Done |
| M1 | RHI resources and passes into textures, the shader pipeline, `Painter` / `DrawList` / `Renderer` (boxes, shadows, polylines, polygons, images, clipping, transforms), borderless windows with hit testing | Done: run on Direct3D 12 and Vulkan (Windows); Metal built, not yet run |
| M2 | Text: HarfBuzz and FreeType, glyph atlas, bidi, line breaking, text layout with cursor and selection | Done, apart from system fonts and visual cursor movement (see [Text](#text)) |
| M3 | Widget core: tree, events, focus, layout, a UI root independent of the platform, window chrome as widgets (with the native extras), live resize | Done, apart from the native extras of the chrome and what is listed in [Widgets](#widgets) |
| M4 | Basic widgets (label, button, line edit, scroll area, splitter), styles, `LocalizedText` and catalogs, right-to-left layouts | Done, apart from plural rules and the formatting of numbers and dates (see [Localization](#localization-and-right-to-left)) |
| M5 | Docking: major and panel tabs, windows merging and splitting by their tabs, both modes, layout persistence | Done: docking in a window, major tabs and saved layouts (M5a), across windows in the full mode (M5b), global panels, saved workspaces and Wayland's restricted mode (M5c). Wayland's drag-and-drop between windows, and keeping restored windows on the screens there are, later; the restricted mode not yet run on Wayland |
| M6 | Validation: a node canvas and a code editor (FlowGraph), a chart widget with custom commands, an engine embedding example | In progress: the node canvas (`Canvas`, `node_editor`) |

## Open questions

- Where GPU support for older machines would be needed (no Vulkan 1.0 / Direct3D 12): if it ever is, an OpenGL backend.
- Text on the node canvas: glyphs rasterized at each zoom level (as now) or MSDF. Measured with the canvas (M6): the wheel
  zooms by steps, so a few sizes are rasterized and kept, and painting stays well under a millisecond; MSDF would matter
  for zoom animated without steps, which would fill the atlas with sizes.
- Whether ICU is needed at all, or the small `Locale` implementation is enough.
