// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/WaylandDrag.hpp"

#include "Platform/Sdl/SdlWindow.hpp"

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>

#include <wayland-client.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace glw::platform::sdl {

namespace {

// GLWidgets' own kind of data: a drag of the application's, between its windows
constexpr const char* mimeType = "application/x-glwidgets-drag";

} // namespace

struct WaylandDrag::Impl {
    wl_display* display = nullptr;
    wl_event_queue* queue = nullptr;
    wl_display* wrapped = nullptr;          // the display, its requests' objects in `queue`
    wl_registry* registry = nullptr;
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_data_device_manager* manager = nullptr;
    wl_data_device* device = nullptr;
    std::uint32_t managerVersion = 0;
    std::uint32_t pressSerial = 0;          // of the last press of a button: what a drag starts from

    // the drag from here
    wl_data_source* source = nullptr;
    // a drag over a window of the application: its offer, whether it is ours, the window, where
    wl_data_offer* offer = nullptr;
    std::vector<std::pair<wl_data_offer*, bool>> offers;    // offered (the compositor's data_offer), ours or not
    bool ours = false;
    SDL_WindowID over = 0;
    Point at;
    bool dropped = false;

    std::deque<Event>* out = nullptr;       // while it dispatches

    ~Impl() {
        if (source)
            wl_data_source_destroy(source);
        for (auto& [o, _] : offers)
            wl_data_offer_destroy(o);
        if (device) {
            if (managerVersion >= 2)
                wl_data_device_release(device);
            else
                wl_data_device_destroy(device);
        }
        if (manager)
            wl_data_device_manager_destroy(manager);
        if (pointer)
            wl_pointer_destroy(pointer);
        if (seat)
            wl_seat_destroy(seat);
        if (registry)
            wl_registry_destroy(registry);
        if (wrapped)
            wl_proxy_wrapper_destroy(wrapped);
        if (queue)
            wl_event_queue_destroy(queue);
    }

    void emit(Event e) {
        if (out)
            out->push_back(std::move(e));
    }

    // The application's window of `surface`, and a point on it in its dips.
    SDL_WindowID windowOf(wl_surface* surface) const {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        SDL_WindowID found = 0;
        for (int i = 0; windows && i < count && !found; ++i) {
            if (SDL_GetPointerProperty(SDL_GetWindowProperties(windows[i]), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr) == surface)
                found = SDL_GetWindowID(windows[i]);
        }
        SDL_free(windows);
        return found;
    }
    Point pointOn(SDL_WindowID window, wl_fixed_t x, wl_fixed_t y) const {
        const float f = SdlWindow::dipsPerUnit(SDL_GetWindowFromID(window));
        return {static_cast<float>(wl_fixed_to_double(x)) * f, static_cast<float>(wl_fixed_to_double(y)) * f};
    }

    bool isOurs(wl_data_offer* o) const {
        for (const auto& [offered, mine] : offers)
            if (offered == o)
                return mine;
        return false;
    }
    void forget(wl_data_offer* o) {
        for (auto it = offers.begin(); it != offers.end(); ++it) {
            if (it->first == o) {
                wl_data_offer_destroy(o);
                offers.erase(it);
                return;
            }
        }
    }
};

namespace {

using Impl = WaylandDrag::Impl;

// ---- the registry: the seat and the data device manager ----

void global(void* data, wl_registry* registry, std::uint32_t name, const char* interface, std::uint32_t version) {
    auto& d = *static_cast<Impl*>(data);
    if (!d.seat && std::strcmp(interface, wl_seat_interface.name) == 0) {
        d.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min<std::uint32_t>(version, 5)));
    } else if (!d.manager && std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        d.managerVersion = std::min<std::uint32_t>(version, 3);
        d.manager = static_cast<wl_data_device_manager*>(wl_registry_bind(registry, name, &wl_data_device_manager_interface, d.managerVersion));
    }
}

