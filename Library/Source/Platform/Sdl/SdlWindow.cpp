// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/SdlWindow.hpp"

#include "Platform/Sdl/SdlPlatform.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keyboard.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace glw::platform::sdl {

namespace {

// A length of dips in the window's units (`f` dips each): rounded up, so that what was measured to fit does (a popup of
// a text one line long; rounded to the nearest, it may lose part of a pixel, and the text wraps).
int sizeInUnits(float dips, float f) {
    return std::max(1, static_cast<int>(std::ceil(dips / f - 0.01f)));
}

#ifdef _WIN32
HWND hwndOf(SDL_Window* window) {
    return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

// A borderless window's messages before SDL's: its maximize button the system's maximize button (HTMAXBUTTON: Windows 11
// shows its snap layouts over it), the mouse over it given back to SDL as the application's (a move, a press, a release),
// so that the button lights and works as it does elsewhere.
LRESULT CALLBACK frameProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    auto* window = reinterpret_cast<SdlWindow*>(data);
    const auto client = [&] {
        POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &p);
        return MAKELPARAM(p.x, p.y);
    };
    switch (message) {
    case WM_NCHITTEST: {
        POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &p);
        const float scale = window->scale();
        if (window->hitAreaAt({static_cast<float>(p.x) / scale, static_cast<float>(p.y) / scale}) == HitArea::MaximizeButton)
            return HTMAXBUTTON;
        break;
    }
    case WM_NCMOUSEMOVE:
        if (wParam == HTMAXBUTTON) {
            // told when it leaves the button; the move the application's (and on to the system: its snap layouts)
            TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
            TrackMouseEvent(&track);
            SendMessageW(hwnd, WM_MOUSEMOVE, 0, client());
        }
        break;
    case WM_MOUSELEAVE: {
        // out of the inside onto the maximize button: still over the application's (SDL would take the mouse as gone)
        POINT p;
        GetCursorPos(&p);
        if (SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y)) == HTMAXBUTTON)
            return 0;
        break;
    }
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (wParam == HTMAXBUTTON) {
            SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, client());
            return 0;       // not the system's: it would maximize the window itself
        }
        break;
    case WM_NCLBUTTONUP:
        if (wParam == HTMAXBUTTON) {
            SendMessageW(hwnd, WM_LBUTTONUP, 0, client());
            return 0;
        }
        break;
    case WM_NCMOUSELEAVE: {
        // off the maximize button and out of the window: gone (into the window: its own moves say where)
        POINT p;
        GetCursorPos(&p);
        if (WindowFromPoint(p) != hwnd)
            PostMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, frameProc, 1);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}
#endif

} // namespace

SdlWindow::SdlWindow(SdlPlatform& platform, const WindowDesc& desc) : platform_(platform) {
    GLW_EXPECTS(desc.size.width > 0 && desc.size.height > 0);
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc.resizable)
        flags |= SDL_WINDOW_RESIZABLE;
    if (desc.borderless)
        flags |= SDL_WINDOW_BORDERLESS;
    if (desc.tool)
        flags |= SDL_WINDOW_UTILITY;
    if (desc.overlay)
        flags |= SDL_WINDOW_UTILITY | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_NOT_FOCUSABLE;
    if (desc.popupOf) {
        // in the parent's units, from its corner: a popup has the parent's scale
        popupOf_ = dynamic_cast<SdlWindow*>(desc.popupOf);
        GLW_EXPECTS(popupOf_ != nullptr);
        const float f = dipsPerUnit(popupOf_->window_);
        const Point at = desc.position.value_or(Point{});
        flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | (desc.tooltip ? SDL_WINDOW_TOOLTIP : SDL_WINDOW_POPUP_MENU);
        window_ = SDL_CreatePopupWindow(popupOf_->window_, static_cast<int>(std::lround(at.x / f)), static_cast<int>(std::lround(at.y / f)),
                                        sizeInUnits(desc.size.width, f), sizeInUnits(desc.size.height, f), flags);
        if (!window_)
            throw Error(std::string("SDL_CreatePopupWindow: ") + SDL_GetError());
        platform_.windowCreated();
        return;
    }
    window_ = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.size.width), static_cast<int>(desc.size.height), flags);
    if (!window_)
        throw Error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    // SDL made it in its window coordinates, not dips everywhere: now that the window has a screen (and so a scale), give it
    // the size asked for, and center it again with that size
    setSize(desc.size);
    if (desc.position) {
        setPosition(*desc.position);
        keepOnScreens();
    } else {
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    if (desc.borderless && !desc.overlay)
        framed();
    platform_.windowCreated();
}

