# GLWidgets

A framework for designing and building graphical user interfaces, in C++20, with everything drawn by the GPU (Vulkan,
Direct3D 12 or Metal) on Windows, Linux and macOS.

One of its goals is to be complete enough to build the script editor of
[FlowGraph](https://github.com/MehrabiManaf/FlowGraph) with it, as a second version of the editor that does not use Qt.
So it has to handle Persian and other right-to-left text, localization, and docking. After that it is meant for user
interfaces inside 3D applications and games, and for charts. Every dependency is permissively licensed.

The project has just started. Windows (with chrome of their own, redrawn while they are resized), events, the GPU layer, the
2D painter, text (Persian and English, mixed, shaped, wrapped, selectable), the widget core (layout, events, focus,
right-to-left mirroring), themes and translations work, with the basic widgets: label, button, check box, text field,
panel, card, scroll area, splitter; and docking, with major tabs in the title bar: tabs and panels move between
windows, tear off into windows of their own, and float (on Wayland, where windows are placed by the compositor, within
their window and into windows of their own); and charts fed live (lines over time, pies, treemaps, stacked bars),
zoomed and panned, drawn from what shows so that millions of points cost what thousands do. The design,
and what comes next, is in
[Docs/Architecture.md](Docs/Architecture.md); the libraries it is built with, and their notices, in
[Docs/ThirdPartyNotices.md](Docs/ThirdPartyNotices.md).

## Building

You need CMake 3.24 or newer, Ninja, and a C++20 compiler (MSVC 2022, GCC 13, Clang 17 / Apple Clang 16). The libraries
(SDL3, FreeType, HarfBuzz, SheenBidi, libunibreak) are downloaded, checked and built by CMake.

```sh
cmake --preset windows-release        # Windows, from a Visual Studio x64 developer prompt
cmake --preset unix-release           # Linux, macOS
cmake --build --preset <the same preset>
```

On Linux, SDL3 needs the development packages of X11, Wayland and IBus: see `.github/workflows/ci.yml` for the list.

Then, in `build/<preset>/bin`:

- `docking_demo [--style studio|fluid|plain|plastic|metal] [--colors dark|light|dusk|sand|turquoise]` is laid out as a script editor: scripts as major tabs in the title bar (+ adds one), each with its
  panels (graph, palette, inspector, output, search) docked around it; drag the tabs to rearrange them. A script's tab
  dragged out of the title bar opens in a window of its own, and dragged onto another window's title bar goes there; a
  panel's tab dragged out of its tab bar floats at once, and its window, dragged by its tab or its title onto a stack's
  guides, docks again. Around the scripts are the application's own panels (files, log), the same whichever script is
  shown, in any window (the files as a tree). `--workspace FILE` saves the windows and everything in them when it ends,
  and starts with them again; `--light` starts it in the light colors (as `--colors light`).
- `widgets_demo [--style ...] [--colors ...]` is an application made of widgets, written with the declarative builders (`glw::ui`): a window with its
  own chrome, split in two by a handle, a form (text field, check boxes, buttons; Tab moves the focus) and, over each
  other, a list of a million items (sorted by a click on its header), a tree of a thousand folders of a thousand files
  in three columns (all open: a million rows, eased as they open and close; a header clicked sorts each folder's files)
  and a table of a million rows of fifty columns, sorted by a click on a header, filtered by a field
  and edited in place (only what shows is widgets); a menu
  (with a submenu, check marks, shortcuts), a combo box, dialogs (a form; the Close button asks first), and panels that
  open and close, eased (a + in one's header adds a row); a button switches to Persian (the window is mirrored), combo
  boxes to another style and other colors (Dark, Light, Dusk, Sand, Turquoise), eased. `--language fa` starts it so.
- `node_editor [--style ...] [--colors ...]` is a node editor on a `Canvas`: nodes with pins and fields for their values, links drawn as curves,
  dragged from pin to pin; nodes moved, selected by a rectangle, framed (F), deleted; comments that carry their nodes;
  a context menu (the right button) to add nodes by category, and a palette to add one by its name (Space, Tab, or a
  link let go on the empty canvas: then only the nodes that have a pin for it, the one added linked).
  The wheel zooms, the middle or right button pans. `--nodes N` adds N more (thousands: only what shows is painted),
  `--rtl` mirrors it.
- `code_editor [--style ...] [--colors ...]` edits C++ (highlighted, line numbers, the system's monospace font) over a log it appends to; Persian in
  strings and comments is shaped in lines that stay left to right. Its menu bar (in the title bar; `--menu-below` puts it
  under it) and toolbar share their actions, with SVG icons and shortcuts. `code_editor FILE` edits a file, `--lines N` adds N
  lines (a million: only what shows is laid out).
- `chart_demo [--rate N] [--race N] [--style ...] [--colors ...]` shows charts fed live: a bar race of `--race` packages'
  monthly downloads, stacked by system (all of them, scrolled; or the top ten, the rest one bar of others); CPU, memory and network over the last
  minute (`--rate` points a second each, ten minutes of history to zoom out and pan into; an area under CPU, a band of
  memory's range), a donut of shares that drift in rings (regions, countries, cities: a click goes into a slice, a click
  in the hole out),
  stacked bars of each second's counts, a treemap of a project's files (a click goes into a folder, a right click out),
  a dashboard of gauges (colored by bands; a needle) and sparklines of the measures now, heat maps of 200 endpoints'
  latency (live, a column a second) and of requests by hour and weekday, traffic by protocol as stacked areas (in shares
  with Shares), a price as candles made of live trades (1s to 5m, by the buttons on the chart), with its volume and two moving
  averages (a click on the legend hides each), a live histogram of requests' latencies (their quantiles and cumulative
  share) and box plots of eight services' latencies, requests' sizes against their latencies as a scatter plot (80000
  points as their density, dots when zoomed in, a trend line), and a profiled program: its threads' work as a live
  timeline and its samples as a flame graph (a click zooms into a frame, the Find box highlights a function), and a
  Sankey diagram of where requests flow, its values drifting.
  The wheel zooms, a drag pans (back to the newest, it follows again), the right button drags a rectangle to zoom into,
  a double click shows all; a legend's item hides its series. Each chart is a docked panel (moved, split, floated, closed
  by its tab); the View menu hides and shows each.
- `hello_window` opens two windows drawn by one GPU device.
- `GLW_LOG_FRAMES=1` makes any of them say how many frames it draws, each second that has some (idle, none), how far
  apart, and how many came late (a refresh missed).
- `glw_benchmarks` times the widget layer with many widgets (laid out, painted, scrolled, a text changed; charts of millions of points), without a
  window: run it from a Release build to compare a change against what was.
- `paint_demo` opens a borderless window with its own title bar, tabs and window buttons, and shows what the painter
  draws: a chart, shapes, a node canvas (the mouse wheel zooms it), text in Persian and English (drag to select).
- `glw_tests` runs the tests (`ctest --preset <preset>`).

The shaders are compiled ahead of time and committed (`Library/Source/Paint/PainterShaders.hpp`). To change one, edit
`Library/Shaders/*.hlsl` and run `python Tools/CompileShaders.py`, which says which compilers it needs.

## License

MIT, see [LICENSE](LICENSE).
