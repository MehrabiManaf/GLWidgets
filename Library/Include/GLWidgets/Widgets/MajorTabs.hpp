// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The documents of a window (scripts, levels, chart sheets), as the tabs of a browser: MajorTabs shows the content of the
// one chosen; its MajorTabStrip, in the title bar, shows their tabs. A tab is chosen by a click, closed by its ×, moved
// along the strip by dragging it; the strip's + asks for a new one. Where the strip has no tab, it drags the window.
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

    void setOnActiveChanged(std::function<void(std::size_t)> handler) { onActiveChanged_ = std::move(handler); }
    // The strip's + was pressed: without a handler, the strip has no +.
    void setOnNewTab(std::function<void()> handler);
    // A tab's × was pressed: without a handler, the tab is removed (and its content destroyed).
    void setOnCloseRequested(std::function<void(std::size_t)> handler) { onCloseRequested_ = std::move(handler); }

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
    std::function<void(std::size_t)> onActiveChanged_;
    std::function<void()> onNewTab_;
    std::function<void(std::size_t)> onCloseRequested_;
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

    MajorTabs* tabs_;
    Widget* row_ = nullptr;
    std::optional<std::size_t> dragging_;
    std::optional<float> insertionX_;
};

} // namespace glw