void SdlWindow::framed() {
#ifdef _WIN32
    const HWND hwnd = hwndOf(window_);
    if (!hwnd)
        return;
    // the system's shadow around it: its frame (none shows: SDL gives it no room) extended into it
    const MARGINS margins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    // Windows 11 (where its corners can be asked for): its snap layouts over the maximize button
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DEFAULT;
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners)))
        SetWindowSubclass(hwnd, frameProc, 1, reinterpret_cast<DWORD_PTR>(this));
#endif
}

void SdlWindow::keepOnScreens() {
    // the screens' usable areas, in its coordinates (where windows can be placed: not Wayland's)
    if (!platform_.placesWindows())
        return;
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    std::vector<Rect> screens;
    for (int i = 0; displays && i < count; ++i) {
        SDL_Rect r{};
        if (SDL_GetDisplayUsableBounds(displays[i], &r))
            screens.push_back({static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)});
    }
    SDL_free(displays);
    const Point at = position();
    const Size size = screenSize();
    const Rect placed = keptOnScreens({at.x, at.y, size.width, size.height}, screens, 32 / dipsPerUnit(window_));
    if (placed.width != size.width || placed.height != size.height)
        SDL_SetWindowSize(window_, static_cast<int>(placed.width), static_cast<int>(placed.height));
    if (placed.x != at.x || placed.y != at.y)
        SDL_SetWindowPosition(window_, static_cast<int>(placed.x), static_cast<int>(placed.y));
}

HitArea SdlWindow::hitAreaAt(Point point) const {
    if (!hitTest_)
        return HitArea::Client;
    try {
        return hitTest_(point);
    } catch (...) {
        return HitArea::Client;         // (called by the system: nothing may be thrown through it)
    }
}

SdlWindow::~SdlWindow() {
    SDL_DestroyWindow(window_);
    platform_.windowDestroyed();
}

WindowId SdlWindow::id() const {
    return SDL_GetWindowID(window_);
}

void SdlWindow::setTitle(std::string_view title) {
    SDL_SetWindowTitle(window_, std::string(title).c_str());
}

Size SdlWindow::size() const {
    const PixelSize pixels = pixelSize();
    const float s = scale();
    return Size{pixels.width / s, pixels.height / s};
}

void SdlWindow::setSize(Size size) {
    GLW_EXPECTS(size.width > 0 && size.height > 0);
    const float f = dipsPerUnit(window_);
    SDL_SetWindowSize(window_, sizeInUnits(size.width, f), sizeInUnits(size.height, f));
    if (cornerRadius_ > 0)
        cutCorners();
}

PixelSize SdlWindow::pixelSize() const {
    PixelSize pixels;
    SDL_GetWindowSizeInPixels(window_, &pixels.width, &pixels.height);
    return pixels;
}

void SdlWindow::show(bool focus) {
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, focus ? "1" : "0");
    SDL_ShowWindow(window_);
    SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "1");
}

void SdlWindow::hide() {
    SDL_HideWindow(window_);
}

bool SdlWindow::visible() const {
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_HIDDEN) == 0;
}

void SdlWindow::raise() {
    SDL_RaiseWindow(window_);
}

bool SdlWindow::focused() const {
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool SdlWindow::setCornerRadius(float radius) {
#ifdef _WIN32
    auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd)
        return false;
    // Windows 11's: DWM rounds the window, smoothly, and clips what it shows (no hairline of its own: the window draws its
    // edge, as round); square again while it is maximized or snapped; before it, the attribute is not known
    const DWM_WINDOW_CORNER_PREFERENCE corners = radius > 0 ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
    if (SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners))) {
        const COLORREF border = DWMWA_COLOR_NONE;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof border);
        cornerRadius_ = 0;
        return radius > 0;
    }
    // before it: a popup's shape cut round (its corners not smoothed); a window stays square (a cut would take its edges'
    // resizing and its shadow)
    if (!popupOf_)
        return false;
    cornerRadius_ = radius;
    cutCorners();
    return radius > 0;
#else
    return false;
#endif
}

void SdlWindow::cutCorners() {
#ifdef _WIN32
    auto* hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd)
        return;
    if (cornerRadius_ <= 0) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(window_, &width, &height);
    const int diameter = static_cast<int>(std::lround(2 * cornerRadius_ * scale()));
    // the region is the system's from then on (not deleted here)
    SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter), TRUE);
#endif
}

void SdlWindow::setOpacity(float opacity) {
    SDL_SetWindowOpacity(window_, opacity);     // false where it cannot: the window stays opaque
}

void SdlWindow::setParent(Window* parent) {
    auto* sdlParent = dynamic_cast<SdlWindow*>(parent);
    SDL_SetWindowParent(window_, sdlParent ? sdlParent->window_ : nullptr);
}

Point SdlWindow::position() const {
    int x = 0, y = 0;
    SDL_GetWindowPosition(window_, &x, &y);
    const Point at{static_cast<float>(x), static_cast<float>(y)};
    return popupOf_ ? popupOf_->position() + at : at;       // a popup's is from its parent's corner
}

