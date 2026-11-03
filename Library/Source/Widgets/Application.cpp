// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Application.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
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

// The window a dragged tab would open: a tinted rectangle, seen through.
class WindowPreview final : public Widget {
protected:
    void paint(PaintContext& context) override {
        context.painter.fillRect({0, 0, size().width, size().height}, context.ui.theme.dock.activeLine);
    }
};

constexpr float previewOpacity = 0.35f;

} // namespace

struct Application::Impl final : DockingHost {
    Application& app;
    std::unique_ptr<platform::Platform> platform;
    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<paint::Renderer> renderer;
    std::unique_ptr<text::FontLibrary> fonts;
    std::unique_ptr<text::GlyphAtlas> atlas;
    std::unique_ptr<Translator> translator;
    std::unique_ptr<UiContext> context;
    std::vector<std::unique_ptr<Window>> opened;       // last: closed before what they draw with
    std::vector<Window*> order;                         // the windows from the one in front (focused last) back
    std::unique_ptr<Window> preview;                    // showWindowPreview's, made the first time; not one of `opened`
    bool quitting = false;
    int exitCode = 0;
    std::function<void()> onQuitting;
    bool toldQuitting = false;
    bool inFrame = false;

    explicit Impl(Application& app) : app(app) {}

    Window* find(platform::WindowId id) const {
        for (const auto& w : opened) {
            if (w->id() == id)
                return w.get();
        }
        return nullptr;
    }

    Window* find(const UiRoot& root) const {
        for (const auto& w : opened) {
            if (&w->root_ == &root)
                return w.get();
        }
        return nullptr;
    }

    // Every window that is not a tool window (a floating panel's) closes, or quit was called: told once.
    void quittingSoon() {
        if (toldQuitting)
            return;
        const bool open = std::any_of(opened.begin(), opened.end(), [](const auto& w) { return !w->closing_ && !w->owner_; });
        if (open && !quitting)
            return;
        toldQuitting = true;
        if (onQuitting)
            onQuitting();
    }

    void toFront(Window* window) {
        std::erase(order, window);
        order.insert(order.begin(), window);
    }

    Window& open(const WindowOptions& options, std::unique_ptr<Widget> content, Window* owner, bool focus = true) {
        opened.push_back(std::unique_ptr<Window>(new Window(app, options, std::move(content), owner, false, focus)));
        toFront(opened.back().get());
        return *opened.back();
    }

    // `closed` is destroyed: the windows above it are on their own (the system would close them with it).
    void destroy(std::unique_ptr<Window> closed, bool unparent) {
        std::erase(order, closed.get());
        for (const auto& w : opened) {
            if (w->owner_ == closed.get()) {
                if (unparent)
                    w->window_->setParent(nullptr);
                w->owner_ = nullptr;
            }
        }
        closed.reset();
    }

