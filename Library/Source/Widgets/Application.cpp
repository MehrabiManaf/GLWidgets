// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Application.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/Icon.hpp>
#include <GLWidgets/Widgets/ImageStore.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string_view>
#include <variant>
#include <limits>
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

// Where a popup `size` big goes (its parent's dips), from `anchor` (the same), on the part of the screen windows may cover.
// Beside its anchor (After), it goes to the start side first when `startFirst` (a submenu of a submenu that went there:
// they go on the way they went), and says in `atStart` where it went.
Point placePopup(const platform::Window& parent, const Rect& anchor, PopupPlacement placement, Size size, bool rtl, bool startFirst,
                 bool& atStart) {
    const Rect screen = parent.usableScreenArea();
    const Point a = parent.fromScreen({screen.x, screen.y}), b = parent.fromScreen({screen.right(), screen.bottom()});
    const Rect area{a.x, a.y, b.x - a.x, b.y - a.y};
    Point at;
    if (placement == PopupPlacement::Center) {
        at = {anchor.center().x - size.width / 2, anchor.center().y - size.height / 2};
    } else if (placement == PopupPlacement::Below) {
        at = {rtl ? anchor.right() - size.width : anchor.x, anchor.bottom()};
        // above, when it does not fit below and there is more room above
        if (at.y + size.height > area.bottom() && anchor.y - area.y > area.bottom() - anchor.bottom())
            at.y = anchor.y - size.height;
    } else {
        // the sides in this window's dips: the end side is the right in left to right
        const float right = anchor.right(), left = anchor.x - size.width;
        const bool fitsRight = right + size.width <= area.right(), fitsLeft = left >= area.x;
        const bool towardRight = rtl == startFirst;
        const bool goRight = towardRight ? fitsRight || !fitsLeft : !(fitsLeft || !fitsRight);
        at = {goRight ? right : left, anchor.y};
        atStart = goRight == rtl;
    }
    at.x = std::clamp(at.x, area.x, std::max(area.x, area.right() - size.width));
    at.y = std::clamp(at.y, area.y, std::max(area.y, area.bottom() - size.height));
    return at;
}

} // namespace

struct Application::Impl final : DockingHost {
    Application& app;
    std::optional<LayoutDirection> direction;           // the application's; none: the language's
    // The windows' layout: mirrored or not.
    LayoutDirection layoutDirection() const {
        if (direction && *direction != LayoutDirection::Inherit)
            return *direction;
        return translator->rightToLeft() ? LayoutDirection::RightToLeft : LayoutDirection::LeftToRight;
    }
    std::unique_ptr<platform::Platform> platform;
    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<paint::Renderer> renderer;
    std::unique_ptr<text::FontLibrary> fonts;
    std::unique_ptr<text::GlyphAtlas> atlas;
    std::unique_ptr<IconAtlas> icons;
    std::unique_ptr<ImageStore> images;
    std::unique_ptr<Translator> translator;
    std::unique_ptr<UiContext> context;
    Scheduler scheduler;                                // before the windows: their timers go first
    // Where the answers of the system's dialogs go (from any thread, maybe after the application is gone): to the
    // scheduler while there is one.
    struct Answers {
        std::mutex mutex;
        Scheduler* scheduler = nullptr;
    };
    std::shared_ptr<Answers> answers = std::make_shared<Answers>();
    // a theme eased to: the themes of the palettes between the one shown and its own, then it
    std::optional<Theme> themeTarget;
    Theme themeFrom;                                    // fading from it
    Animation themeFade{[this] { themeStepped(); }};
    // GLW_LOG_FRAMES: the frames drawn, each second that had some (what is idle draws none), and how evenly: the time
    // between two frames of a run (one drawn within 50 ms of the last), on average and at most, and how many of them were
    // more than one and a half refreshes apart (a refresh missed: a stutter)
    bool logFrames = false;
    int framesLogged = 0;
    TimePoint logSince;
    std::optional<TimePoint> lastFrame;
    double gapSum = 0, gapMost = 0;
    int gaps = 0, missed = 0;
    Window* systemMoving = nullptr;                     // a window of major tabs the system moves by its title bar
    std::vector<std::unique_ptr<Window>> opened;       // last: closed before what they draw with
    std::vector<Window*> order;                         // the windows from the one in front (focused last) back
    std::unique_ptr<Window> preview;                    // showWindowPreview's, made the first time; not one of `opened`
    std::vector<std::unique_ptr<Window>> popups;        // a submenu after its menu; not of `opened`
    bool focusLost = false;                             // a window lost the focus: maybe the application did
    bool quitting = false;
    int exitCode = 0;
    std::function<void()> onQuitting;
    std::function<bool()> onQuitRequested;
    std::function<void(bool)> onSystemThemeChanged;
    bool followsSystemColors = false;
    bool toldQuitting = false;
    bool inFrame = false;

