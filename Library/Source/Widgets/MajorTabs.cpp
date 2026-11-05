// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/MajorTabs.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace glw {

namespace {

constexpr float dragThreshold = 6;
constexpr float stripReach = 24;        // dips above and below a strip where a tab dragged from it stays in it
constexpr float draggedOpacity = 0.6f;  // of a window dragged by its only tab

DockingHost* dockingOf(const Widget& widget) {
    const UiRoot* r = widget.root();
    return r ? r->context().docking : nullptr;
}

// The tab strip (with its tabs) under `point` of `root`.
MajorTabStrip* stripAt(const UiRoot& root, Point point) {
    for (Widget* w = root.widgetAt(point); w; w = w->parent()) {
        if (auto* strip = dynamic_cast<MajorTabStrip*>(w))
            return strip->tabs() ? strip : nullptr;
    }
    return nullptr;
}

void later(const Widget& widget, std::function<void()> call) {
    if (UiRoot* r = widget.root())
        r->defer(std::move(call));
    else
        call();
}

// A tab of the strip: rounded at the top, as a browser's; the one shown has the color of what it shows.
class MajorTab final : public Widget {
public:
    MajorTab(MajorTabStrip& strip, std::size_t index, const LocalizedText& title, bool closable, bool active)
        : strip_(strip), index_(index), closable_(closable), active_(active) {
        setPadding({14, 0, closable ? 34.0f : 14.0f, 0});
        setClipsChildren(true);         // a title longer than the tab is cut
        label_ = &add(std::make_unique<Label>(title));
        label_->setAlignment(Alignment::Start, Alignment::Center);
    }

    void setActive(bool active) {
        active_ = active;
        invalidatePaint();
    }

protected:
    void onRootChanged() override { applyStyle(); }
    void onStyleChanged() override { applyStyle(); }

    void paint(PaintContext& context) override {
        const MajorTabStyle& s = style();
        paint::Painter& p = context.painter;
        const Rect r{0, 0, size().width, size().height};
        const VisualState state{.hovered = hovered(), .pressed = pressed_ && hovered()};
        // rounded at the top only: the bottom corners go below the tab and are cut
        p.pushClip(r);
        p.fillRoundedRect({0, 0, r.width, r.height + std::max(s.cornerRadius, 1.0f)}, s.cornerRadius, active_ ? s.active : s.background.resolve(state));
        p.popClip();
        label_->setColor(active_ ? s.text : s.mutedText);
        if (closable_ && (hovered() || active_)) {
            const Rect c = closeRect();
            if (closeHovered_)
                p.fillRoundedRect(c, s.cornerRadius, context.ui.theme.palette.hover);
            const Point m = c.center();
            p.drawLine({m.x - 4, m.y - 4}, {m.x + 4, m.y + 4}, 1.2f, s.text);
            p.drawLine({m.x - 4, m.y + 4}, {m.x + 4, m.y - 4}, 1.2f, s.text);
        }
    }

    void onMouse(MouseEvent& event) override {
        switch (event.type) {
        case MouseEventType::Press:
            if (event.button != platform::MouseButton::Left)
                return;
            event.handled = true;
            if (closable_ && closeRect().contains(event.position)) {
                pressedClose_ = true;
                return;
            }
            if (MajorTabs* tabs = strip_.tabs())
                tabs->setActive(index_);
            pressed_ = true;
            pressPoint_ = mapToRoot(event.position);
            break;
        case MouseEventType::Move: {
            if (const bool over = closable_ && closeRect().contains(event.position); over != closeHovered_) {
                closeHovered_ = over;
                invalidatePaint();
            }
            if (!pressed_)
                return;
            const Point at = mapToRoot(event.position);
            const Point moved = at - pressPoint_;
            if (!dragging_ && std::hypot(moved.x, moved.y) > dragThreshold) {
                dragging_ = true;
                strip_.beginDrag(index_);
            }
            if (dragging_)
                strip_.dragTo(at);
            event.handled = true;
            break;
        }
        case MouseEventType::Release:
            if (event.button != platform::MouseButton::Left)
                return;
            event.handled = true;
            if (pressedClose_) {
                pressedClose_ = false;
                if (closeRect().contains(event.position))
                    strip_.close(index_);
                return;
            }
            pressed_ = false;
            if (std::exchange(dragging_, false))
                strip_.endDrag(mapToRoot(event.position));
            break;
        case MouseEventType::Wheel:
            break;
        }
    }