    // The windows closed during the events go: told first, while every tree is whole (a floating panel goes home from its
    // window), then destroyed one at a time (what one destroys may close others).
    void closeWindows() {
        for (bool told = true; told;) {
            told = false;
            for (std::size_t i = 0; i < opened.size(); ++i) {
                if (Window& w = *opened[i]; w.closing_ && !w.told_) {
                    w.told_ = true;
                    w.root_.windowClosing();
                    told = true;
                }
            }
        }
        for (;;) {
            const auto it = std::find_if(opened.begin(), opened.end(), [](const auto& w) { return w->closing_; });
            if (it == opened.end())
                break;
            std::unique_ptr<Window> closed = std::move(*it);
            opened.erase(it);
            destroy(std::move(closed), true);
        }
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
            if (const auto size = commands->beginSurfacePass(*w->surface_, context->theme.palette.window)) {
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

    void waitForSurfaces() {
        for (const auto& w : opened) {
            if (!w->closing_ && w->window_->visible() && w->windowState() != platform::WindowState::Minimized)
                w->surface_->waitUntilReady();
        }
        if (preview && preview->window_->visible())
            preview->surface_->waitUntilReady();
    }

    std::vector<Window*> windowsToDraw(bool all) const {
        std::vector<Window*> result;
        for (const auto& w : opened) {
            if (w->closing_ || !w->window_->visible() || w->windowState() == platform::WindowState::Minimized)
                continue;
            if (all || w->root_.needsPaint() || w->root_.needsLayout())
                result.push_back(w.get());
        }
        if (preview && preview->window_->visible() && (all || preview->root_.needsPaint()))
            result.push_back(preview.get());
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
        if (const auto* focus = std::get_if<platform::WindowFocusChanged>(&event); focus && focus->focused)
            toFront(window);
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
        // shown before the system goes on resizing: the window and what is drawn in it keep the same size
        device->waitIdle();
    }

    // ---- docking across the windows ----

    // GLW_DOCKING=restricted: docking as where the system places windows (Wayland), to try it anywhere
    bool placesWindows() const override {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)     // getenv: read, not kept
#endif
        const char* docking = std::getenv("GLW_DOCKING");
#ifdef _MSC_VER
#pragma warning(pop)
#endif
        return platform->placesWindows() && !(docking && std::string_view(docking) == "restricted");
    }
    Point mouseOnScreen() const override { return platform->mousePosition(); }

    Point toScreen(const UiRoot& root, Point point) const override {
        const Window* w = find(root);
        return w ? w->window_->toScreen(point) : point;
    }

    Hit windowAt(Point screen, const UiRoot* except) const override {
        for (Window* w : order) {
            if (w->closing_ || &w->root_ == except || !w->window_->visible() || w->windowState() == platform::WindowState::Minimized)
                continue;
            const Point at = w->window_->position();
            const Size size = w->window_->screenSize();
            if (Rect{at.x, at.y, size.width, size.height}.contains(screen))
                return {&w->root_, w->window_->fromScreen(screen)};
        }
        return {};
    }

    void captureMouse(bool capture) override { platform->captureMouse(capture); }

    UiRoot& openWindow(NewWindow window) override {
        Window* owner = window.owner ? find(*window.owner) : nullptr;
        Window& made =
            open({.title = window.title, .size = window.size, .position = window.position}, std::move(window.content), owner, window.focus);
        if (window.titleCenter && made.frame_)
            made.frame_->titleBar().setCenter(std::move(window.titleCenter));
        if (!window.titleBar && made.frame_)
            made.frame_->titleBar().setVisible(false);
        return made.root_;
    }

    void closeWindow(const UiRoot& root) override {
        if (Window* w = find(root))
            w->close();
    }

    std::vector<UiRoot*> windows() const override {
        std::vector<UiRoot*> roots;
        for (const auto& w : opened)
            roots.push_back(&w->root_);
        return roots;
    }

    bool closing(const UiRoot& root) const override {
        const Window* w = find(root);
        return !w || w->closing_;
    }

    void setWindowVisible(const UiRoot& root, bool visible) override {
        Window* w = find(root);
        if (!w || w->closing_ || w->window_->visible() == visible)
            return;
        if (visible) {
            w->window_->show();
            w->root_.invalidatePaint();
        } else {
            w->window_->hide();
        }
    }

    void setWindowOwner(const UiRoot& root, const UiRoot* owner) override {
        Window* w = find(root);
        Window* o = owner ? find(*owner) : nullptr;
        if (!w || w == o || w->owner_ == o)
            return;
        w->owner_ = o;
        w->window_->setParent(o ? o->window_.get() : nullptr);
    }

    void raiseWindow(const UiRoot& root) override {
        if (Window* w = find(root)) {
            w->window_->raise();
            toFront(w);
        }
    }

    Point windowPosition(const UiRoot& root) const override {
        const Window* w = find(root);
        return w ? w->window_->position() : Point{};
    }

    void moveWindow(const UiRoot& root, Point position) override {
        if (Window* w = find(root))
            w->window_->setPosition(position);
    }

    void setWindowOpacity(const UiRoot& root, float opacity) override {
        if (Window* w = find(root))
            w->window_->setOpacity(opacity);
    }

    void showWindowPreview(std::optional<Point> position, Size size) override {
        if (!position || size.width <= 0 || size.height <= 0) {
            if (preview && preview->window_->visible())
                preview->window_->hide();
            return;
        }
        if (!preview) {
            preview.reset(new Window(app, {.title = "", .size = size, .customChrome = false}, std::make_unique<WindowPreview>(), nullptr, true));
            preview->window_->setOpacity(previewOpacity);
        }
        platform::Window& w = *preview->window_;
        if (w.size() != size) {
            w.setSize(size);
            preview->root_.setSize(size);       // its events are not read: it is told here
        }
        w.setPosition(*position);
        if (!w.visible()) {
            w.show();
            preview->root_.setScale(w.scale());
            preview->root_.invalidatePaint();
        }
    }
};

DockingHost::~DockingHost() = default;

// ---- window ----

Window::Window(Application& app, const WindowOptions& options, std::unique_ptr<Widget> content, Window* owner, bool overlay,
               bool focus)
    : app_(app), root_(*app.impl_->context, this), owner_(owner) {
    Application::Impl& impl = *app.impl_;
    title_ = options.title;
    window_ = impl.platform->createWindow({.title = impl.translator->resolve(title_),
                                           .size = options.size,
                                           .resizable = !overlay,
                                           .borderless = options.customChrome || overlay,
                                           .position = options.position,
                                           .tool = owner != nullptr,
                                           .overlay = overlay});
    if (owner)
        window_->setParent(owner->window_.get());
    surface_ = impl.device->createSurface(*window_);
    if (impl.translator->rightToLeft())
        root_.setLayoutDirection(LayoutDirection::RightToLeft);
    if (options.customChrome && !overlay) {
        auto frame = std::make_unique<WindowFrame>(std::move(content));
        frame->titleBar().setTitle(title_);
        frame_ = frame.get();
        root_.setContent(std::move(frame));
        window_->setHitTest([this](Point p) { return root_.windowHitTest(p); });
    } else {
        root_.setContent(std::move(content));
    }
    root_.setSize(window_->size());
    root_.setScale(window_->scale());
    if (!overlay)
        window_->show(focus);
}

Window::~Window() {
    window_->setHitTest(nullptr);       // it asks the root, which goes first
}

platform::WindowId Window::id() const {
    return window_->id();
}

void Window::setTitle(LocalizedText title) {
    title_ = std::move(title);
    window_->setTitle(app_.impl_->translator->resolve(title_));
    if (frame_)
        frame_->titleBar().setTitle(title_);
}

void Window::languageChanged() {
    window_->setTitle(app_.impl_->translator->resolve(title_));
    root_.setLayoutDirection(app_.impl_->translator->rightToLeft() ? LayoutDirection::RightToLeft : LayoutDirection::LeftToRight);
    root_.languageChanged();
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

void Window::setTextInputArea(const Rect& area) {
    window_->setTextInputArea(area);
}

std::string Window::clipboardText() const {
    return app_.impl_->platform->clipboardText();
}

void Window::setClipboardText(const std::string& text) {
    app_.impl_->platform->setClipboardText(text);
}

// ---- application ----

Application::Application() : impl_(std::make_unique<Impl>(*this)) {
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
    d.translator = std::make_unique<Translator>();
    d.context = std::make_unique<UiContext>(UiContext{*d.fonts, *d.atlas, *d.translator, Theme::dark(), &d});
    d.platform->setExposeHandler([&d](platform::WindowId id) { d.exposed(id); });
}

Application::~Application() {
    Impl& d = *impl_;
    d.platform->setExposeHandler(nullptr);
    d.preview.reset();
    // the last opened first (a floating panel's window before its owner), one at a time: what one destroys may close others
    while (!d.opened.empty()) {
        std::unique_ptr<Window> closed = std::move(d.opened.back());
        d.opened.pop_back();
        d.destroy(std::move(closed), false);
    }
}

Window& Application::openWindow(const WindowOptions& options, std::unique_ptr<Widget> content) {
    return impl_->open(options, std::move(content), nullptr);
}

std::span<const std::unique_ptr<Window>> Application::windows() const {
    return impl_->opened;
}

int Application::run(int frames) {
    Impl& d = *impl_;
    bool retry = false;     // a window had no image to draw into: drawn again soon
    int drawn = 0, attempts = 0;
    while (!d.quitting && !d.opened.empty()) {
        if (frames < 0) {
            if (d.windowsToDraw(false).empty())
                d.platform->waitForEvents();
            else if (retry)
                d.platform->waitForEvents(std::chrono::milliseconds(16));
        }
        retry = false;
        // the windows' next images free (the frame before shown, at the display's refresh) before the input is read: what
        // is drawn next shows the newest of it, not what came a refresh before
        d.waitForSurfaces();

        platform::Event event;
        while (!d.quitting && d.platform->pollEvent(event))
            d.handle(event);
        d.quittingSoon();
        d.closeWindows();
        if (d.quitting || d.opened.empty())
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

void Application::setOnQuitting(std::function<void()> handler) {
    impl_->onQuitting = std::move(handler);
}

DockingHost& Application::docking() {
    return *impl_;
}

void Application::quit(int exitCode) {
    impl_->quitting = true;
    impl_->exitCode = exitCode;
    impl_->platform->wakeUp();
}

const UiContext& Application::context() const {
    return *impl_->context;
}

void Application::setTheme(const Theme& theme) {
    impl_->context->theme = theme;
    for (const auto& w : impl_->opened)
        w->root_.styleChanged();
}

void Application::setLanguage(const std::string& language) {
    impl_->translator->setLanguage(language);
    for (const auto& w : impl_->opened)
        w->languageChanged();
}

Translator& Application::translator() {
    return *impl_->translator;
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