    explicit Impl(Application& app) : app(app) {}

    // `theme` shown by every window
    void applyTheme(const Theme& theme) {
        context->theme = theme;
        for (const auto& w : opened) {
            w->roundCorners();
            w->root_.styleChanged();
        }
        for (const auto& w : popups) {
            w->roundCorners();
            w->root_.styleChanged();
        }
    }

    void themeStepped() {
        if (!themeTarget)
            return;
        if (!themeFade.running()) {
            applyTheme(*std::exchange(themeTarget, std::nullopt));
            return;
        }
        // the colors mixed, and the shapes: corners rounding, controls growing lower (a style changed)
        applyTheme(Theme::blend(themeFrom, *themeTarget, themeFade.value()));
    }

    Window* find(platform::WindowId id) const {
        for (const auto& w : opened) {
            if (w->id() == id)
                return w.get();
        }
        for (const auto& w : popups) {
            if (w->id() == id)
                return w.get();
        }
        return nullptr;
    }

    // ---- popups ----

    // `w` is `ancestor`, or came from it (a popup of a popup of it).
    static bool within(const Window* w, const Window* ancestor) {
        for (; w; w = w->owner_) {
            if (w == ancestor)
                return true;
        }
        return false;
    }

    // The last opened dialog still open.
    Window* topModal() const {
        for (auto it = popups.rbegin(); it != popups.rend(); ++it) {
            if ((*it)->modal_ && !(*it)->closing_)
                return it->get();
        }
        return nullptr;
    }

    // A dialog, or what one came from: only its own closeWindow closes it.
    bool kept(const Window* popup) const {
        for (const auto& p : popups) {
            if (p->modal_ && !p->closing_ && within(p.get(), popup))
                return true;
        }
        return false;
    }

    // The windows with a dialog open from them, dimmed; the others not.
    void updateScrims() {
        for (const auto& w : opened) {
            bool dimmed = false;
            for (const auto& p : popups)
                dimmed = dimmed || (p->modal_ && !p->closing_ && &p->base() == w.get());
            w->root_.setOverlay(dimmed ? std::optional<Color>(context->theme.dialog.scrim) : std::nullopt);
        }
    }

    // Every popup but `keep` and those it came from (the menu of a submenu): all when it is none or not a popup. Whether
    // one closes.
    bool closePopupsBut(const Window* keep) {
        bool closed = false;
        for (const auto& p : popups) {
            bool kept = false;
            for (const Window* w = keep; w && !kept; w = w->owner_)
                kept = w == p.get();
            if (!kept && !p->closing_ && !this->kept(p.get())) {
                p->closing_ = true;
                closed = closed || !p->tooltip_;       // a tooltip going does not take the click
            }
        }
        return closed;
    }

    // The last opened popup that takes the keyboard and came from `window` (or from a popup that did); none when there is
    // none open.
    Window* topPopupOf(const Window* window) const {
        for (auto it = popups.rbegin(); it != popups.rend(); ++it) {
            if ((*it)->closing_ || (*it)->tooltip_)
                continue;
            for (const Window* w = it->get(); w; w = w->owner_) {
                if (w == window)
                    return it->get();
            }
        }
        return nullptr;
    }

    // Those that opened from `window`, and from them.
    void closePopupsOf(const Window* window) {
        for (const auto& p : popups) {
            if (p.get() != window && within(p.get(), window) && !kept(p.get()))
                p->closing_ = true;
        }
    }

    // The application lost the focus (no window of it has the keyboard): its popups close.
    void checkFocus() {
        if (!std::exchange(focusLost, false) || popups.empty())
            return;
        const auto has = [](const auto& w) { return w->window_->focused(); };
        if (std::none_of(opened.begin(), opened.end(), has) && std::none_of(popups.begin(), popups.end(), has))
            closePopupsBut(nullptr);
    }

