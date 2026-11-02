// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Application.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <variant>
#include <vector>

namespace glw {

namespace {

// The window an event is for (0 for those of the application).
platform::WindowId windowOf(const platform::Event& event) {
    return std::visit(
        [](const auto& e) -> platform::WindowId {
            if constexpr (requires { e.window; })
                return e.window;
            else
                return 0;
        },
        event);
}

} // namespace

struct Application::Impl {
    std::unique_ptr<platform::Platform> platform;
    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<paint::Renderer> renderer;
    std::unique_ptr<text::FontLibrary> fonts;
    std::unique_ptr<text::GlyphAtlas> atlas;
    std::unique_ptr<UiContext> context;
    std::vector<std::unique_ptr<Window>> windows;      // last: closed before what they draw with
    bool quitting = false;
    int exitCode = 0;
    bool inFrame = false;

    Window* find(platform::WindowId id) const {
        for (const auto& w : windows) {
            if (w->id() == id)
                return w.get();
        }
        return nullptr;
    }

    // Lays out, paints and presents `which`. False when one had no image to draw into (Vulkan remakes its swap chain after a
    // window is shown or resized): it stays to be drawn.
    bool draw(const std::vector<Window*>& which) {
        inFrame = true;
        for (Window* w : which)
            w->root_.paint(w->list_);       // paint fills the glyph atlas: before the atlas goes to the GPU
        auto commands = device->beginCommands();
        if (atlas->upload(*device, *commands, *renderer)) {
            // the atlas was full and started again: what was painted with it, again
            for (Window* w : which)
                w->root_.paint(w->list_);
            atlas->upload(*device, *commands, *renderer);
        }
        bool all = true;
        for (Window* w : which) {
            renderer->upload(*commands, w->list_, w->buffers_);
            if (const auto size = commands->beginSurfacePass(*w->surface_, context->theme.window)) {
                renderer->draw(*commands, w->list_, w->buffers_, w->surface_->format(), *size, w->root_.scale());
                commands->endPass();
            } else {
                w->root_.invalidatePaint();
                all = false;
            }
        }
        commands->submit();
        inFrame = false;
        return all;
    }

    std::vector<Window*> windowsToDraw(bool all) const {
        std::vector<Window*> result;
        for (const auto& w : windows) {
            if (w->closing_ || w->windowState() == platform::WindowState::Minimized)
                continue;
            if (all || w->root_.needsPaint() || w->root_.needsLayout())
                result.push_back(w.get());
        }
        return result;
    }

    void handle(const platform::Event& event) {
        if (std::holds_alternative<platform::QuitRequested>(event)) {
            quitting = true;
            return;
        }
        Window* window = find(windowOf(event));
        if (!window)
            return;
        if (std::holds_alternative<platform::WindowCloseRequested>(event))
            window->close();
        else
            window->root_.dispatch(event);
    }

