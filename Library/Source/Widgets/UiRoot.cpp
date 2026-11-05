// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Action.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>

#include <algorithm>
#include <utility>
#include <variant>

namespace glw {

namespace {

template <class... F>
struct Overloaded : F... {
    using F::operator()...;
};
template <class... F>
Overloaded(F...) -> Overloaded<F...>;

bool isInside(const Widget* widget, const Widget& subtree) {
    for (; widget; widget = widget->parent()) {
        if (widget == &subtree)
            return true;
    }
    return false;
}

} // namespace

UiHost::~UiHost() = default;

namespace {

// A tooltip's popup: its text, wrapped past the theme's width. It tells its root when it goes.
class ToolTipView final : public Widget {
public:
    ToolTipView(LocalizedText text, std::function<void()> gone) : gone_(std::move(gone)) {
        setPadding({8, 5, 8, 6});
        label_ = &add(std::make_unique<Label>(Label::Props{.text = std::move(text), .fontSize = 12, .wrap = true}));
    }
    void detach() { gone_ = nullptr; }

protected:
    Size measureContent(Size available) override {
        const UiContext* ui = context();
        available.width = std::min(available.width, (ui ? ui->theme.toolTip.maximumWidth : 320.0f) - padding().horizontal());
        return Widget::measureContent(available);
    }
    void paint(PaintContext& context) override {
        const ToolTipStyle& s = context.ui.theme.toolTip;
        const Rect all{0, 0, size().width, size().height};
        context.painter.fillRect(all, s.background);
        context.painter.strokeRoundedRect(all, root() ? root()->windowRadius() : 0.0f, 1, s.border);
        label_->setColor(s.text);
    }
    void onWindowClosing() override {
        if (gone_)
            std::exchange(gone_, nullptr)();
    }

private:
    Label* label_ = nullptr;
    std::function<void()> gone_;
};

} // namespace

UiRoot::UiRoot(const UiContext& context, UiHost* host) : context_(context), host_(host) {}

UiRoot::~UiRoot() {
    hideToolTip(false);
    content_.reset();       // its widgets first, while what they may tell it (shortcuts) is whole
}

// ---- shortcuts ----

void UiRoot::addShortcuts(const void* owner, std::vector<std::shared_ptr<Action>> actions) {
    removeShortcuts(owner);
    shortcuts_.emplace_back(owner, std::move(actions));
}

void UiRoot::removeShortcuts(const void* owner) {
    std::erase_if(shortcuts_, [&](const auto& s) { return s.first == owner; });
}

// ---- tooltips ----

void UiRoot::updateToolTip() {
    // the deepest widget under the mouse with one
    Widget* with = nullptr;
    for (auto it = hovered_.rbegin(); it != hovered_.rend() && !with; ++it) {
        if ((*it)->toolTip())
            with = *it;
    }
    if (with == tipFor_)
        return;
    hideToolTip(false);
    tipFor_ = with;
    tipSuppressed_ = false;
    Scheduler* scheduler = context_.scheduler;
    if (!with || !scheduler || !host_ || capture_)
        return;
    // soon after another closed, quickly: the mouse goes along a toolbar
    const ToolTipStyle& s = context_.theme.toolTip;
    const bool soon = tipClosed_ && scheduler->now() - *tipClosed_ < std::chrono::milliseconds(600);
    tipTimer_.start(*scheduler, soon ? Duration(s.reshowDelay) : Duration(s.delay), [this] { showToolTip(); });
}

void UiRoot::showToolTip() {
    if (!tipFor_ || tip_ || tipSuppressed_ || !host_ || !tipFor_->toolTip())
        return;
    // under the mouse's pointer
    tip_ = host_->openPopup({.content = std::make_unique<ToolTipView>(*tipFor_->toolTip(), [this] { tip_ = nullptr; }),
                             .anchor = {mouse_.x, mouse_.y, 1, 22},
                             .tooltip = true});
}

void UiRoot::hideToolTip(bool suppress) {
    tipTimer_.stop();
    if (suppress)
        tipSuppressed_ = true;
    if (UiRoot* tip = std::exchange(tip_, nullptr)) {
        static_cast<ToolTipView*>(tip->content())->detach();
        if (tip->host())
            tip->host()->closeWindow();
        if (context_.scheduler)
            tipClosed_ = context_.scheduler->now();
    }
}

// ---- content ----

void UiRoot::setContent(std::unique_ptr<Widget> content) {
    takeContent();
    if (!content)
        return;
    GLW_EXPECTS(!content->parent() && !content->uiRoot_);
    content_ = std::move(content);
    content_->setRootOfTree(this);
    invalidateLayout();
}

std::unique_ptr<Widget> UiRoot::takeContent() {
    if (!content_)
        return nullptr;
    forget(*content_);
    std::unique_ptr<Widget> content = std::move(content_);
    content->setRootOfTree(nullptr);
    invalidateLayout();
    return content;
}

void UiRoot::setSize(Size size) {
    if (size != size_) {
        size_ = size;
        invalidateLayout();
    }
}

void UiRoot::setScale(float pixelsPerDip) {
    GLW_EXPECTS(pixelsPerDip > 0);
    if (pixelsPerDip != scale_) {
        scale_ = pixelsPerDip;
        invalidatePaint();
    }
}

void UiRoot::setLayoutDirection(LayoutDirection direction) {
    const bool rtl = direction == LayoutDirection::RightToLeft;
    if (rtl != rightToLeft_) {
        rightToLeft_ = rtl;
        invalidateLayout();
    }
}

// ---- frames ----

namespace {

void tell(Widget& widget, void (*f)(Widget&)) {
    f(widget);
    for (const auto& child : widget.children())
        tell(*child, f);
}

} // namespace

void UiRoot::styleChanged() {
    if (context_.scheduler)
        context_.scheduler->styleChanged();     // the colors of states: the new ones at once
    if (content_)
        tell(*content_, [](Widget& w) { w.onStyleChanged(); });
    invalidateLayout();
}

void UiRoot::languageChanged() {
    if (content_)
        tell(*content_, [](Widget& w) { w.onLanguageChanged(); });
    invalidateLayout();
}

void UiRoot::windowClosing() {
    if (content_)
        tell(*content_, [](Widget& w) { w.onWindowClosing(); });
}

void UiRoot::invalidateLayout() {
    needsLayout_ = true;
    needsPaint_ = true;
    ++layoutGeneration_;        // every measure kept is out of date
}

void UiRoot::layout() {
    if (content_ && content_->visible()) {
        content_->measure(size_);
        arrangeInCell(*content_, {contentShift_.x, contentShift_.y, size_.width, size_.height}, rightToLeft_);
    }
    needsLayout_ = false;
    needsPaint_ = true;
}

void UiRoot::setContentShift(Point shift) {
    if (shift == contentShift_)
        return;
    contentShift_ = shift;
    // placed again where it is (its measure is kept)
    needsLayout_ = true;
    needsPaint_ = true;
}

void UiRoot::paint(paint::DrawList& list) {
    if (needsLayout_)
        layout();
    list.clear();
    paint::Painter painter(list, {0, 0, size_.width, size_.height}, scale_);
    PaintContext context{painter, context_};
    if (content_)
        paintTree(*content_, context, {0, 0, size_.width, size_.height});
    if (overlay_)
        painter.fillRect({0, 0, size_.width, size_.height}, *overlay_);
    needsPaint_ = false;
}

void UiRoot::setOverlay(std::optional<Color> color) {
    if (color == overlay_)
        return;
    overlay_ = color;
    invalidatePaint();
}

void UiRoot::paintTree(Widget& widget, PaintContext& context, const Rect& visible) {
    // what paints a little out of its bounds (a focus ring, a shadow) is painted while that may show
    constexpr float overhang = 24;
    if (!widget.visible_ || !widget.bounds_.inflated(overhang).intersects(visible))
        return;
    paint::Painter& painter = context.painter;
    painter.pushTransform({1, {widget.bounds_.x, widget.bounds_.y}});
    widget.paint(context);
    // what shows of it, in its coordinates: for its children
    Rect inside{visible.x - widget.bounds_.x, visible.y - widget.bounds_.y, visible.width, visible.height};
    if (widget.clipsChildren_) {
        painter.pushClip(widget.childrenClip());
        inside = inside.intersected(widget.childrenClip());
    }
    // its children zoomed and panned (a canvas): what shows of it, in their coordinates
    const paint::Transform transform = widget.childrenTransform();
    const bool transformed = !transform.identity();
    if (transformed) {
        painter.pushTransform(transform);
        inside = transform.unmap(inside);
    }
    for (const auto& child : widget.children_) {
        if (const std::optional<Rect> cut = widget.childClip(*child)) {
            painter.pushClip(*cut);
            paintTree(*child, context, inside.intersected(*cut));
            painter.popClip();
        } else {
            paintTree(*child, context, inside);
        }
    }
    if (transformed)
        painter.popTransform();
    if (widget.clipsChildren_)
        painter.popClip();
    widget.paintOver(context);
    painter.popTransform();
}

// ---- finding widgets ----

Widget* UiRoot::hit(Widget& widget, Point point) {
    if (!widget.visible_ || !widget.bounds_.contains(point))
        return nullptr;
    const Point local{point.x - widget.bounds_.x, point.y - widget.bounds_.y};
    // the last child is painted last, over the others: it is asked first; none where they are cut away
    if (!widget.clipsChildren_ || widget.childrenClip().contains(local)) {
        const Point inner = widget.childrenTransform().unmap(local);
        for (auto it = widget.children_.rbegin(); it != widget.children_.rend(); ++it) {
            if (const std::optional<Rect> cut = widget.childClip(**it); cut && !cut->contains(inner))
                continue;
            if (Widget* found = hit(**it, inner))
                return found;
        }
    }
    return widget.hitTestVisible_ ? &widget : nullptr;
}

Widget* UiRoot::widgetAt(Point point) const {
    return content_ ? hit(*content_, point) : nullptr;
}

platform::HitArea UiRoot::windowHitTest(Point point) const {
    if (!content_)
        return platform::HitArea::Client;
    if (const auto area = content_->frameHitArea(content_->mapFromRoot(point)))
        return *area;
    const Widget* widget = widgetAt(point);
    return widget ? widget->windowArea_ : platform::HitArea::Client;
}

std::vector<Widget*> UiRoot::pathTo(Widget* target) const {
    std::vector<Widget*> path;
    for (Widget* w = target; w; w = w->parent_)
        path.push_back(w);
    std::reverse(path.begin(), path.end());
    // a disabled widget, and what is in it, gets nothing
    const auto disabled = std::find_if(path.begin(), path.end(), [](const Widget* w) { return !w->enabled_; });
    path.erase(disabled, path.end());
    return path;
}

// ---- events ----

void UiRoot::defer(std::function<void()> call) {
    if (dispatching_ > 0)
        deferred_.push_back(std::move(call));
    else
        call();
}

void UiRoot::dispatch(const platform::Event& event) {
    ++dispatching_;
    std::visit(Overloaded{
                   [&](const platform::WindowResized& e) { setSize(e.size); },
                   [&](const platform::WindowScaleChanged& e) { setScale(e.scale); },
                   [&](const platform::WindowExposed&) { invalidatePaint(); },
                   [&](const platform::WindowStateChanged&) { invalidatePaint(); },
                   [&](const platform::WindowFocusChanged& e) {
                       if (!e.focused) {    // the buttons are let go somewhere else
                           capture_ = nullptr;
                           buttonsDown_ = 0;
                       }
                       invalidatePaint();
                   },
                   [&](const platform::MouseCrossed& e) {
                       if (!e.entered && !capture_)
                           updateHover(nullptr);
                   },
                   [&](const platform::MouseMoved& e) {
                       mouseMoved(e.position);
                       updateCursor();
                   },
                   [&](const platform::MouseButtonChanged& e) {
                       mouseButton(e);
                       updateCursor();
                   },
                   [&](const platform::MouseWheelScrolled& e) {
                       hideToolTip(true);
                       MouseEvent wheel{MouseEventType::Wheel, {}, platform::MouseButton::Left, 0, e.delta, modifiers_};
                       wheel.wheelPrecise = e.precise;
                       route(capture_ ? capture_ : widgetAt(e.position), wheel, e.position);
                   },
                   [&](const platform::KeyChanged& e) {
                       modifiers_ = e.modifiers;
                       if (e.pressed)
                           hideToolTip(true);
                       KeyEvent key{e.key, e.modifiers, e.pressed, e.repeat};
                       routeKey(key);
                       // not taken: a shortcut
                       if (!key.handled && e.pressed) {
                           for (const auto& [owner, actions] : std::vector(shortcuts_)) {
                               for (const std::shared_ptr<Action>& action : actions) {
                                   if (!key.handled && action->enabled() && action->shortcut().matches(e.key, e.modifiers)) {
                                       key.handled = true;
                                       action->trigger();
                                   }
                               }
                           }
                       }
                       // Tab moves the focus when the focused widget does not take it
                       if (!key.handled && e.pressed && e.key == platform::Key::Tab)
                           focusNext(platform::hasAny(e.modifiers, platform::KeyModifiers::Shift));
                   },
                   [&](const platform::TextInput& e) {
                       TextEvent text{e.text, false, 0};
                       routeText(text);
                   },
                   [&](const platform::TextComposition& e) {
                       TextEvent text{e.text, true, e.cursor};
                       routeText(text);
                   },
                   [](const auto&) {},
               },
               event);
    // what the handlers left for after the event, which may add more
    if (--dispatching_ == 0)
        runDeferred();
}

Widget* UiRoot::route(Widget* target, MouseEvent& event, Point point) {
    const std::vector<Widget*> path = pathTo(target);
    for (Widget* w : path) {
        event.position = w->mapFromRoot(point);
        w->onPreviewMouse(event);
        if (event.handled)
            return w;
    }
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        event.position = (*it)->mapFromRoot(point);
        (*it)->onMouse(event);
        if (event.handled)
            return *it;
    }
    return nullptr;
}

