// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Docking.hpp>

#include "Widgets/Json.hpp"

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
    DockArea* home = nullptr;               // none once it is destroyed
    std::vector<DockArea*> areas;           // the home first
    DockingHost* host = nullptr;            // the last one the areas were shown with
};

namespace {

constexpr float dragThreshold = 6;      // dips the mouse moves with a tab before it is dragged
constexpr float edgeZone = 0.28f;       // of a stack's width or height along each edge: dropped there, a new stack

// A tab of a stack: its title, a × when the panel can be closed. Pressed, it shows its panel; dragged, it moves it.
class DockTab final : public Widget {
public:
    DockTab(DockTabs& owner, std::size_t index, const LocalizedText& title, bool closable, bool active)
        : owner_(owner), index_(index), closable_(closable), active_(active) {
        setPadding({12, 0, closable ? 32.0f : 12.0f, 0});
        label_ = &add(std::make_unique<Label>(title));
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
        p.fillRect(r, active_ ? s.activeTab : s.tab.resolve(state));
        if (active_)
            p.fillRect({0, 0, r.width, 2}, s.activeLine);
        label_->setColor(active_ ? s.text : s.mutedText);
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
            if (!area->dragging() && std::hypot(moved.x, moved.y) > dragThreshold)
                area->beginDrag(owner_, index_);
            if (area->dragging())
                area->dragTo(at);
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
        return rightToLeft() ? Rect{8, y, 16, 16} : Rect{size().width - 24, y, 16, 16};
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

// ---- layouts as JSON ----

void appendString(std::string& out, std::string_view s) {
    out += '"';
    for (const char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(c));
            out += escaped;
        } else {
            out += c;
        }
    }
    out += '"';
}

void appendNumber(std::string& out, float value) {
    char text[32];
    std::snprintf(text, sizeof text, "%g", static_cast<double>(value));
    out += text;
}

void writeNode(std::string& out, const Widget& node) {
    out += "{\"share\":";
    appendNumber(out, node.stretch() > 0 ? node.stretch() : 1.0f);
    if (const auto* split = dynamic_cast<const DockSplit*>(&node)) {
        out += split->orientation() == Orientation::Horizontal ? ",\"split\":\"horizontal\",\"panes\":[" : ",\"split\":\"vertical\",\"panes\":[";
        bool first = true;
        for (const auto& child : split->children()) {
            if (!first)
                out += ',';
            writeNode(out, *child);
            first = false;
        }
        out += "]}";
    } else if (const auto* tabs = dynamic_cast<const DockTabs*>(&node)) {
        out += ",\"tabs\":[";
        for (std::size_t i = 0; i < tabs->count(); ++i) {
            if (i > 0)
                out += ',';
            appendString(out, tabs->panelId(i));
        }
        out += "],\"active\":";
        appendNumber(out, static_cast<float>(tabs->active()));
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

// The node `saved` describes, with the panels it names taken from `panels`; none when it would be empty.
std::unique_ptr<Widget> buildNode(const json::Value& saved, std::map<std::string, DockPanel>& panels) {
    const float share = std::max(0.01f, numberOf(saved.member("share"), 1));
    if (const json::Value* split = saved.member("split")) {
        auto node = std::make_unique<DockSplit>(*split->string() == "horizontal" ? Orientation::Horizontal : Orientation::Vertical);
        for (const json::Value& pane : *saved.member("panes")->array()) {
            if (auto child = buildNode(pane, panels))
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
    tabs->setStretch(share);
    return tabs;
}

template <class F>
void forEachTabs(Widget& node, F&& f) {
    if (auto* tabs = dynamic_cast<DockTabs*>(&node)) {
        f(*tabs);
        return;
    }
    for (const auto& child : node.children())
        forEachTabs(*child, f);
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
    auto bar = std::make_unique<Widget>();
    bar->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal));
    bar_ = &addChild(std::move(bar));
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
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(index), Entry{std::move(panel.id), std::move(panel.title), &shown, panel.closable});
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
    return DockPanel{std::move(entry.id), std::move(entry.title), std::move(content), entry.closable};
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
    invalidateLayout();
}

void DockTabs::rebuildBar() {
    while (!bar_->children().empty())
        bar_->removeChild(*bar_->children().back());
    for (std::size_t i = 0; i < entries_.size(); ++i)
        bar_->add(std::make_unique<DockTab>(*this, i, entries_[i].title, entries_[i].closable, i == active_));
}

Size DockTabs::measureContent(Size available) {
    const float bar = style().tabHeight;
    bar_->measure({available.width, bar});
    Size content;
    for (const Entry& e : entries_) {
        if (e.content->visible())
            content = e.content->measure({available.width, std::max(0.0f, available.height - bar)});
    }
    return {content.width, bar + content.height};
}

void DockTabs::arrangeContent(const Rect& content) {
    const float bar = style().tabHeight;
    const bool rtl = rightToLeft();
    const float reserve = std::min(reserve_, content.width);
    arrangeInCell(*bar_, {reserveLeft_ ? content.x + reserve : content.x, content.y, content.width - reserve, bar}, rtl);
    const Rect below{content.x, content.y + bar, content.width, std::max(0.0f, content.height - bar)};
    for (const Entry& e : entries_) {
        if (e.content->visible())
            arrangeInCell(*e.content, below, rtl);
    }
}

void DockTabs::setTitleArea(bool caption, float reserve, bool left) {
    bar_->setWindowArea(caption ? platform::HitArea::Caption : platform::HitArea::Client);
    if (reserve == reserve_ && left == reserveLeft_)
        return;
    reserve_ = reserve;
    reserveLeft_ = left;
    arrange(bounds());      // in its area's arrangement: the bar again, nothing measured again
}

void DockTabs::paint(PaintContext& context) {
    const DockStyle& s = style();
    const float bar = s.tabHeight;
    context.painter.fillRect({0, 0, size().width, bar}, s.tabBar);
    context.painter.fillRect({0, bar, size().width, std::max(0.0f, size().height - bar)}, s.background);
}

// ---- the area ----

DockArea::DockArea() : group_(std::make_shared<DockGroup>()) {
    group_->home = this;
    group_->areas.push_back(this);
}

DockArea::~DockArea() {
    std::erase(group_->areas, this);
    if (group_->home != this)
        return;
    // the floating windows go with it, their panels too
    group_->home = nullptr;
    if (group_->host) {
        for (DockArea* area : group_->areas) {
            if (const UiRoot* r = area->root())
                group_->host->closeWindow(*r);
        }
    }
}

DockingHost* DockArea::host() const {
    const UiContext* ui = context();
    return ui ? ui->docking : nullptr;
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

DockTabs& DockArea::addPanel(DockPanel panel, DockTabs* nextTo, DockSide side, float share) {
    if (!node()) {
        auto& tabs = static_cast<DockTabs&>(addChild(std::make_unique<DockTabs>(), 0));     // under the window buttons
        tabs.add(std::move(panel));
        return tabs;
    }
    if (side == DockSide::Center) {
        DockTabs& tabs = nextTo ? *nextTo : *firstTabs();
        tabs.add(std::move(panel));
        return tabs;
    }
    return insertBeside(std::move(panel), nextTo ? static_cast<Widget&>(*nextTo) : *node(), side, share);
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

void DockArea::move(const std::string& id, DockTabs& target, DockSide side) {
    DockTabs* source = tabsOf(id);
    if (!source || (source == &target && side == DockSide::Center))
        return;
    DockArea* from = source->area();
    DockPanel panel = source->take(*source->indexOf(id));
    if (side == DockSide::Center)
        target.add(std::move(panel));
    else
        insertBeside(std::move(panel), target, side, 0.5f);
    prune(*source);         // after: a split it leaves may give way to the target
    from->closeIfEmpty();
    if (DockingHost* h = group_->host; h && target.root() && target.root() != root())
        h->raiseWindow(*target.root());
}

void DockArea::floatPanel(const std::string& id, Point position, Size size) {
    DockingHost* h = host();
    DockTabs* source = tabsOf(id);
    if (!h || !source)
        return;
    DockArea* from = source->area();
    DockPanel panel = source->take(*source->indexOf(id));
    prune(*source);
    auto area = std::make_unique<DockArea>();
    area->group_ = group_;              // its own group goes with it
    area->floating_ = true;
    group_->areas.push_back(area.get());
    LocalizedText title = panel.title;
    area->addPanel(std::move(panel));
    const UiRoot* owner = group_->home ? group_->home->root() : root();
    h->openWindow({.title = std::move(title),
                   .content = std::move(area),
                   .titleCenter = nullptr,
                   .position = position,
                   .size = size,
                   .owner = owner,
                   .titleBar = false});
    from->closeIfEmpty();
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
    if (!h || !r || floating_)
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
    // a floating window closed by the user: its panels go back to the area they came from
    DockArea* to = group_->home;
    if (!floating_ || !to)
        return;
    while (DockTabs* tabs = firstTabs()) {
        while (tabs->count() > 0)
            to->addPanel(tabs->take(0));
        prune(*tabs);
    }
}

// ---- dragging ----

DockTabs* DockArea::tabsAt(const UiRoot& root, Point point) const {
    for (Widget* w = root.widgetAt(point); w; w = w->parent()) {
        if (auto* tabs = dynamic_cast<DockTabs*>(w)) {
            DockArea* area = tabs->area();
            return std::find(group_->areas.begin(), group_->areas.end(), area) != group_->areas.end() ? tabs : nullptr;
        }
    }
    return nullptr;
}

void DockArea::beginDrag(DockTabs& source, std::size_t index) {
    drag_ = Drag{&source, source.panelId(index)};
    const UiRoot* r = root();
    if (DockingHost* h = host(); h && r) {
        h->captureMouse(true);
        // the only panel of a floating window: the window goes with it
        std::size_t own = 0;
        if (Widget* n = node())
            forEachTabs(*n, [&](DockTabs& tabs) { own += tabs.count(); });
        if (floating_ && own == 1) {
            drag_->movingWindow = true;
            drag_->grab = h->mouseOnScreen() - h->windowPosition(*r);
        }
    }
    invalidatePaint();
}

void DockArea::dragTo(Point point) {
    if (!drag_)
        return;
    drag_->target = nullptr;
    DockArea* shown = std::exchange(drag_->targetArea, nullptr);
    UiRoot* r = root();
    if (!r)
        return;
    // the window and the point the mouse is over
    DockingHost* h = host();
    const UiRoot* over = r;
    Point at = point;
    if (h && drag_->movingWindow) {
        const Point mouse = h->mouseOnScreen();
        h->moveWindow(*r, mouse - drag_->grab);
        const DockingHost::Hit hit = h->windowAt(mouse, r);
        over = hit.root;
        at = hit.point;
    } else if (h) {
        const DockingHost::Hit hit = h->windowAt(h->toScreen(*r, point));
        over = hit.root;
        at = hit.point;
    }
    const Point inArea = mapFromRoot(point);
    drag_->outside = over != r || !Rect{0, 0, size().width, size().height}.contains(inArea);
    DockTabs* tabs = over ? tabsAt(*over, at) : nullptr;
    if (!tabs) {
        if (shown) {
            shown->indicator_.reset();
            shown->invalidatePaint();
        }
        return;
    }
    point = at;

    const float bar = tabs->style().tabHeight;
    const Point local = tabs->mapFromRoot(point);
    const Rect content{0, bar, tabs->size().width, std::max(0.0f, tabs->size().height - bar)};
    // over the tabs: among them; near an edge of the content: beside the stack
    int edge = -1;      // left, right, top, bottom: on the screen
    if (local.y >= bar && !content.empty()) {
        const float fx = (local.x - content.x) / content.width, fy = (local.y - content.y) / content.height;
        const float distances[4] = {fx, 1 - fx, fy, 1 - fy};
        const int nearest = static_cast<int>(std::min_element(distances, distances + 4) - distances);
        if (distances[nearest] < edgeZone)
            edge = nearest;
    }
    const bool rtl = tabs->rightToLeft();
    const DockSide side = edge == 0   ? (rtl ? DockSide::End : DockSide::Start)
                          : edge == 1 ? (rtl ? DockSide::Start : DockSide::End)
                          : edge == 2 ? DockSide::Top
                          : edge == 3 ? DockSide::Bottom
                                      : DockSide::Center;
    // where nothing would change: no target
    if (tabs == drag_->source && (side == DockSide::Center || tabs->count() == 1)) {
        if (shown) {
            shown->indicator_.reset();
            shown->invalidatePaint();
        }
        return;
    }
    Rect zone = content;
    if (edge == 0 || edge == 1)
        zone.width /= 2;
    if (edge == 1)
        zone.x += zone.width;
    if (edge == 2 || edge == 3)
        zone.height /= 2;
    if (edge == 3)
        zone.y += zone.height;
    DockArea* area = tabs->area();
    const Point offset = tabs->mapToRoot({}) - area->mapToRoot({});
    drag_->target = tabs;
    drag_->side = side;
    drag_->targetArea = area;
    if (shown && shown != area) {
        shown->indicator_.reset();
        shown->invalidatePaint();
    }
    area->indicator_ = Rect{zone.x + offset.x, zone.y + offset.y, zone.width, zone.height};
    area->invalidatePaint();
}

void DockArea::endDrag(Point point, bool drop) {
    if (!drag_)
        return;
    dragTo(point);
    const Drag done = *drag_;
    drag_.reset();
    if (done.targetArea) {
        done.targetArea->indicator_.reset();
        done.targetArea->invalidatePaint();
    }
    UiRoot* r = root();
    DockingHost* h = host();
    if (h)
        h->captureMouse(false);
    if (!drop)
        return;
    std::function<void()> apply;
    if (done.target) {
        apply = [this, id = done.id, target = done.target, side = done.side] { move(id, *target, side); };
    } else if (h && r && done.outside && !done.movingWindow) {
        // let go out of the area, where no stack takes it: it floats where it was let go, the size of its stack
        const Point grabbed = h->toScreen(*r, {40, 16}) - h->toScreen(*r, {});
        const Size size{std::max(done.source->size().width, 240.0f), std::max(done.source->size().height, 160.0f)};
        apply = [this, id = done.id, position = h->toScreen(*r, point) - grabbed, size] { floatPanel(id, position, size); };
    } else {
        return;
    }
    if (r)
        r->defer(std::move(apply));     // the tab that was dragged goes: not while it handles the event
    else
        apply();
}

void DockArea::paintOver(PaintContext& context) {
    if (!indicator_)
        return;
    const DockStyle& s = context.ui.theme.dock;
    const Rect r = indicator_->inflated(-4);
    context.painter.fillRoundedRect(r, 6, s.dropZone);
    context.painter.strokeRoundedRect(r, 6, 1.5f, s.activeLine);
}

// ---- layouts ----

std::string DockArea::saveLayout() const {
    std::string out;
    if (const Widget* n = node())
        writeNode(out, *n);
    else
        out = "{\"tabs\":[]}";
    return out;
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
        removeChild(*n);
    }
    if (auto built = buildNode(*saved, panels))
        addChild(std::move(built), 0);
    for (auto& [id, panel] : panels)
        addPanel(std::move(panel));
    return true;
}

} // namespace glw
