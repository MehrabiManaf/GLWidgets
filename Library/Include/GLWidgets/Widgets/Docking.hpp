// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Docked panels: the tools of a document (a node palette, an inspector, an output log), each in a tab of a stack, the
// stacks split side by side or one above the other. The user rearranges them with the mouse: while a tab is dragged, the
// stack under the mouse shows its guides, a cross of five in its middle. Let go on the middle one or on the stack's tabs,
// the tab joins them; on one of the four around it, it makes a new stack on that side. Where it would go shows. A stack
// left without tabs goes, and a split left with one pane gives way to it. Each split's handle shares the room between its
// panes.
//
// The widgets are the layout: a DockArea holds one node, a DockTabs (a stack) or a DockSplit (a Splitter of nodes). The
// tree is saved and restored as JSON (saveLayout / restoreLayout), panels known by their ids.
//
// Across windows (with a DockingHost: Application), as in Visual Studio and Unreal Engine: a tab dragged out of its stack's
// tab bar floats at once. Its panel goes into a window of its own, a tool window above the area's with a DockArea of the
// same group, and that window follows the mouse; let go on a guide, it docks there, elsewhere it stays where it is. A
// floating window has no title bar of its own: the tab bars along its top are, with the window buttons at their end.
// Dragged by them, the window moves the same way and docks all its panels where it is let go on a guide; dragged by its
// only tab, too. Panels move between the areas of a group as within one; a floating window goes when its last panel
// leaves, and its panels go back to the area they came from when it is closed. The floating windows show while their area
// does (they hide with its major tab), and close with it.
//
// Every panel says who owns it (DockPanel::scope), and goes only into areas of that scope:
// - MajorTab: a panel of one major tab (a script's palette, its inspector), in the area that major tab shows. It hides with
//   its major tab.
// - Global: a panel of the application (an output log, the files). Each window has an area of its own around its major
//   tabs (setCenter): global panels dock there, in any window, and stay whichever major tab is shown. When a window closes,
//   its global panels go to another one.
//
// A stack's tabs are each as wide as their title (cut with … past the style's maximum); when they do not fit they scroll,
// by the wheel and by a thin bar along the bottom of the tab bar, the tab shown kept in view. At the end of the tab bar are
// the buttons of the panel shown (DockPanel::buttons) and the stack's float button: it floats the panel shown, or, in a
// floating window, docks it back in its area.
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
#include <utility>
#include <vector>

namespace glw {

class DockArea;
class DockingHost;
struct DockGroup;

// Who a panel belongs to, and so which areas take it (see above).
enum class DockScope : std::uint8_t { MajorTab, Global };

enum class DockSide : std::uint8_t {
    Center,                 // in the stack, as a tab
    Start,                  // a new stack beside it, on the start side
    End,
    Top,
    Bottom,
};

// A button at the end of a stack's tab bar, for the panel shown.
struct DockButton {
    std::function<void(PaintContext&, const Rect&, Color)> icon;   // draws it in the rectangle, in the color
    std::function<void()> onClick;          // after the event
};

// A panel to dock: a tool shown in a tab.
struct DockPanel {
    std::string id;                         // unique in its dock area: what saved layouts know it by
    LocalizedText title;
    std::unique_ptr<Widget> content;
    bool closable = true;                   // a × on its tab
    std::vector<DockButton> buttons;        // on the tab bar while it is shown, before the float button
    DockScope scope = DockScope::MajorTab;  // the areas it goes into: of its scope only
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

    // Where its guide for `side` is (its coordinates), shown while a tab is dragged over it: the middle of the cross for
    // Center, the one on that side of it for the others (Start and End mirrored with the direction).
    Rect guide(DockSide side) const;
    // Takes the panel at `index` out: the caller has it now.
    DockPanel take(std::size_t index);

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override;

private:
    friend class DockArea;
    struct Entry {
        std::string id;
        LocalizedText title;
        Widget* content = nullptr;          // a child, shown when it is the active one
        bool closable = true;
        std::vector<DockButton> buttons;
        DockScope scope = DockScope::MajorTab;
    };
    void rebuildBar();
    void rebuildTools();                    // the buttons of the panel shown, and the float button
    // In a floating window: its bar is the window's title where it has no tab (`title`), and leaves `reserve` at its
    // `left` or right for the window buttons.
    void setTitleArea(bool title, float reserve, bool left);

    std::vector<Entry> entries_;
    std::size_t active_ = 0;
    Widget* bar_ = nullptr;                 // the tabs, scrolled when they do not fit
    Widget* tools_ = nullptr;               // the buttons after them
    float reserve_ = 0;
    bool reserveLeft_ = false;
    bool title_ = false;
};

// Nodes side by side (horizontal) or one above the other, with handles between them.
class GLW_EXPORT DockSplit : public Splitter {
public:
    explicit DockSplit(Orientation orientation) : Splitter(Props{orientation, std::nullopt}) {}
};

class GLW_EXPORT DockArea : public Widget {
public:
    explicit DockArea(DockScope scope = DockScope::MajorTab);
    ~DockArea() override;