void UiRoot::routeKey(KeyEvent& event) {
    const std::vector<Widget*> path = pathTo(focus_ ? focus_ : content_.get());
    for (Widget* w : path) {
        w->onPreviewKey(event);
        if (event.handled)
            return;
    }
    for (auto it = path.rbegin(); it != path.rend() && !event.handled; ++it)
        (*it)->onKey(event);
}

void UiRoot::routeText(TextEvent& event) {
    const std::vector<Widget*> path = pathTo(focus_ ? focus_ : content_.get());
    for (auto it = path.rbegin(); it != path.rend() && !event.handled; ++it)
        (*it)->onText(event);
}

void UiRoot::mouseMoved(Point point) {
    mouse_ = point;
    Widget* target = widgetAt(point);
    updateHover(target);
    MouseEvent move{MouseEventType::Move, {}, platform::MouseButton::Left, 0, {}, modifiers_};
    if (capture_) {
        move.position = capture_->mapFromRoot(point);
        capture_->onMouse(move);
    } else {
        route(target, move, point);
    }
}

void UiRoot::mouseButton(const platform::MouseButtonChanged& e) {
    mouse_ = e.position;
    if (e.pressed)
        hideToolTip(true);
    MouseEvent event{e.pressed ? MouseEventType::Press : MouseEventType::Release, {}, e.button, e.clicks, {}, modifiers_};
    if (e.pressed) {
        ++buttonsDown_;
        if (capture_) {     // another button while one is down: to the same widget
            event.position = capture_->mapFromRoot(e.position);
            capture_->onMouse(event);
            return;
        }
        Widget* target = widgetAt(e.position);
        updateHover(target);
        // the focus goes to what was pressed, or to the nearest of its ancestors that takes it; none has it otherwise
        Widget* focusable = target;
        while (focusable && !(focusable->focusable_ && focusable->enabledInTree()))
            focusable = focusable->parent_;
        setFocus(focusable);
        if (Widget* handler = route(target, event, e.position))
            capture_ = handler;
        return;
    }

    buttonsDown_ = std::max(0, buttonsDown_ - 1);
    if (Widget* captured = capture_) {
        if (buttonsDown_ == 0)
            capture_ = nullptr;
        event.position = captured->mapFromRoot(e.position);
        captured->onMouse(event);
    } else {
        route(widgetAt(e.position), event, e.position);
    }
    if (!capture_)
        updateHover(widgetAt(e.position));
}