void SdlWindow::setPosition(Point position) {
    if (popupOf_)
        position = position - popupOf_->position();
    SDL_SetWindowPosition(window_, static_cast<int>(std::lround(position.x)), static_cast<int>(std::lround(position.y)));
}

Point SdlWindow::toScreen(Point point) const {
    // SDL's window coordinates are the screen's, from the window's corner
    const float f = dipsPerUnit(window_);
    const Point corner = position();
    return {corner.x + point.x / f, corner.y + point.y / f};
}

Point SdlWindow::fromScreen(Point point) const {
    const float f = dipsPerUnit(window_);
    const Point corner = position();
    return {(point.x - corner.x) * f, (point.y - corner.y) * f};
}

Size SdlWindow::screenSize() const {
    int w = 0, h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    return {static_cast<float>(w), static_cast<float>(h)};
}

Rect SdlWindow::usableScreenArea() const {
    SDL_Rect r{};
    if (!SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(popupOf_ ? popupOf_->window_ : window_), &r))
        return {position().x, position().y, screenSize().width, screenSize().height};
    return {static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)};
}

WindowState SdlWindow::state() const {
    return stateOf(window_);
}

void SdlWindow::minimize() {
    SDL_MinimizeWindow(window_);
}

void SdlWindow::maximize() {
    SDL_MaximizeWindow(window_);
}

void SdlWindow::restore() {
    SDL_RestoreWindow(window_);
}

void SdlWindow::setHitTest(HitTest hitTest) {
    hitTest_ = std::move(hitTest);
    SDL_SetWindowHitTest(window_, hitTest_ ? &SdlWindow::hitTestCallback : nullptr, this);
}

SDL_HitTestResult SDLCALL SdlWindow::hitTestCallback(SDL_Window* window, const SDL_Point* area, void* data) {
    auto* self = static_cast<SdlWindow*>(data);
    const float f = dipsPerUnit(window);
    HitArea hit = HitArea::Client;
    try {
        hit = self->hitTest_(Point{area->x * f, area->y * f});
    } catch (...) {
        // called by the system from inside SDL: nothing may be thrown through it
    }
    switch (hit) {
    case HitArea::Client: return SDL_HITTEST_NORMAL;
    case HitArea::Caption: return SDL_HITTEST_DRAGGABLE;
    case HitArea::ResizeTopLeft: return SDL_HITTEST_RESIZE_TOPLEFT;
    case HitArea::ResizeTop: return SDL_HITTEST_RESIZE_TOP;
    case HitArea::ResizeTopRight: return SDL_HITTEST_RESIZE_TOPRIGHT;
    case HitArea::ResizeRight: return SDL_HITTEST_RESIZE_RIGHT;
    case HitArea::ResizeBottomRight: return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    case HitArea::ResizeBottom: return SDL_HITTEST_RESIZE_BOTTOM;
    case HitArea::ResizeBottomLeft: return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    case HitArea::ResizeLeft: return SDL_HITTEST_RESIZE_LEFT;
    case HitArea::MaximizeButton: return SDL_HITTEST_NORMAL;       // (the application's: Windows 11's is in frameProc)
    }
    return SDL_HITTEST_NORMAL;
}

WindowState SdlWindow::stateOf(SDL_Window* window) {
    const SDL_WindowFlags flags = window ? SDL_GetWindowFlags(window) : 0;
    if (flags & SDL_WINDOW_MINIMIZED)
        return WindowState::Minimized;
    if (flags & SDL_WINDOW_MAXIMIZED)
        return WindowState::Maximized;
    return WindowState::Normal;
}

void SdlWindow::startTextInput() {
    SDL_StartTextInput(window_);
}

void SdlWindow::stopTextInput() {
    SDL_StopTextInput(window_);
}

void SdlWindow::setTextInputArea(const Rect& area) {
    const float f = dipsPerUnit(window_);       // SDL wants its window coordinates
    const SDL_Rect r{static_cast<int>(std::lround(area.x / f)), static_cast<int>(std::lround(area.y / f)),
                     static_cast<int>(std::lround(area.width / f)), static_cast<int>(std::lround(area.height / f))};
    SDL_SetTextInputArea(window_, &r, 0);
}

float SdlWindow::scaleOf(SDL_Window* window) {
    const float scale = window ? SDL_GetWindowDisplayScale(window) : 0.0f;
    return scale > 0 ? scale : 1.0f;
}

float SdlWindow::dipsPerUnit(SDL_Window* window) {
    if (!window)
        return 1.0f;
    const float density = SDL_GetWindowPixelDensity(window);       // pixels per unit
    return (density > 0 ? density : 1.0f) / scaleOf(window);
}

} // namespace glw::platform::sdl
