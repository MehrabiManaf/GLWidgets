// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The platform on SDL3. Its events are SDL's turned into GLWidgets' (Event.hpp): SDL's window coordinates are pixels on some
// systems (Windows) and points on others (macOS, Wayland), what comes out is in dips everywhere.
#include <GLWidgets/Platform/Platform.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <array>

#include <string>

namespace glw::platform::sdl {

class SdlPlatform final : public Platform {
public:
    SdlPlatform();
    ~SdlPlatform() override;

    std::string_view name() const override { return name_; }
    std::unique_ptr<Window> createWindow(const WindowDesc& desc) override;
    bool pollEvent(Event& event) override;
    void waitForEvents(std::optional<std::chrono::milliseconds> timeout) override;
    void wakeUp() override;
    void setExposeHandler(std::function<void(WindowId)> handler) override;
    bool placesWindows() const override;
    Point mousePosition() const override;
    void captureMouse(bool capture) override;
    void setCursor(Cursor cursor) override;
    std::string clipboardText() const override;
    void setClipboardText(std::string_view text) override;

    // SdlWindow: the windows there are, which must all be gone before the platform
    void windowCreated() { ++windows_; }
    void windowDestroyed() { --windows_; }

private:
    bool translate(const SDL_Event& in, Event& out) const;      // false for an event GLWidgets does not report
    // SDL's event watch: it sees events as they are made, from inside the system's loops too
    static bool SDLCALL watch(void* data, SDL_Event* event);

    std::string name_;
    Uint32 wakeUpEvent_ = 0;
    int windows_ = 0;
    std::function<void(WindowId)> exposeHandler_;
    bool watching_ = false;
    std::array<SDL_Cursor*, cursorCount> cursors_{};       // made the first time they are shown
    Cursor cursor_ = Cursor::Arrow;
};

} // namespace glw::platform::sdl
