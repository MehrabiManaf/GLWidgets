// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The documents of a window (scripts, levels, chart sheets), as the tabs of a browser: MajorTabs shows the content of the
// one chosen; its MajorTabStrip, in the title bar, shows their tabs. A tab is chosen by a click, closed by its ×, moved
// along the strip by dragging it; the strip's + asks for a new one. Where the strip has no tab, it drags the window.
//
// Across windows (with a DockingHost: Application): a tab dragged out of its strip goes into a new window of its own;
// dropped on another window's strip, it goes there. Dragging the only tab of a window drags the window, and dropping it on
// another window's strip merges them (it is seen through while it moves, the strip under it showing where its tab would
// go). Where a tab would open a window, an outline shows it. A window left without tabs closes. The new windows' MajorTabs have the same
// handlers, which are given the MajorTabs they are for. When the MajorTabs is the center of a window's dock area (of its
// global panels, Docking.hpp), a new window gets one of the same group around its MajorTabs.
//
//   auto tabs = std::make_unique<glw::MajorTabs>();
//   tabs->addTab(glw::tr("Script"), std::move(dockArea));
//   glw::MajorTabs& documents = *tabs;
//   glw::Window& window = app.openWindow({.title = ...}, std::move(tabs));
//   window.frame()->titleBar().setCenter(documents.makeStrip());
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

class MajorTabStrip;

class GLW_EXPORT MajorTabs : public Widget {
public:
    MajorTabs();
    ~MajorTabs() override;

    // Adds a tab showing `content`, at the end, and shows it. Its index.
    std::size_t addTab(LocalizedText title, std::unique_ptr<Widget> content, bool closable = true);
    // The same at `index`.
    void insertTab(std::size_t index, LocalizedText title, std::unique_ptr<Widget> content, bool closable = true);
    // Takes the tab at `index` out: its content goes back to the caller.
    std::unique_ptr<Widget> removeTab(std::size_t index);
    // The tab at `from` goes to `to` (its index after the move).
    void moveTab(std::size_t from, std::size_t to);
    std::size_t count() const { return tabs_.size(); }
    std::size_t active() const { return active_; }
    void setActive(std::size_t index);
    const LocalizedText& title(std::size_t index) const { return tabs_[index].title; }
    void setTitle(std::size_t index, LocalizedText title);
    Widget* content(std::size_t index) const { return tabs_[index].content; }
    bool closable(std::size_t index) const { return tabs_[index].closable; }
    const MajorTabStyle& style() const;

    void setOnActiveChanged(std::function<void(MajorTabs&, std::size_t)> handler) { onActiveChanged_ = std::move(handler); }
    // The strip's + was pressed: without a handler, the strip has no +.
    void setOnNewTab(std::function<void(MajorTabs&)> handler);
    // A tab's × was pressed: without a handler, the tab is removed (and its content destroyed), and a window left without
    // tabs closes.
    void setOnCloseRequested(std::function<void(MajorTabs&, std::size_t)> handler) { onCloseRequested_ = std::move(handler); }

    // The strip that shows the tabs, for the title bar (TitleBar::setCenter). One at a time.
    std::unique_ptr<MajorTabStrip> makeStrip();

private:
    friend class MajorTabStrip;
    struct Tab {
        LocalizedText title;
        Widget* content = nullptr;
        bool closable = true;
    };
    void changed();                     // the tabs: the strip shows them again
    void requestClose(std::size_t index);
    void requestNew();

    std::vector<Tab> tabs_;
    std::size_t active_ = 0;
    MajorTabStrip* strip_ = nullptr;
    std::function<void(MajorTabs&, std::size_t)> onActiveChanged_;
    std::function<void(MajorTabs&)> onNewTab_;
    std::function<void(MajorTabs&, std::size_t)> onCloseRequested_;
};

class GLW_EXPORT MajorTabStrip : public Widget {
public:
    explicit MajorTabStrip(MajorTabs& tabs);
    ~MajorTabStrip() override;

    MajorTabs* tabs() const { return tabs_; }

    // ---- what its tabs call ----
    void beginDrag(std::size_t index);
    void dragTo(Point point);           // the root's coordinates
    void endDrag(Point point);
    void close(std::size_t index);      // a tab's ×: after the event
    void newTab();                      // the +: after the event

protected:
    void paintOver(PaintContext& context) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    friend class MajorTabs;
    void rebuild();
    void refreshActive();               // the tabs told which one is shown, in place (one may be in its own handler)
    // Where a tab dropped at `point` would go: an index from 0 to the number of tabs, and its x on the strip.
    std::pair<std::size_t, float> slotAt(Point point) const;
    // The strip a dragged tab would go to (this one, another window's, or none), and where in it.
    void setTarget(MajorTabStrip* target, Point point);
    void showInsertion(std::optional<float> x);
    // Where a tab let go at `point` (the root's) would open a window of its own: its position and size.
    std::pair<Point, Size> tearPlace(Point point) const;

    MajorTabs* tabs_;
    Widget* row_ = nullptr;
    std::optional<std::size_t> dragging_;
    bool movingWindow_ = false;         // the only tab dragged: the window follows the mouse
    Point grab_;                        // the mouse on the screen less the window's position, while it moves
    MajorTabStrip* target_ = nullptr;
    Point targetPoint_;                 // in the target's root
    std::optional<float> insertionX_;   // shown on this strip, by the strip a tab is dragged from
};

} // namespace glw
