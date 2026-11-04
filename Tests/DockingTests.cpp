// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Docking: the tree addPanel makes, panels moved (by the API and by the mouse), closed, the layout saved and restored; major
// tabs and their strip.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <string>

using namespace glw;

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 0.5f;
}

DockPanel panel(const std::string& id) {
    return DockPanel{id, LocalizedText(id), std::make_unique<Widget>()};
}

// graph in the middle, palette at its start, output under it with search as its second tab
struct Docked {
    UiRoot root{tests::testContext()};
    DockArea* area = nullptr;
    DockTabs* graph = nullptr;
    DockTabs* palette = nullptr;
    DockTabs* output = nullptr;

    explicit Docked(LayoutDirection direction = LayoutDirection::LeftToRight) {
        auto a = std::make_unique<DockArea>();
        area = a.get();
        root.setSize({800, 600});
        root.setLayoutDirection(direction);
        root.setContent(std::move(a));
        graph = &area->addPanel(panel("graph"));
        palette = &area->addPanel(panel("palette"), graph, DockSide::Start, 0.25f);
        output = &area->addPanel(panel("output"), graph, DockSide::Bottom, 0.3f);
        area->addPanel(panel("search"), output);
        root.layout();
    }

    // a tab of a stack, the root's coordinates of its middle (on its title: its × is at its end)
    Point tab(DockTabs& tabs, std::size_t index) {
        const Widget& t = *tabs.children()[0]->children()[index];
        return t.mapToRoot({t.size().width / 2, t.size().height / 2});
    }
    Point middleOf(const Widget& w) { return w.mapToRoot({w.size().width / 2, w.size().height / 2 + 15}); }
    // the middle of a stack's guide for `side`, the root's coordinates
    Point guide(DockTabs& tabs, DockSide side) { return tabs.mapToRoot(tabs.guide(side).center()); }

    void drag(Point from, Point to) {
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, from});
        root.dispatch(platform::MouseMoved{1, {from.x + 10, from.y}});
        root.dispatch(platform::MouseMoved{1, to});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, to});
        root.layout();
    }
};

} // namespace

TEST(addPanelMakesTheTree) {
    Docked d;
    // the area: a horizontal split [palette | vertical split [graph / output+search]]
    auto* top = dynamic_cast<DockSplit*>(d.area->children()[0].get());
    CHECK(top && top->orientation() == Orientation::Horizontal && top->children().size() == 2);
    CHECK(top->children()[0].get() == d.palette);
    auto* right = dynamic_cast<DockSplit*>(top->children()[1].get());
    CHECK(right && right->orientation() == Orientation::Vertical);
    CHECK(right->children()[0].get() == d.graph && right->children()[1].get() == d.output);
    CHECK(d.output->count() == 2 && d.output->panelId(1) == "search" && d.output->active() == 1);
    // the shares
    CHECK(near(d.palette->bounds().width, (800 - tests::testContext().theme.dock.splitter.thickness) * 0.25f));
    CHECK(d.palette->mapToRoot({}).x < d.graph->mapToRoot({}).x);
    CHECK((d.area->panelIds() == std::vector<std::string>{"palette", "graph", "output", "search"}));
}

TEST(startIsOnTheRightInRightToLeft) {
    Docked d(LayoutDirection::RightToLeft);
    CHECK(d.palette->mapToRoot({}).x > d.graph->mapToRoot({}).x);
}

TEST(draggingATabOntoAnotherStack) {
    Docked d;
    // palette's only tab onto the graph's tabs: the palette's stack goes, and its split gives way
    d.drag(d.tab(*d.palette, 0), d.tab(*d.graph, 0));
    CHECK(d.graph->count() == 2 && d.graph->panelId(1) == "palette");
    CHECK(d.area->tabsOf("palette") == d.graph);
    CHECK(dynamic_cast<DockSplit*>(d.area->children()[0].get())->orientation() == Orientation::Vertical);
}

TEST(draggingATabToAGuide) {
    Docked d;
    // the guides: a cross in the middle of the stack, below its tabs; Start on the left in left to right
    const Rect center = d.graph->guide(DockSide::Center);
    CHECK(center.y > DockStyle{}.tabHeight || center.y > 30);
    CHECK(d.graph->guide(DockSide::Start).x < center.x && d.graph->guide(DockSide::End).x > center.x);
    CHECK(d.graph->guide(DockSide::Top).y < center.y && d.graph->guide(DockSide::Bottom).y > center.y);

    // near the right edge of graph, on no guide: nothing (the edges are not drop zones any more)
    const std::string before = d.area->saveLayout();
    const Rect g{d.graph->mapToRoot({}).x, d.graph->mapToRoot({}).y, d.graph->size().width, d.graph->size().height};
    d.drag(d.tab(*d.output, 1), {g.right() - 10, g.y + g.height / 2});
    CHECK(d.area->saveLayout() == before);

    // search, out of output, onto graph's End guide: a new stack on graph's end side
    d.drag(d.tab(*d.output, 1), d.guide(*d.graph, DockSide::End));
    DockTabs* search = d.area->tabsOf("search");
    CHECK(search && search != d.output && search->count() == 1);
    CHECK(search->mapToRoot({}).x > d.graph->mapToRoot({}).x);
    CHECK(near(search->mapToRoot({}).y, d.graph->mapToRoot({}).y));
    CHECK(d.output->count() == 1);

    // output onto graph's Center guide: in its stack
    d.drag(d.tab(*d.output, 0), d.guide(*d.graph, DockSide::Center));
    CHECK(d.area->tabsOf("output") == d.graph);
}

TEST(guidesAreMirroredRightToLeft) {
    Docked d(LayoutDirection::RightToLeft);
    CHECK(d.graph->guide(DockSide::Start).x > d.graph->guide(DockSide::Center).x);
    d.drag(d.tab(*d.output, 1), d.guide(*d.graph, DockSide::Start));
    CHECK(d.area->tabsOf("search")->mapToRoot({}).x > d.graph->mapToRoot({}).x);     // on the right: the start
}

TEST(droppingWhereNothingChanges) {
    Docked d;
    const std::string before = d.area->saveLayout();
    d.drag(d.tab(*d.palette, 0), d.middleOf(*d.palette));      // its only tab onto its own stack
    CHECK(d.area->saveLayout() == before);
}

TEST(closingPanels) {
    Docked d;
    std::string closed;
    d.area->setOnPanelClosed([&](DockPanel p) { closed = p.id; });
    d.area->closePanel("palette");
    CHECK(closed == "palette");
    CHECK(!d.area->tabsOf("palette"));
    // its split gave way: graph and output are now the area's only split
    CHECK(dynamic_cast<DockSplit*>(d.area->children()[0].get())->orientation() == Orientation::Vertical);
    std::unique_ptr<Widget> content = d.area->removePanel("graph");
    CHECK(content != nullptr);
    CHECK(d.area->children()[0].get() == d.output);             // one stack left: the node itself
}

TEST(layoutsSavedAndRestored) {
    Docked d;
    d.drag(d.tab(*d.output, 1), d.tab(*d.palette, 0));        // search with palette
    d.root.layout();
    const std::string saved = d.area->saveLayout();
    CHECK(saved.find("\"tabs\":[\"palette\",\"search\"]") != std::string::npos);

    // the same panels, added otherwise, arranged as saved
    Docked other;
    std::string error;
    CHECK(other.area->restoreLayout(saved, &error));
    other.root.layout();
    CHECK(other.area->saveLayout() == saved);

    // a layout naming a panel the area does not have, and not naming one it has
    Docked third;
    CHECK(third.area->restoreLayout(R"({"tabs":["graph","ghost"]})", &error));
    CHECK(third.area->tabsOf("graph") == third.area->tabsOf("palette"));     // the others joined the first stack
    CHECK(third.area->tabsOf("graph")->active() == 0);                       // which shows the tab the layout says
    CHECK(third.area->panelIds().size() == 4);
    CHECK(!third.area->restoreLayout(R"({"split":"diagonal","panes":[]})", &error));
    CHECK(!error.empty());
}

TEST(majorTabs) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    root.setSize({800, 600});
    auto tabs = std::make_unique<MajorTabs>();
    MajorTabs& documents = *tabs;
    auto strip = documents.makeStrip();
    MajorTabStrip* s = strip.get();
    auto page = std::make_unique<Widget>();
    page->setLayout(std::make_unique<StackLayout>(Orientation::Vertical));
    Widget& bar = page->add(std::make_unique<Widget>());
    bar.setMinimumSize({0, 40});
    bar.addChild(std::move(strip));
    page->addChild(std::move(tabs)).setStretch(1);
    root.setContent(std::move(page));

    std::vector<std::size_t> shown;
    documents.setOnActiveChanged([&](MajorTabs&, std::size_t i) { shown.push_back(i); });
    documents.addTab("one", std::make_unique<Widget>());
    documents.addTab("two", std::make_unique<Widget>());
    documents.addTab("three", std::make_unique<Widget>());
    root.layout();
    CHECK(documents.count() == 3 && documents.active() == 2);
    CHECK(!documents.content(0)->visible() && documents.content(2)->visible());

    // a click on the first tab shows it; its × closes it
    const Widget& row = *s->children()[0];
    const Point first = row.children()[0]->mapToRoot({20, 15});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, first});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, first});
    CHECK(documents.active() == 0 && documents.content(0)->visible());

    // dragged after the last: it goes to the end, still shown
    root.layout();
    const Point start = row.children()[0]->mapToRoot({20, 15});
    const Point end = row.children()[2]->mapToRoot({row.children()[2]->size().width - 5, 15});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, start});
    root.dispatch(platform::MouseMoved{1, {start.x + 20, start.y}});
    root.dispatch(platform::MouseMoved{1, end});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, end});
    CHECK(documents.title(2) == "one" && documents.active() == 2);
    CHECK(documents.title(0) == "two");

    // the strip drags the window where it has no tab
    root.layout();
    CHECK(root.windowHitTest(row.children()[0]->mapToRoot({20, 15})) == platform::HitArea::Client);
    CHECK(root.windowHitTest({790, 20}) == platform::HitArea::Caption);

    std::unique_ptr<Widget> removed = documents.removeTab(2);
    CHECK(documents.count() == 2 && documents.active() == 1);
}