    // The panels it takes: of this scope only.
    DockScope scope() const { return scope_; }
    // What its panels dock around (before it has any): a window's major tabs, for a Global area. It is not a panel: never
    // moved nor closed, with no panel it fills the area, and a tab dragged over it docks on one of its sides.
    void setCenter(std::unique_ptr<Widget> center);
    Widget* center() const { return center_; }
    // An area of the same group around `center`, for another window: panels move between them as between an area and its
    // floating windows (a window opened by a major tab torn off gets one, around its major tabs).
    std::unique_ptr<DockArea> makeSibling(std::unique_ptr<Widget> center);

    // Adds `panel` (of its scope). The first one fills the area (beside its center, on the End side, taking `share`, when
    // it has one). Then: in the stack of `nextTo` (Center), or in a new stack beside it taking `share` of its room; with no
    // `nextTo`, in the first stack (Center) or beside everything there is.
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

    // Where the guide for `side` is on `node` (a stack, or an area's center), in its coordinates.
    static Rect guide(const Widget& node, DockSide side);

    // The area has the mouse from now until it is let go (the tab may go before).
    void beginDrag(DockTabs& source, std::size_t index);
    // The mouse at `point` (the root's coordinates): out of its stack's tab bar, the panel floats; a window dragged follows
    // the mouse; where it would go is shown (by the area it would go to).
    void dragTo(Point point);
    // Let go at `point`: the panel (the panels of the window dragged) moves there (after the event) when `drop` and it can.
    void endDrag(Point point, bool drop);
    bool dragging() const { return drag_.has_value(); }
    // A tab's × pressed: the panel goes (after the event).
    void closePanel(std::string_view id);
    // The float button: the panel `id` floats in a window by its stack's place; in a floating window, it goes back to the
    // area the window belongs to (after the event).
    void toggleFloating(std::string_view id);

protected:
    void arrangeContent(const Rect& content) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onRootChanged() override;
    void onStyleChanged() override;
    void onVisibleChanged(bool visible) override;
    void onWindowClosing() override;

private:
    struct Drag {
        DockTabs* source = nullptr;         // the stack of the (first) panel dragged
        std::vector<std::string> ids;       // a tab's panel, or every panel of a floating window dragged by its title
        Widget* target = nullptr;           // a stack, or a center (on one of its sides)
        DockSide side = DockSide::Center;
        DockArea* shownArea = nullptr;      // the one showing guides (and where it would go)
        const UiRoot* window = nullptr;     // the window following the mouse: a floating one, or the one torn off
        Point grab;                         // the mouse on the screen less that window's position
        bool tearing = false;               // the panel goes into a window of its own, after the event
    };

    Widget* node() const;                   // its first child: the root of the tree
    DockTabs* firstTabs() const;
    DockingHost* host() const;
    // What of the group is under `point` of `root`: a stack, or an area's center; and that area.
    std::pair<Widget*, DockArea*> nodeAt(const UiRoot& root, Point point) const;
    bool inGroup(const DockArea* area) const;
    // Where the group's floating windows go above, and its panels go back to: an area with a window that stays open.
    DockArea* homeFor() const;
    // Where the panel of `source` would float, let go at `point` (the root's): its window's position and size.
    std::pair<Point, Size> floatPlace(const DockTabs& source, Point point) const;
    // Neither guides nor where a tab would go, shown.
    void clearDrop();
    // A new stack with `panel`, beside `target` (a node), on `side`, taking `share` of its room.
    static DockTabs& insertBeside(DockPanel panel, Widget& target, DockSide side, float share);
    // `tabs` without panels goes; a split left with one pane gives way to it.
    static void prune(DockTabs& tabs);
    void move(const std::string& id, Widget& target, DockSide side);
    // All of them: the first as `move`, the others in its stack.
    void move(const std::vector<std::string>& ids, Widget& target, DockSide side);
    // The panel `id` into a floating window at `position` (the screen) of `size` (dips); its area.
    DockArea* floatPanel(const std::string& id, Point position, Size size, bool focus = true);
    // The panel dragged into a window of its own, which the drag goes on with (after the event).
    void tearOff(Point point);
    // A floating area: whether `point` (its coordinates) is on its title, a top tab bar where it has no tab.
    bool onTitle(Point point) const;
    // A floating area dragged by its title: all its panels, its window following the mouse.
    void beginTitleDrag();
    // A floating area without panels: its window closes.
    void closeIfEmpty();
    // Its floating windows above its window, shown when it is.
    void showFloating(bool visible);
    // A floating area: its window buttons, in the order of the system.
    void makeButtons();

    std::shared_ptr<DockGroup> group_;
    DockScope scope_;
    Widget* center_ = nullptr;
    bool floating_ = false;
    std::optional<Drag> drag_;
    std::optional<Point> titlePress_;       // the root's: its title pressed, not dragged yet
    std::optional<Rect> indicator_;         // where a tab dragged (from this area or another) would go
    struct Guides {
        Widget* node = nullptr;             // the stack (or the center) under the mouse
        std::optional<DockSide> hovered;
        bool center = true;                 // a stack the tab is not dragged from: the middle guide too
    };
    std::optional<Guides> guides_;
    Widget* buttons_ = nullptr;             // a floating area's window buttons, over its node
    std::function<void(DockPanel)> onPanelClosed_;
};

} // namespace glw