void globalRemove(void*, wl_registry*, std::uint32_t) {}

const wl_registry_listener registryListener{global, globalRemove};

// ---- the pointer: only the serial of a press ----

void pointerEnter(void*, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {}
void pointerLeave(void*, wl_pointer*, std::uint32_t, wl_surface*) {}
void pointerMotion(void*, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}
void pointerButton(void* data, wl_pointer*, std::uint32_t serial, std::uint32_t, std::uint32_t, std::uint32_t state) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED)
        static_cast<Impl*>(data)->pressSerial = serial;
}
void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}

wl_pointer_listener makePointerListener() {
    wl_pointer_listener l{};        // (the events of later versions than the one bound are not sent)
    l.enter = pointerEnter;
    l.leave = pointerLeave;
    l.motion = pointerMotion;
    l.button = pointerButton;
    l.axis = pointerAxis;
    l.frame = pointerFrame;
    l.axis_source = pointerAxisSource;
    l.axis_stop = pointerAxisStop;
    l.axis_discrete = pointerAxisDiscrete;
    return l;
}
const wl_pointer_listener pointerListener = makePointerListener();

void seatCapabilities(void* data, wl_seat* seat, std::uint32_t capabilities) {
    auto& d = *static_cast<Impl*>(data);
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !d.pointer) {
        d.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(d.pointer, &pointerListener, &d);
    }
}
void seatName(void*, wl_seat*, const char*) {}

const wl_seat_listener seatListener{seatCapabilities, seatName};

// ---- offers: of what kinds ----

void offerOffer(void* data, wl_data_offer* offer, const char* type) {
    auto& d = *static_cast<Impl*>(data);
    if (std::strcmp(type, mimeType) != 0)
        return;
    for (auto& [o, mine] : d.offers)
        if (o == offer)
            mine = true;
}
void offerSourceActions(void*, wl_data_offer*, std::uint32_t) {}
void offerAction(void*, wl_data_offer*, std::uint32_t) {}

const wl_data_offer_listener offerListener{offerOffer, offerSourceActions, offerAction};

// ---- the data device: a drag over the application's windows ----

void deviceDataOffer(void* data, wl_data_device*, wl_data_offer* offer) {
    auto& d = *static_cast<Impl*>(data);
    d.offers.emplace_back(offer, false);
    wl_data_offer_add_listener(offer, &offerListener, &d);
}