TEST(tabsAreAsWideAsTheirTitlesAndLongOnesAreCut) {
    Docked d;
    d.area->addPanel({"long", "A panel whose title is far longer than any tab may be", std::make_unique<Widget>()}, d.graph);
    d.root.layout();
    paint::DrawList list;
    d.root.paint(list);
    const DockStyle& s = tests::testContext().theme.dock;
    const Widget& row = *d.graph->children()[0];
    const Widget& graphTab = *row.children()[0];
    const Widget& longTab = *row.children()[1];
    CHECK(graphTab.size().width >= s.tabMinimumWidth && graphTab.size().width < s.tabMaximumWidth);
    CHECK(near(longTab.size().width, s.tabMaximumWidth));
    CHECK(static_cast<const Label&>(*longTab.children()[0]).elided());
    CHECK(!static_cast<const Label&>(*graphTab.children()[0]).elided());
}

TEST(tabsThatDoNotFitScroll) {
    Docked d;
    // the palette's stack (a quarter of the area) with many tabs: the one shown, the last added, is in view
    for (int i = 0; i < 12; ++i)
        d.area->addPanel(panel("tool " + std::to_string(i)), d.palette);
    d.root.layout();
    const Widget& row = *d.palette->children()[0];
    const Rect rowRect{row.mapToRoot({}).x, row.mapToRoot({}).y, row.size().width, row.size().height};
    auto onScreen = [&](std::size_t i) {
        const Widget& tab = *row.children()[i];
        const Point at = tab.mapToRoot({});
        return at.x >= rowRect.x - 0.5f && at.x + tab.size().width <= rowRect.right() + 0.5f;
    };
    CHECK(d.palette->active() == 12 && onScreen(12) && !onScreen(0));
    CHECK(row.mapToRoot({}).x + row.size().width <= d.palette->mapToRoot({}).x + d.palette->size().width);

    // the wheel up: back to the start
    const Point over = row.mapToRoot({20, 10});
    for (int i = 0; i < 40; ++i)
        d.root.dispatch(platform::MouseWheelScrolled{1, {0, 1}, over});
    CHECK(onScreen(0) && !onScreen(12));

    // pressed, a tab is shown and brought into view; the thin bar at the bottom, dragged to its end, shows the last
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, over});
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, over});
    CHECK(d.palette->active() == 0);
    const Point bar{rowRect.x + 10, rowRect.bottom() - 2};
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, bar});
    d.root.dispatch(platform::MouseMoved{1, {rowRect.right() + 50, bar.y}});
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, {rowRect.right() + 50, bar.y}});
    CHECK(onScreen(12) && d.palette->active() == 0);
}

TEST(panelButtonsOnTheTabBar) {
    Docked d;
    int cleared = 0;
    DockPanel log{"log", "Log", std::make_unique<Widget>()};
    log.buttons.push_back({[](PaintContext&, const Rect&, Color) {}, [&] { ++cleared; }});
    d.area->addPanel(std::move(log), d.graph);
    d.root.layout();
    // shown while its panel is (no float button without a docking host)
    const Widget& tools = *d.graph->children()[1];
    CHECK(tools.children().size() == 1);
    const Widget& button = *tools.children()[0];
    CHECK(button.mapToRoot({}).x > d.graph->children()[0]->mapToRoot({}).x);      // after the tabs
    const Point at = button.mapToRoot({12, 12});
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
    d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
    CHECK(cleared == 1);
    d.graph->setActive(0);
    CHECK(tools.children().empty());
    // it goes with its panel
    DockPanel taken = d.graph->take(1);
    CHECK(taken.buttons.size() == 1);
}

TEST(aStacksTitleMovesItsGroup) {
    Docked d;
    // output's stack (output, search) by its title onto the graph's tabs: both join the graph's stack
    const Widget& last = *d.output->children()[0]->children().back();
    const Point title = last.mapToRoot({last.size().width + 20, last.size().height / 2});
    d.drag(title, d.tab(*d.graph, 0));
    CHECK(d.area->tabsOf("output") == d.graph && d.area->tabsOf("search") == d.graph && d.graph->count() == 3);
    CHECK(d.graph->panelId(d.graph->active()) == "search");     // the one output's stack showed
}

TEST(aCenterKeepsItsPlace) {
    UiRoot root(tests::testContext());
    root.setSize({800, 600});
    auto made = std::make_unique<DockArea>();
    DockArea& area = *made;
    auto view = std::make_unique<Widget>();
    Widget* center = view.get();
    area.setCenter(std::move(view));
    CHECK(center->minimumSize().width >= 160 && center->minimumSize().height >= 100);     // its room is kept
    DockTabs& palette = area.addPanel(panel("palette"), center, DockSide::Start, 0.25f);
    DockTabs& output = area.addPanel(panel("output"), center, DockSide::Bottom, 0.3f);
    area.addPanel(panel("search"), &output);
    root.setContent(std::move(made));
    root.layout();
    CHECK(palette.mapToRoot({}).x < center->mapToRoot({}).x);
    CHECK(output.mapToRoot({}).y > center->mapToRoot({}).y && near(output.mapToRoot({}).x, center->mapToRoot({}).x));

    auto drag = [&](Point from, Point to) {
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, from});
        root.dispatch(platform::MouseMoved{1, {from.x + 10, from.y}});
        root.dispatch(platform::MouseMoved{1, to});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, to});
        root.layout();
    };
    auto tabOf = [&](const std::string& id) {
        DockTabs* tabs = area.tabsOf(id);
        const Widget& t = *tabs->children()[0]->children()[*tabs->indexOf(id)];
        return t.mapToRoot({t.size().width / 2, t.size().height / 2});
    };
    // a tab let go in the middle of the center: not a place for it; on the center's End guide: beside it
    const std::string before = area.saveLayout();
    drag(tabOf("search"), center->mapToRoot({center->size().width / 2, center->size().height / 2}));
    CHECK(area.saveLayout() == before);
    drag(tabOf("search"), center->mapToRoot(DockArea::guide(*center, DockSide::End).center()));
    DockTabs* search = area.tabsOf("search");
    CHECK(search && search->count() == 1 && search->mapToRoot({}).x > center->mapToRoot({}).x);
    CHECK(area.center() == center && center->root() == &root);

    // saved and restored: in its place
    const std::string saved = area.saveLayout();
    CHECK(saved.find("\"center\":true") != std::string::npos);
    CHECK(area.restoreLayout(saved));
    root.layout();
    CHECK(area.saveLayout() == saved && area.center() == center);

    // the panels all gone: the center fills the area
    for (const char* id : {"palette", "output", "search"})
        area.removePanel(id);
    root.layout();
    CHECK(near(center->size().width, area.size().width) && near(center->size().height, area.size().height));
}

TEST(aStackOfOnePanelHidesItsTabBar) {
    Docked d;
    auto click = [&](Point at) {
        d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
        d.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
        d.root.layout();
    };
    // the palette's stack (one panel): its hide button, the last of its tools (no float button without a docking host)
    const Widget& tools = *d.palette->children()[1];
    CHECK(tools.children().size() == 1);
    click(tools.children().back()->mapToRoot({12, 12}));
    CHECK(d.palette->tabBarHidden() && d.palette->barHeight() == 0);
    // the panel takes all of the stack
    const Widget& content = *d.palette->panelContent(0);
    CHECK(near(content.mapToRoot({}).y, d.palette->mapToRoot({}).y) && near(content.size().height, d.palette->size().height));
    // saved hidden, restored hidden
    const std::string saved = d.area->saveLayout();
    CHECK(saved.find("\"hidden\":true") != std::string::npos);
    CHECK(d.area->restoreLayout(saved));
    d.root.layout();
    CHECK(d.area->tabsOf("palette")->tabBarHidden());
    // its corner pressed: the bar again
    DockTabs& palette = *d.area->tabsOf("palette");
    click(palette.mapToRoot(palette.revealCorner().center()));
    CHECK(!palette.tabBarHidden() && palette.barHeight() > 0);

    // a stack of two does not hide its bar (the stacks are others since the layout was restored)
    DockTabs& output = *d.area->tabsOf("output");
    output.setTabBarHidden(true);
    CHECK(!output.tabBarHidden());
    // another panel joining a hidden stack: its bar again
    palette.setTabBarHidden(true);
    d.root.layout();
    CHECK(palette.tabBarHidden());
    d.drag(d.tab(output, *output.indexOf("search")), palette.mapToRoot(palette.guide(DockSide::Center).center()));
    CHECK(palette.count() == 2 && !palette.tabBarHidden());
}