    void onHoverChanged(bool hovered) override {
        if (!hovered)
            closeHovered_ = false;
        invalidatePaint();
    }

private:
    const MajorTabStyle& style() const {
        if (const UiContext* ui = context())
            return ui->theme.majorTab;
        static const MajorTabStyle unstyled = Theme::dark().majorTab;
        return unstyled;
    }

    void applyStyle() {
        const MajorTabStyle& s = style();
        setMinimumSize({s.minimumWidth, 0});
        setMaximumSize({s.maximumWidth, unbounded});
    }

    Rect closeRect() const {
        const float y = size().height / 2 - 8;
        return rightToLeft() ? Rect{10, y, 16, 16} : Rect{size().width - 26, y, 16, 16};
    }

    MajorTabStrip& strip_;
    std::size_t index_;
    bool closable_;
    bool active_;
    Label* label_ = nullptr;
    bool pressed_ = false;
    bool pressedClose_ = false;
    bool closeHovered_ = false;
    bool dragging_ = false;
    Point pressPoint_;
};

// The + after the tabs.
class NewTabButton final : public Widget {
public:
    explicit NewTabButton(MajorTabStrip& strip) : strip_(strip) { setAlignment(Alignment::Start, Alignment::Center); }

protected:
    Size measureContent(Size) override { return {28, 28}; }

    void paint(PaintContext& context) override {
        const Rect r{0, 0, size().width, size().height};
        if (hovered())
            context.painter.fillRoundedRect(r, context.ui.theme.majorTab.cornerRadius, pressed_ ? context.ui.theme.palette.pressed : context.ui.theme.palette.hover);
        const Point c = r.center();
        const Color color = context.ui.theme.palette.text;
        context.painter.drawLine({c.x - 6, c.y}, {c.x + 6, c.y}, 1.4f, color);
        context.painter.drawLine({c.x, c.y - 6}, {c.x, c.y + 6}, 1.4f, color);
    }

    void onMouse(MouseEvent& event) override {
        if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Move || event.type == MouseEventType::Wheel)
            return;
        event.handled = true;
        if (event.type == MouseEventType::Press) {
            pressed_ = true;
        } else if (std::exchange(pressed_, false) && Rect{0, 0, size().width, size().height}.contains(event.position)) {
            strip_.newTab();
        }
        invalidatePaint();
    }

    void onHoverChanged(bool) override { invalidatePaint(); }

private:
    MajorTabStrip& strip_;
    bool pressed_ = false;
};

} // namespace

// ---- the tabs ----

MajorTabs::MajorTabs() = default;

MajorTabs::~MajorTabs() {
    if (strip_)
        strip_->tabs_ = nullptr;
}

const MajorTabStyle& MajorTabs::style() const {
    if (const UiContext* ui = context())
        return ui->theme.majorTab;
    static const MajorTabStyle unstyled = Theme::dark().majorTab;
    return unstyled;
}

std::size_t MajorTabs::addTab(LocalizedText title, std::unique_ptr<Widget> content, bool closable) {
    insertTab(tabs_.size(), std::move(title), std::move(content), closable);
    return tabs_.size() - 1;
}

void MajorTabs::insertTab(std::size_t index, LocalizedText title, std::unique_ptr<Widget> content, bool closable) {
    index = std::min(index, tabs_.size());
    Widget& shown = addChild(content ? std::move(content) : std::make_unique<Widget>());
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(index), Tab{std::move(title), &shown, closable});
    if (tabs_.size() > 1 && index <= active_)
        ++active_;          // the one shown moved along
    changed();
    const bool first = tabs_.size() == 1;
    setActive(index);
    if (first && onActiveChanged_)
        onActiveChanged_(*this, active_);       // shown, though the index did not change
}

std::unique_ptr<Widget> MajorTabs::removeTab(std::size_t index) {
    const Tab tab = tabs_[index];
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(index));
    std::unique_ptr<Widget> content = removeChild(*tab.content);
    content->setVisible(true);
    if (index < active_ || active_ >= tabs_.size())
        active_ = active_ > 0 ? active_ - 1 : 0;
    changed();
    for (std::size_t i = 0; i < tabs_.size(); ++i)
        tabs_[i].content->setVisible(i == active_);
    if (onActiveChanged_ && !tabs_.empty())
        onActiveChanged_(*this, active_);
    return content;
}

