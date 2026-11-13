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
#include <cstdint>
#include <span>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glw::platform {

// A kind of files a file dialog offers: "FlowGraph scripts" and "flow" (extensions, without dots, separated by ';').
struct FileFilter {
    std::string name;
    std::string extensions;
};

enum class FileDialogKind : std::uint8_t { Open, OpenMany, Save, Folder };

struct FileDialogRequest {
    FileDialogKind kind = FileDialogKind::Open;
    Window* parent = nullptr;               // the window it belongs to (modal to it where the system makes it so)
    std::string title;                      // the system's own when empty
    std::string location;                   // the folder it starts in, or a file's path (its name proposed when saving)
    std::vector<FileFilter> filters;        // none: every file
};

// What a file dialog answered: the paths chosen (UTF-8), none when it was cancelled; or, when it could not be shown or
// failed, why (the files then none).
struct FileDialogResult {
    std::vector<std::string> files;
    std::string error;

    bool failed() const { return !error.empty(); }
};

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
    // Called as a window moves, the system's moves too (dragged by its title bar: from inside its own loop of moving it,
    // on Windows and macOS). None: not called.
    virtual void setMoveHandler(std::function<void(WindowId)> handler) = 0;

    // Whether windows go where the application puts them, and the mouse is known wherever it is: not on Wayland, where
    // the compositor places windows and an application sees the mouse over its own windows only. Without it, Window's
    // positions mean nothing and mousePosition is not to be relied on.
    virtual bool placesWindows() const = 0;
    // A drag the system carries from `from` (a button held down in it) to the application's other windows: what they are
    // told of as DragMoved, DragLeft, DragDropped, and DragEnded when it ends. False where the system carries none (it
    // does not need to: where it places windows, the application follows the mouse itself).
    virtual bool startDrag(Window& from) { return false; }
    // Where the mouse is, in screen coordinates (Window::position), wherever it is.
    virtual Point mousePosition() const = 0;
    // Whether its left button is held, wherever it is (a window being moved by the system: not let go yet).
    virtual bool mouseHeld() const = 0;
    // While on, the window the mouse was pressed in gets the mouse's events outside of it too (dragging out of it).
    virtual void captureMouse(bool capture) = 0;
    // The shape of the pointer while it is over the application's windows (where they are the application's: over the
    // edges a window resizes by, the system shows its own).
    virtual void setCursor(Cursor cursor) = 0;

    // The system's dialog for choosing files or a folder. `done` gets what was chosen, or why it failed, later, on any
    // thread (Application::showFileDialog brings it to the thread of the user interface).
    virtual void showFileDialog(const FileDialogRequest& request, std::function<void(FileDialogResult)> done) = 0;
    // The folder the user's preferences of `application` (of `organization`) go into, made if it was not there: UTF-8,
    // ending with a separator; empty when there is none to be had.
    virtual std::string preferencesFolder(std::string_view organization, std::string_view application) const = 0;

    // Whether the system's appearance is dark (true) or light (false); none when it does not say. Its changes come as
    // SystemThemeChanged.
    virtual std::optional<bool> systemDark() const { return std::nullopt; }

    // Writes `pixels` (rows of size.width, the top one first; RGBA, its red in the lowest byte, not premultiplied) as a
    // PNG file at `path` (UTF-8). False when it could not, and why in `error`.
    virtual bool savePng(const std::string& path, PixelSize size, std::span<const std::uint32_t> pixels, std::string* error) = 0;

    // The text of the system's clipboard (UTF-8; empty when it holds none).
    virtual std::string clipboardText() const = 0;
    virtual void setClipboardText(std::string_view text) = 0;

protected:
    Platform() = default;
};

// The platform of this system. Throws Error when it cannot start (no display...) or when there is a platform already.
GLW_EXPORT std::unique_ptr<Platform> createPlatform();

} // namespace glw::platform
