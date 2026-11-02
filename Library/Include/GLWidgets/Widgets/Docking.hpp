// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Docked panels: the tools of a document (a node palette, an inspector, an output log), each in a tab of a stack, the
// stacks split side by side or one above the other. The user rearranges them with the mouse: a tab dragged onto a stack's
// tabs joins them, dragged to an edge of a stack makes a new stack there; a stack left without tabs goes, and a split
// left with one pane gives way to it. Each split's handle shares the room between its panes.
//
// The widgets are the layout: a DockArea holds one node, a DockTabs (a stack) or a DockSplit (a Splitter of nodes). The
// tree is saved and restored as JSON (saveLayout / restoreLayout), panels known by their ids.
//
// Across windows (with a DockingHost: Application): a tab let go where no stack takes it, out of its area, floats: its panel
// goes into a window of its own, a tool window above the area's, with a DockArea of the same group. Panels move between the
// areas of a group as within one; a floating window goes when its last panel leaves, and its panels go back to the area
// they came from when it is closed. Dragging the only tab of a floating window drags the window. The floating windows show
// while their area does (they hide with its major tab), and close with it.
//
// Start and End are the sides of the layout direction (Start is the right in right to left).
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Splitter.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glw {

class DockArea;
class DockingHost;
struct DockGroup;

enum class DockSide : std::uint8_t {
    Center,                 // in the stack, as a tab
    Start,                  // a new stack beside it, on the start side
    End,
    Top,
    Bottom,
};

// A panel to dock: a tool shown in a tab.
struct DockPanel {
    std::string id;                         // unique in its dock area: what saved layouts know it by
    LocalizedText title;
    std::unique_ptr<Widget> content;
    bool closable = true;                   // a × on its tab
};

// A stack of panels: their tabs, and the content of the one shown under them.
class GLW_EXPORT DockTabs : public Widget {
public:
    DockTabs();

    std::size_t count() const { return entries_.size(); }
    const std::string& panelId(std::size_t index) const { return entries_[index].id; }
    const LocalizedText& panelTitle(std::size_t index) const { return entries_[index].title; }
    Widget* panelContent(std::size_t index) const { return entries_[index].content; }
    std::optional<std::size_t> indexOf(std::string_view id) const;
    std::size_t active() const { return active_; }
    void setActive(std::size_t index);
    DockArea* area() const;
    const DockStyle& style() const;

    // Adds `panel` (at `index`, at the end by default) and shows it.
    void add(DockPanel panel, std::size_t index = std::numeric_limits<std::size_t>::max());
    // Takes the panel at `index` out: the caller has it now.
    DockPanel take(std::size_t index);

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;

private:
    struct Entry {
        std::string id;
        LocalizedText title;
        Widget* content = nullptr;          // a child, shown when it is the active one
        bool closable = true;
    };
    void rebuildBar();

    std::vector<Entry> entries_;
    std::size_t active_ = 0;
    Widget* bar_ = nullptr;
};

// Nodes side by side (horizontal) or one above the other, with handles between them.
class GLW_EXPORT DockSplit : public Splitter {
public:
    explicit DockSplit(Orientation orientation) : Splitter(Props{orientation, std::nullopt}) {}
};

class GLW_EXPORT DockArea : public Widget {
public:
    DockArea();
    ~DockArea() override;

    // Adds `panel`. The first one fills the area. Then: in the stack of `nextTo` (Center), or in a new stack beside it
    // taking `share` of its room; with no `nextTo`, in the first stack (Center) or beside everything there is.
    DockTabs& addPanel(DockPanel panel, DockTabs* nextTo = nullptr, DockSide side = DockSide::Center, float share = 0.5f);
    // Takes the panel `id` out: its content goes back to the caller (none when there is no such panel).
    std::unique_ptr<Widget> removePanel(std::string_view id);
    // These, and removePanel, know the panels of the floating windows of the area too.
    DockTabs* tabsOf(std::string_view id) const;
    std::vector<std::string> panelIds() const;
    // An area of a floating window (of another area: home()).
    bool floating() const { return floating_; }
    DockArea* home() const;
    // The areas of its floating windows (of its home's, for one of them).
    std::vector<DockArea*> floatingAreas() const;
    // A panel's × was pressed: `handler` gets it (out of the area); without one it is destroyed.
    void setOnPanelClosed(std::function<void(DockPanel)> handler) { onPanelClosed_ = std::move(handler); }

    // The arrangement as JSON: nodes, their shares, the panels of each stack by id, the tab shown. Not the floating windows'.
    std::string saveLayout() const;
    // Arranges the panels it has as `layout` (from saveLayout) says. Panels it does not name go in the first stack; names
    // of panels it does not have are passed over. False, and `error`, when `layout` is not a layout (nothing changes).
    bool restoreLayout(std::string_view layout, std::string* error = nullptr);

    // ---- dragging a tab (DockTabs' tabs do it) ----

    void beginDrag(DockTabs& source, std::size_t index);
    // The mouse at `point` (the root's coordinates): where the tab would go, shown (by the area it would go to).
    void dragTo(Point point);
    // Let go at `point`: the panel moves there (after the event) when `drop` and it can.
    void endDrag(Point point, bool drop);
    bool dragging() const { return drag_.has_value(); }
    // A tab's × pressed: the panel goes (after the event).
    void closePanel(std::string_view id);

protected:
    void paintOver(PaintContext& context) override;
    void onRootChanged() override;
    void onVisibleChanged(bool visible) override;
    void onWindowClosing() override;

private:
    struct Drag {
        DockTabs* source = nullptr;
        std::string id;
        DockTabs* target = nullptr;
        DockSide side = DockSide::Center;
        DockArea* targetArea = nullptr;     // the one showing where it would go
        bool outside = false;               // the mouse is out of this area
        bool movingWindow = false;          // the only tab of a floating window: the window follows the mouse
        Point grab;                         // the mouse on the screen less the window's position, while it moves
    };

    Widget* node() const;                   // its one child: the root of the tree
    DockTabs* firstTabs() const;
    DockingHost* host() const;
    // The stack of the group under `point` of `root`.
    DockTabs* tabsAt(const UiRoot& root, Point point) const;
    // A new stack with `panel`, beside `target` (a node), on `side`, taking `share` of its room.
    static DockTabs& insertBeside(DockPanel panel, Widget& target, DockSide side, float share);
    // `tabs` without panels goes; a split left with one pane gives way to it.
    static void prune(DockTabs& tabs);
    void move(const std::string& id, DockTabs& target, DockSide side);
    // The panel `id` into a floating window at `position` (the screen) of `size` (dips).
    void floatPanel(const std::string& id, Point position, Size size);
    // A floating area without panels: its window closes.
    void closeIfEmpty();
    // Its floating windows above its window, shown when it is.
    void showFloating(bool visible);

    std::shared_ptr<DockGroup> group_;
    bool floating_ = false;
    std::optional<Drag> drag_;
    std::optional<Rect> indicator_;         // where a tab dragged (from this area or another) would go
    std::function<void(DockPanel)> onPanelClosed_;
};

} // namespace glw