void MajorTabs::moveTab(std::size_t from, std::size_t to) {
    to = std::min(to, tabs_.size() - 1);
    if (from == to || from >= tabs_.size())
        return;
    const Tab tab = tabs_[from];
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(from));
    tabs_.insert(tabs_.begin() + static_cast<std::ptrdiff_t>(to), tab);
    // the tab shown stays the same tab
    if (active_ == from)
        active_ = to;
    else if (from < active_ && to >= active_)
        --active_;
    else if (from > active_ && to <= active_)
        ++active_;
    changed();
}

void MajorTabs::setActive(std::size_t index) {
    if (tabs_.empty())
        return;
    index = std::min(index, tabs_.size() - 1);
    const bool changedTab = index != active_;
    active_ = index;
    for (std::size_t i = 0; i < tabs_.size(); ++i)
        tabs_[i].content->setVisible(i == active_);
    if (strip_)
        strip_->refreshActive();
    if (changedTab && onActiveChanged_)
        onActiveChanged_(*this, active_);
}

void MajorTabs::setTitle(std::size_t index, LocalizedText title) {
    tabs_[index].title = std::move(title);
    changed();
}

void MajorTabs::setOnNewTab(std::function<void(MajorTabs&)> handler) {
    onNewTab_ = std::move(handler);
    changed();
}

std::unique_ptr<MajorTabStrip> MajorTabs::makeStrip() {
    return std::make_unique<MajorTabStrip>(*this);
}

void MajorTabs::changed() {
    if (strip_)
        strip_->rebuild();
}

void MajorTabs::requestClose(std::size_t index) {
    if (index >= tabs_.size())
        return;
    if (onCloseRequested_) {
        onCloseRequested_(*this, index);
        return;
    }
    removeTab(index);
    if (UiRoot* r = root(); r && r->host() && tabs_.empty())
        r->host()->closeWindow();
}

void MajorTabs::requestNew() {
    if (const auto handler = onNewTab_)
        handler(*this);
}

// ---- the strip ----

MajorTabStrip::MajorTabStrip(MajorTabs& tabs) : tabs_(&tabs) {
    if (tabs.strip_)
        tabs.strip_->tabs_ = nullptr;   // one strip at a time
    tabs.strip_ = this;
    setWindowArea(platform::HitArea::Caption);      // where it has no tab, it drags the window
    auto row = std::make_unique<Widget>();
    row->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 2.0f));
    row->setHitTestVisible(false);
    row->setAlignment(Alignment::Start, Alignment::Stretch);
    row_ = &addChild(std::move(row));
    rebuild();
}

MajorTabStrip::~MajorTabStrip() {
    if (tabs_ && tabs_->strip_ == this)
        tabs_->strip_ = nullptr;
}

void MajorTabStrip::onRootChanged() {
    onStyleChanged();
}

void MajorTabStrip::onStyleChanged() {
    const UiContext* ui = context();
    const float gap = ui ? ui->theme.majorTab.topGap : 4.0f;
    setPadding({8, gap, 8, 0});
}

void MajorTabStrip::rebuild() {
    while (!row_->children().empty())
        row_->removeChild(*row_->children().back());
    if (!tabs_)
        return;
    for (std::size_t i = 0; i < tabs_->count(); ++i)
        row_->add(std::make_unique<MajorTab>(*this, i, tabs_->title(i), tabs_->closable(i), i == tabs_->active()));
    if (tabs_->onNewTab_)
        row_->add(std::make_unique<NewTabButton>(*this));
}

void MajorTabStrip::refreshActive() {
    if (!tabs_)
        return;
    for (std::size_t i = 0; i < tabs_->count() && i < row_->children().size(); ++i)
        static_cast<MajorTab&>(*row_->children()[i]).setActive(i == tabs_->active());
}

void MajorTabStrip::close(std::size_t index) {
    later(*this, [tabs = tabs_, index] {
        if (tabs)
            tabs->requestClose(index);
    });
}

void MajorTabStrip::newTab() {
    later(*this, [tabs = tabs_] {
        if (tabs)
            tabs->requestNew();
    });
}

std::pair<std::size_t, float> MajorTabStrip::slotAt(Point point) const {
    const Point local = mapFromRoot(point);
    const bool rtl = rightToLeft();
    const std::size_t count = tabs_ ? std::min(tabs_->count(), row_->children().size()) : 0;
    // the tabs whose middle is before the point, in the reading order
    std::size_t slot = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const Rect b = row_->children()[i]->bounds();
        const float middle = row_->bounds().x + b.center().x;
        if (rtl ? middle > local.x : middle < local.x)
            slot = i + 1;
    }
    if (count == 0)
        return {std::size_t{0}, 0.0f};
    // its x: the start edge of the tab it would go before, or the end edge of the last
    const bool afterLast = slot == count;
    const Rect b = row_->children()[afterLast ? count - 1 : slot]->bounds();
    const float startEdge = rtl ? b.right() : b.left(), endEdge = rtl ? b.left() : b.right();
    return {slot, row_->bounds().x + (afterLast ? endEdge : startEdge)};
}

