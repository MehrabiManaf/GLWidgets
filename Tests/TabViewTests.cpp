// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/TabView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace glw;

namespace {

// A tab view of three pages filling a root (600 by 300), what it tells kept.
struct Tabbed {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    TabView* view = nullptr;
    std::vector<Widget*> pages;
    std::vector<std::optional<std::size_t>> currents;
    std::vector<std::size_t> closes;
    std::vector<std::pair<std::size_t, std::size_t>> moves;

    explicit Tabbed(bool handleClose = true) {
        TabView::Props props{.onCurrentChanged = [this](std::optional<std::size_t> i) { currents.push_back(i); },
                             .onMoved = [this](std::size_t from, std::size_t to) { moves.emplace_back(from, to); }};
        if (handleClose)
            props.onCloseRequested = [this](std::size_t i) { closes.push_back(i); };
        std::unique_ptr<Widget> v = ui::TabView(std::move(props)).bind(view);
        for (const char* title : {"EventGraph", "Update", "Helper"}) {
            auto page = std::make_unique<Widget>();
            pages.push_back(page.get());
            view->addTab(LocalizedText(title), std::move(page));
        }
        root.setSize({600, 300});
        root.setContent(std::move(v));
        root.layout();
        currents.clear();
    }

    Point middleOf(std::size_t i) const { return view->tabRect(i).center(); }
    Point closeOf(std::size_t i) const {
        const Rect r = view->tabRect(i);
        return {r.x + r.width - 14, r.y + r.height / 2};
    }
    void press(Point p, platform::MouseButton b = platform::MouseButton::Left) {
        root.dispatch(platform::MouseMoved{1, p});
        root.dispatch(platform::MouseButtonChanged{1, b, true, 1, p});
    }
    void move(Point p) { root.dispatch(platform::MouseMoved{1, p}); }
    void release(Point p, platform::MouseButton b = platform::MouseButton::Left) {
        root.dispatch(platform::MouseButtonChanged{1, b, false, 1, p});
        root.layout();
    }
    void click(Point p, platform::MouseButton b = platform::MouseButton::Left) {
        press(p, b);
        release(p, b);
    }
};

} // namespace

TEST(aTabViewShowsThePageOfTheTabClicked) {
    Tabbed t;
    // the last added shown, its page under the bar, the others hidden
    CHECK(t.view->count() == 3 && t.view->current() == 2);
    CHECK(t.pages[2]->visible() && !t.pages[0]->visible() && !t.pages[1]->visible());
    const float bar = t.view->style().tabHeight;
    CHECK(t.pages[2]->bounds().y == bar && t.pages[2]->bounds().height == 300 - bar - 1);
    // the tabs side by side from the start
    CHECK(t.view->tabRect(0).x == 0 && t.view->tabRect(1).x == t.view->tabRect(0).right());
    t.click(t.middleOf(0));
    CHECK(t.view->current() == 0 && t.pages[0]->visible() && !t.pages[2]->visible());
    CHECK((t.currents == std::vector<std::optional<std::size_t>>{0}));
    t.click(t.middleOf(0));                             // shown already: nothing
    CHECK(t.currents.size() == 1);
    CHECK(t.view->indexOf(*t.pages[1]) == 1);
}

TEST(aTabViewsTabsAskToCloseByTheirCrossOrTheMiddleButton) {
    Tabbed t;
    t.click(t.closeOf(1));
    CHECK((t.closes == std::vector<std::size_t>{1}) && t.view->count() == 3);   // asked: the handler removes it
    CHECK(t.view->current() == 2);
    t.click(t.middleOf(0), platform::MouseButton::Middle);
    CHECK((t.closes == std::vector<std::size_t>{1, 0}));
    // pressed on the ×, let go elsewhere: nothing
    t.press(t.closeOf(1));
    t.release(t.middleOf(2));
    CHECK(t.closes.size() == 2);

    // without a handler: removed
    Tabbed u(false);
    u.click(u.closeOf(2));
    CHECK(u.view->count() == 2 && u.view->current() == 1 && u.pages[1]->visible());
    CHECK((u.currents == std::vector<std::optional<std::size_t>>{1}));
}

TEST(aTabViewsTabIsDraggedAlongTheBar) {
    Tabbed t;
    // the first, dragged past the others' middles: last; still shown
    t.press(t.middleOf(0));
    const Point end = t.middleOf(2);
    t.move({t.middleOf(0).x + 10, end.y});
    t.move({end.x + 10, end.y});
    t.release({end.x + 10, end.y});
    CHECK(t.view->title(2).source == "EventGraph" && t.view->title(0).source == "Update");
    CHECK(t.view->content(2) == t.pages[0] && t.view->current() == 2);
    CHECK((t.moves == std::vector<std::pair<std::size_t, std::size_t>>{{0, 2}}));
    // a click without a drag moves nothing
    t.click(t.middleOf(1));
    CHECK(t.moves.size() == 1 && t.view->current() == 1);
}

TEST(aTabViewKeepsWhatItShowsAsTabsComeAndGo) {
    Tabbed t;
    t.view->setCurrent(1);
    t.currents.clear();
    std::unique_ptr<Widget> first = t.view->removeTab(0);           // before it: the same page, one place back
    CHECK(first.get() == t.pages[0] && first->visible());
    CHECK(t.view->current() == 0 && t.view->content(0) == t.pages[1]);
    t.view->removeTab(0);                                            // it: the one in its place
    CHECK(t.view->current() == 0 && t.view->content(0) == t.pages[2] && t.pages[2]->visible());
    t.view->removeTab(0);
    CHECK(!t.view->current() && t.view->count() == 0);
    CHECK((t.currents == std::vector<std::optional<std::size_t>>{0, 0, std::nullopt}));
    t.view->setTitle(0, "nothing there");                            // no tab: nothing
}
