// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A window of the system. GLWidgets draws everything in it with the GPU (rhi::Surface): the window itself has no content of the
// system's. Sizes are in dips (Geometry.hpp), so a window has the same physical size on every screen.
// Made by Platform::createWindow; destroying it closes it. It must not outlive its platform, nor a surface its window.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Platform/Event.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace glw::platform {

class Window;

struct WindowDesc {
    std::string title;              // UTF-8
    Size size{800, 600};
    bool resizable = true;
    // No title bar or borders of the system: the application draws them, and says with setHitTest where they are.
    bool borderless = false;
    // Its top left corner on the screen (screen coordinates, Window::position). None: centered.
    std::optional<Point> position;
    // A tool window: not in the task bar; above its parent (setParent).
    bool tool = false;
    // Shown above every window, never given the focus (a drop preview while a tab is dragged).
    bool overlay = false;
    // A popup of this window (a menu, a list to choose from, a tooltip): no frame, not in the task bar, above its parent,
    // hidden and destroyed with it (destroy it first). `position` is from the parent's top left corner, in the parent's
    // dips; the system keeps it on the screen. A menu has the keyboard while it is the topmost; a tooltip takes no input.
    Window* popupOf = nullptr;
    bool tooltip = false;
};

// What a point of a window is, for the system to move and resize it (Window::setHitTest).
enum class HitArea : std::uint8_t {
    Client,                 // the application's: what it draws and handles, the window buttons too
    Caption,                // drags the window; a double click maximizes or restores it
    ResizeTopLeft, ResizeTop, ResizeTopRight, ResizeRight, ResizeBottomRight, ResizeBottom, ResizeBottomLeft, ResizeLeft,
};

// The shape of the mouse's pointer (Platform::setCursor): the system's own.
enum class Cursor : std::uint8_t {
    Arrow,
    Text,                   // an I-beam: over text that can be selected or typed in
    Pointer,                // a hand: over a link
    Move,                   // what is dragged moves, any way
    ResizeHorizontal,       // left and right: a vertical splitter's handle, a window's side
    ResizeVertical,         // up and down
    ResizeDiagonal,         // top left and bottom right
    ResizeAntiDiagonal,     // top right and bottom left
    NotAllowed,
    Wait,
    Crosshair,
};
inline constexpr std::size_t cursorCount = static_cast<std::size_t>(Cursor::Crosshair) + 1;

// Called by the system with a point of the window (dips), whenever it needs to know - often: it must be quick.
using HitTest = std::function<HitArea(Point)>;

class GLW_EXPORT Window {
public:
    virtual ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    virtual WindowId id() const = 0;

    virtual void setTitle(std::string_view title) = 0;      // UTF-8

    virtual Size size() const = 0;
    virtual void setSize(Size size) = 0;
    virtual PixelSize pixelSize() const = 0;
    virtual float scale() const = 0;                        // pixels per dip

    // A new window is hidden: show it once what it shows is ready. Not given the focus, when `focus` is false (a window
    // that appears under a drag must not take it from the window being dragged from).
    virtual void show(bool focus = true) = 0;
    virtual void hide() = 0;
    virtual bool visible() const = 0;
    // Above the other windows, and with the focus.
    virtual void raise() = 0;
    // It has the keyboard.
    virtual bool focused() const = 0;
    // A tool window stays above `parent` (and goes with it); none: on its own.
    virtual void setParent(Window* parent) = 0;
    // From 0 (not seen) to 1 (opaque), where the system can (not on Wayland).
    virtual void setOpacity(float opacity) = 0;

    // Screen coordinates: the system's, for the whole desktop (pixels on Windows and X11, points on macOS). Its top left
    // corner (of the whole window), and from its dips to the screen and back.
    virtual Point position() const = 0;
    virtual void setPosition(Point position) = 0;
    virtual Point toScreen(Point point) const = 0;
    virtual Point fromScreen(Point point) const = 0;
    // Its size in screen coordinates.
    virtual Size screenSize() const = 0;
    // The part of the screen it is on that windows may cover (without the task bar), screen coordinates.
    virtual Rect usableScreenArea() const = 0;

    virtual WindowState state() const = 0;
    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;             // from minimized or maximized

    // What each point of a borderless window is. None (the default): all of it is client area.
    virtual void setHitTest(HitTest hitTest) = 0;

    // Text input: TextInput and TextComposition events, and the input method of the system (the on-screen keyboard...). On while
    // a text field has the focus, off otherwise: with it on, keys may type text instead of being shortcuts.
    virtual void startTextInput() = 0;
    virtual void stopTextInput() = 0;
    // Where text is typed (dips): the input method puts its candidate window by it.
    virtual void setTextInputArea(const Rect& area) = 0;

protected:
    Window() = default;
};

} // namespace glw::platform
