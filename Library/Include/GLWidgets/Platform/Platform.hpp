// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The system GLWidgets runs on: its windows and their events. The rest of GLWidgets uses only this interface; the
// implementation is SDL3 (Source/Platform/Sdl) and stays behind it, so what SDL3 cannot do can be done with the system's own
// API without changing the users of Platform.
//
// One platform per process, made and used on the main thread (macOS requires it), as everything of the user interface.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Platform/Event.hpp>
#include <GLWidgets/Platform/Window.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace glw::platform {

class GLW_EXPORT Platform {
public:
    virtual ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    // What runs below, for logs and bug reports: "SDL 3.4.18 (windows)", "SDL 3.4.18 (wayland)"...
    virtual std::string_view name() const = 0;

    virtual std::unique_ptr<Window> createWindow(const WindowDesc& desc) = 0;

    // The next event, without waiting. False when there is none.
    virtual bool pollEvent(Event& event) = 0;

    // Waits until there is an event to poll, at most `timeout` (no timeout: as long as it takes), or until wakeUp.
    virtual void waitForEvents(std::optional<std::chrono::milliseconds> timeout = std::nullopt) = 0;

    // Ends waitForEvents. The only function of a platform that may be called from any thread.
    virtual void wakeUp() = 0;

    // Called when a window must be drawn now, even from inside the system's own loops: while the user resizes or moves a
    // window (Windows, macOS), pollEvent and waitForEvents do not return until the mouse is let go, and the window would
    // show nothing new meanwhile. Also called for windows exposed otherwise (from pollEvent then). None: not called.
    virtual void setExposeHandler(std::function<void(WindowId)> handler) = 0;

protected:
    Platform() = default;
};

// The platform of this system. Throws Error when it cannot start (no display...) or when there is a platform already.
GLW_EXPORT std::unique_ptr<Platform> createPlatform();

} // namespace glw::platform