namespace {

std::optional<platform::Cursor> edgeCursor(platform::HitArea area) {
    using platform::Cursor;
    using platform::HitArea;
    switch (area) {
    case HitArea::ResizeLeft:
    case HitArea::ResizeRight: return Cursor::ResizeHorizontal;
    case HitArea::ResizeTop:
    case HitArea::ResizeBottom: return Cursor::ResizeVertical;
    case HitArea::ResizeTopLeft:
    case HitArea::ResizeBottomRight: return Cursor::ResizeDiagonal;
    case HitArea::ResizeTopRight:
    case HitArea::ResizeBottomLeft: return Cursor::ResizeAntiDiagonal;
    default: return std::nullopt;
    }
}

} // namespace

void UiRoot::updateCursor() {
    std::optional<platform::Cursor> shape;
    // what has the mouse, wherever it is; or the window's edge; or what is under the mouse (what is enabled of it)
    Widget* from = capture_;
    if (!from && content_) {
        if (const auto area = content_->frameHitArea(content_->mapFromRoot(mouse_)))
            shape = edgeCursor(*area);
        if (!shape && !hovered_.empty())
            from = hovered_.back();
    }
    for (Widget* w = from; w && !shape; w = w->parent_)
        shape = w->cursorAt(w->mapFromRoot(mouse_));
    cursor_ = shape.value_or(platform::Cursor::Arrow);
    // told each time: another window may have shown another shape meanwhile (the platform keeps what it shows)
    if (host_)
        host_->setCursor(cursor_);
}

