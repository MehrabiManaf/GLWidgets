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
their window and into windows of their own). The design,
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

- `docking_demo` is laid out as a script editor: scripts as major tabs in the title bar (+ adds one), each with its
  panels (graph, palette, inspector, output, search) docked around it; drag the tabs to rearrange them. A script's tab
  dragged out of the title bar opens in a window of its own, and dragged onto another window's title bar goes there; a
  panel's tab dragged out of its tab bar floats at once, and its window, dragged by its tab or its title onto a stack's
  guides, docks again. Around the scripts are the application's own panels (files, log), the same whichever script is
  shown, in any window. `--workspace FILE` saves the windows and everything in them when it ends, and starts with them
  again.
- `widgets_demo` is an application made of widgets, written with the declarative builders (`glw::ui`): a window with its
  own chrome, split in two by a handle, a form (text field, check boxes, buttons; Tab moves the focus) and a long list,
  each scrolling; buttons switch to Persian (the window is mirrored) and to the light theme. `--language fa` and
  `--light` start it so.
- `hello_window` opens two windows drawn by one GPU device.
- `glw_benchmarks` times the widget layer with many widgets (laid out, painted, scrolled, a text changed), without a
  window: run it from a Release build to compare a change against what was.
- `paint_demo` opens a borderless window with its own title bar, tabs and window buttons, and shows what the painter
  draws: a chart, shapes, a node canvas (the mouse wheel zooms it), text in Persian and English (drag to select).
- `glw_tests` runs the tests (`ctest --preset <preset>`).

The shaders are compiled ahead of time and committed (`Library/Source/Paint/PainterShaders.hpp`). To change one, edit
`Library/Shaders/*.hlsl` and run `python Tools/CompileShaders.py`, which says which compilers it needs.

## License

MIT, see [LICENSE](LICENSE).