void MajorTabStrip::systemMoved() {
    UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    if (!r || !host || !host->placesWindows() || !tabs_ || tabs_->count() == 0 || dragging_)
        return;
    if (!std::exchange(systemMoving_, true))
        host->setWindowOpacity(*r, draggedOpacity);     // seen through: the strip under it shows
    const DockingHost::Hit hit = host->windowAt(host->mouseOnScreen(), r);
    setTarget(hit.root ? stripAt(*hit.root, hit.point) : nullptr, hit.point);
}

void MajorTabStrip::systemMoveEnded() {
    if (!systemMoving_)
        return;
    systemMoved();          // where it was let go: the strip under the mouse now
    systemMoving_ = false;
    MajorTabStrip* target = target_;
    const Point at = targetPoint_;
    setTarget(nullptr, {});
    UiRoot* r = root();
    if (DockingHost* host = dockingOf(*this); r && host)
        host->setWindowOpacity(*r, 1);
    if (target && target != this)
        mergeInto(*target, at);
}

void MajorTabStrip::mergeInto(MajorTabStrip& target, Point point) {
    UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    if (!r || !host || !tabs_)
        return;
    MajorTabs* into = target.tabs_;
    const std::size_t slot = target.slotAt(point).first;
    later(*this, [tabs = tabs_, into, slot, host, r] {
        if (!tabs || !into)
            return;
        const std::size_t shown = tabs->active();
        std::size_t place = std::min(slot, into->count());
        for (std::size_t i = 0; tabs->count() > 0; ++i, ++place) {
            LocalizedText title = tabs->title(0);
            const bool closable = tabs->closable(0);
            std::string id = tabs->tabId(0);
            std::unique_ptr<Widget> content = tabs->removeTab(0);
            into->insertTab(place, std::move(title), std::move(content), closable);
            into->setTabId(place, std::move(id));
            if (i == shown)
                into->setActive(place);
        }
        if (const UiRoot* intoRoot = into->root())
            host->raiseWindow(*intoRoot);
        host->closeWindow(*r);
    });
}

void MajorTabStrip::beginDrag(std::size_t index) {
    dragging_ = index;
    const UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    // the only tab of a window drags the window (not a maximized one, which stays where it is)
    movingWindow_ = host && host->placesWindows() && r && tabs_ && tabs_->count() == 1 &&
                    !(r->host() && r->host()->windowState() == platform::WindowState::Maximized);
    if (host) {
        host->captureMouse(true);
        if (movingWindow_) {
            grab_ = host->mouseOnScreen() - host->windowPosition(*r);
            host->setWindowOpacity(*r, draggedOpacity);
        }
    }
}

std::pair<Point, Size> MajorTabStrip::tearPlace(Point point) const {
    const UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    if (!r || !host)
        return {};
    // the mouse on the first tab of the new window, as high in it as here (at most in the title bar); the size of this one
    const float firstTab = r->rightToLeft() ? r->size().width - 60 : 60;
    const Point grabbed = host->toScreen(*r, {firstTab, std::min(point.y, 20.0f)}) - host->toScreen(*r, {});
    return {host->toScreen(*r, point) - grabbed, r->size()};
}

void MajorTabStrip::dragTo(Point point) {
    if (!dragging_)
        return;
    UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    if (!host || !r) {
        setTarget(this, point);
        return;
    }
    if (movingWindow_) {
        // the window follows the mouse; under it, another window's strip takes the tab
        const Point mouse = host->mouseOnScreen();
        host->moveWindow(*r, mouse - grab_);
        const DockingHost::Hit hit = host->windowAt(mouse, r);
        MajorTabStrip* target = hit.root ? stripAt(*hit.root, hit.point) : nullptr;
        setTarget(target, hit.point);
        return;
    }
    // along this strip (a little above or below it too), the tab moves in it
    const Point local = mapFromRoot(point);
    if (local.x >= 0 && local.x <= size().width && local.y >= -stripReach && local.y <= size().height + stripReach) {
        setTarget(this, point);
        host->showWindowPreview(std::nullopt);
        return;
    }
    if (!host->placesWindows()) {
        setTarget(nullptr, point);      // the other windows are not known where they are: out of the strip, a window of its own
        return;
    }
    const DockingHost::Hit hit = host->windowAt(host->toScreen(*r, point));
    MajorTabStrip* target = hit.root ? stripAt(*hit.root, hit.point) : nullptr;
    setTarget(target, hit.point);
    // out of every strip: where it would open a window
    if (!target && tabs_ && tabs_->count() >= 2) {
        const auto [position, size] = tearPlace(point);
        host->showWindowPreview(position, size);
    } else {
        host->showWindowPreview(std::nullopt);
    }
}

