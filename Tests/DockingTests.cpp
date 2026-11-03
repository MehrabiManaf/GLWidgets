// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Docking: the tree addPanel makes, panels moved (by the API and by the mouse), closed, the layout saved and restored; major
// tabs and their strip.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Docking.hpp>
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
    CHECK(near(d.palette->bounds().width, (800 - 6) * 0.25f));
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