void deviceEnter(void* data, wl_data_device*, std::uint32_t serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer) {
    auto& d = *static_cast<Impl*>(data);
    d.offer = offer;
    d.dropped = false;
    d.over = surface ? d.windowOf(surface) : 0;
    // ours, from one of its windows (while the drag from here goes on): taken, as a move
    d.ours = offer && d.source && d.isOurs(offer) && d.over;
    if (!offer)
        return;
    if (d.ours) {
        wl_data_offer_accept(offer, serial, mimeType);
        if (d.managerVersion >= 3)
            wl_data_offer_set_actions(offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
        d.at = d.pointOn(d.over, x, y);
        d.emit(DragMoved{d.over, d.at});
    } else {
        wl_data_offer_accept(offer, serial, nullptr);
    }
}

void deviceLeave(void* data, wl_data_device*) {
    auto& d = *static_cast<Impl*>(data);
    if (d.ours && !d.dropped)
        d.emit(DragLeft{d.over});
    if (d.offer)
        d.forget(std::exchange(d.offer, nullptr));
    d.ours = false;
    d.over = 0;
}

void deviceMotion(void* data, wl_data_device*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto& d = *static_cast<Impl*>(data);
    if (!d.ours)
        return;
    d.at = d.pointOn(d.over, x, y);
    d.emit(DragMoved{d.over, d.at});
}

void deviceDrop(void* data, wl_data_device*) {
    auto& d = *static_cast<Impl*>(data);
    if (!d.ours || !d.offer)
        return;
    d.dropped = true;
    d.emit(DragDropped{d.over, d.at});
    // nothing to read (the drag is the application's): done
    if (d.managerVersion >= 3)
        wl_data_offer_finish(d.offer);
}

void deviceSelection(void* data, wl_data_device*, wl_data_offer* offer) {
    // (the clipboard is SDL's: an offer of it here is not kept)
    if (offer)
        static_cast<Impl*>(data)->forget(offer);
}

const wl_data_device_listener deviceListener{deviceDataOffer, deviceEnter, deviceLeave, deviceMotion, deviceDrop, deviceSelection};

// ---- the drag from here ----

void sourceTarget(void*, wl_data_source*, const char*) {}
void sourceSend(void*, wl_data_source*, const char*, std::int32_t fd) {
    close(fd);          // nothing is sent: the drag is the application's
}
void sourceEnded(Impl& d, bool dropped) {
    if (d.source) {
        wl_data_source_destroy(d.source);
        d.source = nullptr;
    }
    d.emit(DragEnded{dropped});
}
void sourceCancelled(void* data, wl_data_source*) {
    sourceEnded(*static_cast<Impl*>(data), false);
}
void sourceDropPerformed(void*, wl_data_source*) {}
void sourceFinished(void* data, wl_data_source*) {
    sourceEnded(*static_cast<Impl*>(data), true);
}
void sourceAction(void*, wl_data_source*, std::uint32_t) {}

const wl_data_source_listener sourceListener{sourceTarget, sourceSend, sourceCancelled, sourceDropPerformed, sourceFinished, sourceAction};

} // namespace

WaylandDrag::WaylandDrag(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

WaylandDrag::~WaylandDrag() = default;

std::unique_ptr<WaylandDrag> WaylandDrag::make() {
    const char* driver = SDL_GetCurrentVideoDriver();
    if (!driver || std::strcmp(driver, "wayland") != 0)
        return nullptr;
    auto* display = static_cast<wl_display*>(SDL_GetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, nullptr));
    if (!display)
        return nullptr;
    auto d = std::make_unique<Impl>();
    d->display = display;
    d->queue = wl_display_create_queue(display);
    d->wrapped = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(d->wrapped), d->queue);
    d->registry = wl_display_get_registry(d->wrapped);
    wl_registry_add_listener(d->registry, &registryListener, d.get());
    wl_display_roundtrip_queue(display, d->queue);          // the globals
    if (!d->seat || !d->manager)
        return nullptr;
    wl_seat_add_listener(d->seat, &seatListener, d.get());
    d->device = wl_data_device_manager_get_data_device(d->manager, d->seat);
    wl_data_device_add_listener(d->device, &deviceListener, d.get());
    wl_display_roundtrip_queue(display, d->queue);          // the seat's capabilities: its pointer
    return std::unique_ptr<WaylandDrag>(new WaylandDrag(std::move(d)));
}

bool WaylandDrag::start(SDL_Window* window) {
    Impl& d = *impl_;
    auto* surface = static_cast<wl_surface*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
    if (!surface || !d.pressSerial || d.source)
        return false;
    d.source = wl_data_device_manager_create_data_source(d.manager);
    wl_data_source_add_listener(d.source, &sourceListener, &d);
    wl_data_source_offer(d.source, mimeType);
    if (d.managerVersion >= 3)
        wl_data_source_set_actions(d.source, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
    wl_data_device_start_drag(d.device, d.source, surface, nullptr, d.pressSerial);
    wl_display_flush(d.display);
    return true;
}

void WaylandDrag::dispatch(std::deque<Event>& out) {
    Impl& d = *impl_;
    d.out = &out;
    wl_display_dispatch_queue_pending(d.display, d.queue);
    d.out = nullptr;
    wl_display_flush(d.display);
}

bool WaylandDrag::active() const {
    return impl_->source != nullptr || impl_->ours;
}

} // namespace glw::platform::sdl
