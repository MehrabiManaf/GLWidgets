// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Wayland's drag-and-drop between the application's windows, for what a window cannot do there: know where the mouse is
// out of it. A dragged tab leaving its window is handed to the compositor as a drag of GLWidgets' own kind of data
// (nothing is sent: the drag is the application's, from one of its windows to another); its other windows are told it is
// over them, where, and that it was let go there, and the one it came from that it ended. Platform::startDrag.
//
// Its own objects on SDL's connection (SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER), in a queue of their own so as
// not to be dispatched by SDL: the seat, a pointer (only for the serial of the press a drag starts from, which SDL does
// not give), the data device manager and the seat's data device. Its events come out as GLWidgets' (DragMoved...).
#include <GLWidgets/Platform/Event.hpp>

#include <SDL3/SDL_video.h>

#include <deque>
#include <memory>

namespace glw::platform::sdl {

class WaylandDrag {
public:
    // None when SDL's video is not Wayland's (or the compositor has no drag-and-drop).
    static std::unique_ptr<WaylandDrag> make();
    ~WaylandDrag();
    WaylandDrag(const WaylandDrag&) = delete;
    WaylandDrag& operator=(const WaylandDrag&) = delete;

    // A drag from `window`, its button held: false when it cannot be (no press known).
    bool start(SDL_Window* window);
    // Its events read so far (SDL reads the connection), into `out` as GLWidgets'.
    void dispatch(std::deque<Event>& out);
    // A drag is going on (as the one it came from, or over a window): its events come often.
    bool active() const;

    struct Impl;

private:
    explicit WaylandDrag(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace glw::platform::sdl
