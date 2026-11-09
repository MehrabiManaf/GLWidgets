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
| Window shadow and rounded corners | Windows 11 | Popups' corners rounded (`Window::setCornerRadius`: DWM's round corners on Windows 11, the window's region cut round before it); windows' shadows and their own corners still to come |
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
- **Reliefs** (`Painter::fillRelief`): a rounded box raised above what it is on (sunk into it, below zero), its edge
  rounding down over a bevel as a quarter circle, lit by a light. Still four vertices and no mesh: the fragment shader
  takes its height at each pixel from the same distance field, its normal from the slope of that height (the field's
  gradient, exact, along its nearest edge or out of its corner), and lights it with a diffuse and a specular term,
  scaled so that its flat top shows its color as it is; sunk, its rim casts a shadow on its floor (the field asked
  where a ray toward the light leaves it); a metal's highlight is of its color, and its surface brushed (lines half a
  dip high, each a little lighter or darker). The light (`paint::Lighting`: a direction, ambient, diffuse, specular,
  shininess, the rim's shadow, the finish: metal, brushed) is the fragment shader's uniform buffer, the Renderer's (`setLighting`, the theme's); an
  engine drawing the draw lists itself pushes it too. A sector of a ring is a relief as well (`fillArcRelief`: a slice of
  a pie as a lit style's control), lit the same way from its own distance field (two radii and two sides moved in), a
  metal one turned (its lines around it).
- **Lines and polygons:** polylines of any width, and convex polygons, tessellated on the CPU with a one-pixel
  anti-aliasing fringe. A polyline's joints are mitered where it turns by 120 degrees or less, beveled where it turns
  more sharply (a jagged signal turning back on itself), so that it is as wide everywhere; a miter held in there would
  pinch it to nothing at each such turn, and a line of many would look dashed. This is what charts and node connections are made of. For charts also: the area
  between a line and a level (`fillArea`: a chart's area under its line, in a gradient from the line down), and a sector
  of a ring (`fillArc`: a slice of a pie or a donut, its sides moved in so that slices side by side are a gap apart
  all along, a color at its inner radius and another at its outer, or one at its top and another at its bottom).
- **Images:** a texture (by id) on a rectangle, tinted; sampled smoothly, or (a texture added so: `Renderer::addTexture`'s
  filter) each of its pixels a sharp square however large it is drawn. A widget makes images of its own, pixel by pixel,
  in the application's `ImageStore` (`UiContext::images`): it holds an `ImageStore::Image`, writes into its rows, says
  what it changed, and draws it by its texture; what changed goes to the GPU once a frame, before the windows are drawn,
  its texture made again when its size changes, gone when the image is (a heat map's cells; later a picture, a
  spectrogram).
- **Masks:** the red channel of a texture as coverage, in a color. Glyphs are masks of the glyph atlas (see [Text](#text)).

**The vertex format and one shader are the whole contract of a draw list.** A vertex has a position, texture coordinates,
the parameters of a box (half size, corner radius, border width, blur), its kind (textured, box, mask, relief, ring relief) and an RGBA8
color. A command is a range of indices, a clip rectangle and a texture id. One pipeline with one shader pair draws all of it
(`Library/Shaders/Painter.hlsl`), with premultiplied alpha blending. An engine that draws GLWidgets' lists itself needs
exactly that shader and one pipeline.

`glw::paint::Renderer` draws draw lists with the RHI: it uploads a list's geometry into the buffers of its target, then
draws it inside the target's pass, one draw call per command. Texture ids are mapped to RHI textures by the renderer; 0 is
white.

**For charts:** points are mapped from data to dips by the chart, in double precision, before they reach the painter, and
a series of more points than the plot has columns of pixels is drawn as what each column shows (see [Charts](#charts)):
a frame tessellates a few points for each column whatever the data's size, so a million points or ten million cost what
a few thousand do. Should a chart ever need to draw every point (a scatter of a million dots), a draw list will hold
*custom commands* *(planned)*: a callback that draws with the RHI directly (instanced markers, a line shader over a
storage buffer of points) in the same pass, between the commands around it; they exist only when GLWidgets renders the
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

**A line placed alone in a box is centered by its letters** (`TextLayout::opticalOffset`, what labels and fields add to
its origin). The default font is made for Persian: its height has room for marks above and for deep descenders below
(0.54 em of 1.56), and Latin letters centered by it look 1 to 2 dips high. A line without Arabic script is moved down so
that the middle of its letters (between their x-height and cap height, from the font's OS/2 table) is the middle of its
height; a line with Arabic script stays as the font centers it, and a text of several lines is not moved.

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
kind of widget, made from the palette by `Theme::fromPalette` in one of the library's looks (`ThemeStyle`), each in the
dark and in the light (`Theme::dark(style)`, `Theme::light(style)`, with the style's own palettes), or in one of three
more color schemes (`ColorScheme`, `Theme::of(scheme, style)`): **Dusk**, dark of other colors (plum grays, a violet
accent); **Sand**, light of other colors (paper and sand, warm hairlines and shades, a terracotta accent); **Turquoise**,
the colors of glazed tiles (a deep turquoise, lapis lazuli hairlines and light under the mouse, a saffron gold
accent, ivory text). Each is the same in every style, but Metal's controls a little further from what they are on; the
code in a text edit is in the scheme's colors too. An application changes look with `Application::setStyle` (in the
colors it is in) and colors with `Application::setColorScheme` (in the style it is in), eased as `setTheme` is; a style
and a scheme have names (`themeStyleNamed`, `colorSchemeNamed`: what the demos' `--style` and `--colors` take). From the theme shown to
the new one, the palettes' colors mixed and the shapes too (`Theme::blend`: corners rounding, controls growing lower,
bevels fading; a pill's radius as round as a control is high, so that it rounds as it goes):

- **Studio** (the default), described below: square, controls raised and lit from above, fields sunk, quick motions (what the tests and
  the benchmarks measure).
- **Fluid**, modern: Plastic's and Metal's reliefs (below), lit softly (a broad, gentle white highlight), and rounder:
  buttons, fields, combo boxes and tool buttons pills (a half circle at each end: the painter takes a radius past half a
  box's height as half of it), panels 10, menus' items and the menu bar's titles 6, docks and cards 10 (the tab shown a
  folder's tab of its stack, 8), nodes 12, bars and tiles 6, the scroll thumbs pills; their edges rounding down over
  further; deeper palettes with a brighter accent; more motion and longer (150, 260
  and 380 ms, the wheel 240): a press on a button or a row ripples out of where it was (`Ripple`: as
  high as the control and as round, out sideways to its ends, full until halfway and then fading: inside a pill's ends
  too; `ButtonStyle::ripple` and `motion.ripple`), the selection of a list, tree or table slides from the row it was on
  to the one it goes to (`ItemViewStyle::slideSelection`), rows are pills without lines between them; popups' windows
  (not menus', square in every style: `PopupRequest::windowRadius`) are rounded (`WindowFrameStyle::popupRadius`, 8 dips: on Windows 11 by the system, smoothly; before it, the window's
  shape cut round; square on macOS and Linux for now, their edges then drawn square: `UiRoot::windowRadius`), come in
  from 12 dips.
- A collapsible panel is a card: its body a step above the window (not sunk below it), the header over it. In Studio, a
  line between them (`CollapsiblePanelStyle::separator`), a soft edge around it and a soft shadow under it (`shadow`);
  in Plain the same, crisp and without the shadow; in Plastic, Metal and Fluid its header a relief. In none is there a ring of the
  focus: the header shows it, lighter (`header.focused`), as under the mouse. As it opens, its header's lower corners
  square, eased with it.
- **Plain**, simple: square and flat, no light from above, no shadows, no glow of the focus (its edge says it), gray
  palettes without a tint; nothing moves (`MotionStyle::still`: every motion at once, the wheel's too; rows inserted
  and removed, panels opened and closed, popups, all at once).
- **Plastic** and **Metal**, real: their controls reliefs lit by the theme's light (`Theme::lighting`, from the upper
  left, a little in front; `Bevel::relief`, `roundness`): buttons, combo boxes, tool buttons under the mouse, panels'
  headers and a checked box rise out of what they are on, their edges turning toward the light lit, those turning away
  in shade, a soft shadow under them; pressed, a button goes down; fields and boxes are sunk, their rims shadowing their
  floors. Low reliefs (a button 1.6 dips high, its edge rounding over 2.5), and as controls are: the headers of lists,
  trees and tables (`ItemViewStyle::headerBackground`, `headerBevel`) and the rows' highlights, under the mouse and
  selected (`rowBevel`); corners a little round (4 to 6 dips), Studio's motions. They differ by their finish:
  - **Plastic**: glossy, a strong, tight, white highlight where a surface turns toward the light; Studio's palettes.
  - **Metal**: its highlights of the metal's own color, broad (`Lighting::metal`), its surfaces brushed: fine lines along
    them a little lighter and darker (`Lighting::brushed`); gunmetal grays in the dark, aluminum in the light.
  `paintRaised` and `paintSunken` draw every control's relief, so a widget of one's own that draws with them has one
  too.

Studio's look is of the family of Qt's Fusion, compact and softer: neutral grays with a little blue in them,
in layers (the title bar the darkest, the window's background, panels over it), controls that stand out of them lit from
above. A raised control (a button, a combo box, a panel's header; a tool button under the mouse) is lighter at its top,
has a light line inside along its top that fades down its sides and a soft shadow under it, and sinks when pressed; a
sunken one (a field, an unchecked box) is darker than what is around it, with a shadow inside along its top and a light
line under it (`Bevel`, painted by `paintRaised` and `paintSunken` of `Widgets/Bevel.hpp`, for widgets of one's own).
The edges of controls are darker than what they are on; one accent: amber (a warm orange) with dark marks on it in the
dark theme, azure with white ones in the light. What has the focus has an edge of the accent and a soft glow of it
around (`paintFocusGlow`), both eased in. A check box's box fills with the accent as it is checked, its mark drawn
stroke by stroke; the item chosen in a menu is in the accent. Everything is square by default: every style's
`cornerRadius` is the theme's, 0 (a theme of its own rounds them: controls, panels, tabs, scroll thumbs). Docked
stacks are islands on the window with gaps between them; the tab shown is raised over the bar and joined to its content
as Fusion's, and a selected row is on the accent's tint (strong enough to stay orange on the dark, not brown): no bars
mark them. Text is 13 dips, tabs 28 high; controls are low in every style: 3 dips above and below a button's, a
field's or a combo box's text, panels' headers 30, tool buttons 28, rows 26. The painter fills and strokes boxes in vertical gradients (`fillRoundedRect(rect, radius, top, bottom)`), with no
more vertices than a flat box.

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
- **Every scroll bar is one** (`ScrollBar`, `Widgets/ScrollBar.hpp`): a scroll area's, a list's and a tree's, a table's
  across, a text edit's (down and across: a line longer than shows), a dock's tabs' (in their strip, without a track),
  a bar race's. Not a widget: a part of its widget, which places it as it is laid out and scrolled, paints it, and gives
  it its mouse events first. So it looks the same everywhere, as the style says (`ScrollBarStyle`: its track, its
  thumb's colors under the mouse and pressed, its width, its least length, its corners; thin until the mouse is over it,
  then wide, eased), and does the same: its thumb dragged follows the mouse; its track pressed beside the thumb pages
  toward it, nine tenths of what shows, eased from where the scroll was going (pressed again on the way, a page
  further). A vertical bar is at the end side (on the left in right to left; a text edit's at the right, its lines left
  to right either way), a horizontal one's thumb from the start side. The wheel goes through `wheelScroll` in all of
  them: a step from where the scroll was going, further as the wheel turns faster (`SmoothScroll::aim`), a touchpad's
  fractions as they come, and not taken when it moves nothing, so that what is around it scrolls instead. A bar takes
  the length of what shows as it shows: a list's (a tree's) as its rows come in or go out, eased (a node opened or
  closed), so that its thumb shrinks and grows with them rather than at once.
- **`Splitter`** has panes with handles between them. Each pane takes a share of the room by its stretch, and never goes
  below its minimum size. Dragging a handle sets the stretches to the panes' lengths, so a resized splitter keeps its
  proportions. A double click on a handle shares the room of the two panes beside it equally, eased (`shareEqually`);
  a pane that is, or only holds, a splitter the same way gives its pane by the handle, the others keeping theirs, so
  two docks side by side share their room though they are in nested splits. Side by side panes start on the start
  side.
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
than a millisecond (`glw_benchmarks`). A ListView has a current item, selected in Single mode (on the accent's tint), chosen by the mouse and the keys, activated by a double click or Enter; a
`StringListModel` shows its texts by itself.

A `TreeView` is a ListView of the nodes that show: the top level's and, in order, the children of those expanded (an
entry each: the node, its depth, whether it has children and is expanded). A `TreeModel` gives a node's children and
whether it has any before they are counted, so a folder is read when it is expanded; its nodes are ids of its own
(`TreeNode`). Expanding inserts a node's subtree in the entries (the list keeps what shows in place), collapsing removes
it; a node expanded stays so when its parent is collapsed and expanded again. The view draws each row's indent and arrow
(a chevron, mirrored in right to left), the delegate the rest; Right and Left go in and out, a double click or Enter
expands, collapses and activates. With a thousand folders of a thousand files, all of them expanded (a million rows),
painting and scrolling take less than a tenth of a millisecond, and a folder collapsed or expanded above them 1.5 ms;
eased (its children coming out from under it, the million rows after them moving), each of its twelve frames takes
0.07 ms on average, the first, which binds the rows that come, 0.4 ms (`glw_benchmarks`).

A `TableView` is a ListView of the rows of a `TableModel` (rows and columns, with headers; it tells what changed by
ranges of rows, and when its columns change), under a header that stays as they scroll. Each row is a strip of the cells
of the columns that show, and so is the header: only those cells are widgets, in both directions, put aside and used
again (by a row for another column) as the table scrolls down or across. A delegate makes a cell's widget for a kind
(`cellKind(column)`: cells of one kind show any column of it, so a column of check boxes beside columns of texts is
reused as such) and shows a cell in it (`bindCell(widget, row, column)`); `TableTextDelegate` shows texts. Columns are
as wide as set (the theme's until then), where each starts summed once when one changes; the last takes the room the
others leave. Dragging a header's edge resizes its column (the edge lit in the accent color); a double click on it
fits the column to what it shows, eased (`fitColumn`: its header and its widest cell, measured in a cell of its kind
bound to each row: the rows that show and a thousand more spread across them by the golden ratio, so rows that repeat
in a period are not all missed; `setFitRows` for more, or all; a tree's first column with its indents and arrows).
Measuring a cell costs about 10 µs: a fit takes 9 ms, every row of a million 10 s (`glw_benchmarks`). A click on a header is
told (to sort by it, say: sorting is the model's), and the table scrolls across with its own bar, the wheel turned with
Shift or tilted, and Left and Right; in right to left the first column is at the right and scrolling goes left. With a
million rows of a hundred columns (or a hundred million of a thousand), some 190 widgets show it; painting and scrolling
down or across take less than half a millisecond, a jump to anywhere 3 ms.

**A tree of columns is a table.** A TreeView's rows are those of a TableView (its entries as a table's model), so a
`TreeModel` of several columns (`columnCount`, `header`, and in a column `flags`, `sortable`, `compare`: the first
column's by default) shows them as a table does, resized by their edges and scrolled across; the first column holds the
indent and the arrow, the delegate's cell after them (a `TreeDelegate` makes cells by kind and binds a node's cell in a
column, as a table's delegate does). The table's header says which column sorts, the tree sorting each node's children
by it (the table's rows left as they are: `TableView::setSortIndicator`); a cell of the first column is edited after
the arrow (`TableView::setEditorArea`). A tree of one column is a list still: no header unless that column has a title.

**Headers look as controls do**, in every style: a strip behind their titles (`ItemViewStyle::headerBackground`)
raised as the style's controls are (`headerBevel`: lit from above in Studio, flat in Plain, a
relief in Plastic, Metal and Fluid).

**Headers show and hide.** A list has a header of one column when it has a title (`ListView::setHeader`): its rows
under it, a click sorting them (ascending, descending, not; an arrow in it), unless sorting is off. A table's header,
a tree's (shown when it has several columns or a titled one) and a list's are hidden at will (`setHeaderVisible`), the
rows taking their place.

**Several items are selected at a time** in `SelectionMode::Multiple`, as in a file browser (Qt's extended selection):
a click selects an item alone, Ctrl+click (Cmd on macOS: `platform::shortcutModifier`) adds or takes one, Shift+click
selects from the anchor (the item last clicked alone or with Ctrl) to it, Ctrl+Shift adds that to what was; the keys move
as they do, Shift with them selecting from the anchor, Ctrl moving the current item only (Ctrl+Space takes or leaves
it); Ctrl+A selects all. The selection is the model's items' (`selectedItems`, `setSelected`, `selectAll`,
`setOnSelectionChanged`), kept as ranges of indices (`IndexSet`: a million selected are one range), moved on and back as
the model inserts and removes items before and among them, the same as the list is sorted; what the filter leaves out is
no longer selected. A table selects its rows (`selectedRows`); a tree its nodes (`selectedNodes`), kept by node when it
is sorted, filtered or its model reset, those under a node collapsed no longer.

**A tree reveals a node** wherever it is (`TreeView::reveal`): its ancestors expanded from the top down, it made current
and scrolled to. The model tells a node's parent (`TreeModel::parent`; one that does not, by default, has its nodes
revealed only where they show).

**Cells' borders** are the style's (`ItemViewStyle`: `rowLines`, a line under each row; `columnLines`, a line between a
table's columns, through its header and down to its last row; `gridLine`, `gridLineWidth`). The theme's `table` style
shows both, as a spreadsheet does; its `itemView`, of lists and trees, the lines between the rows. A view takes its own (`setStyle`), or
the theme's; with borders, the rows' backgrounds (under the mouse, selected) fill their cells rather than being pills.

**Views sort, filter and edit what their models hold, without the models changing.** A model says how two of its
items compare (`ListModel::compare`, `TableModel::compare` in a column, with `sortable`; `TreeModel::compare` of two
children) and what can be done with an item (`flags`: `ItemFlags::Editable`, as Qt's item flags). A list keeps the
order it shows its items in, sorted (`setSortOrder`: the model's comparison or another, ascending or descending, equals
in the model's order) and filtered (`setFilter`: the items a function accepts): a position to a model's item and back
(`modelIndex`, `position`), nothing at all while it shows them as they are. Its delegate, its handlers and what is asked
of it (`current`, `setCurrent`, `rowWidget`, `itemRect`) speak of the model's items still, so a delegate written for
the model shows it sorted and filtered unchanged. A table sorts by a click on a sortable column's header (ascending,
descending, not; an arrow in the header says which) and filters its rows; a tree sorts the children of each node, and
its filter keeps the nodes it accepts with their ancestors, opened to show them (every node of the model is asked). A
million rows sort in 0.4 s, filter in 0.2 s (`glw_benchmarks`).

**The model's changes reach the order item by item**, for models that change all the time (a live feed of a million
rows): the items a notification names only are compared and filtered. An item inserted goes where it sorts (a binary
search among those shown), one removed goes, one changed stays where it is when it still sorts between its neighbors
(two comparisons) and moves where it sorts otherwise; the filter is asked of those items only. The order is an
`ItemOrder`: each item a handle, in two sequences, the model's order (a handle's place is its index) and the order shown
(its place is its position), each in blocks of a few hundred handles with where each block starts, so an item is
inserted, erased, found and moved in about the square root of their count, and an item inserted before others shifts no
index. The current item and the item at the top stay where they were; the rows that show are bound again only when the
change reaches them. In a table of a million rows sorted by price, a price changed costs 20 µs, a row appended 25 µs,
one inserted first 18 µs, the oldest removed 11 µs (`glw_benchmarks`: a thousand of each, then a frame painted); many at
once (more than a quarter of what shows) sort it all again. A tree sorted or filtered sorts and filters all of it again
as its model changes.

**A model tells a batch of changes** (`beginChanges` / `endChanges`: all that came since the last frame, nested, the
outermost counting) as it tells each, and the views put what they mean in order once, at its end. In a list or table
sorted or filtered, the items inserted and changed in it wait out of the order (what was removed goes at once), and at
its end each is placed once where it sorts then, however many times it changed; many waiting (more than a two
thousandth of what shows) are sorted among themselves and merged into the order in one pass, each found from where the
one before it went. The current item and the item at the top are kept by their handles through it. A tree sorted or
filtered sorts and filters all of it again once, at its end. An order book sorted by price then time, its frame's
changes a thousand orders gone, a thousand new and two thousand repriced: 4 ms told in a batch for ten thousand orders
(13 ms one by one), 10 ms for a hundred thousand (23 ms), 64 ms for a million (94 ms; sorting it all again, 660 ms).
Nothing is to be asked of a view in a batch: what it shows meanwhile is not in order.

An editable item is edited in place, in any widget its delegate makes for it (`makeEditor`: a field, a check box, a combo
box...), in a frame over it (over a tree's node after its arrow; a table's cell, within its rows): by a double click
(which then does not activate it), F2, or typing on it (what is typed goes into the editor), as `setEditTriggers` says.
Enter, or the focus leaving the editor (another item pressed, another widget), puts what was edited in (`commitEdit`,
the delegate's, into the model); Escape leaves it as it was; in a table, Tab goes on to the next editable cell of the row.
The editor follows its item as the view scrolls, and goes when the model changes under it, or the view is sorted or
filtered again. The text delegates edit in a field when told where the text goes (`setOnEdited`); a `StringListModel` made
editable edits its texts by itself. A widget learns that the focus left its subtree (`onFocusWithinChanged`).

**Popups are windows of their own** (`UiHost::openPopup`, `PopupRequest`), as native menus are, so that a menu of a
small floating panel may go past the panel's edges: SDL's popup windows (`SDL_CreatePopupWindow`: Wayland's xdg_popup
too), placed by the window of the widget they open from, from an anchor (below it, or beside it for a submenu), on the
part of the screen windows may cover: above the anchor when there is more room there, at the other side when there is
none, and a submenu of a submenu on the side its menu went. A popup is as big as its content measures; the host keeps its
root until it closes: by its host's `closeWindow`, a click outside it (in a window of the application: that click does
nothing else), the application losing the focus, or its window resized or closing (its widgets are told by
`onWindowClosing`). The keys and the text typed in a window with a menu open go to its topmost menu, whatever the system
does with the keyboard. Popups are not among the application's windows (docking does not see them). A host without popups
(an engine's) opens none.

`showContextMenu` / `showMenu` (`Widgets/Menu.hpp`) open a menu of `MenuItem`s: a text, a handler, a shortcut shown at its
end, a check mark, a submenu, separators, disabled items. The mouse chooses by going over an item (a submenu opens) and
triggers by letting go over it; the keys choose (Up, Down, Home, End, past separators and disabled items), go in and out
of submenus (Right and Left, mirrored in right to left), trigger (Enter, Space) and close (Escape). A `ComboBox` is a text
chosen from such a menu under it, the current one checked; Up and Down change it without opening it. A search popup
(`showSearchPopup`, `Widgets/SearchPopup.hpp`) is a field over a list filtered as it is typed in: every word typed must
be in an item's text or detail (case aside), those starting with the first come first, then those with a word that does;
Up and Down choose while the field keeps the focus, Enter or a click takes one (what adds a node by its name, completes a
word, runs a command). A widget's
tooltip (`Widget::setToolTip`) is a popup that takes no input, under the mouse's pointer, shown by the root after the
mouse stayed over the widget half a second (80 ms when another closed a moment ago: along a toolbar), gone when the
mouse leaves the widget, presses, scrolls or a key is pressed, and not again until it leaves.

A modal popup (`PopupRequest::modal`) is a dialog: while it is open, the other windows of the application take no input
(their mouse does nothing but close the dialog's own popups, their keys and typed text are the dialog's, they do not
close), and the window it opened from shows dimmed (`UiRoot::setOverlay`, the theme's scrim). Only its host's
`closeWindow` closes it, or its window closing: not a click outside, not the application losing the focus. A popup's text
is typed in the window it came from (which keeps the system's keyboard) and routed to it, so a field works in any popup.
`showDialog` (`Widgets/Dialog.hpp`) opens a `Dialog` in the middle of the window: a title, content of any widgets, a row
of buttons at its end (the default one in the accent color); a button closes it and says which it was, Enter is the
default button (a field without its own Enter handler leaves Enter to it), Escape closes it without a choice; the focus
starts on its first field. `showMessage` is a dialog of a wrapped text.

**What the user can do is an `Action`** (`Widgets/Action.hpp`), once for every place it is offered: a text (a & before
its mnemonic), an icon, a shortcut, a tooltip, enabled or not, checkable; what shows it follows it (an Undo disabled in the
menu and in the toolbar at once). A menu's item shows its action (`MenuItem::action`: its text, icon, shortcut, state) and
triggers it. A key the focused widget does not take triggers the first enabled action of the root it is the shortcut of
(`UiRoot::addShortcuts`, by owner: a menu bar, a toolbar, the application). A `MenuBar` holds a window's menus by their
titles: a click opens one under its title, the mouse over another title while one is open opens that one, Left and Right
go across, Alt and a mnemonic or F10 open them; it goes in the title bar (`TitleBar::setStart`, before the title or the
tabs) or in a row under it (`MenuBarPlacement`). A `ToolBar` is a row of actions as `ToolButton`s (an icon, a text if
asked, pressed while checked, the action's tooltip or its text and shortcut), separators and any widgets; what does not fit
goes into a menu at its end. `Window::setMenuBar` and `setToolBar` put them in the window, with its custom chrome or the
system's. **Icons** (`Widgets/Icon.hpp`) come from SVG, parsed and rasterized by ThorVG (its CPU rasterizer), at the size in pixels they show at,
once, into an RGBA atlas every window shares (`UiContext::icons`, uploaded with the glyph atlas), or are painted by a
function. A monochrome icon is its shapes' coverage in the color it is given (the theme's text: it follows the theme);
another keeps its colors.

**A `TextEdit` holds a text of many lines** (`Widgets/TextEdit.hpp`): code, a log appended to, a field of several lines.
Its lines are kept apart (positions are lines and bytes in them), and only those that show are laid out (one
`TextLayout` each, kept while it does not change, those far from what shows let go) and painted; a line's height is the
fonts', so where a line is, is a product. Lines are not wrapped; it scrolls both ways. Each line is laid out left to right
whatever is in it, the Persian in its strings and comments shaped and ordered as written. It has the keys of a code
editor (words, Home to the first character, Up and Down keeping their column, Page Up / Down, Shift to select; Enter
keeping the indent, Tab and Shift+Tab indenting the lines selected, Backspace back to the tab stop in an indent), the
mouse (a drag selects, a double click a word, a triple one a line), the clipboard, and undo and redo (what is typed one
character after the other undone as one; an indent of many lines as one). A `Highlighter` colors a line at a time from
the state the line before left (a comment going on), so an edit is colored again from its line down only as the lines
show; `CppHighlighter` does C and what looks like it, in the theme's colors of code (`textEdit`: Dark+ and Light+).
`text::drawText` takes color spans for it, and leaves out the glyphs of a long line out of the clip. Read only, it is a
log: `append` adds lines and keeps it at its end while it was there. With a million lines of highlighted C++, painting
takes 0.4 ms, scrolling a page 1.3 ms, typing a character 0.6 ms; reading it 0.2 s, going to its end (colored from the top
once) 0.14 s; some 250 lines are laid out at any time (`glw_benchmarks`). Still to come: find and replace, folding,
wrapped lines, completion (on the search popup), several carets.

**Tabs look as the style's controls do**: in Studio the tab shown is raised over its bar and joined to its content,
as Fusion's (a browser's in the title bar), in Plain the same, flat; in Plastic and Metal the stacks and cards are
raised too, slabs (`DockStyle::islandBevel`, `CardStyle::bevel`), and the tab shown is a folder's tab of its stack: a
relief as high in its color, its lower part over the slab's top edge (cut there), so that its face is the slab's with no
seam; the slabs only a little round (5 dips) and low, the first tab after the slab's rounded corner, not over it; the
other tabs are on the window, one under the mouse a little raised (a major tab joined to the window's content
the same way); Fluid's the same, rounder. A style may have the tab shown stand on its bar as a control does instead,
raised (`DockStyle::tabsJoined`, `tabBevel`, `tabRadius`: a pill past half its height; `MajorTabStyle::joined`, `bevel`,
`raised`).

**A node of a node editor looks as the style's controls do** (`NodeStyle`, `Theme::node`: what node_editor draws its
nodes with): its corners (3 dips in Studio, square in Plain, 8 in Plastic and Metal, 12 in Fluid), its body and its
header of its kind's color raised as controls are (`bevel`, `bodyBevel`: lit from above, flat, reliefs), a shadow under
it (none in Plain), its edge and the ring around it selected. On a zoomed canvas its relief is scaled with it.

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

**Nothing is drawn for time alone** (`Widgets/Animation.hpp`). One `Scheduler`, shared by every root of an application
(`UiContext::scheduler`), runs the timers (`Timer`: a tooltip's delay, a caret's blink) and the animations (`Animation`:
a number eased to a target over a time; `ColorTransition`: a color). The application sleeps until an event comes or the
next timer is due (`nextDeadline`); only while an animation runs does it draw, at each refresh of the display (vsync paces
it), and then only the windows whose widgets an animation changed: each step calls the animation's `onStep`, where a
widget invalidates its paint. When the last animation ends, the frames stop. A widget does not say when its state
changes, only what it shows now: `ColorTransition::to(color)` in `paint` eases from what it showed to that color (at
once the first time). Buttons, check boxes, fields, tool buttons and combo boxes ease to their states' colors for the
theme's `motion.fast` (100 ms), the glow of the focus for `motion.normal` (200 ms), as a check mark is drawn; a field's caret blinks half a second each way and rests,
shown, after twenty, so a field no one types in draws nothing. Animations can be turned off (`setAnimationsEnabled`:
reduced motion; the tests); the tests run on a clock of their own. An engine driving its own roots calls `tick()` before
it paints and waits as `nextDeadline` says. While an animation runs, its next step is taken before the loop would sleep,
so that a frame it draws is drawn at once, at the display's pace (the swap chain waits for the refresh), rather than a
fixed 16 ms after the last, out of step with the refresh (frames 17 to 23 ms apart, some shown twice: a stutter; now
16.7 ms, at most 18, scrolling a list). Measured with `GLW_LOG_FRAMES` (the frames drawn, each second that had some, how
far apart and how many late): an idle window draws none; a hover draws about six frames (100 ms at 60 Hz) and its
tooltip one; a caret two a second for ten seconds, then none.

A `CollapsiblePanel` (`Widgets/CollapsiblePanel.hpp`) is a header over a column of content that opens and closes, as the
sections of a details panel do. Its height eases between the header's and the header's with all of its content (the
theme's `motion.slow`, 300 ms, in and out), its content laid out at its full height and cut to what shows, the header's
arrow turning with it; so the widgets under it move with it, laid out again at each step. Closed all the way, its content
is hidden (not painted, not under the mouse, not in the order of the focus); the focus in it goes to the header as it
closes. A click on the header, Enter or Space toggle it, Right and Left open and close it (mirrored in right to left);
buttons at the header's end trigger actions (a + that adds an item). `ui::CollapsiblePanel` takes its content as children.

**What moves, moves eased**, from where it shows, and is drawn only while it does:
- **Scrolling** (`SmoothScroll`): the wheel, a page, a press on a bar's track and an area brought into view ease the
  offset there (the theme's `motion.scroll`, 180 ms), in scroll areas, lists, trees, tables (across too) and text
  editors. From rest it starts as fast as it goes and slows down to a stop; sent on while it moves, it goes on from the
  speed it has (the curve 1 - (1 - t)^3 (1 + c t), its speed at the start 3 - c of the distance over the time, chosen
  to be the speed it had, and at least 1), so a wheel turned on and on moves evenly rather than jumping at each step.
  Turned again on the way, the wheel goes further from where it aimed, by a step as large as the wheel turns fast
  (`SmoothScroll::boosted`): by the time between its steps, smoothed over the last few, a step at 120 ms apart or more,
  4 steps at 20 ms or less, eased in between, so slow steps stay steps and a flick flies; a pause or the other way, a
  step again. In lists, trees and
  tables it goes to the nearest row's top (across, a column's edge; in a text editor, a line's), or the end, so that
  what is at the top shows whole; the steps add up from where the wheel aimed, not from where that was snapped to
  (`SmoothScroll::aim`), so they stay even. A thumb let go goes there too. A thumb dragged, an offset set, and what
  follows the caret are at once.
- **A touchpad scrolls by what it turned**: a wheel that turns by fractions of a step (a touchpad, a wheel that spins
  freely or turns finely; and its whole steps for a moment after, `MouseWheelScrolled::precise`) eases the offset on
  from where it is going by just what it turned, not boosted nor snapped to rows: each fraction would go back to the
  same row's top.
- **A tree's nodes open and close eased** (`ListView::setAnimatesChanges`): the rows inserted among those that show come
  out from under the row before them, cut to what of them shows (`Widget::childClip`), the rows after them going down
  with them; rows removed go back under it showing what they showed, the rows after them coming up. At most a view's
  height moves: a thousand children come in as fast as ten.
- **Popups come in**: faded in (the window's opacity) and their content sliding 6 dips out of their anchor (down from
  what a menu opens under, up when it went above, across from a submenu's item, up into the middle for a dialog; a
  tooltip only fades), over `motion.normal`. They close at once.
- **Scroll bars** are thin by their outer edge, and widen while the mouse is over them or their thumb is dragged
  (`paintScrollThumb`).
- **A tree's arrows** turn as their node opens or closes, as a collapsible panel's (kept by node: the rows showing
  them are bound again as the entries change); **a stack's tabs** slide to their places when one before them
  closes or one is moved; **where a dragged tab would go** moves from guide to guide and fades in and out.
- **A canvas** zooms at the mouse eased (the point under it staying), and is framed (F) eased: the zoom in proportion,
  the middle moving to the area's.
- **The theme** (`Application::setTheme`) eases through the themes of the palettes between the one shown and the new
  one (`motion.slow`), then is the new one; the colors of states do not ease after it (`Scheduler::styleChanged`).
- **A text editor's caret** glides to where it goes (`motion.fast`).
- In the node editor, links fade in as they are made and out as they go, and the pins a dragged link could end on glow.

**The pointer's shape** (`platform::Cursor`, the system's own shapes) is what the widget under the mouse says
(`Widget::setCursor`, or `cursorAt` for a widget whose parts differ), or its nearest ancestor that says one, the arrow
otherwise; while a widget has the mouse (a button pressed on it), its shape stays wherever the mouse goes. The root works
it out as the mouse moves or a button changes and tells its host (`UiHost::setCursor`, the platform's `setCursor`). Fields
and text editors show an I-beam over their text (the arrow over a text editor's gutter and bar), splitters and docked
stacks' gaps a resize shape along their direction, a table header's edges left and right, a canvas being panned the move
shape, the edges of a borderless window the resize shapes of its sides and corners. A disabled widget shows its parent's.

### Charts

Charts (`Library/Include/GLWidgets/Charts`) are widgets made for data that changes as it comes: a feed of measures, of
events, of shares. Eight for now: **lines** over values or time and stacked areas (`LineChart`), **pies**, donuts and sunbursts
(`PieChart`), **treemaps** (`TreeMap`), **bars**, stacked, grouped or in percent, standing or lying (`BarChart`), **bar
races** (`BarRace`), **heat maps** (`HeatMap`), and for dashboards **sparklines** (`Sparkline`) and **gauges**
(`Gauge`). What they share is in `Chart`:

- **The theme.** `ChartStyle` (`Theme::chart`): the grid, the axes, the labels, the crosshair; ten series colors from the
  color scheme, the accent first, the others told apart by hue and as bright (Dusk, Sand and Turquoise have their own);
  bars and tiles raised as the style's controls are (`barBevel`, `barRadius`: lit from above in Studio, square and flat in
  Plain, reliefs in Plastic, Metal and Fluid, Fluid's rounder; a pie's slices and a gauge's arc the same: lit from above
  with a light line along their top in Studio, flat in Plain, reliefs in Plastic, Metal and Fluid, their corners rounded
  as the bars' are: `Painter::fillArcRelief`'s `corner`, no more than half as far as across the slice there, so that a
  thin one is a capsule).
  A chart may have its own (`setStyle`).
- **Motion, optional.** New data, a new view, a series hidden are eased to (the theme's motions; none in Plain; none when
  `setAnimated(false)`): the axes glide as data comes, bars grow to their new lengths, slices to their new angles, tiles
  from where they were. A change while one is on its way goes on from where it shows, so that data coming at any rate
  moves smoothly rather than jumping.
- **Room.** A chart takes the room it gets and uses all of it: its axes as wide as their labels, as many ticks as their
  labels fit (round values, 1, 2 or 5 times a power of ten; round local times: every 5 seconds, quarter of an hour, at
  midnight, on the first of a month: `niceTicks`, `timeTicks`), its legend in as many rows as it needs, a pie as large as
  its square, tiles squarified. The axes of values and of times are one (`Chart::Axis`: a line chart's x and y, a bar
  chart's values, a race's): their ticks for a range and a length, their labels, the room those take, the grid across
  the plot, the labels beside it, a value as a tooltip shows it; so that every chart's axes look and fit alike.
- **A title, a legend** (a click on an item hides its series, slice or group, eased, and shows it again), **a tooltip** by
  the mouse (the values under it, with their colors).

**Live data.** A line series keeps its points in a `SeriesBuffer`: appended at the end in constant time, the oldest
dropped past a capacity, in chunks of 64 points, each chunk's lowest and highest x and y in an array beside them. Where
an x is is found by halves (by steps that double from where the last search ended, for the columns of a plot one after
another), and the lowest and highest y between two points with the chunks between seen whole. A change only marks the
chart to be painted; once a frame, the chart makes what shows of what of its data shows: the points in its view (by
halves, x sorted), and when there are more of them than columns of pixels, each column as what it shows: a stroke from
its lowest to its highest, and a line on through its first and last points (always forward: a line turning back on
itself would be pinched to nothing at the turn). It looks as all the points would, and costs as much for ten million as
for a thousand: 1.3 ms a frame for a million points, 2.7 ms for ten million, 5.4 ms for four series of a million, a
thousand more appended to each every frame (`glw_benchmarks`, a Release build, 1280 x 720).

**Lines** (`LineChart`, `LineSeries`): an x axis of values or of time (seconds since 1970, `chartTime`; labels in local
time); a series a line, an area fading down under it, steps, dots when its points are far apart; a gap where y is NaN.
It shows all of its data, or its last `window` of x, following what comes (a live chart); its y axis fitted to what
shows (ending on its ticks; with 0 if asked), or a range of its own. The wheel zooms x at the mouse (a live chart: its
window, still following), with Ctrl y; with Shift it pans. A drag pans: a live chart stops following, and follows again
when dragged back to its newest data. The right button drags a rectangle to zoom into; a double click shows what it
showed first. The mouse over it shows a line at the nearest x, a dot on each series there, and their values. Its y axis
is on the start side; x grows to the right in either direction, as time does on a clock's chart.

Its areas (`AreaSeries`, as Qt's `QAreaSeries`) are of a color and an opacity of their own: under a series's line down to
the axis (fading down, if asked), or between two series's lines (a band: a range, a spread around a mean; the two
series' points at any x, the band zipped between them: `Painter::fillBand`). They are drawn under the lines, have their
own legend items, and fade with their series; a series may be an area's edge only (no line of its own; without a name,
in neither the legend nor the tooltip).

**Stacked areas**: series stacked (`LineSeries::Props::stacked`) lie on one another in the order they were added, each
its area filled down to the one under it (fuller at its top), their lines over all of them; the y axis from 0 to the
highest sum that shows, or (`LineChart::Props::percent`, `setPercent`) their shares of their sum at each x, from 0 to
100 %. Their points need not be at the same x: the stack is taken where any of them has a point (each of the others
along its line there; past its ends, its end's value as far as its points are apart, so that a feed a point behind does
not drop out at the edge), or once a column of pixels when they have more points than that. A series hidden by its
legend item eases out of the stack, the ones over it coming down. The mouse shows a dot on each one's top, each one's
value (and share), the highest first, and their total.

**Pies** (`PieChart`): slices of a circle or of a ring (`hole`), a gap apart all along (`ChartStyle::gap`), each with its
share in it where it fits; a donut's hole shows the total, or the value of the slice under the mouse, which comes out a
little. The first time it shows, its slices sweep around into place. A slice may have parts (`PieSlice::children`): rings
further out, each part within its parent's angle (a sunburst, as a disk usage analyzer shows folders), the parts shades
of their parent's color, rings a gap apart. Zooming is going into a slice: a click on one with parts makes it the
whole, eased as a zoomable sunburst is: the part of the turn it had opens to all of it (its siblings squeezed out) and
its rings move in. Rings are further apart than a slice comes out under the mouse (along its middle: a wide slice's far
side comes in as much), so that it goes into neither the ring after it nor the one before; a click in the hole, a right click or Backspace goes back up. The hole then shows its name and total,
the legend its parts. Slices are matched by the labels on their paths, so new data eases each from where it was.
Of many parts (heavy data), only the `top` largest of each slice show (found again as the values change: a partial sort
of each slice's parts), the rest together as one slice of others, a neutral gray, in the legend too (with their count; a
click hides them all); the others' slice grows and shrinks as parts leave the top ones or come into them.

**Treemaps** (`TreeMap`): nodes as rectangles as large as their share of their parent, squarified (Bruls, Huizing and
van Wijk: as near to squares as they can be), a few levels at once (a node with children: its name along its top, its
children in it). The top level takes the series colors, their children shades of them. Zooming is going into a node: a
click on a tile with children, or the wheel turned in over it, makes it fill the chart, its tiles growing out of where
they were; the path to it shows along the top, each name a way back there; a right click, Backspace or the wheel turned
out goes up a level. New data is eased to, each tile from where it was (tiles matched by the names on their paths). Its
labels move with their tiles meanwhile: one shows wholly where it fits in its tile and not at all where it does not, and
one that fitted where its tile was and does not where it goes (or the other way) fades between, cut by its tile.

**Bars** (`BarChart`): series' values in categories, stacked (the negative ones below 0), grouped, or in percent of each
category's whole; standing, or lying (categories down the side). Categories may come as they happen
(`appendCategory`), the oldest dropped past a capacity; a live chart shows its last `window` of them and scrolls on
smoothly as one comes (categories counted from the first ever appended, so that the view moves rather than the bars).
The wheel zooms along the categories, a drag pans them, a double click shows what it showed first; the value axis is
fitted to what shows. A series hidden shrinks out of the stacks.

**Bar races** (`BarRace`): items ranked by their values as bars lying from the start side, the highest at the top; all of
them, a few rows shown at once and the rest scrolled to (the wheel, eased; a drag; the style's scroll bar at the end side,
as a scroll area's: its thumb dragged, its track pressed for a page; with Ctrl the wheel shows more rows or fewer). Or
only the `top` ones, found by a partial sort (n log k), the rest one bar of others under them (gray, their parts' sums,
their count; the scale fitted to the top ones, so that it is cut at the end of the plot when it is longer): one that
comes into the top rises out of the others' bar, one that leaves sinks into it. As the values change (a period after another, shown large
in a corner), the bars slide to their new places and grow or shrink; one going further than a screen of rows comes from
a screen away, the way it comes, not across all of them. The value axis is fitted to the highest bar that shows, its
ticks gliding with the scale, so that the ranks far down are as readable as the first. A bar may be stacked of parts
(what each item's value is made of, colored by part: the legend names them, and a click takes one out of every bar, the
ranks found again). Built for heavy data: a value changes in constant time and only marks the race to be painted; the
items' values, places and ranks are kept side by side in arrays; once a frame all of them are ranked again from the
ranking they had (most move a little if at all: by insertion, nearly linear, within a budget; a sort when they are all
over the place, and at once the next time), and only the rows that show are painted: twenty thousand items of three
parts, all changing, in 2.1 ms a frame (2.5 ms changed at random), two hundred thousand in 31 ms (`glw_benchmarks`). A
change shows over as long as the time since the one before, linearly, so that a race flows on at the pace its data
comes (or over `Props::pace`).

**Sparklines** (`Sparkline`): a measure's recent values at a glance, small (120 x 28 dips when nothing says), without
axes: a line with an area fading under it, or columns from 0 (those under it fainter); a dot on its last value, on its
lowest and highest if asked, a level across it (a target, a limit). Its values are appended (the oldest dropped past its
capacity; until it has as many, they fill it from the left), its range fitted to them and eased there, or its own; the
value under the mouse in a tooltip. More values than it has columns of pixels: each column shows what falls in it, as a
line chart's do, so a million values draw as a line of its width.

**Gauges** (`Gauge`): a value against its range, an arc (270 degrees, open at the bottom; 180 a half circle; or any
sweep) filled to it, raised as the style's bars are: the same ring sectors as a pie's slices (`Chart::paintArc`: a
relief, lit, or flat by the style). Its value large in its middle with its unit under it, its range's ends under the
arc's. Bands along its inside (fine, near a limit, over it) color the value as the band it is in; a needle, if asked,
points at it: tapering from a hub to a round tip, a short tail behind, its two faces lit and shaded by the theme's light,
a shadow under it and a cap over its root raised as the bars are (flat in Plain); the value under it. A new value is eased to.

**Heat maps** (`HeatMap`): a matrix of values as colors, rows (named, the first at the top) across columns (of times, of
values, or named), each cell of its value's color on a `ColorScale`: sequential (from the plot's color through the accent
to a light end; a dark one in the light), diverging (from the second series color through a neutral one at its center to
the accent: its range as far below the center as above), or stops of one's own; linear or by powers of ten; its range its
own, or fitted to the values (exactly: their lowest and highest kept as they come, found again only when a column that
held one goes), out to round values, or up to a quantile of them (a sample's: a few spikes at its end, not all the rest
faint). The scale is a bar beside it with its values; the mouse over the bar leaves only the cells of the values there
(between two of its ticks) in their colors. Columns come as they come (`appendColumn`: the oldest dropped past a capacity;
a live one shows its last window, following), or all at once (`setValues`), a cell changed alone (`setValue`). Large cells
are drawn one by one, round as the style's bars, their values in them when they fit (in the color that reads on them),
eased to new values. Cells smaller than four dips, or more than forty thousand of them, are an image (`ImageStore`, its
pixels sharp): of buckets of columns and rows, as many as a pixel shows rounded up to a power of two, each the highest
of its values (a spike stays seen; or their mean); the buckets of columns are a ring kept from frame to frame (bucket b at
b modulo its width), so that only those that came or changed are made again and uploaded, the view moving and scaling
the image's one or two quads; zoomed by a power of two or panned along the rows, it is made again. Its cost is its
pixels', not its cells': a thousand rows of ten thousand columns, a column appended and a frame in a fraction of a
millisecond (`glw_benchmarks`). The mouse works as on a line chart (the wheel zooms the columns at the mouse, with Ctrl
the rows; a drag pans, a live one following again when dragged back to its newest; the right button drags a rectangle to
zoom into; a double click shows all); the cell under it is outlined (its row and column lit across when they are thin),
its column's label, its row's name and its value in a tooltip; a click on a cell is told (`setOnCellClicked`).

`chart_demo` shows them all, fed live: a race of 500 packages' monthly downloads by system (`--race N` of them), three series at 200 points a second each over a minute (and ten minutes of
history to pan into; an area under one, a band of the range around another), shares that drift in rings (regions, their
countries, a few cities), a second's counts appended every second, a project's files changing now and
then, a dashboard of the measures now (gauges, sparklines), heat maps of 200 endpoints' latency (a column a second,
incidents and spikes in it, by powers of ten) and of requests by hour and weekday, a network's traffic by protocol stacked
(in shares, if asked); the style, the colors, the motion and the bars' layout chosen above them.

Still to come: more kinds (scatter, candlesticks, histograms; calendars and densities of points as heat maps), logarithmic axes, a
second y axis, Persian digits and the Solar Hijri calendar on axes (with the `Locale` of
[Localization](#localization-and-right-to-left)).

Still to come in the widget core: the native extras of the chrome (Windows 11 shadow, rounded corners and snap layouts).

## Docking

*Done: in a window and across windows, with global panels and saved workspaces (`Widgets/Docking.hpp`,
`Widgets/MajorTabs.hpp`, `Widgets/DockingHost.hpp`, `Widgets/Workspace.hpp`), in the full mode and Wayland's restricted
one. Wayland's drag-and-drop between windows: later.*

Two kinds of tabs, as in Slate:

- **Major tabs** are documents or workspaces: a script, a level, a chart sheet. They live only in the tab strip at the top
  of a window, which is the window's title bar, as in Chrome or Firefox. Dragging a major tab out of its strip makes a new
  window with it. Dropping it on another window's strip moves it there. Dragging the only tab of a window drags the window,
  and so does dragging a window by its strip, all its tabs merging into the strip it is dropped on: windows merge and
  split by their tabs.
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
- **A tab's menu** (the right button on it): Float (or Dock, in a floating window; with a docking host), Hide the tab bar,
  Close (when the panel can be closed).
- **A stack of one panel can hide its tab bar** (by its tab's menu, `DockTabs::setTabBarHidden`), as Unreal Engine's "hide
  tab well": the bar slides up out of the stack (the theme's normal motion, 200 ms) and the panel takes all of it; a little
  tongue hanging from its top edge at its start shows the bar again, sliding back. The tongue is drawn as the style's
  tabs are (`DockStyle::tabBevel`: raised, flat or a relief); it comes out as the bar goes and goes back in as it comes,
  and under the mouse it grows longer and wider (eased), tinted toward the accent, a chevron down in it. It comes back by itself when another panel joins the
  stack; a floating window keeps its bar, which is its title. Saved layouts keep it.
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
  that window. So does a window moved by its title bar: the system moves it (snapped to the screen's edges, maximized
  by a double click, as any window), and the application follows (`Platform::setMoveHandler`, told of each move, from
  inside the system's loop of moving too; `Platform::mouseHeld`): a move with the mouse held over its caption is a drag
  of the window, which is seen through as it goes, the strip under the mouse showing where its tabs would go
  (`MajorTabStrip::systemMoved`, drawn from inside the loop); let go over another window's strip, all its tabs go there
  in order, the one it showed shown, and it closes (`systemMoveEnded`). A window left without tabs closes.
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
| M6 | Validation: a node canvas and a code editor (FlowGraph), charts, an engine embedding example | In progress: the node canvas (`Canvas`, `node_editor`), menus, popups and dialogs, the code editor (`TextEdit`, `code_editor`), charts (lines, pies, treemaps, bars, races, heat maps, sparklines, gauges: `chart_demo`) |

## Open questions

- Where GPU support for older machines would be needed (no Vulkan 1.0 / Direct3D 12): if it ever is, an OpenGL backend.
- Text on the node canvas: glyphs rasterized at each zoom level (as now) or MSDF. Measured with the canvas (M6): the wheel
  zooms by steps, so a few sizes are rasterized and kept, and painting stays well under a millisecond; MSDF would matter
  for zoom animated without steps, which would fill the atlas with sizes.
- Whether ICU is needed at all, or the small `Locale` implementation is enough.
