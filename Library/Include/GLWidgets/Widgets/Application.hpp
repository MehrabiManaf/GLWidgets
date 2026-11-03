// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A standalone application: the platform, the GPU device, the fonts and the glyph atlas, shared by its windows, and the
// loop that gives the windows their events and draws them when they changed (and only then).
//
//   glw::Application app;
//   app.openWindow({.title = "Hello"}, std::move(content));
//   return app.run();
//
// One application per process, on the main thread. It is the DockingHost of its windows (DockingHost.hpp): tabs and panels
// are dragged from one to another.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Paint/Renderer.hpp>
#include <GLWidgets/Platform/Platform.hpp>
#include <GLWidgets/Rhi/Device.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace glw::text {
class FontLibrary;
} // namespace glw::text

namespace glw {

class Application;
class DockingHost;
class WindowFrame;

struct WindowOptions {
    LocalizedText title = "GLWidgets";
    Size size{960, 640};
    std::optional<Point> position;          // its top left corner on the screen (platform::Window::position); none: centered
    // A borderless window in a WindowFrame: its title bar and buttons drawn by GLWidgets. Otherwise the system's.
    bool customChrome = true;
};

// A window of the application, showing a widget tree. Made by Application::openWindow, which keeps it.
class GLW_EXPORT Window final : public UiHost {
public:
    ~Window() override;

    platform::WindowId id() const;
    UiRoot& ui() { return root_; }
    WindowFrame* frame() const { return frame_; }       // with custom chrome
    void setTitle(LocalizedText title);
    // Closes it once the event being handled is done: a button may close the window it is in.
    void close() { closing_ = true; }
    bool closing() const { return closing_; }

    platform::WindowState windowState() const override;
    void minimizeWindow() override;
    void toggleMaximizeWindow() override;
    void closeWindow() override { close(); }
    void startTextInput() override;
    void stopTextInput() override;
    void setTextInputArea(const Rect& area) override;
    std::string clipboardText() const override;
    void setClipboardText(const std::string& text) override;

private:
    friend class Application;
    // With an owner: a tool window above it. An overlay: above every window, never focused, hidden, no frame.
    Window(Application& app, const WindowOptions& options, std::unique_ptr<Widget> content, Window* owner = nullptr,
           bool overlay = false, bool focus = true);
    void languageChanged();

    Application& app_;
    LocalizedText title_;
    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<rhi::Surface> surface_;             // after the window: destroyed before it
    UiRoot root_;
    WindowFrame* frame_ = nullptr;
    paint::DrawList list_;
    paint::RenderBuffers buffers_;
    Window* owner_ = nullptr;
    bool closing_ = false;
    bool told_ = false;                                 // its root was told it closes
};

class GLW_EXPORT Application {
public:
    // Throws Error when the platform or the GPU cannot start.
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // A window showing `content` (in a WindowFrame with custom chrome).
    Window& openWindow(const WindowOptions& options, std::unique_ptr<Widget> content);
    std::span<const std::unique_ptr<Window>> windows() const;

    // Called once when the application is about to end (its last window closes, or quit is called), while its windows are
    // all still there: to save them (Workspace.hpp). The tool windows of floating panels do not keep it running.
    void setOnQuitting(std::function<void()> handler);
    // What docking across the windows asks of them, to save and restore them (Workspace.hpp).
    DockingHost& docking();

    // Runs until the last window is closed or quit is called, and returns the exit code. With `frames` (a smoke test), draws
    // every window that many times in a row and returns: 1 when the frames did not reach the windows.
    int run(int frames = -1);
    void quit(int exitCode = 0);

    const UiContext& context() const;
    // The theme of every window, which shows it at once.
    void setTheme(const Theme& theme);
    // The language every window shows its texts in (its catalog loaded in translator()), which shows them at once; a
    // language written right to left mirrors the windows.
    void setLanguage(const std::string& language);
    Translator& translator();
    const text::FontLibrary& fonts() const;
    platform::Platform& platform();
    rhi::Device& device();
    paint::Renderer& renderer();

private:
    friend class Window;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw
