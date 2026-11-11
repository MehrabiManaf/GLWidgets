// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Pages in tabs, in one place: the graphs of a script, the files of a code editor. A click on a tab shows its page; its ×,
// or the middle button, asks to close it; dragged along the bar, it moves there. Tabs that do not fit scroll (the wheel),
// the one shown kept in view. It looks as a dock's stack (the theme's DockStyle), but stays where it is: its tabs go to no
// other place (Docking.hpp does that).
//
//   auto view = std::make_unique<glw::TabView>(glw::TabView::Props{.onCloseRequested = [&](std::size_t i) { ... }});
//   view->addTab(glw::tr("Event Graph"), std::move(graph));
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

class GLW_EXPORT TabView : public Widget {
public:
    // What a tab view is made with (ui::TabView): designated initializers, in this order.
    struct Props {
        bool closable = true;                       // its tabs have a ×
        bool movable = true;                        // its tabs are dragged along the bar
        // The page shown changed: by a click, a tab added, moved or removed (none: no page left).
        std::function<void(std::optional<std::size_t>)> onCurrentChanged;
        // A tab's × or the middle button: without a handler, the tab is removed (after the event; its page destroyed).
        std::function<void(std::size_t)> onCloseRequested;
        // A tab dragged from `from` to `to` (its index now).
        std::function<void(std::size_t from, std::size_t to)> onMoved;
    };

    TabView();
    explicit TabView(Props props);
    ~TabView() override;

    // A tab showing `content`, at the end (insertTab: at `index`), shown. Its index.
    std::size_t addTab(LocalizedText title, std::unique_ptr<Widget> content);
    void insertTab(std::size_t index, LocalizedText title, std::unique_ptr<Widget> content);
    // The tab at `index` out: its page back to the caller.
    std::unique_ptr<Widget> removeTab(std::size_t index);
    void moveTab(std::size_t from, std::size_t to);
    std::size_t count() const { return tabs_.size(); }
    std::optional<std::size_t> current() const { return current_; }
    void setCurrent(std::size_t index);
    const LocalizedText& title(std::size_t index) const { return tabs_[index].title; }
    void setTitle(std::size_t index, LocalizedText title);
    // What shows by the mouse over the tab (its title when none, if it is cut).
    void setTabToolTip(std::size_t index, LocalizedText text);
    Widget* content(std::size_t index) const { return tabs_[index].content; }
    std::optional<std::size_t> indexOf(const Widget& content) const;

    void setClosable(bool closable);
    void setOnCurrentChanged(std::function<void(std::optional<std::size_t>)> handler) { props_.onCurrentChanged = std::move(handler); }
    void setOnCloseRequested(std::function<void(std::size_t)> handler) { props_.onCloseRequested = std::move(handler); }
    void setOnMoved(std::function<void(std::size_t, std::size_t)> handler) { props_.onMoved = std::move(handler); }
    const DockStyle& style() const;

    // The tab's place in the view (its coordinates), as arranged: for a test, or a popup by it.
    Rect tabRect(std::size_t index) const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override { onStyleChanged(); }
    void onStyleChanged() override;

private:
    class Bar;
    friend class Bar;
    struct Tab {
        LocalizedText title;
        Widget* content = nullptr;
        std::optional<LocalizedText> toolTip;
    };
    // The bar's tabs made again: their titles, which is shown.
    void rebuild();
    void current(std::optional<std::size_t> index, bool tell);
    // A tab's close asked for (after the event).
    void requestClose(std::size_t index);

    Props props_;
    std::vector<Tab> tabs_;
    std::optional<std::size_t> current_;
    Bar* bar_ = nullptr;
};

} // namespace glw
