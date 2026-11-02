// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What docking across windows asks of the application: where its windows are on the screen, which one is under the mouse,
// new windows and closing them. Application is a DockingHost (UiContext::docking). Without one (an engine, a test of one
// window), tabs and panels stay in their window.
//
// Screen coordinates are the system's, for the whole desktop (platform::Window::position).
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>

#include <memory>
#include <optional>

namespace glw {

class UiRoot;
class Widget;

class GLW_EXPORT DockingHost {
public:
    virtual ~DockingHost();

    // ---- where things are ----

    virtual Point mouseOnScreen() const = 0;
    virtual Point toScreen(const UiRoot& root, Point point) const = 0;
    struct Hit {
        UiRoot* root = nullptr;             // none: no window of the application there
        Point point;                        // in that root's dips
    };
    // The window of the application at `screen`, the one in front when several are (not `except`).
    virtual Hit windowAt(Point screen, const UiRoot* except = nullptr) const = 0;
    // Keeps giving the mouse to the window it was pressed in while it is dragged out of it.
    virtual void captureMouse(bool capture) = 0;

    // ---- windows ----

    struct NewWindow {
        LocalizedText title;
        std::unique_ptr<Widget> content;
        std::unique_ptr<Widget> titleCenter;    // what the title bar shows instead of the title: a tab strip
        Point position;                         // its top left corner, on the screen
        Size size;                              // dips
        const UiRoot* owner = nullptr;          // a tool window above this one's (a floating panel's); none: a window
        bool titleBar = true;                   // none: the content draws it (a floating panel's tabs, with the buttons)
    };
    virtual UiRoot& openWindow(NewWindow window) = 0;
    // After the event being handled.
    virtual void closeWindow(const UiRoot& root) = 0;
    virtual void setWindowVisible(const UiRoot& root, bool visible) = 0;
    // A tool window goes above `owner`'s now (none: on its own).
    virtual void setWindowOwner(const UiRoot& root, const UiRoot* owner) = 0;
    virtual void raiseWindow(const UiRoot& root) = 0;
    // Its top left corner on the screen.
    virtual Point windowPosition(const UiRoot& root) const = 0;
    virtual void moveWindow(const UiRoot& root, Point position) = 0;
};

} // namespace glw
