# GLWidgets

A framework for designing and building graphical user interfaces, in C++20, with everything drawn by the GPU (Vulkan,
Direct3D 12 or Metal) on Windows, Linux and macOS.

One of its goals is to be complete enough to build the script editor of
[FlowGraph](https://github.com/MehrabiManaf/FlowGraph) with it, as a second version of the editor that does not use Qt.
So it has to handle Persian and other right-to-left text, localization, and docking. After that it is meant for user
interfaces inside 3D applications and games, and for charts. Every dependency is permissively licensed.

The project has just started: windows (with chrome of their own), events, the GPU layer and the 2D painter work. Text and
widgets do not exist yet. The design, and what comes next, is in [Docs/Architecture.md](Docs/Architecture.md).

## Building

You need CMake 3.24 or newer, Ninja, and a C++20 compiler (MSVC 2022, GCC 13, Clang 17 / Apple Clang 16). SDL3 is
downloaded and built by CMake.

```sh
cmake --preset windows-release        # Windows, from a Visual Studio x64 developer prompt
cmake --preset unix-release           # Linux, macOS
cmake --build --preset <the same preset>
```

On Linux, SDL3 needs the development packages of X11, Wayland and IBus: see `.github/workflows/ci.yml` for the list.

Then, in `build/<preset>/bin`:

- `hello_window` opens two windows drawn by one GPU device.
- `paint_demo` opens a borderless window with its own title bar, tabs and window buttons, and shows what the painter
  draws: a chart, shapes, a node canvas (the mouse wheel zooms it).

The shaders are compiled ahead of time and committed (`Library/Source/Paint/PainterShaders.hpp`). To change one, edit
`Library/Shaders/*.hlsl` and run `python Tools/CompileShaders.py`, which says which compilers it needs.

## License

MIT, see [LICENSE](LICENSE).