void MajorTabStrip::endDrag(Point point) {
    if (!dragging_)
        return;
    dragTo(point);
    const std::size_t from = *dragging_;
    MajorTabStrip* target = target_;
    const Point at = targetPoint_;
    const bool moved = movingWindow_;
    dragging_.reset();
    movingWindow_ = false;
    setTarget(nullptr, {});
    UiRoot* r = root();
    DockingHost* host = dockingOf(*this);
    if (host) {
        host->captureMouse(false);
        host->showWindowPreview(std::nullopt);
        if (moved && r)
            host->setWindowOpacity(*r, 1);
    }
    if (!tabs_ || !r)
        return;

    if (target == this) {
        const std::size_t slot = slotAt(at).first;
        const std::size_t to = slot > from ? slot - 1 : slot;
        if (to != from)
            later(*this, [tabs = tabs_, from, to] {
                if (tabs)
                    tabs->moveTab(from, to);
            });
        return;
    }
    if (!host)
        return;
    if (target) {
        // into another window's strip; this window goes when it has no tab left
        MajorTabs* into = target->tabs_;
        const std::size_t slot = target->slotAt(at).first;
        later(*this, [tabs = tabs_, from, into, slot, host, r] {
            if (!tabs || from >= tabs->count())
                return;
            LocalizedText title = tabs->title(from);
            const bool closable = tabs->closable(from);
            std::string id = tabs->tabId(from);
            std::unique_ptr<Widget> content = tabs->removeTab(from);
            into->insertTab(slot, std::move(title), std::move(content), closable);
            into->setTabId(std::min(slot, into->count() - 1), std::move(id));
            if (const UiRoot* intoRoot = into->root())
                host->raiseWindow(*intoRoot);
            if (tabs->count() == 0)
                host->closeWindow(*r);
        });
        return;
    }
    if (moved || tabs_->count() < 2)
        return;
    // out of every strip: a window of its own, where the outline was
    const auto [position, size] = tearPlace(point);
    later(*this, [tabs = tabs_, from, host, position, size] {
        if (!tabs || from >= tabs->count())
            return;
        auto torn = std::make_unique<MajorTabs>();
        torn->onActiveChanged_ = tabs->onActiveChanged_;
        torn->onNewTab_ = tabs->onNewTab_;
        torn->onCloseRequested_ = tabs->onCloseRequested_;
        const bool closable = tabs->closable(from);
        LocalizedText title = tabs->title(from);
        std::string id = tabs->tabId(from);
        torn->insertTab(0, title, tabs->removeTab(from), closable);
        torn->setTabId(0, std::move(id));
        std::unique_ptr<MajorTabStrip> strip = torn->makeStrip();
        // the center of a window's area (of its global panels): the new window gets one of the same group around them
        std::unique_ptr<Widget> content = std::move(torn);
        for (Widget* w = tabs->parent(); w; w = w->parent()) {
            if (auto* area = dynamic_cast<DockArea*>(w); area && area->center() == tabs) {
                content = area->makeSibling(std::move(content));
                break;
            }
        }
        host->openWindow({.title = std::move(title),
                          .content = std::move(content),
                          .titleCenter = std::move(strip),
                          .position = position,
                          .size = size,
                          .owner = nullptr});
    });
}

void MajorTabStrip::setTarget(MajorTabStrip* target, Point point) {
    if (target != target_ && target_)
        target_->showInsertion(std::nullopt);
    target_ = target;
    targetPoint_ = point;
    if (target_)
        target_->showInsertion(target_->slotAt(point).second);
}

void MajorTabStrip::showInsertion(std::optional<float> x) {
    insertionX_ = x;
    invalidatePaint();
}

void MajorTabStrip::paintOver(PaintContext& context) {
    if (!insertionX_)
        return;
    const float top = padding().top;
    context.painter.fillRect({*insertionX_ - 1, top + 4, 2, std::max(0.0f, size().height - top - 8)}, context.ui.theme.majorTab.insertion);
}

} // namespace glw
