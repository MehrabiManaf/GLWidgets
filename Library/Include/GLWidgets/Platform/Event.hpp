// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What the platform reports: one plain struct per kind of event, an Event is one of them. Every event of a window carries its
// WindowId (Window::id); positions are in dips, relative to the top left corner of the window's content.
// An id may come after its window was destroyed (the event was queued before): an application ignores ids it does not know.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Platform/Keys.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace glw::platform {

using WindowId = std::uint32_t;

// The user or the system asks the application to quit (Command+Q on macOS, the session ends...). Closing the last window does
// not: the application decides what that means.
struct QuitRequested {};

// The close button of a window, Alt+F4... The window stays until the application destroys it.
struct WindowCloseRequested {
    WindowId window = 0;
};

// The window's content changed size, in dips or in pixels.
struct WindowResized {
    WindowId window = 0;
    Size size;
    PixelSize pixelSize;
};

// The window's scale (pixels per dip) changed: it moved to another screen, the user changed the system's scale.
struct WindowScaleChanged {
    WindowId window = 0;
    float scale = 1;
};

// The system needs the window's content drawn again.
struct WindowExposed {
    WindowId window = 0;
};

enum class WindowState : std::uint8_t { Normal, Minimized, Maximized };

// The window was minimized, maximized or restored: by the application or by the user (the system's shortcuts, snapping).
struct WindowStateChanged {
    WindowId window = 0;
    WindowState state = WindowState::Normal;
};

struct WindowFocusChanged {
    WindowId window = 0;
    bool focused = false;
};

// The mouse entered or left the window.
struct MouseCrossed {
    WindowId window = 0;
    bool entered = false;
};

struct MouseMoved {
    WindowId window = 0;
    Point position;
};

struct MouseButtonChanged {
    WindowId window = 0;
    MouseButton button = MouseButton::Left;
    bool pressed = false;
    int clicks = 1;                 // 2 for a double click...
    Point position;
};

// Scrolled by `delta` steps of the wheel (fractions with a touchpad): positive y away from the user, positive x to the right.
struct MouseWheelScrolled {
    WindowId window = 0;
    Point delta;
    Point position;
    // In fractions of a step, often: a touchpad, a wheel that spins freely or turns finely. Eased on as it comes, not
    // boosted nor snapped to rows.
    bool precise = false;
};

struct KeyChanged {
    WindowId window = 0;
    Key key = Key::Unknown;
    KeyModifiers modifiers = KeyModifiers::None;
    bool pressed = false;
    bool repeat = false;            // pressed again because the key is held down
};

// Text typed (UTF-8) while the window takes text input (Window::startTextInput): a key of any layout, the end of an input
// method composition.
struct TextInput {
    WindowId window = 0;
    std::string text;
};

// The text an input method is composing (UTF-8), not typed yet: shown at the cursor, replaced by the next one, ended by a
// TextInput or an empty composition. `cursor` and `selectionLength` are in code points.
struct TextComposition {
    WindowId window = 0;
    std::string text;
    int cursor = 0;
    int selectionLength = 0;
};

using Event = std::variant<QuitRequested, WindowCloseRequested, WindowResized, WindowScaleChanged, WindowExposed,
                           WindowStateChanged, WindowFocusChanged, MouseCrossed, MouseMoved, MouseButtonChanged, MouseWheelScrolled, KeyChanged,
                           TextInput, TextComposition>;

} // namespace glw::platform