    // While the user resizes a window, the system's loop runs instead of ours: the window is drawn from in there.
    void exposed(platform::WindowId id) {
        Window* window = find(id);
        if (inFrame || !window || window->closing_)
            return;
        window->root_.setSize(window->window_->size());
        window->root_.setScale(window->window_->scale());
        window->root_.invalidatePaint();
        draw({window});
    }
};

// ---- window ----

Window::Window(Application& app, const WindowOptions& options, std::unique_ptr<Widget> content)
    : app_(app), root_(*app.impl_->context, this) {
    Application::Impl& impl = *app.impl_;
    window_ = impl.platform->createWindow({.title = options.title, .size = options.size, .borderless = options.customChrome});
    surface_ = impl.device->createSurface(*window_);
    if (options.customChrome) {
        auto frame = std::make_unique<WindowFrame>(std::move(content));
        frame->titleBar().setTitle(options.title);
        frame_ = frame.get();
        root_.setContent(std::move(frame));
        window_->setHitTest([this](Point p) { return root_.windowHitTest(p); });
    } else {
        root_.setContent(std::move(content));
    }
    root_.setSize(window_->size());
    root_.setScale(window_->scale());
    window_->show();
}

Window::~Window() {
    window_->setHitTest(nullptr);       // it asks the root, which goes first
}

platform::WindowId Window::id() const {
    return window_->id();
}

void Window::setTitle(const std::string& title) {
    window_->setTitle(title);
    if (frame_)
        frame_->titleBar().setTitle(title);
}

platform::WindowState Window::windowState() const {
    return window_->state();
}

void Window::minimizeWindow() {
    window_->minimize();
}

void Window::toggleMaximizeWindow() {
    if (window_->state() == platform::WindowState::Maximized)
        window_->restore();
    else
        window_->maximize();
}

void Window::startTextInput() {
    window_->startTextInput();
}

void Window::stopTextInput() {
    window_->stopTextInput();
}

// ---- application ----

Application::Application() : impl_(std::make_unique<Impl>()) {
    Impl& d = *impl_;
    d.platform = platform::createPlatform();
#ifdef NDEBUG
    d.device = rhi::createDevice(*d.platform);
#else
    d.device = rhi::createDevice(*d.platform, {.debug = true});
#endif
    d.renderer = std::make_unique<paint::Renderer>(*d.device);
    d.fonts = std::make_unique<text::FontLibrary>();
    d.atlas = std::make_unique<text::GlyphAtlas>(d.renderer->addTexture());
    d.context = std::make_unique<UiContext>(UiContext{*d.fonts, *d.atlas, Theme{}});
    d.platform->setExposeHandler([&d](platform::WindowId id) { d.exposed(id); });
}

Application::~Application() {
    impl_->platform->setExposeHandler(nullptr);
    impl_->windows.clear();
}

Window& Application::openWindow(const WindowOptions& options, std::unique_ptr<Widget> content) {
    impl_->windows.push_back(std::unique_ptr<Window>(new Window(*this, options, std::move(content))));
    return *impl_->windows.back();
}

std::span<const std::unique_ptr<Window>> Application::windows() const {
    return impl_->windows;
}

int Application::run(int frames) {
    Impl& d = *impl_;
    bool retry = false;     // a window had no image to draw into: drawn again soon
    int drawn = 0, attempts = 0;
    while (!d.quitting && !d.windows.empty()) {
        if (frames < 0) {
            if (d.windowsToDraw(false).empty())
                d.platform->waitForEvents();
            else if (retry)
                d.platform->waitForEvents(std::chrono::milliseconds(16));
        }
        retry = false;

        platform::Event event;
        while (!d.quitting && d.platform->pollEvent(event))
            d.handle(event);
        std::erase_if(d.windows, [](const auto& w) { return w->closing_; });
        if (d.quitting || d.windows.empty())
            break;

        const std::vector<Window*> which = d.windowsToDraw(frames >= 0);
        if (which.empty())
            continue;
        retry = !d.draw(which);

        // the smoke test counts the frames that reached the windows, and fails when they do not
        if (frames >= 0) {
            drawn += retry ? 0 : 1;
            if (drawn >= frames)
                return 0;
            if (++attempts == frames * 10) {
                std::fprintf(stderr, "GLWidgets: %d of %d frames reached the windows\n", drawn, attempts);
                return 1;
            }
        }
    }
    return d.exitCode;
}

void Application::quit(int exitCode) {
    impl_->quitting = true;
    impl_->exitCode = exitCode;
    impl_->platform->wakeUp();
}

UiContext& Application::context() {
    return *impl_->context;
}

const text::FontLibrary& Application::fonts() const {
    return *impl_->fonts;
}

platform::Platform& Application::platform() {
    return *impl_->platform;
}

rhi::Device& Application::device() {
    return *impl_->device;
}

paint::Renderer& Application::renderer() {
    return *impl_->renderer;
}

} // namespace glw