void UiRoot::captureMouse(Widget& widget) {
    if (buttonsDown_ > 0 && widget.root() == this)
        capture_ = &widget;
}

void UiRoot::updateHover(Widget* target) {
    std::vector<Widget*> path = target ? pathTo(target) : std::vector<Widget*>{};
    // what the mouse left, from the deepest; then what it entered, from the root down
    for (auto it = hovered_.rbegin(); it != hovered_.rend(); ++it) {
        if (std::find(path.begin(), path.end(), *it) == path.end()) {
            (*it)->hovered_ = false;
            (*it)->onHoverChanged(false);
        }
    }
    for (Widget* w : path) {
        if (!w->hovered_) {
            w->hovered_ = true;
            w->onHoverChanged(true);
        }
    }
    hovered_ = std::move(path);
    updateToolTip();
}

// ---- focus ----

void UiRoot::setFocus(Widget* widget) {
    if (widget == focus_)
        return;
    GLW_EXPECTS(!widget || widget->root() == this);
    Widget* previous = std::exchange(focus_, widget);
    // what the widgets told do that removes widgets waits until they all are told
    ++dispatching_;
    if (previous)
        previous->onFocusChanged(false);
    if (focus_)
        focus_->onFocusChanged(true);
    // the subtrees it left (from the deepest), then those it came into
    std::vector<Widget*> before, after;
    for (Widget* w = previous; w; w = w->parent_)
        before.push_back(w);
    for (Widget* w = focus_; w; w = w->parent_)
        after.push_back(w);
    for (Widget* w : before) {
        if (std::find(after.begin(), after.end(), w) == after.end())
            w->onFocusWithinChanged(false);
    }
    for (Widget* w : after) {
        if (std::find(before.begin(), before.end(), w) == before.end())
            w->onFocusWithinChanged(true);
    }
    invalidatePaint();
    if (--dispatching_ == 0)
        runDeferred();
}

