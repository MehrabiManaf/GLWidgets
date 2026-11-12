// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/SdlPlatform.hpp"

#include "Platform/Sdl/SdlKeys.hpp"
#include "Platform/Sdl/SdlWindow.hpp"
#ifdef GLW_HAVE_WAYLAND
#include "Platform/Sdl/WaylandDrag.hpp"
#endif

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string_view>
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
    // a click on a window of the application that is not in front acts too (a tab of it, after a floating panel's window)
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::string error = sdlError("SDL_Init");
        platformExists = false;
        throw Error(error);
    }
    wakeUpEvent_ = SDL_RegisterEvents(1);
#ifdef GLW_HAVE_WAYLAND
    wayland_ = WaylandDrag::make();         // (none on another video driver)
#endif

    const int version = SDL_GetVersion();
    const char* driver = SDL_GetCurrentVideoDriver();
    name_ = "SDL " + std::to_string(SDL_VERSIONNUM_MAJOR(version)) + "." + std::to_string(SDL_VERSIONNUM_MINOR(version)) + "." +
            std::to_string(SDL_VERSIONNUM_MICRO(version)) + " (" + (driver ? driver : "no video driver") + ")";
}

SdlPlatform::~SdlPlatform() {
    GLW_ASSERT(windows_ == 0);
#ifdef GLW_HAVE_WAYLAND
    wayland_.reset();       // (its objects on SDL's connection: before SDL goes)
#endif
    if (watching_)
        SDL_RemoveEventWatch(&SdlPlatform::watch, this);
    for (SDL_Cursor* cursor : cursors_) {
        if (cursor)
            SDL_DestroyCursor(cursor);
    }
    SDL_Quit();
    platformExists = false;
}

std::unique_ptr<Window> SdlPlatform::createWindow(const WindowDesc& desc) {
    return std::make_unique<SdlWindow>(*this, desc);
}