    // The popups closing, with those that opened from them and from the windows closing: told, then destroyed, the last
    // opened first.
    void closePopups() {
        for (bool more = true; more;) {
            more = false;
            for (const auto& p : popups) {
                if (!p->closing_ && p->owner_->closing_)
                    p->closing_ = more = true;
            }
        }
        for (const auto& p : popups) {
            if (p->closing_ && !p->told_) {
                p->told_ = true;
                p->root_.windowClosing();
            }
        }
        bool closed = false;
        for (std::size_t i = popups.size(); i-- > 0;) {
            if (popups[i]->closing_) {
                popups.erase(popups.begin() + static_cast<std::ptrdiff_t>(i));
                closed = true;
            }
        }
        if (closed)
            updateScrims();
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
        if (systemMoving == closed.get())
            systemMoving = nullptr;
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
        closePopups();
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

    // `w`'s list with glass in it: drawn into its own texture (its depth with it), cut at each capture (the texture
    // copied at half its size, for the glass after it to read), then copied onto the image of its surface (`size`).
    void drawWithGlass(Window& w, rhi::CommandList& commands, PixelSize size, const std::vector<std::size_t>& captures, bool depth) {
        const rhi::TextureFormat format = w.surface_->format();
        if (!w.offscreen_ || w.offscreen_->size() != size || w.offscreen_->format() != format) {
            const auto both = rhi::TextureUsage::Sampled | rhi::TextureUsage::RenderTarget;
            w.offscreen_ = device->createTexture({size, format, both});
            w.offscreenDepth_.reset();
            w.backdrop_ = device->createTexture({{std::max(1, (size.width + 1) / 2), std::max(1, (size.height + 1) / 2)}, format, both});
        }
        if (depth && !w.offscreenDepth_)
            w.offscreenDepth_ = device->createTexture({size, rhi::TextureFormat::Depth32Float, rhi::TextureUsage::DepthTarget});
        rhi::Texture* depthTexture = depth ? w.offscreenDepth_.get() : nullptr;
        const float scale = w.root_.scale();
        const rhi::Texture* backdrop = w.backdrop_.get();
        commands.beginTexturePass(*w.offscreen_, context->theme.palette.window, depthTexture, true);
        std::size_t from = 0;
        for (const std::size_t at : captures) {
            renderer->draw(commands, w.list_, w.buffers_, format, size, scale, depth, from, at, backdrop);
            commands.endPass();
            // what is under the glass that comes: halved (a linear blit)
            commands.blit(*w.offscreen_, *w.backdrop_);
            commands.beginTexturePass(*w.offscreen_, std::nullopt, depthTexture, false);
            from = at;
        }
        renderer->draw(commands, w.list_, w.buffers_, format, size, scale, depth, from, std::numeric_limits<std::size_t>::max(), backdrop);
        commands.endPass();
        commands.blitToSurface(*w.offscreen_, *w.surface_);
    }

    // Lays out, paints and presents `which`. False when one had no image to draw into (Vulkan remakes its swap chain after a
    // window is shown or resized): it stays to be drawn.
    bool draw(const std::vector<Window*>& which) {
        inFrame = true;
        renderer->setLighting(context->theme.lighting);     // what lights reliefs (a style's that has them)
        renderer->setEnvironment({.exposure = 1, .dark = context->theme.isDark()});       // ... and bricks
        for (Window* w : which)
            w->root_.paint(w->list_);       // paint fills the glyph atlas: before the atlas goes to the GPU
        auto commands = device->beginCommands();
        const bool glyphsAgain = atlas->upload(*device, *commands, *renderer);
        const bool iconsAgain = icons->upload(*device, *commands, *renderer);
        if (glyphsAgain || iconsAgain) {
            // an atlas was full and started again: what was painted with it, again
            for (Window* w : which)
                w->root_.paint(w->list_);
            atlas->upload(*device, *commands, *renderer);
            icons->upload(*device, *commands, *renderer);
        }
        images->upload(*device, *commands, *renderer);
        bool all = true;
        for (Window* w : which) {
            renderer->upload(*commands, w->list_, w->buffers_);
            // a depth buffer only for bricks that may pass through one another (falling into their places)
            const bool depth = w->list_.depth && !w->list_.sceneVertices.empty();
            // glass: drawn into a texture first, copied and blurred where the glass reads what is under it
            if (const std::vector<std::size_t> captures = paint::Renderer::backdropCaptures(w->list_); !captures.empty()) {
                if (const auto size = commands->acquireSurface(*w->surface_)) {
                    drawWithGlass(*w, *commands, *size, captures, depth);
                } else {
                    w->root_.invalidatePaint();
                    all = false;
                }
                continue;
            }
            if (const auto size = commands->beginSurfacePass(*w->surface_, context->theme.palette.window, depth)) {
                renderer->draw(*commands, w->list_, w->buffers_, w->surface_->format(), *size, w->root_.scale(), depth);
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

    void frameDrawn(std::size_t windows) {
        ++framesLogged;
        const TimePoint now = scheduler.now();
        if (lastFrame) {
            const double gap = std::chrono::duration<double, std::milli>(now - *lastFrame).count();
            if (gap < 50) {
                gapSum += gap;
                gapMost = std::max(gapMost, gap);
                ++gaps;
                missed += gap > 1.5 * 1000.0 / 60 ? 1 : 0;
            }
        }
        lastFrame = now;
        if (now - logSince >= std::chrono::seconds(1)) {
            std::fprintf(stderr, "GLWidgets: %d frames in %.1f s (%zu windows in the last)", framesLogged,
                         std::chrono::duration<double>(now - logSince).count(), windows);
            if (gaps > 0)
                std::fprintf(stderr, "; %.1f ms apart, at most %.1f; %d of %d late", gapSum / gaps, gapMost, missed, gaps);
            std::fprintf(stderr, "\n");
            framesLogged = gaps = missed = 0;
            gapSum = gapMost = 0;
            logSince = now;
        }
    }

    void waitForSurfaces() {
        for (const auto& w : opened) {
            if (!w->closing_ && w->window_->visible() && w->windowState() != platform::WindowState::Minimized)
                w->surface_->waitUntilReady();
        }
        for (const auto& w : popups) {
            if (!w->closing_ && w->window_->visible())
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
        for (const auto& w : popups) {
            if (!w->closing_ && w->window_->visible() && (all || w->root_.needsPaint() || w->root_.needsLayout()))
                result.push_back(w.get());
        }
        if (preview && preview->window_->visible() && (all || preview->root_.needsPaint()))
            result.push_back(preview.get());
        return result;
    }

    void handle(const platform::Event& event) {
        if (std::holds_alternative<platform::QuitRequested>(event)) {
            if (!onQuitRequested || onQuitRequested())
                quitting = true;
            return;
        }
        if (const auto* e = std::get_if<platform::SystemThemeChanged>(&event)) {
            if (followsSystemColors)
                app.setColorScheme(e->dark ? ColorScheme::Dark : ColorScheme::Light);
            if (onSystemThemeChanged)
                onSystemThemeChanged(e->dark);
            return;
        }
        if (handleSystemDrag(event))
            return;
        Window* window = find(windowOf(event));
        if (!window)
            return;
        // under a dialog: the keys and typed text are the dialog's (or those of a popup of it); the mouse does nothing, but
        // close the popups of the dialog when pressed; the window does not close
        if (Window* modal = topModal(); modal && !within(window, modal)) {
            if (std::holds_alternative<platform::KeyChanged>(event) || std::holds_alternative<platform::TextInput>(event) ||
                std::holds_alternative<platform::TextComposition>(event)) {
                topPopupOf(modal)->root_.dispatch(event);
                return;
            }
            if (const auto* button = std::get_if<platform::MouseButtonChanged>(&event); button && button->pressed)
                closePopupsBut(modal);
            if (std::holds_alternative<platform::MouseButtonChanged>(event) || std::holds_alternative<platform::MouseMoved>(event) ||
                std::holds_alternative<platform::MouseWheelScrolled>(event) || std::holds_alternative<platform::WindowCloseRequested>(event))
                return;
        }
        // the keys and typed text of a window with a menu open: the topmost menu's (the system may not move the keyboard
        // to it)
        if (std::holds_alternative<platform::KeyChanged>(event) || std::holds_alternative<platform::TextInput>(event) ||
            std::holds_alternative<platform::TextComposition>(event)) {
            if (Window* menu = topPopupOf(window); menu && menu != window) {
                menu->root_.dispatch(event);
                return;
            }
        }
        // a press outside the popups closes them; in a window that is not one of them, it does nothing else
        if (const auto* button = std::get_if<platform::MouseButtonChanged>(&event); button && button->pressed && !popups.empty()) {
            if (closePopupsBut(window) && !window->popup_)
                return;
        }
        if (const auto* focus = std::get_if<platform::WindowFocusChanged>(&event); focus && !focus->focused)
            focusLost = true;
        // the window they opened from resized, minimized or maximized: they would be out of place
        if (!window->popup_ && (std::holds_alternative<platform::WindowResized>(event) || std::holds_alternative<platform::WindowStateChanged>(event)))
            closePopupsOf(window);
        if (const auto* focus = std::get_if<platform::WindowFocusChanged>(&event); focus && focus->focused)
            toFront(window);
        if (std::holds_alternative<platform::WindowCloseRequested>(event))
            window->requestClose();
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

    // ---- a window of major tabs moved by the system, merging into another (MajorTabStrip::systemMoved) ----

    // Its tab strip, in its title bar.
    static MajorTabStrip* stripOf(const Widget& widget, int depth = 3) {
        if (auto* strip = dynamic_cast<MajorTabStrip*>(const_cast<Widget*>(&widget)))
            return strip->tabs() ? strip : nullptr;
        if (depth == 0)
            return nullptr;
        for (const auto& child : widget.children()) {
            if (MajorTabStrip* strip = child->visible() ? stripOf(*child, depth - 1) : nullptr)
                return strip;
        }
        return nullptr;
    }

    // A window moved, by the system or not: by its title bar (the mouse held over its caption), its strip is told.
    void moved(platform::WindowId id) {
        Window* w = find(id);
        if (!w || w->closing_ || !w->root_.content() || !placesWindows())
            return;
        if (systemMoving != w) {
            if (systemMoving || !platform->mouseHeld())
                return;
            const Point at = w->window_->fromScreen(platform->mousePosition());
            if (w->root_.windowHitTest(at) != platform::HitArea::Caption || !stripOf(*w->root_.content()))
                return;
            systemMoving = w;
        }
        if (MajorTabStrip* strip = stripOf(*w->root_.content()))
            strip->systemMoved();
        // drawn now, from inside the system's loop too: the strip it would go to shows where
        if (!inFrame) {
            const std::vector<Window*> which = windowsToDraw(false);
            if (!which.empty())
                draw(which);
        }
    }

    // The mouse let go: where it is, the window merges or stays.
    void systemMoveEnds() {
        if (!systemMoving || platform->mouseHeld())
            return;
        Window* w = std::exchange(systemMoving, nullptr);
        if (MajorTabStrip* strip = w->root_.content() ? stripOf(*w->root_.content()) : nullptr)
            strip->systemMoveEnded();
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

    // ---- a drag the system carries (Wayland) ----

    std::optional<SystemDrag> systemDrag;

    bool startSystemDrag(const UiRoot& from, SystemDrag drag) override {
        Window* w = find(from);
        if (!w || !platform->startDrag(*w->window_))
            return false;
        systemDrag = std::move(drag);
        return true;
    }

    void cancelSystemDrag() override { systemDrag.reset(); }

    // Its events: to the drag going on (none: nothing); true when it was one of them.
    bool handleSystemDrag(const platform::Event& event) {
        const auto rootOf = [&](platform::WindowId id) -> const UiRoot* {
            const Window* w = find(id);
            return w && !w->closing_ ? &w->root_ : nullptr;
        };
        if (const auto* moved = std::get_if<platform::DragMoved>(&event)) {
            if (systemDrag && systemDrag->moved)
                systemDrag->moved(rootOf(moved->window), moved->position);
        } else if (std::holds_alternative<platform::DragLeft>(event)) {
            if (systemDrag && systemDrag->moved)
                systemDrag->moved(nullptr, {});
        } else if (const auto* dropped = std::get_if<platform::DragDropped>(&event)) {
            if (std::optional<SystemDrag> drag = std::exchange(systemDrag, std::nullopt); drag && drag->ended)
                drag->ended(rootOf(dropped->window), dropped->position);
        } else if (std::holds_alternative<platform::DragEnded>(event)) {
            // not let go on one of its windows (one that was told so already)
            if (std::optional<SystemDrag> drag = std::exchange(systemDrag, std::nullopt); drag && drag->ended)
                drag->ended(nullptr, {});
        } else {
            return false;
        }
        return true;
    }

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
    root_.setLayoutDirection(impl.layoutDirection());
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

Window::Window(Application& app, PopupRequest request, Window& parent)
    : app_(app), root_(*app.impl_->context, this), owner_(&parent), popup_(true), tooltip_(request.tooltip), modal_(request.modal),
      popupRadius_(request.windowRadius) {
    Application::Impl& impl = *app.impl_;
    const bool rtl = parent.root_.rightToLeft();
    root_.setLayoutDirection(rtl ? LayoutDirection::RightToLeft : LayoutDirection::LeftToRight);
    root_.setContent(std::move(request.content));
    const Size wanted = root_.content()->measure({unbounded, unbounded});
    const Size size{std::max(1.0f, std::ceil(wanted.width)), std::max(1.0f, std::ceil(wanted.height))};
    const Point at = placePopup(*parent.window_, request.anchor, request.placement, size, rtl, parent.popupAtStart_, popupAtStart_);
    window_ = impl.platform->createWindow({.title = "",
                                           .size = size,
                                           .resizable = false,
                                           .borderless = true,
                                           .position = at,
                                           .popupOf = parent.window_.get(),
                                           .tooltip = request.tooltip});
    surface_ = impl.device->createSurface(*window_);
    root_.setSize(window_->size());
    root_.setScale(window_->scale());
    roundCorners();
    // it comes in: faded in, sliding a little out of its anchor (down from what it opens under, up when it went above,
    // across from a menu beside it, up into the middle for a dialog); a tooltip only fades
    const UiContext& ui = *impl.context;
    if (ui.scheduler && ui.scheduler->animationsEnabled()) {
        // or (a style's) rising into its window, or landing in it
        entranceKind_ = request.modal ? ui.theme.motion.dialogEntrance : ui.theme.motion.popupEntrance;
        const float distance = ui.theme.motion.popupSlide;
        if (entranceKind_ != PopupEntrance::Slide)
            slide_ = {};
        else if (request.tooltip)
            slide_ = {};
        else if (request.placement == PopupPlacement::Below)
            slide_ = {0, at.y >= request.anchor.y ? -distance : distance};
        else if (request.placement == PopupPlacement::After)
            slide_ = {at.x >= request.anchor.x ? -distance : distance, 0};
        else
            slide_ = {0, distance * 1.5f};
        entrance_.set(0);
        if (entranceKind_ == PopupEntrance::Slide)
            entrance_.animateTo(ui.scheduler, 1, ui.theme.motion.normal, Easing::OutCubic);
        else            // (the arrival eases itself)
            entrance_.animateTo(ui.scheduler, 1, entranceKind_ == PopupEntrance::Land ? ui.theme.motion.slow : ui.theme.motion.normal,
                                Easing::Linear);
        entering();
    }
    window_->show(!request.tooltip);
}

void Window::roundCorners() {
    // a popup's window rounded as the theme says, where the system rounds it; what draws its edge follows. A window's too
    // (the system squares it while it is maximized or snapped)
    const WindowFrameStyle& frame = app_.impl_->context->theme.frame;
    if (!popup_) {
        window_->setCornerRadius(frame.windowRadius);
        return;
    }
    const float radius = popupRadius_.value_or(frame.popupRadius);
    const bool rounded = window_->setCornerRadius(radius);
    root_.setWindowRadius(rounded ? radius : 0.0f);
}

void Window::entering() {
    const float shown = entrance_.value();
    if (entranceKind_ == PopupEntrance::Slide) {
        window_->setOpacity(shown);
        root_.setContentShift(slide_ * (1 - shown));
        return;
    }
    // its window there at once, nearly (what is in it comes in)
    window_->setOpacity(std::min(1.0f, shown * 4));
    const Rect all{0, 0, root_.size().width, root_.size().height};
    if (shown >= 1)
        root_.setContentArrival(std::nullopt);
    else if (entranceKind_ == PopupEntrance::Rise)
        root_.setContentArrival(Arrival{shown, true});
    else
        root_.setContentArrival(Arrival{shown, false, all});
}

UiRoot* Window::openPopup(PopupRequest request) {
    if (closing_ || !request.content)
        return nullptr;
    Application::Impl& impl = *app_.impl_;
    const bool modal = request.modal;
    impl.popups.push_back(std::unique_ptr<Window>(new Window(app_, std::move(request), *this)));
    Window& popup = *impl.popups.back();
    if (modal) {
        // what the mouse was over under it is not any more
        Window& below = popup.base();
        below.root_.dispatch(platform::MouseCrossed{below.id(), false});
        impl.updateScrims();
    }
    return &popup.root_;
}

Window& Window::base() {
    Window* w = this;
    while (w->popup_ && w->owner_)
        w = w->owner_;
    return *w;
}

Window::~Window() {
    window_->setHitTest(nullptr);       // it asks the root, which goes first
}

MenuBar& Window::setMenuBar(std::unique_ptr<MenuBar> menuBar, MenuBarPlacement placement) {
    if (frame_)
        return frame_->setMenuBar(std::move(menuBar), placement);
    Widget& box = bars();
    if (!box.children().empty() && dynamic_cast<MenuBar*>(box.children().front().get()))
        box.removeChild(*box.children().front());
    auto& bar = static_cast<MenuBar&>(box.addChild(menuBar ? std::move(menuBar) : std::make_unique<MenuBar>(), 0));
    bar.setAlignment(Alignment::Stretch, Alignment::Start);
    return bar;
}

ToolBar& Window::setToolBar(std::unique_ptr<ToolBar> toolBar) {
    if (frame_)
        return frame_->setToolBar(std::move(toolBar));
    Widget& box = bars();
    for (const auto& child : box.children()) {
        if (dynamic_cast<ToolBar*>(child.get())) {
            box.removeChild(*child);
            break;
        }
    }
    const bool menu = !box.children().empty() && dynamic_cast<MenuBar*>(box.children().front().get());
    return static_cast<ToolBar&>(box.addChild(toolBar ? std::move(toolBar) : std::make_unique<ToolBar>(), menu ? 1 : 0));
}

StatusBar& Window::setStatusBar(std::unique_ptr<StatusBar> statusBar) {
    if (frame_)
        return frame_->setStatusBar(std::move(statusBar));
    Widget& box = bars();
    for (const auto& child : box.children()) {
        if (dynamic_cast<StatusBar*>(child.get())) {
            box.removeChild(*child);
            break;
        }
    }
    return static_cast<StatusBar&>(box.addChild(statusBar ? std::move(statusBar) : std::make_unique<StatusBar>()));
}

Widget& Window::bars() {
    if (!bars_) {
        // the content under a column of bars
        std::unique_ptr<Widget> content = root_.takeContent();
        auto box = std::make_unique<Widget>();
        box->setLayout(std::make_unique<StackLayout>(Orientation::Vertical));
        if (content) {
            content->setStretch(1);
            box->addChild(std::move(content));
        }
        bars_ = box.get();
        root_.setContent(std::move(box));
    }
    return *bars_;
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
    root_.setLayoutDirection(app_.impl_->layoutDirection());
    root_.languageChanged();
}

platform::WindowState Window::windowState() const {
    return window_->state();
}

void Window::minimizeWindow() {
    window_->minimize();
}

void Window::setFullScreen(bool fullScreen) {
    window_->setFullScreen(fullScreen);
}

bool Window::fullScreen() const {
    return window_->fullScreen();
}

void Window::toggleMaximizeWindow() {
    if (window_->state() == platform::WindowState::Maximized)
        window_->restore();
    else
        window_->maximize();
}

// A popup's text is typed in the window it came from (which keeps the keyboard), and routed to it.
void Window::startTextInput() {
    base().window_->startTextInput();
}

void Window::stopTextInput() {
    base().window_->stopTextInput();
}

void Window::setTextInputArea(const Rect& area) {
    Window& in = base();
    if (&in == this) {
        window_->setTextInputArea(area);
        return;
    }
    const Point corner = in.window_->fromScreen(window_->toScreen({area.x, area.y}));
    in.window_->setTextInputArea({corner.x, corner.y, area.width, area.height});
}

std::string Window::clipboardText() const {
    return app_.impl_->platform->clipboardText();
}

void Window::setClipboardText(const std::string& text) {
    app_.impl_->platform->setClipboardText(text);
}

void Window::setCursor(platform::Cursor cursor) {
    app_.impl_->platform->setCursor(cursor);
}

// ---- application ----

Application::Application() : impl_(std::make_unique<Impl>(*this)) {
    Impl& d = *impl_;
    d.platform = platform::createPlatform();
    d.scheduler.setWakeUp([&d] { d.platform->wakeUp(); });
#ifdef NDEBUG
    d.device = rhi::createDevice(*d.platform);
#else
    d.device = rhi::createDevice(*d.platform, {.debug = true});
#endif
    d.renderer = std::make_unique<paint::Renderer>(*d.device);
    d.fonts = std::make_unique<text::FontLibrary>();
    // (its distance fields: the text of what zooms, sharp at any size)
    d.atlas = std::make_unique<text::GlyphAtlas>(d.renderer->addTexture(), 2048, d.renderer->addTexture(), 1024);
    d.icons = std::make_unique<IconAtlas>(d.renderer->addTexture());
    d.images = std::make_unique<ImageStore>([renderer = d.renderer.get()](rhi::Filter filter) { return renderer->addTexture(nullptr, filter); });
    d.translator = std::make_unique<Translator>();
    d.context = std::make_unique<UiContext>(UiContext{*d.fonts, *d.atlas, *d.translator, Theme::dark(), &d, &d.scheduler, d.icons.get(), d.images.get()});
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)     // getenv: read, not kept
#endif
    d.logFrames = std::getenv("GLW_LOG_FRAMES") != nullptr;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    d.logSince = d.scheduler.now();
    d.platform->setExposeHandler([&d](platform::WindowId id) { d.exposed(id); });
    d.platform->setMoveHandler([&d](platform::WindowId id) { d.moved(id); });
}

Application::~Application() {
    Impl& d = *impl_;
    {
        // a dialog's answer from now on: dropped
        const std::lock_guard lock(d.answers->mutex);
        d.answers->scheduler = nullptr;
    }
    d.platform->setExposeHandler(nullptr);
    d.platform->setMoveHandler(nullptr);
    d.preview.reset();
    while (!d.popups.empty())       // the last opened (a submenu) first
        d.popups.pop_back();
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
            // an animation running: its next step now, so that a frame it draws is drawn at once, at the display's pace
            // (waitForSurfaces waits for its refresh), not a fixed sleep's after the last (out of step with the refresh,
            // a frame of it shown twice now and then: a stutter)
            if (d.scheduler.animating() && d.windowsToDraw(false).empty())
                d.scheduler.tick();
            // nothing to draw: asleep until an event comes or a timer is due (an animation that runs where nothing is
            // drawn waits a refresh's worth, not a busy loop)
            if (d.windowsToDraw(false).empty()) {
                std::optional<std::chrono::milliseconds> wait;
                if (d.scheduler.animating() || d.systemMoving) {
                    wait = std::chrono::milliseconds(16);       // a window moved by the system: till it is let go
                } else if (const std::optional<TimePoint> next = d.scheduler.nextDeadline()) {
                    const auto left = std::chrono::ceil<std::chrono::milliseconds>(*next - d.scheduler.now());
                    wait = std::max(left, std::chrono::milliseconds(0));
                }
                d.platform->waitForEvents(wait);
            } else if (retry) {
                d.platform->waitForEvents(std::chrono::milliseconds(16));
            }
        }
        retry = false;
        // the windows' next images free (the frame before shown, at the display's refresh) before the input is read: what
        // is drawn next shows the newest of it, not what came a refresh before
        d.waitForSurfaces();

        platform::Event event;
        while (!d.quitting && d.platform->pollEvent(event))
            d.handle(event);
        d.systemMoveEnds();
        d.checkFocus();
        // the timers due and the animations' steps: what they change is drawn in this frame
        d.scheduler.tick();
        d.quittingSoon();
        d.closeWindows();
        if (d.quitting || d.opened.empty())
            break;

        const std::vector<Window*> which = d.windowsToDraw(frames >= 0);
        if (which.empty())
            continue;
        retry = !d.draw(which);
        if (d.logFrames)
            d.frameDrawn(which.size());

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

void Application::setOnQuitRequested(std::function<bool()> handler) {
    impl_->onQuitRequested = std::move(handler);
}

void Application::post(std::function<void()> fn) {
    impl_->scheduler.post(std::move(fn));
}

void Application::showFileDialog(platform::FileDialogRequest request, std::function<void(platform::FileDialogResult)> done,
                                 Window* parent) {
    if (parent)
        request.parent = parent->window_.get();
    {
        const std::lock_guard lock(impl_->answers->mutex);
        impl_->answers->scheduler = &impl_->scheduler;
    }
    impl_->platform->showFileDialog(request, [answers = impl_->answers, done = std::move(done)](platform::FileDialogResult result) {
        const std::lock_guard lock(answers->mutex);
        if (!answers->scheduler)
            return;
        answers->scheduler->post([done, result = std::move(result)] {
            if (done)
                done(result);
        });
    });
}

void Window::requestClose() {
    if (!onCloseRequested_ || onCloseRequested_())
        close();
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

void Application::setTheme(const Theme& theme, bool animated) {
    Impl& impl = *impl_;
    if (!animated || !impl.scheduler.animationsEnabled() || impl.opened.empty()) {
        impl.themeFade.set(1);
        impl.themeTarget.reset();
        impl.applyTheme(theme);
        return;
    }
    // from what shows now (a fade on its way, from where it is)
    impl.themeFrom = impl.context->theme;
    impl.themeTarget = theme;
    impl.themeFade.set(0);
    impl.themeFade.animateTo(&impl.scheduler, 1, impl.context->theme.motion.slow, Easing::InOutCubic);
    impl.themeStepped();
}

void Application::setStyle(ThemeStyle style, bool animated) {
    setTheme(Theme::of(colorScheme(), style), animated);
}

ThemeStyle Application::style() const {
    return impl_->themeTarget ? impl_->themeTarget->style : impl_->context->theme.style;
}

void Application::setColorScheme(ColorScheme scheme, bool animated) {
    setTheme(Theme::of(scheme, style()), animated);
}

std::optional<bool> Application::systemPrefersDark() const {
    return impl_->platform->systemDark();
}

void Application::setOnSystemThemeChanged(std::function<void(bool)> handler) {
    impl_->onSystemThemeChanged = std::move(handler);
}

void Application::setFollowsSystemColors(bool follows) {
    impl_->followsSystemColors = follows;
    if (!follows)
        return;
    if (const std::optional<bool> dark = systemPrefersDark())
        setColorScheme(*dark ? ColorScheme::Dark : ColorScheme::Light, false);
}

bool Application::followsSystemColors() const {
    return impl_->followsSystemColors;
}

ColorScheme Application::colorScheme() const {
    return impl_->themeTarget ? impl_->themeTarget->scheme : impl_->context->theme.scheme;
}

void Application::setLanguage(const std::string& language) {
    impl_->translator->setLanguage(language);
    for (const auto& w : impl_->opened)
        w->languageChanged();
    for (const auto& w : impl_->popups)
        w->languageChanged();
}

void Application::setLayoutDirection(std::optional<LayoutDirection> direction) {
    impl_->direction = direction;
    for (const auto& w : impl_->opened)
        w->languageChanged();
    for (const auto& w : impl_->popups)
        w->languageChanged();
}

Translator& Application::translator() {
    return *impl_->translator;
}

const text::FontLibrary& Application::fonts() const {
    return *impl_->fonts;
}

text::FontLibrary& Application::fonts() {
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
