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
#include <GLWidgets/Widgets/MenuBar.hpp>
#include <GLWidgets/Widgets/StatusBar.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
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
    // Its menu bar: in its title bar (custom chrome) or under it; its toolbar, under them.
    MenuBar& setMenuBar(std::unique_ptr<MenuBar> menuBar, MenuBarPlacement placement = MenuBarPlacement::TitleBar);
    ToolBar& setToolBar(std::unique_ptr<ToolBar> toolBar);
    // Its status bar, along its bottom (none: a new one).
    StatusBar& setStatusBar(std::unique_ptr<StatusBar> statusBar = nullptr);
    void setTitle(LocalizedText title);
    // Closes it once the event being handled is done: a button may close the window it is in. Always: what the user asks
    // (its close button, the system's) goes through requestClose.
    void close() { closing_ = true; }
    // The user asks to close it: its handler says whether it closes (false: it stays; to ask whether to save first, a
    // dialog that calls close() itself). It closes when there is none.
    void setOnCloseRequested(std::function<bool()> handler) { onCloseRequested_ = std::move(handler); }
    void requestClose() override;
    bool closing() const { return closing_; }

    platform::WindowState windowState() const override;
    void minimizeWindow() override;
    void toggleMaximizeWindow() override;
    // Its next frame, drawn as ever and saved as a PNG file at `path` (UTF-8): a screenshot of what it shows (not of the
    // system's frame around it). `done` is told after that frame, with why it was not saved (empty: it was).
    void saveImage(std::string path, std::function<void(const std::string& error)> done = {});
    // The whole screen (Platform::Window::setFullScreen), or as it was.
    void setFullScreen(bool fullScreen);
    bool fullScreen() const;
    void closeWindow() override { close(); }
    void startTextInput() override;
    void stopTextInput() override;
    void setTextInputArea(const Rect& area) override;
    std::string clipboardText() const override;
    void setClipboardText(const std::string& text) override;
    void setCursor(platform::Cursor cursor) override;
    UiRoot* openPopup(PopupRequest request) override;

private:
    // The window it is in, or that its popup came from (through popups of popups): what has the keyboard.
    Window& base();
    friend class Application;
    // A popup of `parent`: as big as its content measures, placed by the request's anchor, on the screen.
    Window(Application& app, PopupRequest request, Window& parent);
    // With an owner: a tool window above it. An overlay: above every window, never focused, hidden, no frame.
    Window(Application& app, const WindowOptions& options, std::unique_ptr<Widget> content, Window* owner = nullptr,
           bool overlay = false, bool focus = true);
    void languageChanged();
    // With the system's chrome: what holds the bars over its content (made when there is one).
    Widget& bars();

    Application& app_;
    LocalizedText title_;
    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<rhi::Surface> surface_;             // after the window: destroyed before it
    UiRoot root_;
    WindowFrame* frame_ = nullptr;
    Widget* bars_ = nullptr;
    paint::DrawList list_;
    paint::RenderBuffers buffers_;
    // What it is drawn into first while it shows glass (Renderer::backdropCaptures): its image and depth, then copied
    // onto it; and what the glass reads of it, at half its size.
    std::unique_ptr<rhi::Texture> offscreen_;
    std::unique_ptr<rhi::Texture> offscreenDepth_;
    std::unique_ptr<rhi::Texture> backdrop_;
    Window* owner_ = nullptr;                           // a tool window's, a popup's
    bool popup_ = false;
    bool tooltip_ = false;                              // a popup that takes no input
    bool modal_ = false;                                // a popup the other windows wait for
    bool popupAtStart_ = false;                         // a popup beside its anchor: at the anchor's start side
    bool closing_ = false;
    bool told_ = false;                                 // its root was told it closes
    std::function<bool()> onCloseRequested_;
    // saveImage: where its next frame goes, and who is told
    std::optional<std::string> imagePath_;
    std::function<void(const std::string&)> imageSaved_;
    // a popup coming in: faded in, its content sliding `slide_` dips to its place
    void entering();
    void roundCorners();                // a popup's: as the theme says, or as it was asked
    std::optional<float> popupRadius_;
    Animation entrance_{[this] { entering(); }};
    Point slide_;
    PopupEntrance entranceKind_ = PopupEntrance::Slide;
    int entranceWay_ = 0;                               // which of its kind's ways (a style's that varies them)
    paint::Edge hinge_ = paint::Edge::Top;              // its side toward its anchor: what it unfolds from
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
    // The system asks the application to quit (Command+Q, the session ending): the handler says whether it does (false: it
    // goes on; to ask whether to save first, then quit()). It quits when there is none.
    void setOnQuitRequested(std::function<bool()> handler);
    // From any thread: `fn` runs on the thread of the user interface, soon, in the order posted (Scheduler::post).
    void post(std::function<void()> fn);
    // The system's dialog for choosing files or a folder (the request's parent: the window it belongs to, if any); `done`
    // gets what was chosen (UTF-8 paths; none: cancelled) or why it failed, on the thread of the user interface. An answer
    // that comes after the application is gone is dropped.
    void showFileDialog(platform::FileDialogRequest request, std::function<void(platform::FileDialogResult)> done,
                        Window* parent = nullptr);
    // What docking across the windows asks of them, to save and restore them (Workspace.hpp).
    DockingHost& docking();

    // Runs until the last window is closed or quit is called, and returns the exit code. With `frames` (a smoke test), draws
    // every window that many times in a row and returns: 1 when the frames did not reach the windows.
    int run(int frames = -1);
    void quit(int exitCode = 0);

    const UiContext& context() const;
    // The theme of every window, which shows it at once.
    // Eased there (the theme's slow motion): through the themes of the palettes between the one shown and its own, then
    // itself. At once when `animated` is false, with animations off, or before a window opens.
    void setTheme(const Theme& theme, bool animated = true);
    // The theme of another style (ThemeStyle), in the colors it is in now (its ColorScheme); eased as setTheme.
    void setStyle(ThemeStyle style, bool animated = true);
    ThemeStyle style() const;
    // The theme of other colors (ColorScheme), in the style it is in now; eased as setTheme.
    void setColorScheme(ColorScheme scheme, bool animated = true);
    ColorScheme colorScheme() const;
    // Whether the system's appearance is dark, light, or none when it does not say (Platform::systemDark).
    std::optional<bool> systemPrefersDark() const;
    // Told when the system's appearance changes (after the colors follow it, when they do).
    void setOnSystemThemeChanged(std::function<void(bool dark)> handler);
    // The colors follow the system's appearance: Dark or Light (the style kept), now and as it changes; off, they stay as
    // they are. A system that does not say leaves them.
    void setFollowsSystemColors(bool follows);
    bool followsSystemColors() const;
    // The language every window shows its texts in (its catalog loaded in translator()), which shows them at once; a
    // language written right to left mirrors the windows (unless setLayoutDirection says otherwise).
    void setLanguage(const std::string& language);
    // The direction of every window's layout whatever the language: an application that translates its texts but keeps
    // its layout (a diagram's, a code editor's, which do not mirror) says LeftToRight. None (or Inherit): the language's.
    // Texts are still written in their own direction, right to left in Persian, wherever they are.
    void setLayoutDirection(std::optional<LayoutDirection> direction);
    Translator& translator();
    const text::FontLibrary& fonts() const;
    // To load fonts into (a monospace font for code): what widgets show with them comes from there.
    text::FontLibrary& fonts();
    platform::Platform& platform();
    rhi::Device& device();
    paint::Renderer& renderer();

private:
    friend class Window;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw
