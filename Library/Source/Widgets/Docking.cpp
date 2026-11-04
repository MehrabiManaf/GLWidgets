// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Docking.hpp>

#include "Widgets/Json.hpp"

#include <GLWidgets/Core/Assert.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>

namespace glw {

// A dock area and the areas of its floating windows: what a tab of one may be dragged to.
struct DockGroup {
    DockArea* home = nullptr;               // where floating windows go above (none once no area is left for it)
    std::vector<DockArea*> areas;           // with the floating windows' and, for Global, every window's
    DockingHost* host = nullptr;            // the last one the areas were shown with
};

namespace {

constexpr float dragThreshold = 6;      // dips the mouse moves with a tab before it is dragged
constexpr Size centerMinimum{160, 100}; // what an area's center keeps at least
constexpr float draggedOpacity = 0.6f;  // of a window dragged by its only tab
constexpr float selectedTabTop = 3;     // dips between the bar's top and the top of the tab shown, among others
constexpr float tearReach = 24;         // dips above and below a stack's tab bar where a tab dragged from it stays in it
constexpr float wheelStep = 40;         // dips the tabs scroll by a step of the wheel
constexpr float thumbZone = 7;          // dips along the bottom of a tab bar where a press is on its scroll bar
constexpr float titleGrip = 28;         // dips after the tabs of a floating window's title, always there to drag it by

// A tab of a stack: its title, a × when the panel can be closed. Pressed, it shows its panel; dragged, it moves it.
class DockTab final : public Widget {
public:
    DockTab(DockTabs& owner, std::size_t index, const LocalizedText& title, bool closable, bool active)
        : owner_(owner), index_(index), closable_(closable), active_(active) {
        setPadding({10, 0, closable ? 28.0f : 10.0f, 0});
        setClipsChildren(true);
        // as wide as its title, between the style's widths: a longer one is cut with …
        setMinimumSize({owner.style().tabMinimumWidth, 0});
        setMaximumSize({owner.style().tabMaximumWidth, unbounded});
        label_ = &add(std::make_unique<Label>(Label::Props{.text = title, .elide = true}));
        label_->setAlignment(Alignment::Start, Alignment::Center);
    }

    void setActive(bool active) {
        active_ = active;
        invalidatePaint();
    }

protected:
    Size measureContent(Size available) override {
        const Size text = Widget::measureContent(available);
        return {text.width, std::max(text.height, owner_.style().tabHeight)};
    }

    void paint(PaintContext& context) override {
        const DockStyle& s = owner_.style();
        paint::Painter& p = context.painter;
        const Rect r{0, 0, size().width, size().height};
        const VisualState state{.hovered = hovered(), .pressed = pressed_ && hovered()};
        if (active_ && owner_.count() > 1) {
            // among others, as Fusion's: raised over the bar, lit at its top, its edge around it but under it, where it
            // joins its content (its lower corners and edge below the bar, cut)
            const Rect shape{0, selectedTabTop, r.width, r.height - selectedTabTop + s.cornerRadius};
            const float radius = std::min(s.cornerRadius, 4.0f);
            p.pushClip(r);
            p.fillRoundedRect(shape, radius, s.selectedTab, s.background);
            p.strokeRoundedRect(shape.inflated(-1), radius - 1, 1, s.selectedTabHighlight, withAlpha(s.selectedTabHighlight, 0));
            p.strokeRoundedRect(shape, radius, 1, s.selectedTabBorder);
            p.popClip();
            p.fillRect({1, r.height - 1, r.width - 2, 1}, s.background);     // the hairline under the bar open under it
        } else {
            p.fillRoundedRect(r.inflated(-3), 4, active_ ? s.activeTab : s.tab.resolve(state));
        }
        label_->setColor(active_ || hovered() ? s.text : s.mutedText);
        if (closable_ && (hovered() || active_)) {
            const Rect c = closeRect();
            if (closeHovered_)
                p.fillRoundedRect(c, 4, context.ui.theme.palette.hover);
            const Point m = c.center();
            p.drawLine({m.x - 4, m.y - 4}, {m.x + 4, m.y + 4}, 1.2f, s.text);
            p.drawLine({m.x - 4, m.y + 4}, {m.x + 4, m.y - 4}, 1.2f, s.text);
        }
    }

    void onMouse(MouseEvent& event) override {
        DockArea* area = owner_.area();
        switch (event.type) {
        case MouseEventType::Press:
            if (event.button != platform::MouseButton::Left)
                return;
            event.handled = true;
            if (closable_ && closeRect().contains(event.position)) {
                pressedClose_ = true;
                return;
            }
            owner_.setActive(index_);
            pressed_ = true;
            pressPoint_ = mapToRoot(event.position);
            invalidatePaint();
            break;
        case MouseEventType::Move: {
            if (const bool over = closable_ && closeRect().contains(event.position); over != closeHovered_) {
                closeHovered_ = over;
                invalidatePaint();
            }
            if (!pressed_ || !area)
                return;
            const Point at = mapToRoot(event.position);
            const Point moved = at - pressPoint_;
            if (std::hypot(moved.x, moved.y) > dragThreshold) {
                // the area goes on with it (it has the mouse now): this tab may go
                pressed_ = false;
                invalidatePaint();
                area->beginDrag(owner_, index_);
                area->dragTo(at);
            }
            event.handled = true;
            break;
        }
        case MouseEventType::Release:
            if (event.button != platform::MouseButton::Left)
                return;
            event.handled = true;
            if (pressedClose_) {
                pressedClose_ = false;
                if (area && closeRect().contains(event.position))
                    area->closePanel(owner_.panelId(index_));      // after the event: this tab goes with it
                return;
            }
            pressed_ = false;
            invalidatePaint();
            if (area && area->dragging())
                area->endDrag(mapToRoot(event.position), true);     // may take this tab away (after the event)
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
    // The × at the end side.
    Rect closeRect() const {
        const float y = size().height / 2 - 8;
        return rightToLeft() ? Rect{6, y, 16, 16} : Rect{size().width - 22, y, 16, 16};
    }

    DockTabs& owner_;
    std::size_t index_;
    bool closable_;
    bool active_;
    Label* label_ = nullptr;
    bool pressed_ = false;
    bool pressedClose_ = false;
    bool closeHovered_ = false;
    Point pressPoint_;
};

// The tabs of a stack side by side, each as wide as it wants. When they do not fit they are scrolled: by the wheel, by
// the thin bar along its bottom (shown while the mouse is over the tabs; dragged, or pressed beside its thumb), and so
// that the tab shown is in view.
class TabRow final : public Widget {
public:
    TabRow() { setClipsChildren(true); }

    // The tab at `index` comes into view when it is next arranged.
    void reveal(std::size_t index) {
        reveal_ = index;
        invalidateLayout();
    }

protected:
    Size measureContent(Size available) override {
        Size size;
        for (const auto& tab : children()) {
            const Size s = tab->measure({unbounded, available.height});
            size.width += s.width;
            size.height = std::max(size.height, s.height);
        }
        return size;
    }

    void arrangeContent(const Rect& content) override {
        area_ = content;
        std::vector<float> widths;
        total_ = 0;
        for (const auto& tab : children()) {
            widths.push_back(tab->measure({unbounded, content.height}).width);
            total_ += widths.back();
        }
        if (reveal_ && *reveal_ < widths.size()) {
            float start = 0;
            for (std::size_t i = 0; i < *reveal_; ++i)
                start += widths[i];
            const float end = start + widths[*reveal_];
            if (start < offset_)
                offset_ = start;
            else if (end > offset_ + content.width)
                offset_ = end - content.width;
        }
        reveal_.reset();
        offset_ = std::clamp(offset_, 0.0f, maxOffset());
        // from the start side, less what is scrolled
        const bool rtl = rightToLeft();
        float at = -offset_;
        for (std::size_t i = 0; i < widths.size(); ++i) {
            const float x = rtl ? content.right() - at - widths[i] : content.x + at;
            children()[i]->arrange({x, content.y, widths[i], content.height});
            at += widths[i];
        }
    }

    void paintOver(PaintContext& context) override {
        if (maxOffset() <= 0 || !(hovered() || grab_))
            return;
        const Rect t = thumb();
        context.painter.fillRoundedRect(t, t.height / 2, context.ui.theme.dock.scrollThumb);
    }

    // the bar along the bottom before the tab there: dragged, or pressed beside its thumb to go there
    void onPreviewMouse(MouseEvent& event) override {
        if (event.type != MouseEventType::Press || event.button != platform::MouseButton::Left || maxOffset() <= 0 ||
            event.position.y < size().height - thumbZone)
            return;
        event.handled = true;
        Rect t = thumb();
        if (event.position.x < t.x || event.position.x > t.right()) {
            const float travel = std::max(1.0f, area_.width - 4 - t.width);
            const float fromStart = rightToLeft() ? area_.right() - 2 - event.position.x - t.width / 2 : event.position.x - area_.x - 2 - t.width / 2;
            scrollTo(fromStart / travel * maxOffset());
        }
        grab_ = event.position.x;
        grabOffset_ = offset_;
    }

    void onMouse(MouseEvent& event) override {
        switch (event.type) {
        case MouseEventType::Wheel:
            if (maxOffset() <= 0)
                return;
            // up (or left in left to right) towards the start
            scrollTo(offset_ + (-event.wheelDelta.y + (rightToLeft() ? -event.wheelDelta.x : event.wheelDelta.x)) * wheelStep);
            event.handled = true;
            break;
        case MouseEventType::Move:
            if (!grab_)
                return;
            {
                const float travel = std::max(1.0f, area_.width - 4 - thumb().width);
                const float moved = (event.position.x - *grab_) * (rightToLeft() ? -1.0f : 1.0f);
                scrollTo(grabOffset_ + moved / travel * maxOffset());
            }
            event.handled = true;
            break;
        case MouseEventType::Release:
            if (grab_) {
                grab_.reset();
                invalidatePaint();
                event.handled = true;
            }
            break;
        case MouseEventType::Press:
            break;
        }
    }

    void onHoverChanged(bool) override { invalidatePaint(); }

private:
    float maxOffset() const { return std::max(0.0f, total_ - area_.width); }

    Rect thumb() const {
        const float track = std::max(0.0f, area_.width - 4);
        const float width = std::min(track, std::max(24.0f, track * area_.width / std::max(total_, 1.0f)));
        const float along = maxOffset() > 0 ? offset_ / maxOffset() * (track - width) : 0;
        const float x = rightToLeft() ? area_.right() - 2 - along - width : area_.x + 2 + along;
        const float height = grab_ ? 4.0f : 3.0f;
        return {x, size().height - height - 1, width, height};
    }

    void scrollTo(float offset) {
        reveal_.reset();        // scrolled by the user: where the user says
        offset_ = std::clamp(offset, 0.0f, maxOffset());
        rearrange();            // the tabs move: nothing measured again
        invalidatePaint();
    }

    Rect area_;
    float total_ = 0;
    float offset_ = 0;          // dips scrolled from the start
    std::optional<std::size_t> reveal_;
    std::optional<float> grab_; // the thumb dragged: where it was pressed
    float grabOffset_ = 0;
};

// A button after a stack's tabs: an icon, a background while the mouse is over it; clicked, its handler after the event.
class DockToolButton final : public Widget {
public:
    DockToolButton(std::function<void(PaintContext&, const Rect&, Color)> icon, std::function<void()> onClick)
        : icon_(std::move(icon)), onClick_(std::move(onClick)) {
        setAlignment(Alignment::Start, Alignment::Center);
    }

protected:
    Size measureContent(Size) override { return {24, 24}; }

    void paint(PaintContext& context) override {
        const Palette& palette = context.ui.theme.palette;
        const Rect r{0, 0, size().width, size().height};
        if (hovered())
            context.painter.fillRoundedRect(r, 4, pressed_ ? palette.pressed : palette.hover);
        const DockStyle& s = context.ui.theme.dock;
        if (icon_)
            icon_(context, {r.center().x - 6, r.center().y - 6, 12, 12}, hovered() ? s.text : s.mutedText);
    }

    void onMouse(MouseEvent& event) override {
        if (event.button != platform::MouseButton::Left || event.type == MouseEventType::Move || event.type == MouseEventType::Wheel)
            return;
        event.handled = true;
        if (event.type == MouseEventType::Press) {
            pressed_ = true;
        } else if (std::exchange(pressed_, false) && Rect{0, 0, size().width, size().height}.contains(event.position) && onClick_) {
            // it may take this button away (another panel shown, this one floated): after the event
            if (UiRoot* r = root())
                r->defer(onClick_);
            else
                onClick_();
        }
        invalidatePaint();
    }

    void onHoverChanged(bool) override { invalidatePaint(); }

private:
    std::function<void(PaintContext&, const Rect&, Color)> icon_;
    std::function<void()> onClick_;
    bool pressed_ = false;
};

// The float button's icons: a box with an arrow out of its corner (float), a box filled at its bottom (dock).
void floatIcon(PaintContext& context, const Rect& r, Color color) {
    paint::Painter& p = context.painter;
    p.strokeRoundedRect({r.x + 0.5f, r.y + 3.5f, r.width - 4, r.height - 4}, 1.5f, 1.2f, color);
    p.drawLine({r.x + r.width * 0.45f, r.y + r.height * 0.55f}, {r.right(), r.y}, 1.2f, color);
    p.drawLine({r.right() - 5, r.y}, {r.right(), r.y}, 1.2f, color);
    p.drawLine({r.right(), r.y}, {r.right(), r.y + 5}, 1.2f, color);
}

// The hide button's icon: a box with the corner the bar goes back to.
void hideIcon(PaintContext& context, const Rect& r, Color color) {
    paint::Painter& p = context.painter;
    p.strokeRoundedRect({r.x + 0.5f, r.y + 0.5f, r.width - 1, r.height - 1}, 1.5f, 1.2f, color);
    const Point corner[] = {{r.x + 2, r.y + 2}, {r.x + r.width * 0.55f, r.y + 2}, {r.x + 2, r.y + r.height * 0.55f}};
    p.fillConvexPolygon(corner, color);
}

void dockIcon(PaintContext& context, const Rect& r, Color color) {
    paint::Painter& p = context.painter;
    p.strokeRoundedRect({r.x + 0.5f, r.y + 0.5f, r.width - 1, r.height - 1}, 1.5f, 1.2f, color);
    p.fillRect({r.x + 0.5f, r.y + r.height * 0.55f, r.width - 1, r.height * 0.45f - 0.5f}, color);
}

// ---- layouts as JSON ----

void appendNumber(std::string& out, float value) {
    char text[32];
    std::snprintf(text, sizeof text, "%g", static_cast<double>(value));
    out += text;
}

void writeNode(std::string& out, const Widget& node, const Widget* center) {
    out += "{\"share\":";
    appendNumber(out, node.stretch() > 0 ? node.stretch() : 1.0f);
    if (&node == center) {
        out += ",\"center\":true}";
    } else if (const auto* split = dynamic_cast<const DockSplit*>(&node)) {
        out += split->orientation() == Orientation::Horizontal ? ",\"split\":\"horizontal\",\"panes\":[" : ",\"split\":\"vertical\",\"panes\":[";
        bool first = true;
        for (const auto& child : split->children()) {
            if (!first)
                out += ',';
            writeNode(out, *child, center);
            first = false;
        }
        out += "]}";
    } else if (const auto* tabs = dynamic_cast<const DockTabs*>(&node)) {
        out += ",\"tabs\":[";
        for (std::size_t i = 0; i < tabs->count(); ++i) {
            if (i > 0)
                out += ',';
            json::appendString(out, tabs->panelId(i));
        }
        out += "],\"active\":";
        appendNumber(out, static_cast<float>(tabs->active()));
        if (tabs->tabBarHidden())
            out += ",\"hidden\":true";
        out += '}';
    }
}

// The shape of a saved node, before anything is changed for it.
bool validNode(const json::Value& node, std::string& error) {
    if (!node.object()) {
        error = "a node is an object";
        return false;
    }
    if (const json::Value* split = node.member("split")) {
        const json::Value* panes = node.member("panes");
        if (!split->string() || (*split->string() != "horizontal" && *split->string() != "vertical") || !panes || !panes->array()) {
            error = "a split is \"horizontal\" or \"vertical\", with its \"panes\"";
            return false;
        }
        return std::all_of(panes->array()->begin(), panes->array()->end(), [&](const json::Value& pane) { return validNode(pane, error); });
    }
    if (node.member("center"))
        return true;
    const json::Value* tabs = node.member("tabs");
    if (!tabs || !tabs->array() ||
        !std::all_of(tabs->array()->begin(), tabs->array()->end(), [](const json::Value& id) { return id.string() != nullptr; })) {
        error = "a node is a split or has its \"tabs\" (panel ids)";
        return false;
    }
    return true;
}

float numberOf(const json::Value* value, float otherwise) {
    const double* number = value ? std::get_if<double>(&value->data) : nullptr;
    return number ? static_cast<float>(*number) : otherwise;
}

// The node `saved` describes, with the panels it names taken from `panels` (and the center, where it says); none when it
// would be empty.
std::unique_ptr<Widget> buildNode(const json::Value& saved, std::map<std::string, DockPanel>& panels, std::unique_ptr<Widget>& center,
                                  bool barsHide) {
    const float share = std::max(0.01f, numberOf(saved.member("share"), 1));
    if (saved.member("center")) {
        if (center)
            center->setStretch(share);
        return std::move(center);
    }
    if (const json::Value* split = saved.member("split")) {
        auto node = std::make_unique<DockSplit>(*split->string() == "horizontal" ? Orientation::Horizontal : Orientation::Vertical);
        for (const json::Value& pane : *saved.member("panes")->array()) {
            if (auto child = buildNode(pane, panels, center, barsHide))
                node->addChild(std::move(child));
        }
        if (node->children().empty())
            return nullptr;
        if (node->children().size() == 1) {     // a split of one is that one
            std::unique_ptr<Widget> only = node->removeChild(*node->children().front());
            only->setStretch(share);
            return only;
        }
        node->setStretch(share);
        return node;
    }
    auto tabs = std::make_unique<DockTabs>();
    for (const json::Value& id : *saved.member("tabs")->array()) {
        const auto panel = panels.find(*id.string());
        if (panel == panels.end())
            continue;
        tabs->add(std::move(panel->second));
        panels.erase(panel);
    }
    if (tabs->count() == 0)
        return nullptr;
    tabs->setActive(std::min(static_cast<std::size_t>(std::max(0.0f, numberOf(saved.member("active"), 0))), tabs->count() - 1));
    const json::Value* hidden = saved.member("hidden");
    if (const bool* yes = hidden ? std::get_if<bool>(&hidden->data) : nullptr; yes && *yes && barsHide)
        tabs->setTabBarHidden(true);
    tabs->setStretch(share);
    return tabs;
}

// The stacks of a tree: in its splits, not in its center (whose areas are others).
template <class F>
void forEachTabs(Widget& node, F&& f) {
    if (auto* tabs = dynamic_cast<DockTabs*>(&node)) {
        f(*tabs);
        return;
    }
    if (dynamic_cast<DockSplit*>(&node)) {
        for (const auto& child : node.children())
            forEachTabs(*child, f);
    }
}

// The guide for `side` in `content`, a cross of five in its middle, as large as it lets them be (12 to 32 dips).
Rect guideIn(const Rect& content, bool rtl, DockSide side) {
    constexpr float gap = 4;
    const float g = std::clamp((std::min(content.width, content.height) - 2 * gap - 8) / 3, 12.0f, 32.0f);
    const Point c = content.center();
    float dx = 0, dy = 0;
    switch (side) {
    case DockSide::Center: break;
    case DockSide::Start: dx = rtl ? 1.0f : -1.0f; break;
    case DockSide::End: dx = rtl ? -1.0f : 1.0f; break;
    case DockSide::Top: dy = -1; break;
    case DockSide::Bottom: dy = 1; break;
    }
    return {c.x - g / 2 + dx * (g + gap), c.y - g / 2 + dy * (g + gap), g, g};
}

// Where a dropped tab goes in a stack (below its tabs) or a center (all of it), in its coordinates.
Rect dropRoom(const Widget& node) {
    const auto* tabs = dynamic_cast<const DockTabs*>(&node);
    const float bar = tabs ? tabs->barHeight() : 0.0f;
    return {0, bar, node.size().width, std::max(0.0f, node.size().height - bar)};
}

Rect guideOf(const Widget& node, DockSide side) {
    return guideIn(dropRoom(node), node.rightToLeft(), side);
}

void collectPanels(const json::Value& node, std::vector<std::string>& ids) {
    if (const json::Value* tabs = node.member("tabs"); tabs && tabs->array()) {
        for (const json::Value& id : *tabs->array()) {
            if (id.string())
                ids.push_back(*id.string());
        }
    }
    if (const json::Value* panes = node.member("panes"); panes && panes->array()) {
        for (const json::Value& pane : *panes->array())
            collectPanels(pane, ids);
    }
}

std::size_t indexIn(const Widget& parent, const Widget& child) {
    const auto& children = parent.children();
    return static_cast<std::size_t>(
        std::find_if(children.begin(), children.end(), [&](const auto& c) { return c.get() == &child; }) - children.begin());
}

} // namespace

// ---- stacks ----

DockTabs::DockTabs() {
    setMinimumSize({90, 70});
    setClipsChildren(true);
    bar_ = &addChild(std::make_unique<TabRow>());
    auto tools = std::make_unique<Widget>();
    tools->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal));
    tools->setHitTestVisible(false);        // between the buttons: the stack
    tools->setPadding({2, 0, 4, 0});
    tools_ = &addChild(std::move(tools));
}

DockArea* DockTabs::area() const {
    for (Widget* w = parent(); w; w = w->parent()) {
        if (auto* area = dynamic_cast<DockArea*>(w))
            return area;
    }
    return nullptr;
}

const DockStyle& DockTabs::style() const {
    if (const UiContext* ui = context())
        return ui->theme.dock;
    static const DockStyle unstyled = Theme::dark().dock;
    return unstyled;
}

std::optional<std::size_t> DockTabs::indexOf(std::string_view id) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == id)
            return i;
    }
    return std::nullopt;
}

void DockTabs::add(DockPanel panel, std::size_t index) {
    index = std::min(index, entries_.size());
    std::unique_ptr<Widget> content = panel.content ? std::move(panel.content) : std::make_unique<Widget>();
    Widget& shown = addChild(std::move(content));
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(index),
                    Entry{std::move(panel.id), std::move(panel.title), &shown, panel.closable, std::move(panel.buttons), panel.scope});
    if (entries_.size() > 1)
        setTabBarHidden(false);     // its tabs: to choose between them
    rebuildBar();
    setActive(index);
}

DockPanel DockTabs::take(std::size_t index) {
    Entry entry = std::move(entries_[index]);
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index));
    std::unique_ptr<Widget> content = removeChild(*entry.content);
    content->setVisible(true);
    if (index < active_ || active_ >= entries_.size())
        active_ = active_ > 0 ? active_ - 1 : 0;
    rebuildBar();
    if (!entries_.empty())
        setActive(active_);
    else
        rebuildTools();
    return DockPanel{std::move(entry.id), std::move(entry.title), std::move(content), entry.closable, std::move(entry.buttons), entry.scope};
}

void DockTabs::setActive(std::size_t index) {
    if (entries_.empty())
        return;
    active_ = std::min(index, entries_.size() - 1);
    for (std::size_t i = 0; i < entries_.size(); ++i)
        entries_[i].content->setVisible(i == active_);
    // the tabs are told in place: one of them may be the one pressed, in its own handler
    for (std::size_t i = 0; i < bar_->children().size(); ++i)
        static_cast<DockTab&>(*bar_->children()[i]).setActive(i == active_);
    static_cast<TabRow&>(*bar_).reveal(active_);
    rebuildTools();
    invalidateLayout();
}

void DockTabs::rebuildTools() {
    while (!tools_->children().empty())
        tools_->removeChild(*tools_->children().back());
    if (entries_.empty())
        return;
    for (const DockButton& button : entries_[active_].buttons)
        tools_->add(std::make_unique<DockToolButton>(button.icon, button.onClick));
    // one panel, docked (a floating window's bar is its title): its tab bar may be hidden
    if (const DockArea* a = area(); entries_.size() == 1 && a && !a->floating())
        tools_->add(std::make_unique<DockToolButton>(hideIcon, [this] { setTabBarHidden(true); }));
    // the float button, where there are windows to float in
    const UiContext* ui = context();
    if (!ui || !ui->docking)
        return;
    auto icon = [this](PaintContext& context, const Rect& r, Color color) {
        const DockArea* a = area();
        (a && a->floating() ? dockIcon : floatIcon)(context, r, color);
    };
    tools_->add(std::make_unique<DockToolButton>(icon, [this] {
        if (DockArea* a = area(); a && !entries_.empty())
            a->toggleFloating(entries_[active_].id);
    }));
}

void DockTabs::onRootChanged() {
    rebuildTools();     // the float button: with a docking host only
}

void DockTabs::rebuildBar() {
    while (!bar_->children().empty())
        bar_->removeChild(*bar_->children().back());
    for (std::size_t i = 0; i < entries_.size(); ++i)
        bar_->add(std::make_unique<DockTab>(*this, i, entries_[i].title, entries_[i].closable, i == active_));
}

Size DockTabs::measureContent(Size available) {
    const float bar = barHeight();
    bar_->measure({available.width, bar});
    tools_->measure({unbounded, bar});
    Size content;
    for (const Entry& e : entries_) {
        if (e.content->visible())
            content = e.content->measure({available.width, std::max(0.0f, available.height - bar)});
    }
    return {content.width, bar + content.height};
}

void DockTabs::arrangeContent(const Rect& content) {
    const float bar = barHeight();
    const bool rtl = rightToLeft();
    // the bar: what the window buttons leave (a floating window's), the tools at its end, the tabs before them
    const float reserve = std::min(reserve_, content.width);
    const Rect line{reserveLeft_ ? content.x + reserve : content.x, content.y, content.width - reserve, bar};
    const float tools = std::min(tools_->measure({unbounded, bar}).width, line.width);
    arrangeInCell(*tools_, {rtl ? line.x : line.right() - tools, line.y, tools, bar}, rtl);
    // a window's title keeps a little room after its tabs, to drag the window by
    const float grip = title_ ? std::min(titleGrip, line.width - tools) : 0.0f;
    const float tabs = std::max(0.0f, line.width - tools - grip);
    arrangeInCell(*bar_, {rtl ? line.right() - tabs : line.x, line.y, tabs, bar}, rtl);
    const Rect below{content.x, content.y + bar, content.width, std::max(0.0f, content.height - bar)};
    for (const Entry& e : entries_) {
        if (e.content->visible())
            arrangeInCell(*e.content, below, rtl);
    }
}

void DockTabs::setTitleArea(bool title, float reserve, bool left) {
    // where the system places windows, it moves them too: there, the title is its caption
    const UiContext* ui = context();
    const bool system = title && ui && ui->docking && !ui->docking->placesWindows();
    const platform::HitArea caption = system ? platform::HitArea::Caption : platform::HitArea::Client;
    bar_->setWindowArea(caption);
    setWindowArea(caption);
    if (reserve == reserve_ && left == reserveLeft_ && title == title_)
        return;
    title_ = title;     // its area drags the window from there itself: docking as it goes
    reserve_ = reserve;
    reserveLeft_ = left;
    rearrange();            // in its area's arrangement: the bar again, nothing measured again
}

Rect DockTabs::guide(DockSide side) const {
    return guideIn(dropRoom(*this), rightToLeft(), side);
}

void DockTabs::paint(PaintContext& context) {
    // an island: a rounded layer on the window; its tabs over a hairline
    const DockStyle& s = style();
    const Rect r{0, 0, size().width, size().height};
    context.painter.fillRoundedRect(r, s.cornerRadius, s.background);
    context.painter.strokeRoundedRect(r, s.cornerRadius, 1, s.border);
    if (!barHidden_)
        context.painter.fillRect({s.cornerRadius / 2, s.tabHeight - 1, std::max(0.0f, r.width - s.cornerRadius), 1}, s.separator);
}

float DockTabs::barHeight() const {
    return barHidden_ ? 0.0f : style().tabHeight;
}

void DockTabs::setTabBarHidden(bool hidden) {
    const DockArea* a = area();
    hidden = hidden && entries_.size() == 1 && !(a && a->floating());
    if (hidden == barHidden_)
        return;
    barHidden_ = hidden;
    cornerHovered_ = false;
    bar_->setVisible(!hidden);
    tools_->setVisible(!hidden);
    invalidateLayout();
}

Rect DockTabs::revealCorner() const {
    constexpr float size = 14;
    return rightToLeft() ? Rect{this->size().width - size, 0, size, size} : Rect{0, 0, size, size};
}

void DockTabs::paintOver(PaintContext& context) {
    if (!barHidden_)
        return;
    // the corner the bar went to: a little triangle, the accent's under the mouse
    const Rect c = revealCorner();
    const float inset = 3, length = cornerHovered_ ? 11.0f : 8.0f;
    const bool rtl = rightToLeft();
    const float x = rtl ? c.right() - inset : c.x + inset, dx = rtl ? -length : length;
    const Point triangle[] = {{x, c.y + inset}, {x + dx, c.y + inset}, {x, c.y + inset + length}};
    const DockStyle& s = style();
    context.painter.fillConvexPolygon(triangle, cornerHovered_ ? s.activeLine : s.mutedText);
}

void DockTabs::onPreviewMouse(MouseEvent& event) {
    if (!barHidden_)
        return;
    const bool over = revealCorner().contains(event.position);
    if (over != cornerHovered_) {
        cornerHovered_ = over;
        invalidatePaint();
    }
    // pressed: the bar back (before the panel under it gets the press)
    if (over && event.type == MouseEventType::Press && event.button == platform::MouseButton::Left) {
        event.handled = true;
        setTabBarHidden(false);
    }
}

void DockTabs::onHoverChanged(bool hovered) {
    if (!hovered && cornerHovered_) {
        cornerHovered_ = false;
        invalidatePaint();
    }
}

void DockSplit::onRootChanged() {
    onStyleChanged();
}

void DockSplit::onStyleChanged() {
    if (const UiContext* ui = context())
        setStyle(ui->theme.dock.splitter);
}

// ---- the area ----

DockArea::DockArea(DockScope scope) : group_(std::make_shared<DockGroup>()), scope_(scope) {
    group_->home = this;
    group_->areas.push_back(this);
}

DockArea::~DockArea() {
    std::erase(group_->areas, this);
    if (group_->home != this)
        return;
    // the floating windows go above another area of the group (another window's, for Global), or go, their panels too
    group_->home = homeFor();
    if (DockArea* home = group_->home) {
        home->showFloating(home->visibleInTree());
        return;
    }
    if (group_->host) {
        for (DockArea* area : group_->areas) {
            if (const UiRoot* r = area->root())
                group_->host->closeWindow(*r);
        }
    }
}

DockArea* DockArea::homeFor() const {
    DockingHost* h = group_->host;
    DockArea* found = nullptr;
    for (DockArea* area : group_->areas) {
        const UiRoot* r = area->root();
        if (area->floating_ || !r || (h && h->closing(*r)))
            continue;
        if (area == group_->home)
            return area;        // the home stays, when it can
        found = found ? found : area;
    }
    return found;
}

Rect DockArea::guide(const Widget& node, DockSide side) {
    return guideOf(node, side);
}

bool DockArea::inGroup(const DockArea* area) const {
    return std::find(group_->areas.begin(), group_->areas.end(), area) != group_->areas.end();
}

void DockArea::setCenter(std::unique_ptr<Widget> center) {
    GLW_EXPECTS(center && !center_ && !node());
    // its room kept: the splits around it do not take it below this
    const Size own = center->minimumSize();
    center->setMinimumSize({std::max(own.width, centerMinimum.width), std::max(own.height, centerMinimum.height)});
    center_ = &addChild(std::move(center), 0);
}

std::unique_ptr<DockArea> DockArea::makeSibling(std::unique_ptr<Widget> center) {
    auto area = std::make_unique<DockArea>(scope_);
    area->group_ = group_;              // its own group goes with it
    group_->areas.push_back(area.get());
    if (center)
        area->setCenter(std::move(center));
    return area;
}

DockingHost* DockArea::host() const {
    const UiContext* ui = context();
    return ui ? ui->docking : nullptr;
}

bool DockArea::placesWindows() const {
    DockingHost* h = host();
    return h && h->placesWindows();
}

std::size_t DockArea::panelCount() const {
    std::size_t count = 0;
    if (Widget* n = node())
        forEachTabs(*n, [&](DockTabs& tabs) { count += tabs.count(); });
    return count;
}

DockArea* DockArea::home() const {
    return group_->home;
}

std::vector<DockArea*> DockArea::floatingAreas() const {
    std::vector<DockArea*> areas;
    for (DockArea* area : group_->areas) {
        if (area->floating_)
            areas.push_back(area);
    }
    return areas;
}

Widget* DockArea::node() const {
    return children().empty() || children().front().get() == buttons_ ? nullptr : children().front().get();
}

DockTabs* DockArea::firstTabs() const {
    DockTabs* found = nullptr;
    if (Widget* n = node())
        forEachTabs(*n, [&](DockTabs& tabs) { found = found ? found : &tabs; });
    return found;
}

DockTabs* DockArea::tabsOf(std::string_view id) const {
    DockTabs* found = nullptr;
    for (const DockArea* area : group_->areas) {
        if (Widget* n = area->node())
            forEachTabs(*n, [&](DockTabs& tabs) { found = !found && tabs.indexOf(id) ? &tabs : found; });
    }
    return found;
}

std::vector<std::string> DockArea::panelIds() const {
    std::vector<std::string> ids;
    for (const DockArea* area : group_->areas) {
        if (Widget* n = area->node()) {
            forEachTabs(*n, [&](DockTabs& tabs) {
                for (std::size_t i = 0; i < tabs.count(); ++i)
                    ids.push_back(tabs.panelId(i));
            });
        }
    }
    return ids;
}

DockTabs& DockArea::addPanel(DockPanel panel, Widget* nextTo, DockSide side, float share) {
    GLW_EXPECTS(panel.scope == scope_);     // a panel goes only into areas of its scope
    auto* stack = dynamic_cast<DockTabs*>(nextTo);
    GLW_EXPECTS(!nextTo || stack || (center_ && nextTo == center_));
    // beside the center (never in it)
    if (center_ && nextTo == center_)
        return insertBeside(std::move(panel), *center_, side == DockSide::Center ? DockSide::End : side, share);
    if (center_ && !firstTabs() && (side == DockSide::Center || !nextTo))
        return insertBeside(std::move(panel), *node(), side == DockSide::Center ? DockSide::End : side, share);
    if (!node()) {
        auto& tabs = static_cast<DockTabs&>(addChild(std::make_unique<DockTabs>(), 0));     // under the window buttons
        tabs.add(std::move(panel));
        return tabs;
    }
    if (side == DockSide::Center) {
        DockTabs& tabs = stack ? *stack : *firstTabs();
        tabs.add(std::move(panel));
        return tabs;
    }
    return insertBeside(std::move(panel), stack ? static_cast<Widget&>(*stack) : *node(), side, share);
}

DockTabs& DockArea::insertBeside(DockPanel panel, Widget& target, DockSide side, float share) {
    const Orientation orientation = side == DockSide::Start || side == DockSide::End ? Orientation::Horizontal : Orientation::Vertical;
    const bool before = side == DockSide::Start || side == DockSide::Top;
    share = std::clamp(share, 0.05f, 0.95f);
    auto created = std::make_unique<DockTabs>();
    DockTabs& tabs = *created;
    tabs.add(std::move(panel));

    Widget* parent = target.parent();
    const float targetShare = target.stretch() > 0 ? target.stretch() : 1.0f;
    if (auto* split = dynamic_cast<DockSplit*>(parent); split && split->orientation() == orientation) {
        // a split of that way already: a pane more in it, taking its share from the target's
        const std::size_t at = indexIn(*split, target) + (before ? 0 : 1);
        created->setStretch(targetShare * share);
        target.setStretch(targetShare * (1 - share));
        split->addChild(std::move(created), at);
        return tabs;
    }
    // the target becomes a split of itself and the new stack, where it was
    const std::size_t at = indexIn(*parent, target);
    auto split = std::make_unique<DockSplit>(orientation);
    split->setStretch(targetShare);
    std::unique_ptr<Widget> moved = parent->removeChild(target);
    moved->setStretch(1 - share);
    created->setStretch(share);
    if (before) {
        split->addChild(std::move(created));
        split->addChild(std::move(moved));
    } else {
        split->addChild(std::move(moved));
        split->addChild(std::move(created));
    }
    parent->addChild(std::move(split), at);
    return tabs;
}

void DockArea::prune(DockTabs& tabs) {
    if (tabs.count() > 0)
        return;
    Widget* parent = tabs.parent();
    parent->removeChild(tabs);
    auto* split = dynamic_cast<DockSplit*>(parent);
    if (!split || split->children().size() != 1)
        return;
    // a split of one gives way to it, with its share
    Widget* grandparent = split->parent();
    const std::size_t at = indexIn(*grandparent, *split);
    std::unique_ptr<Widget> only = split->removeChild(*split->children().front());
    only->setStretch(split->stretch());
    grandparent->removeChild(*split);
    grandparent->addChild(std::move(only), at);
}

std::unique_ptr<Widget> DockArea::removePanel(std::string_view id) {
    DockTabs* tabs = tabsOf(id);
    if (!tabs)
        return nullptr;
    DockArea* from = tabs->area();
    DockPanel panel = tabs->take(*tabs->indexOf(id));
    prune(*tabs);
    from->closeIfEmpty();
    return std::move(panel.content);
}

void DockArea::closePanel(std::string_view id) {
    auto close = [this, id = std::string(id)] {
        DockTabs* tabs = tabsOf(id);
        if (!tabs)
            return;
        DockArea* from = tabs->area();
        DockPanel panel = tabs->take(*tabs->indexOf(id));
        prune(*tabs);
        from->closeIfEmpty();
        // the home's handler, for a panel of a floating window too
        DockArea* handling = group_->home ? group_->home : this;
        if (handling->onPanelClosed_)
            handling->onPanelClosed_(std::move(panel));
    };
    if (UiRoot* r = root())
        r->defer(std::move(close));
    else
        close();
}

void DockArea::move(const std::string& id, Widget& target, DockSide side, bool raise) {
    DockTabs* source = tabsOf(id);
    auto* stack = dynamic_cast<DockTabs*>(&target);
    if (!source || (side == DockSide::Center && (!stack || stack == source)))
        return;
    DockArea* from = source->area();
    DockPanel panel = source->take(*source->indexOf(id));
    if (side == DockSide::Center)
        stack->add(std::move(panel));
    else
        insertBeside(std::move(panel), target, side, 0.5f);
    prune(*source);         // after: a split it leaves may give way to the target
    from->closeIfEmpty();
    if (DockingHost* h = group_->host; raise && h && target.root() && target.root() != root())
        h->raiseWindow(*target.root());
}

DockArea* DockArea::floatPanels(const std::vector<std::string>& ids, const std::string& shown, Point position, Size size, bool focus) {
    if (ids.empty())
        return nullptr;
    DockArea* floated = floatPanel(ids.front(), position, size, focus);
    DockTabs* stack = floated ? floated->tabsOf(ids.front()) : nullptr;
    if (!stack)
        return floated;
    for (std::size_t i = 1; i < ids.size(); ++i)
        move(ids[i], *stack, DockSide::Center, false);      // its window not raised: a drag may go on
    if (const auto index = stack->indexOf(shown))
        stack->setActive(*index);
    return floated;
}

void DockArea::toggleFloating(std::string_view id) {
    auto apply = [this, id = std::string(id)] {
        DockTabs* tabs = tabsOf(id);
        DockingHost* h = group_->host;
        const UiRoot* r = tabs ? tabs->root() : nullptr;
        if (!r || !h)
            return;
        DockArea* from = tabs->area();
        if (from->floating_) {
            // back to the area its window belongs to, in its first stack
            DockArea* to = homeFor();
            if (!to || !to->root())
                return;
            DockPanel panel = tabs->take(*tabs->indexOf(id));
            prune(*tabs);
            to->addPanel(std::move(panel));
            from->closeIfEmpty();
            h->raiseWindow(*to->root());
            return;
        }
        // by where its stack is, a little in from its corner, as large
        const Point position = h->toScreen(*r, tabs->mapToRoot({24, 24}));
        floatPanel(id, position, {std::max(tabs->size().width, 240.0f), std::max(tabs->size().height, 160.0f)});
    };
    if (UiRoot* r = root())
        r->defer(std::move(apply));
    else
        apply();
}

void DockArea::move(const std::vector<std::string>& ids, Widget& target, DockSide side) {
    if (ids.empty())
        return;
    move(ids.front(), target, side);
    for (std::size_t i = 1; i < ids.size(); ++i) {
        if (DockTabs* into = tabsOf(ids.front()))
            move(ids[i], *into, DockSide::Center);
    }
}

DockArea* DockArea::floatPanel(const std::string& id, Point position, Size size, bool focus) {
    DockingHost* h = host();
    DockTabs* source = tabsOf(id);
    if (!h || !source)
        return nullptr;
    DockArea* from = source->area();
    DockPanel panel = source->take(*source->indexOf(id));
    prune(*source);
    std::unique_ptr<DockArea> area = makeSibling(nullptr);
    area->floating_ = true;
    LocalizedText title = panel.title;
    area->addPanel(std::move(panel));
    DockArea* floated = area.get();
    // above the window it was in (the home's, from a floating one)
    const DockArea* above = !from->floating_ ? from : group_->home ? group_->home : this;
    const UiRoot* owner = above->root();
    h->openWindow({.title = std::move(title),
                   .content = std::move(area),
                   .titleCenter = nullptr,
                   .position = position,
                   .size = size,
                   .owner = owner,
                   .titleBar = false,
                   .focus = focus});
    from->closeIfEmpty();
    return floated;
}

void DockArea::closeIfEmpty() {
    if (floating_ && !node() && group_->host) {
        if (const UiRoot* r = root())
            group_->host->closeWindow(*r);
    }
}

void DockArea::showFloating(bool visible) {
    DockingHost* h = group_->host;
    const UiRoot* r = root();
    if (!h || !r || floating_ || group_->home != this)
        return;
    for (DockArea* area : group_->areas) {
        if (const UiRoot* window = area->floating_ ? area->root() : nullptr) {
            h->setWindowOwner(*window, r);
            h->setWindowVisible(*window, visible);
        }
    }
}

void DockArea::onRootChanged() {
    if (DockingHost* h = host())
        group_->host = h;
    if (root())
        showFloating(visibleInTree());
    makeButtons();
}

void DockArea::onStyleChanged() {
    makeButtons();
}

void DockArea::makeButtons() {
    const UiContext* ui = context();
    if (!floating_ || !ui)
        return;
    const bool atStart = ui->theme.frame.buttonsAtStart;
    if (buttons_ && atStart == (static_cast<const WindowButton&>(*buttons_->children().front()).kind() == WindowButtonKind::Close))
        return;
    if (buttons_)
        removeChild(*buttons_);
    auto buttons = std::make_unique<Widget>();
    buttons->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal));
    buttons->setHitTestVisible(false);      // between them, the bar under them
    // the order of the system (TitleBar); a tool window is not minimized on its own
    for (WindowButtonKind kind : atStart ? std::vector{WindowButtonKind::Close, WindowButtonKind::Maximize}
                                         : std::vector{WindowButtonKind::Maximize, WindowButtonKind::Close})
        buttons->add(std::make_unique<WindowButton>(kind));
    buttons_ = &addChild(std::move(buttons));       // last: over the node
}

void DockArea::arrangeContent(const Rect& content) {
    const bool rtl = rightToLeft();
    Widget* n = node();
    if (n)
        arrangeInCell(*n, content, rtl);
    if (!buttons_)
        return;
    // the buttons at the top, at the side of the system (mirrored with the direction), as high as a tab bar
    const UiContext* ui = context();
    const bool left = (ui && ui->theme.frame.buttonsAtStart) != rtl;
    const float height = ui ? ui->theme.dock.tabHeight : 30.0f;
    const float width = std::min(buttons_->measure({unbounded, height}).width, content.width);
    buttons_->arrange({left ? content.x : content.right() - width, content.y, width, height});
    // the stacks along the top drag the window; the one under the buttons leaves them room
    if (!n)
        return;
    const Point origin = mapToRoot({});
    forEachTabs(*n, [&](DockTabs& tabs) {
        const Point at = tabs.mapToRoot({}) - origin;
        const bool top = at.y <= content.y + 0.5f;
        const bool under = top && (left ? at.x <= content.x + 0.5f : at.x + tabs.size().width >= content.right() - 0.5f);
        tabs.setTitleArea(top, under ? width : 0.0f, left);
    });
}

void DockArea::onVisibleChanged(bool visible) {
    showFloating(visible);
}

void DockArea::onWindowClosing() {
    // a floating window closed by the user: its panels go back to the area they came from; a window closed (with a
    // Global area): its panels go to another window's
    if (!floating_ && group_->home == this) {
        // the floating windows go above another window, when there is one; otherwise they go with this area
        if (DockArea* other = homeFor()) {
            group_->home = other;
            other->showFloating(other->visibleInTree());
        }
    }
    DockArea* to = homeFor();
    if (!to || to == this || (!floating_ && scope_ != DockScope::Global))
        return;
    while (DockTabs* tabs = firstTabs()) {
        while (tabs->count() > 0)
            to->addPanel(tabs->take(0));
        prune(*tabs);
    }
}

// ---- dragging ----

std::pair<Widget*, DockArea*> DockArea::nodeAt(const UiRoot& root, Point point) const {
    // up from what is under the mouse: past the stacks of other groups (a major tab's, in a window's center)
    for (Widget* w = root.widgetAt(point); w; w = w->parent()) {
        if (auto* tabs = dynamic_cast<DockTabs*>(w); tabs && inGroup(tabs->area()))
            return {tabs, tabs->area()};
        for (DockArea* area : group_->areas) {
            if (area->center_ == w)
                return {w, area};
        }
    }
    return {nullptr, nullptr};
}

void DockArea::beginDrag(DockTabs& source, std::size_t index) {
    startDrag(source, {source.panelId(index)}, source.panelId(index));
}

void DockArea::startDrag(DockTabs& source, std::vector<std::string> ids, std::string shown) {
    const std::size_t dragged = ids.size();
    drag_ = Drag{&source, std::move(ids), std::move(shown)};
    UiRoot* r = root();
    if (!r)
        return;
    r->captureMouse(*this);
    if (DockingHost* h = host()) {
        h->captureMouse(true);
        // all the panels of a floating window: the window goes with them, seen through so the guides under it show
        if (floating_ && panelCount() == dragged && h->placesWindows()) {
            drag_->window = r;
            drag_->grab = h->mouseOnScreen() - h->windowPosition(*r);
            h->setWindowOpacity(*r, draggedOpacity);
        }
    }
}

void DockArea::beginTitleDrag() {
    UiRoot* r = root();
    DockingHost* h = host();
    DockTabs* first = firstTabs();
    if (!r || !h || !first)
        return;
    std::vector<std::string> ids;
    forEachTabs(*node(), [&](DockTabs& tabs) {
        for (std::size_t i = 0; i < tabs.count(); ++i)
            ids.push_back(tabs.panelId(i));
    });
    drag_ = Drag{first, std::move(ids), first->panelId(first->active())};
    drag_->window = r;
    drag_->grab = h->mouseOnScreen() - h->windowPosition(*r);
    h->captureMouse(true);
    h->setWindowOpacity(*r, draggedOpacity);
}

DockTabs* DockArea::titleAt(Point point) const {
    const UiRoot* r = root();
    Widget* w = r ? r->widgetAt(mapToRoot(point)) : nullptr;
    if (!w)
        return nullptr;
    // the stack itself (by its buttons, after its tabs) on its bar, or its tab row where it has no tab
    auto* tabs = dynamic_cast<DockTabs*>(w);
    if (tabs && tabs->mapFromRoot(mapToRoot(point)).y >= tabs->barHeight())
        return nullptr;
    if (!tabs) {
        tabs = dynamic_cast<DockTabs*>(w->parent());
        if (tabs && w != tabs->bar_)
            return nullptr;
    }
    return tabs && tabs->area() == this && tabs->count() > 0 ? tabs : nullptr;
}

bool DockArea::onTitle(Point point) const {
    const DockTabs* tabs = titleAt(point);
    return tabs && tabs->title_;
}

void DockArea::onMouse(MouseEvent& event) {
    const bool left = event.button == platform::MouseButton::Left;
    if (drag_) {
        // the area has the mouse while a tab of it, or its title, is dragged
        if (event.type == MouseEventType::Move)
            dragTo(mapToRoot(event.position));
        else if (event.type == MouseEventType::Release && left)
            endDrag(mapToRoot(event.position), true);
        event.handled = true;
        return;
    }
    // a title: a floating window's (pressed twice, the window is maximized or restored; dragged, it moves and docks), or a
    // stack's (dragged, its group of panels floats and docks)
    switch (event.type) {
    case MouseEventType::Press: {
        DockTabs* tabs = left ? titleAt(event.position) : nullptr;
        if (!tabs)
            return;
        const bool window = floating_ && tabs->title_;
        if (window && !placesWindows())
            return;     // where the system places windows, it moves them by their title itself
        event.handled = true;
        if (window && event.clicks == 2) {
            if (UiHost* h = root() ? root()->host() : nullptr)
                h->toggleMaximizeWindow();
            return;
        }
        titlePress_ = mapToRoot(event.position);
        pressedStack_ = window ? nullptr : tabs;
        break;
    }
    case MouseEventType::Move:
        if (!titlePress_)
            return;
        event.handled = true;
        if (const Point moved = mapToRoot(event.position) - *titlePress_; std::hypot(moved.x, moved.y) > dragThreshold) {
            titlePress_.reset();
            if (DockTabs* stack = std::exchange(pressedStack_, nullptr)) {
                std::vector<std::string> ids;
                for (std::size_t i = 0; i < stack->count(); ++i)
                    ids.push_back(stack->panelId(i));
                startDrag(*stack, std::move(ids), stack->panelId(stack->active()));
            } else {
                beginTitleDrag();
            }
            dragTo(mapToRoot(event.position));
        }
        break;
    case MouseEventType::Release:
        if (titlePress_ && left) {
            titlePress_.reset();
            pressedStack_ = nullptr;
            event.handled = true;
        }
        break;
    case MouseEventType::Wheel:
        break;
    }
}

std::pair<Point, Size> DockArea::floatPlace(const DockTabs& source, Point point) const {
    const UiRoot* r = root();
    DockingHost* h = host();
    if (!r || !h)
        return {};
    // the mouse a little into its tab bar, the size of its stack
    const Point grabbed = h->toScreen(*r, {40, 16}) - h->toScreen(*r, {});
    return {h->toScreen(*r, point) - grabbed, Size{std::max(source.size().width, 240.0f), std::max(source.size().height, 160.0f)}};
}

void DockArea::clearDrop() {
    if (indicator_ || guides_) {
        indicator_.reset();
        guides_.reset();
        invalidatePaint();
    }
}

void DockArea::tearOff(Point point) {
    UiRoot* r = root();
    DockingHost* h = host();
    if (!r || !h)
        return;
    drag_->tearing = true;
    const auto [position, size] = floatPlace(*drag_->source, point);
    r->defer([this, h, position, size] {
        if (!drag_)
            return;
        drag_->tearing = false;
        // shown without the focus: the window dragged from keeps it, and with it the mouse
        DockArea* floated = floatPanels(drag_->ids, drag_->shown, position, size, false);
        if (!floated || !floated->root()) {
            drag_.reset();
            h->captureMouse(false);
            return;
        }
        drag_->source = floated->tabsOf(drag_->ids.front());
        drag_->window = floated->root();
        drag_->grab = h->mouseOnScreen() - position;
        h->setWindowOpacity(*drag_->window, draggedOpacity);
    });
}

void DockArea::dragTo(Point point) {
    if (!drag_ || drag_->tearing)
        return;
    drag_->target = nullptr;
    DockArea* shown = std::exchange(drag_->shownArea, nullptr);
    UiRoot* r = root();
    if (!r)
        return;
    // the window and the point the mouse is over
    DockingHost* h = host();
    const UiRoot* over = r;
    Point at = point;
    if (h && drag_->window) {
        // a window following the mouse: what is under it
        const Point mouse = h->mouseOnScreen();
        h->moveWindow(*drag_->window, mouse - drag_->grab);
        const DockingHost::Hit hit = h->windowAt(mouse, drag_->window);
        over = hit.root;
        at = hit.point;
    } else if (h && h->placesWindows()) {
        // a tab along its stack's tab bar stays there; out of it, it floats now
        const DockTabs& source = *drag_->source;
        const Point o = source.mapToRoot({});
        const float bar = source.barHeight();
        if (!Rect{o.x, o.y - tearReach, source.size().width, bar + 2 * tearReach}.contains(point)) {
            if (shown)
                shown->clearDrop();
            tearOff(point);
        }
        return;
    }
    // in its window only: out of it, it would float in a window the system places
    drag_->outOfWindow = !Rect{0, 0, r->size().width, r->size().height}.contains(point);
    auto [node, area] = over ? nodeAt(*over, at) : std::pair<Widget*, DockArea*>{};
    if (shown && shown != area)
        shown->clearDrop();

    // the guides of the stack (or of the center); on one of them (or on the stack's tabs), where the tab would go
    auto* tabs = dynamic_cast<DockTabs*>(node);
    const bool own = tabs && tabs == drag_->source && !drag_->window;
    if (own && tabs->count() <= drag_->ids.size()) {
        area->clearDrop();      // all its tabs dragged: nothing they could do there
        node = nullptr;
    }
    if (!node)
        return;
    const Point local = node->mapFromRoot(at);
    const float bar = tabs ? tabs->barHeight() : 0.0f;
    std::optional<DockSide> side;
    if (tabs && local.y >= 0 && local.y < bar && local.x >= 0 && local.x < tabs->size().width)
        side = DockSide::Center;
    const bool middle = tabs && !own;       // a center takes a tab on its sides only
    for (DockSide s : {DockSide::Center, DockSide::Start, DockSide::End, DockSide::Top, DockSide::Bottom}) {
        if ((s != DockSide::Center || middle) && guideOf(*node, s).contains(local))
            side = s;
    }
    if (side == DockSide::Center && !middle)
        side.reset();       // where it is already
    area->guides_ = Guides{node, side, middle};
    area->indicator_.reset();
    if (side) {
        Rect zone = dropRoom(*node);
        const bool rtl = node->rightToLeft();
        if (*side == DockSide::Start || *side == DockSide::End) {
            zone.width /= 2;
            if ((*side == DockSide::End) != rtl)        // the right half
                zone.x += zone.width;
        } else if (*side == DockSide::Top || *side == DockSide::Bottom) {
            zone.height /= 2;
            if (*side == DockSide::Bottom)
                zone.y += zone.height;
        }
        const Point offset = node->mapToRoot({}) - area->mapToRoot({});
        area->indicator_ = Rect{zone.x + offset.x, zone.y + offset.y, zone.width, zone.height};
        drag_->target = node;
        drag_->side = *side;
    }
    area->invalidatePaint();
    drag_->shownArea = area;
}

void DockArea::endDrag(Point point, bool drop) {
    if (!drag_)
        return;
    dragTo(point);
    const Drag done = *drag_;
    drag_.reset();
    if (done.shownArea)
        done.shownArea->clearDrop();
    UiRoot* r = root();
    DockingHost* h = host();
    if (h) {
        h->captureMouse(false);
        if (done.window)
            h->setWindowOpacity(*done.window, 1);
    }
    if (!drop)
        return;
    std::function<void()> apply;
    if (done.target) {
        apply = [this, ids = done.ids, shown = done.shown, target = done.target, side = done.side] {
            move(ids, *target, side);
            if (DockTabs* tabs = tabsOf(shown))
                tabs->setActive(*tabs->indexOf(shown));
        };
    } else if (h && done.window && done.window != r) {
        apply = [h, window = done.window] { h->raiseWindow(*window); };    // torn off: it has the focus now
    } else if (h && !h->placesWindows() && done.outOfWindow && !(floating_ && panelCount() == 1)) {
        // let go out of its window: a window of its own, where the system puts it (the only panel of a floating window
        // has one already)
        const auto [position, size] = floatPlace(*done.source, point);
        apply = [this, ids = done.ids, shown = done.shown, position, size] { floatPanels(ids, shown, position, size, true); };
    } else {
        return;
    }
    if (r)
        r->defer(std::move(apply));     // the tab that was dragged goes: not while it handles the event
    else
        apply();
}

void DockArea::paintOver(PaintContext& context) {
    const DockStyle& s = context.ui.theme.dock;
    paint::Painter& p = context.painter;
    if (indicator_) {
        const Rect r = indicator_->inflated(-4);
        p.fillRoundedRect(r, 6, s.dropZone);
        p.strokeRoundedRect(r, 6, 1.5f, s.activeLine);
    }
    if (!guides_)
        return;
    // the cross: each guide a small picture of where the tab would go, the one under the mouse lit
    const Widget& node = *guides_->node;
    const Point offset = node.mapToRoot({}) - mapToRoot({});
    const bool rtl = node.rightToLeft();
    const Palette& palette = context.ui.theme.palette;
    for (DockSide side : {DockSide::Center, DockSide::Start, DockSide::End, DockSide::Top, DockSide::Bottom}) {
        if (side == DockSide::Center && !guides_->center)
            continue;
        const Rect local = guideOf(node, side);
        const Rect g{local.x + offset.x, local.y + offset.y, local.width, local.height};
        const bool lit = guides_->hovered == side;
        p.fillRoundedRect(g, 4, lit ? s.activeLine : palette.surface);
        p.strokeRoundedRect(g, 4, 1, s.activeLine);
        const Color ink = lit ? palette.window : s.activeLine;
        Rect picture = g.inflated(-g.width * 0.22f);
        p.strokeRoundedRect(picture, 1, 1, ink);
        if (side == DockSide::Start || side == DockSide::End) {
            picture.width /= 2;
            if ((side == DockSide::End) != rtl)
                picture.x += picture.width;
        } else if (side == DockSide::Top || side == DockSide::Bottom) {
            picture.height /= 2;
            if (side == DockSide::Bottom)
                picture.y += picture.height;
        }
        p.fillRect(picture, ink);
    }
}

// ---- layouts ----

std::string DockArea::saveLayout() const {
    std::string out;
    if (const Widget* n = node())
        writeNode(out, *n, center_);
    else
        out = "{\"tabs\":[]}";
    return out;
}

std::vector<std::string> DockArea::panelsIn(std::string_view layout) {
    std::vector<std::string> ids;
    std::string message;
    if (const std::optional<json::Value> saved = json::parse(layout, &message); saved && validNode(*saved, message))
        collectPanels(*saved, ids);
    return ids;
}

bool DockArea::restoreFloating(std::string_view layout, Point position, Size size) {
    DockingHost* h = host();
    if (!h || !root())
        return false;
    // the panels it names, out of where they are, into a floating area of the group
    std::unique_ptr<DockArea> area = makeSibling(nullptr);
    area->floating_ = true;
    for (const std::string& id : panelsIn(layout)) {
        DockTabs* tabs = tabsOf(id);
        if (!tabs || tabs->area() == area.get())
            continue;
        DockArea* from = tabs->area();
        DockPanel panel = tabs->take(*tabs->indexOf(id));
        prune(*tabs);
        area->addPanel(std::move(panel));
        from->closeIfEmpty();
    }
    DockTabs* first = area->firstTabs();
    if (!first)
        return false;
    area->restoreLayout(layout);
    LocalizedText title = area->firstTabs()->panelTitle(0);
    DockArea* home = homeFor();
    h->openWindow({.title = std::move(title),
                   .content = std::move(area),
                   .titleCenter = nullptr,
                   .position = position,
                   .size = size,
                   .owner = home ? home->root() : root(),
                   .titleBar = false});
    if (home)
        home->showFloating(home->visibleInTree());      // hidden with its major tab, when that is not shown
    return true;
}

bool DockArea::restoreLayout(std::string_view layout, std::string* error) {
    std::string message;
    const std::optional<json::Value> saved = json::parse(layout, &message);
    if (!saved || !validNode(*saved, message)) {
        if (error)
            *error = saved ? message : "not JSON: " + message;
        return false;
    }
    // every panel out, the tree made again from the layout, those it does not name in the first stack
    std::map<std::string, DockPanel> panels;
    if (Widget* n = node()) {
        std::vector<DockTabs*> stacks;
        forEachTabs(*n, [&](DockTabs& tabs) { stacks.push_back(&tabs); });
        for (DockTabs* tabs : stacks) {
            while (tabs->count() > 0) {
                DockPanel panel = tabs->take(0);
                std::string id = panel.id;
                panels.emplace(std::move(id), std::move(panel));
            }
        }
    }
    // the center out too, kept for where the layout puts it
    std::unique_ptr<Widget> center = center_ ? center_->parent()->removeChild(*center_) : nullptr;
    if (Widget* n = node())
        removeChild(*n);
    std::unique_ptr<Widget> built = buildNode(*saved, panels, center, !floating_);     // a floating window's bars are its title
    if (center) {
        // a layout without the center: the center first, the layout beside it
        center->setStretch(3);
        if (built) {
            auto split = std::make_unique<DockSplit>(Orientation::Horizontal);
            built->setStretch(1);
            split->addChild(std::move(center));
            split->addChild(std::move(built));
            built = std::move(split);
        } else {
            built = std::move(center);
        }
    }
    if (built)
        addChild(std::move(built), 0);
    // those it does not name join the first stack, which still shows the tab the layout says
    DockTabs* first = firstTabs();
    const std::size_t shown = first ? first->active() : 0;
    for (auto& [id, panel] : panels)
        addPanel(std::move(panel));
    if (first && first == firstTabs())
        first->setActive(shown);
    return true;
}

} // namespace glw
