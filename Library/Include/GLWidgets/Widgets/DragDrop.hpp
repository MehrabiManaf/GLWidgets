// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Dragging something from one widget to another, in a window or between the application's windows: a variable from a
// list onto a canvas, a row to another place in its list.
//
// The widget pressed starts it from its onMouse, once the mouse moved far enough (UiRoot::startDrag); from then on the
// mouse moves it, and the widgets under the mouse are asked whether they take it (Widget::onDragOver), the deepest first,
// then its ancestors. Let go over one that takes it, it is dropped there (Widget::onDrop); Escape cancels it. The pointer
// shows whether it would be taken where it is. What started it is told how it ended (DragRequest::finished).
//
//   void onMouse(MouseEvent& e) override {
//       if (e.type == MouseEventType::Move && pressed_ && far(e.position))
//           root()->startDrag({.data = {"flowgraph/variable", name_},
//                              .finished = [](DropAction a) { ... }});
//   }
//   DropAction onDragOver(const DragEvent& e) override {
//       return e.data.format == "flowgraph/variable" ? DropAction::Copy : DropAction::None;
//   }
//   DropAction onDrop(const DragEvent& e) override { addGetter(e.data.content, e.position); return DropAction::Copy; }
//
// Between windows, the application's DockingHost says which window is under the mouse (where the system carries drags
// between windows, Wayland, through its drag-and-drop). Without one (an engine), in its window only.
#include <GLWidgets/Core/Geometry.hpp>
#include <GLWidgets/Platform/Event.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace glw {

// What the widget it is let go over does with what is dragged.
enum class DropAction : std::uint8_t {
    None,       // nothing: not taken there
    Copy,       // something made from it (a node from a variable)
    Move,       // it moves there (a row to another place)
    Link,       // something made that refers to it
};

// What is dragged: the application's own name for the kind of thing (targets take the kinds they know), and what it is.
struct DragData {
    std::string format;                     // "flowgraph/variable"
    std::string content;                    // a name, an id, JSON
};

struct DragRequest {
    DragData data;
    // After it ended, after the event: what was done with it (None: let go where it is not taken, or cancelled).
    std::function<void(DropAction)> finished;
};

// A drag over a widget, or let go on it.
struct DragEvent {
    const DragData& data;
    Point position;                         // in the widget's coordinates
    platform::KeyModifiers modifiers;       // (Ctrl: a copy rather than a move, where a target makes both)
};

} // namespace glw
