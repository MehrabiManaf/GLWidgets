// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
#include <GLWidgets/Platform/Window.hpp>

#include <SDL3/SDL_video.h>

namespace glw::platform::sdl {

class SdlPlatform;

class SdlWindow final : public Window {
public:
    SdlWindow(SdlPlatform& platform, const WindowDesc& desc);
    ~SdlWindow() override;

    SDL_Window* handle() const { return window_; }

    WindowId id() const override;
    void setTitle(std::string_view title) override;
    Size size() const override;
    void setSize(Size size) override;
    PixelSize pixelSize() const override;
    float scale() const override { return scaleOf(window_); }
    void show() override;
    void hide() override;
    WindowState state() const override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void setHitTest(HitTest hitTest) override;
    void startTextInput() override;
    void stopTextInput() override;

    // Pixels per dip of `window` (1 when there is no window).
    static float scaleOf(SDL_Window* window);
    // Dips per unit of SDL's window coordinates (its mouse positions, its window sizes): 1 / the scale where they are pixels
    // (Windows, X11), 1 where they are points (macOS, Wayland).
    static float dipsPerUnit(SDL_Window* window);
    static WindowState stateOf(SDL_Window* window);

private:
    static SDL_HitTestResult SDLCALL hitTestCallback(SDL_Window* window, const SDL_Point* area, void* data);

    SdlPlatform& platform_;
    SDL_Window* window_ = nullptr;
    HitTest hitTest_;
};

} // namespace glw::platform::sdl
