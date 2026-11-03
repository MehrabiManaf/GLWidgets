// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/SdlWindow.hpp"

#include "Platform/Sdl/SdlPlatform.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Core/Error.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keyboard.h>

#include <cmath>
#include <string>
#include <utility>

namespace glw::platform::sdl {

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
    window_ = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.size.width), static_cast<int>(desc.size.height), flags);
    if (!window_)
        throw Error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    // SDL made it in its window coordinates, not dips everywhere: now that the window has a screen (and so a scale), give it
    // the size asked for, and center it again with that size
    setSize(desc.size);
    if (desc.position)
        setPosition(*desc.position);
    else
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    platform_.windowCreated();
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
    SDL_SetWindowSize(window_, static_cast<int>(std::lround(size.width / f)), static_cast<int>(std::lround(size.height / f)));
}

PixelSize SdlWindow::pixelSize() const {
    PixelSize pixels;
    SDL_GetWindowSizeInPixels(window_, &pixels.width, &pixels.height);
    return pixels;
}

void SdlWindow::show() {
    SDL_ShowWindow(window_);
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
    return {static_cast<float>(x), static_cast<float>(y)};
}

void SdlWindow::setPosition(Point position) {
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
