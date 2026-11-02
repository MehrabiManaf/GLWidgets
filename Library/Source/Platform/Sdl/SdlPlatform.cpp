// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/SdlPlatform.hpp"

#include "Platform/Sdl/SdlKeys.hpp"
#include "Platform/Sdl/SdlWindow.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <utility>

namespace glw::platform {

std::unique_ptr<Platform> createPlatform() {
    return std::make_unique<sdl::SdlPlatform>();
}

namespace sdl {

namespace {

std::atomic<bool> platformExists{false};

std::string sdlError(const char* what) {
    return std::string(what) + ": " + SDL_GetError();
}

} // namespace

SdlPlatform::SdlPlatform() {
    if (platformExists.exchange(true))
        throw Error("there is a platform already: one per process");
    // closing the last window is the application's business (QuitRequested, Event.hpp)
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::string error = sdlError("SDL_Init");
        platformExists = false;
        throw Error(error);
    }
    wakeUpEvent_ = SDL_RegisterEvents(1);

    const int version = SDL_GetVersion();
    const char* driver = SDL_GetCurrentVideoDriver();
    name_ = "SDL " + std::to_string(SDL_VERSIONNUM_MAJOR(version)) + "." + std::to_string(SDL_VERSIONNUM_MINOR(version)) + "." +
            std::to_string(SDL_VERSIONNUM_MICRO(version)) + " (" + (driver ? driver : "no video driver") + ")";
}

SdlPlatform::~SdlPlatform() {
    GLW_ASSERT(windows_ == 0);
    if (watching_)
        SDL_RemoveEventWatch(&SdlPlatform::watch, this);
    SDL_Quit();
    platformExists = false;
}

std::unique_ptr<Window> SdlPlatform::createWindow(const WindowDesc& desc) {
    return std::make_unique<SdlWindow>(*this, desc);
}

bool SdlPlatform::pollEvent(Event& event) {
    SDL_Event in;
    while (SDL_PollEvent(&in)) {
        if (translate(in, event))
            return true;
    }
    return false;
}

void SdlPlatform::waitForEvents(std::optional<std::chrono::milliseconds> timeout) {
    Sint32 ms = -1;
    if (timeout)
        ms = static_cast<Sint32>(std::clamp<std::int64_t>(timeout->count(), 0, INT32_MAX));
    // without an event to fill, SDL leaves it in the queue for pollEvent
    SDL_WaitEventTimeout(nullptr, ms);
}

void SdlPlatform::wakeUp() {
    SDL_Event event{};
    event.type = wakeUpEvent_;
    SDL_PushEvent(&event);
}

void SdlPlatform::setExposeHandler(std::function<void(WindowId)> handler) {
    exposeHandler_ = std::move(handler);
    if (exposeHandler_ && !watching_)
        watching_ = SDL_AddEventWatch(&SdlPlatform::watch, this);
}

bool SDLCALL SdlPlatform::watch(void* data, SDL_Event* event) {
    auto* self = static_cast<SdlPlatform*>(data);
    // SDL sends an exposure from inside the modal loops of resizing and moving: drawn right there
    if (event->type == SDL_EVENT_WINDOW_EXPOSED && self->exposeHandler_) {
        try {
            self->exposeHandler_(event->window.windowID);
        } catch (...) {
            // called from inside SDL (and the system): nothing may be thrown through it
        }
    }
    return true;    // the event stays in the queue
}

bool SdlPlatform::translate(const SDL_Event& in, Event& out) const {
    switch (in.type) {
    case SDL_EVENT_QUIT:
        out = QuitRequested{};
        return true;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        out = WindowCloseRequested{in.window.windowID};
        return true;
    // the size in dips is that in pixels over the scale: SDL's own size (SDL_EVENT_WINDOW_RESIZED) is in its window coordinates
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        const float scale = SdlWindow::scaleOf(SDL_GetWindowFromID(in.window.windowID));
        const PixelSize pixels{in.window.data1, in.window.data2};
        out = WindowResized{in.window.windowID, Size{pixels.width / scale, pixels.height / scale}, pixels};
        return true;
    }
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        out = WindowScaleChanged{in.window.windowID, SdlWindow::scaleOf(SDL_GetWindowFromID(in.window.windowID))};
        return true;
    case SDL_EVENT_WINDOW_EXPOSED:
        out = WindowExposed{in.window.windowID};
        return true;
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
        out = WindowStateChanged{in.window.windowID, SdlWindow::stateOf(SDL_GetWindowFromID(in.window.windowID))};
        return true;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        out = WindowFocusChanged{in.window.windowID, in.type == SDL_EVENT_WINDOW_FOCUS_GAINED};
        return true;
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        out = MouseCrossed{in.window.windowID, in.type == SDL_EVENT_WINDOW_MOUSE_ENTER};
        return true;

    case SDL_EVENT_MOUSE_MOTION: {
        const float f = SdlWindow::dipsPerUnit(SDL_GetWindowFromID(in.motion.windowID));
        out = MouseMoved{in.motion.windowID, Point{in.motion.x * f, in.motion.y * f}};
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const auto button = toMouseButton(in.button.button);
        if (!button)
            return false;
        const float f = SdlWindow::dipsPerUnit(SDL_GetWindowFromID(in.button.windowID));
        out = MouseButtonChanged{in.button.windowID, *button, in.button.down, in.button.clicks,
                                 Point{in.button.x * f, in.button.y * f}};
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        const float f = SdlWindow::dipsPerUnit(SDL_GetWindowFromID(in.wheel.windowID));
        const float sign = in.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;     // "natural" scrolling
        out = MouseWheelScrolled{in.wheel.windowID, Point{in.wheel.x * sign, in.wheel.y * sign},
                                 Point{in.wheel.mouse_x * f, in.wheel.mouse_y * f}};
        return true;
    }

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        out = KeyChanged{in.key.windowID, toKey(in.key.key), toModifiers(in.key.mod), in.key.down, in.key.repeat};
        return true;
    case SDL_EVENT_TEXT_INPUT:
        out = TextInput{in.text.windowID, in.text.text ? in.text.text : ""};
        return true;
    case SDL_EVENT_TEXT_EDITING:
        out = TextComposition{in.edit.windowID, in.edit.text ? in.edit.text : "", in.edit.start, in.edit.length};
        return true;

    default:
        // wakeUp's event, and what GLWidgets does not report (yet)
        return false;
    }
}

} // namespace sdl

} // namespace glw::platform