bool SdlPlatform::pollEvent(Event& event) {
#ifdef GLW_HAVE_WAYLAND
    // a drag carried by Wayland between the windows: its events (read with SDL's) first
    if (wayland_)
        wayland_->dispatch(pending_);
#endif
    if (!pending_.empty()) {
        event = std::move(pending_.front());
        pending_.pop_front();
        return true;
    }
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
#ifdef GLW_HAVE_WAYLAND
    // while Wayland carries a drag: awake often (SDL's wait does not end for the events of its queue)
    if (wayland_ && wayland_->active())
        ms = ms < 0 ? 10 : std::min<Sint32>(ms, 10);
#endif
    if (!pending_.empty())
        ms = 0;
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

void SdlPlatform::setMoveHandler(std::function<void(WindowId)> handler) {
    moveHandler_ = std::move(handler);
    if (moveHandler_ && !watching_)
        watching_ = SDL_AddEventWatch(&SdlPlatform::watch, this);
}

bool SdlPlatform::startDrag(Window& from) {
#ifdef GLW_HAVE_WAYLAND
    auto* window = dynamic_cast<SdlWindow*>(&from);
    return wayland_ && window && wayland_->start(window->sdlWindow());
#else
    (void)from;
    return false;
#endif
}

bool SdlPlatform::placesWindows() const {
    const char* driver = SDL_GetCurrentVideoDriver();
    return !driver || std::string_view(driver) != "wayland";
}

Point SdlPlatform::mousePosition() const {
    float x = 0, y = 0;
    SDL_GetGlobalMouseState(&x, &y);
    return {x, y};
}

bool SdlPlatform::mouseHeld() const {
    return (SDL_GetGlobalMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
}

void SdlPlatform::captureMouse(bool capture) {
    SDL_CaptureMouse(capture);
}

namespace {

// A drag's pointer (Cursor::DragCopy, DragMove, DragLink): the arrow, and by it a small box with its mark, drawn here (SDL's
// system shapes have none of them) at `scale` times 32 pixels, smoothed (each pixel sampled four by four).
SDL_Cursor* dragCursor(Cursor cursor, float scale) {
    const int size = std::max(32, static_cast<int>(std::lround(32 * scale)));
    const float k = static_cast<float>(size) / 32;
    SDL_Surface* surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_ARGB8888);
    if (!surface)
        return nullptr;
    // the arrow: its outline (in 32-pixel units, its tip at the top left)
    static constexpr float arrow[][2] = {{1, 1}, {1, 18}, {5.2f, 14.2f}, {8, 20.5f}, {10.8f, 19.3f}, {8.1f, 13.1f}, {13.6f, 13.1f}};
    const auto inArrow = [](float x, float y, float grow) {
        // inside the polygon grown by `grow` (its edges moved out): by the distance to each edge, its side by the winding
        bool inside = false;
        float nearest = 1e9f;
        const int n = 7;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            const float xi = arrow[i][0], yi = arrow[i][1], xj = arrow[j][0], yj = arrow[j][1];
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
                inside = !inside;
            const float ex = xj - xi, ey = yj - yi;
            const float t = std::clamp(((x - xi) * ex + (y - yi) * ey) / (ex * ex + ey * ey), 0.0f, 1.0f);
            nearest = std::min(nearest, std::hypot(x - (xi + ex * t), y - (yi + ey * t)));
        }
        return inside || nearest <= grow;
    };
    // the box: lower right of the arrow; its mark in it
    const float bx = 15, by = 17, bw = 13, bh = 12;
    const auto inBox = [&](float x, float y, float grow) {
        return x >= bx - grow && x <= bx + bw + grow && y >= by - grow && y <= by + bh + grow;
    };
    const auto inMark = [&](float x, float y) {
        const float cx = bx + bw / 2, cy = by + bh / 2;
        switch (cursor) {
        case Cursor::DragCopy:      // a plus
            return (std::abs(x - cx) <= 0.9f && std::abs(y - cy) <= 3.6f) || (std::abs(y - cy) <= 0.9f && std::abs(x - cx) <= 3.6f);
        case Cursor::DragLink: {    // a bent arrow up to the right: its stem, its head
            const bool stem = (std::abs(x - (bx + 4)) <= 0.9f && y >= cy - 1 && y <= by + bh - 2.5f) ||
                              (std::abs(y - (cy - 1)) <= 0.9f && x >= bx + 4 && x <= bx + bw - 3.5f);
            const bool head = x >= bx + bw - 6 && x <= bx + bw - 2.5f && std::abs(y - (cy - 1)) <= (bx + bw - 2.5f - x) * 0.9f;
            return stem || head;
        }
        default:                    // moved: a smaller box in it
            return (x >= cx - 3.2f && x <= cx + 3.2f && y >= cy - 2.6f && y <= cy + 2.6f) &&
                   !(x >= cx - 1.8f && x <= cx + 1.8f && y >= cy - 1.2f && y <= cy + 1.2f);
        }
    };
    auto* pixels = static_cast<std::uint32_t*>(surface->pixels);
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            float a = 0, white = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float x = (static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / 4) / k;
                    const float y = (static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / 4) / k;
                    // white inside, a black edge around (the arrow's, the box's), the mark black
                    const bool body = inArrow(x, y, 0) || inBox(x, y, -1);
                    const bool edge = inArrow(x, y, 1.0f) || inBox(x, y, 0);
                    if (body && !inMark(x, y)) {
                        a += 1;
                        white += 1;
                    } else if (edge) {
                        a += 1;
                    }
                }
            }
            a /= 16;
            const float v = a > 0 ? white / 16 / a : 0;
            const auto c = static_cast<std::uint32_t>(std::lround(v * 255));
            pixels[py * (surface->pitch / 4) + px] = static_cast<std::uint32_t>(std::lround(a * 255)) << 24 | c << 16 | c << 8 | c;
        }
    }
    SDL_Cursor* made = SDL_CreateColorCursor(surface, 1, 1);
    SDL_DestroySurface(surface);
    return made;
}

} // namespace

void SdlPlatform::setCursor(Cursor cursor) {
    if (cursor == cursor_)
        return;
    static constexpr std::array<SDL_SystemCursor, cursorCount> system{
        SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_TEXT,       SDL_SYSTEM_CURSOR_POINTER,     SDL_SYSTEM_CURSOR_MOVE,
        SDL_SYSTEM_CURSOR_EW_RESIZE,   SDL_SYSTEM_CURSOR_NS_RESIZE,  SDL_SYSTEM_CURSOR_NWSE_RESIZE, SDL_SYSTEM_CURSOR_NESW_RESIZE,
        SDL_SYSTEM_CURSOR_NOT_ALLOWED, SDL_SYSTEM_CURSOR_WAIT,       SDL_SYSTEM_CURSOR_CROSSHAIR,  SDL_SYSTEM_CURSOR_DEFAULT,
        SDL_SYSTEM_CURSOR_DEFAULT,     SDL_SYSTEM_CURSOR_DEFAULT,
    };
    const auto index = static_cast<std::size_t>(cursor);
    if (!cursors_[index]) {
        if (cursor == Cursor::DragCopy || cursor == Cursor::DragMove || cursor == Cursor::DragLink) {
            // as large as the system's own at the display's scale
            float scale = 1;
            if (const SDL_DisplayID display = SDL_GetPrimaryDisplay(); display != 0)
                scale = std::max(1.0f, SDL_GetDisplayContentScale(display));
            cursors_[index] = dragCursor(cursor, scale);
        } else {
            cursors_[index] = SDL_CreateSystemCursor(system[index]);
        }
    }
    // a system without that shape: the arrow
    SDL_Cursor* shown = cursors_[index] ? cursors_[index] : SDL_GetDefaultCursor();
    if (shown && SDL_SetCursor(shown))
        cursor_ = cursor;
}