void UiRoot::runDeferred() {
    while (!deferred_.empty()) {
        auto calls = std::exchange(deferred_, {});
        for (auto& call : calls)
            call();
    }
}

void UiRoot::focusNext(bool backwards) {
    std::vector<Widget*> candidates;
    const auto collect = [&](auto& self, Widget& w) -> void {
        if (!w.visible_ || !w.enabled_)
            return;
        if (w.focusable_)
            candidates.push_back(&w);
        for (const auto& child : w.children_)
            self(self, *child);
    };
    if (content_)
        collect(collect, *content_);
    if (candidates.empty())
        return;
    const auto it = std::find(candidates.begin(), candidates.end(), focus_);
    const auto n = static_cast<std::ptrdiff_t>(candidates.size());
    std::ptrdiff_t index = 0;
    if (it == candidates.end())
        index = backwards ? n - 1 : 0;
    else
        index = ((it - candidates.begin()) + (backwards ? n - 1 : 1)) % n;
    setFocus(candidates[static_cast<std::size_t>(index)]);
    // reached by the keyboard: it may be scrolled out of view
    focus_->scrollIntoView();
}

void UiRoot::forget(Widget& subtree) {
    if (isInside(tipFor_, subtree)) {
        hideToolTip(false);
        tipFor_ = nullptr;
    }
    if (isInside(focus_, subtree)) {
        Widget* previous = std::exchange(focus_, nullptr);
        previous->onFocusChanged(false);
    }
    if (isInside(capture_, subtree)) {
        capture_ = nullptr;
        buttonsDown_ = 0;
    }
    std::erase_if(hovered_, [&](Widget* w) {
        if (!isInside(w, subtree))
            return false;
        w->hovered_ = false;
        w->onHoverChanged(false);
        return true;
    });
    invalidateLayout();
}

} // namespace glw
