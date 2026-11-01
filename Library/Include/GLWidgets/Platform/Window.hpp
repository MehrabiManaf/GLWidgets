// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A window of the system. GLWidgets draws everything in it with the GPU (rhi::Surface): the window itself has no content of the
// system's. Sizes are in dips (Geometry.hpp), so a window has the same physical size on every screen.
// Made by Platform::createWindow; destroying it closes it. It must not outlive its platform, nor a surface its window.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Platform/Event.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace glw::platform {

struct WindowDesc {
    std::string title;              // UTF-8
    Size size{800, 600};
    bool resizable = true;
    // No title bar or borders of the system: the application draws them, and says with setHitTest where they are.
    bool borderless = false;
};

// What a point of a window is, for the system to move and resize it (Window::setHitTest).
enum class HitArea : std::uint8_t {
    Client,                 // the application's: what it draws and handles, the window buttons too
    Caption,                // drags the window; a double click maximizes or restores it
    ResizeTopLeft, ResizeTop, ResizeTopRight, ResizeRight, ResizeBottomRight, ResizeBottom, ResizeBottomLeft, ResizeLeft,
};

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

    // A new window is hidden: show it once what it shows is ready.
    virtual void show() = 0;
    virtual void hide() = 0;

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

protected:
    Window() = default;
};

} // namespace glw::platform