std::string SdlPlatform::clipboardText() const {
    char* text = SDL_GetClipboardText();
    std::string result = text ? text : "";
    SDL_free(text);
    return result;
}

void SdlPlatform::setClipboardText(std::string_view text) {
    SDL_SetClipboardText(std::string(text).c_str());
}

namespace {
// What SDL's dialog needs until it answers (it keeps pointers to the filters): freed by its callback.
struct PendingDialog {
    std::vector<std::string> names, patterns;
    std::vector<SDL_DialogFileFilter> filters;
    std::function<void(FileDialogResult)> done;
};
} // namespace

void SdlPlatform::showFileDialog(const FileDialogRequest& request, std::function<void(FileDialogResult)> done) {
    auto* pending = new PendingDialog;
    pending->done = std::move(done);
    for (const FileFilter& f : request.filters) {
        pending->names.push_back(f.name);
        pending->patterns.push_back(f.extensions.empty() ? std::string("*") : f.extensions);
    }
    for (std::size_t i = 0; i < pending->names.size(); ++i)
        pending->filters.push_back({pending->names[i].c_str(), pending->patterns[i].c_str()});
    SDL_FileDialogType type = SDL_FILEDIALOG_OPENFILE;
    if (request.kind == FileDialogKind::Save)
        type = SDL_FILEDIALOG_SAVEFILE;
    else if (request.kind == FileDialogKind::Folder)
        type = SDL_FILEDIALOG_OPENFOLDER;
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (!pending->filters.empty()) {
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, pending->filters.data());
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<Sint64>(pending->filters.size()));
    }
    if (auto* parent = dynamic_cast<SdlWindow*>(request.parent))
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, parent->sdlWindow());
    if (!request.location.empty())
        SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, request.location.c_str());
    if (!request.title.empty())
        SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, request.title.c_str());
    SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, request.kind == FileDialogKind::OpenMany);
    SDL_ShowFileDialogWithProperties(
        type,
        [](void* data, const char* const* files, int) {
            std::unique_ptr<PendingDialog> p(static_cast<PendingDialog*>(data));
            FileDialogResult result;
            if (!files) {
                // (none at all, not an empty list: it failed)
                const char* why = SDL_GetError();
                result.error = why && *why ? why : "the file dialog failed";
            }
            for (const char* const* f = files; f && *f; ++f)
                result.files.emplace_back(*f);
            if (p->done)
                p->done(std::move(result));
        },
        pending, props);
    SDL_DestroyProperties(props);
}

std::string SdlPlatform::preferencesFolder(std::string_view organization, std::string_view application) const {
    char* path = SDL_GetPrefPath(std::string(organization).c_str(), std::string(application).c_str());
    std::string result = path ? path : "";
    SDL_free(path);
    return result;
}

bool SDLCALL SdlPlatform::watch(void* data, SDL_Event* event) {
    auto* self = static_cast<SdlPlatform*>(data);
    // SDL sends an exposure from inside the modal loops of resizing and moving: drawn right there; and the moves, as
    // they go (from inside the loop of moving too)
    try {
        if (event->type == SDL_EVENT_WINDOW_EXPOSED && self->exposeHandler_)
            self->exposeHandler_(event->window.windowID);
        else if (event->type == SDL_EVENT_WINDOW_MOVED && self->moveHandler_)
            self->moveHandler_(event->window.windowID);
    } catch (...) {
        // called from inside SDL (and the system): nothing may be thrown through it
    }
    return true;    // the event stays in the queue
}

std::optional<bool> SdlPlatform::systemDark() const {
    switch (SDL_GetSystemTheme()) {
    case SDL_SYSTEM_THEME_DARK: return true;
    case SDL_SYSTEM_THEME_LIGHT: return false;
    default: return std::nullopt;
    }
}

bool SdlPlatform::translate(const SDL_Event& in, Event& out) const {
    switch (in.type) {
    case SDL_EVENT_QUIT:
        out = QuitRequested{};
        return true;

    case SDL_EVENT_SYSTEM_THEME_CHANGED:
        out = SystemThemeChanged{SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK};
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
        // precise: by a fraction of a step, or soon after one (a touchpad's whole steps among its fractions)
        if (in.wheel.x != std::round(in.wheel.x) || in.wheel.y != std::round(in.wheel.y))
            preciseWheel_ = in.wheel.timestamp;
        const bool precise = preciseWheel_ != 0 && in.wheel.timestamp - preciseWheel_ < 250'000'000;
        out = MouseWheelScrolled{in.wheel.windowID, Point{in.wheel.x * sign, in.wheel.y * sign},
                                 Point{in.wheel.mouse_x * f, in.wheel.mouse_y * f}, precise};
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
