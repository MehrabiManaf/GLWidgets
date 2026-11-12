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
    void show(bool focus = true) override;
    void hide() override;
    bool visible() const override;
    void raise() override;
    bool focused() const override;
    void setParent(Window* parent) override;
    void setOpacity(float opacity) override;
    bool setCornerRadius(float radius) override;
    Point position() const override;
    void setPosition(Point position) override;
    Point toScreen(Point point) const override;
    Point fromScreen(Point point) const override;
    Size screenSize() const override;
    Rect usableScreenArea() const override;
    WindowState state() const override;
    void minimize() override;
    void maximize() override;
    void setFullScreen(bool fullScreen) override;
    bool fullScreen() const override;
    void restore() override;
    void setHitTest(HitTest hitTest) override;
    void startTextInput() override;
    void stopTextInput() override;
    void setTextInputArea(const Rect& area) override;

    // Pixels per dip of `window` (1 when there is no window).
    static float scaleOf(SDL_Window* window);
    // Dips per unit of SDL's window coordinates (its mouse positions, its window sizes): 1 / the scale where they are pixels
    // (Windows, X11), 1 where they are points (macOS, Wayland).
    static float dipsPerUnit(SDL_Window* window);
    static WindowState stateOf(SDL_Window* window);
    SDL_Window* sdlWindow() const { return window_; }
    // What its hit test says is at `point` (dips), Client without one.
    HitArea hitAreaAt(Point point) const;

private:
    static SDL_HitTestResult SDLCALL hitTestCallback(SDL_Window* window, const SDL_Point* area, void* data);

    SdlPlatform& platform_;
    SDL_Window* window_ = nullptr;
    SdlWindow* popupOf_ = nullptr;          // a popup's parent: SDL keeps its position from the parent's corner
    float cornerRadius_ = 0;                // its shape cut round by this (where the system does not round it): again as it resizes
    void cutCorners();
    // Windows: a borderless window's shadow and what the system adds (snap layouts on the maximize button)
    void framed();
    // Where it was put: on the screens there are (keptOnScreens), when it was not.
    void keepOnScreens();
    HitTest hitTest_;
};

} // namespace glw::platform::sdl
