// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Two windows drawn by one GPU device, as the windows of a docking layout will be, each drawn only when it needs it: the
// application sleeps in waitForEvents otherwise. Escape quits, closing a window closes it.
//
//   hello_window [--frames N]
//
// --frames N draws both windows N times in a row and exits: a smoke test of the platform and the GPU backend.
#include <GLWidgets/Core/Color.hpp>
#include <GLWidgets/Platform/Platform.hpp>
#include <GLWidgets/Rhi/Device.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace platform = glw::platform;

template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

struct View {
    std::unique_ptr<platform::Window> window;
    std::unique_ptr<glw::rhi::Surface> surface;     // after the window: destroyed before it
    glw::Color background;
    bool dirty = true;
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;    // -1: draw when needed, until the windows are closed
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
    }

    try {
        auto system = platform::createPlatform();
#ifdef NDEBUG
        auto device = glw::rhi::createDevice(*system);
#else
        auto device = glw::rhi::createDevice(*system, {.debug = true});
#endif
        std::printf("platform: %s, GPU: %s\n", std::string(system->name()).c_str(), std::string(device->backendName()).c_str());

        std::vector<View> views;
        auto open = [&](std::string title, glw::Size size, glw::Color background) {
            View view;
            view.window = system->createWindow({.title = std::move(title), .size = size});
            view.surface = device->createSurface(*view.window);
            view.background = background;
            view.window->show();
            views.push_back(std::move(view));
        };
        open("GLWidgets", {960, 600}, {0.12f, 0.13f, 0.16f, 1});
        open("GLWidgets — پنجره‌ی شناور", {420, 300}, {0.16f, 0.22f, 0.32f, 1});

        auto find = [&](platform::WindowId id) -> View* {
            auto it = std::find_if(views.begin(), views.end(), [&](const View& v) { return v.window->id() == id; });
            return it != views.end() ? &*it : nullptr;
        };
        auto invalidate = [&](platform::WindowId id) {
            if (View* view = find(id))
                view->dirty = true;
        };

        int drawn = 0;
        bool quit = false;
        while (!quit && !views.empty()) {
            const bool dirty = std::any_of(views.begin(), views.end(), [](const View& v) { return v.dirty; });
            if (frames < 0 && !dirty)
                system->waitForEvents();

            platform::Event event;
            while (system->pollEvent(event)) {
                std::visit(Overloaded{
                               [&](const platform::QuitRequested&) { quit = true; },
                               [&](const platform::WindowCloseRequested& e) {
                                   std::erase_if(views, [&](const View& v) { return v.window->id() == e.window; });
                               },
                               [&](const platform::WindowResized& e) { invalidate(e.window); },
                               [&](const platform::WindowScaleChanged& e) { invalidate(e.window); },
                               [&](const platform::WindowExposed& e) { invalidate(e.window); },
                               [&](const platform::KeyChanged& e) {
                                   if (e.pressed && e.key == platform::Key::Escape)
                                       quit = true;
                               },
                               [](const auto&) {},
                           },
                           event);
            }
            if (quit)
                break;

            if (frames >= 0) {
                for (View& view : views)
                    view.dirty = true;
            }
            if (std::none_of(views.begin(), views.end(), [](const View& v) { return v.dirty; }))
                continue;

            // one command list for every window drawn this frame
            auto commands = device->beginCommands();
            for (View& view : views) {
                if (!view.dirty)
                    continue;
                if (commands->beginSurfacePass(*view.surface, view.background))
                    commands->endPass();
                view.dirty = false;
            }
            commands->submit();

            if (frames >= 0 && ++drawn == frames) {
                std::printf("drew %d frames\n", drawn);
                break;
            }
        }
        views.clear();      // the surfaces before the device, the windows before the platform
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "hello_window: %s\n", e.what());
        return 1;
    }
}
