// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Docking across windows, with windows that are only roots at places of a pretend screen: major tabs moved from one window
// to another, torn off into a new one, a window dragged by its only tab and merged; panels floated, docked back, sent home
// when their window closes, hidden with their area and closed with it.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/DockingHost.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace glw;

namespace {

// The windows: a screen of dips (one screen unit per dip), the one in front first.
struct Screen final : DockingHost {
    struct Win {
        tests::FakeHost host;               // its closeWindow counts: closing
        std::unique_ptr<UiRoot> root;
        Point position;
        bool visible = true;
        float opacity = 1;
        bool closing = false;
        bool told = false;                  // its root was told it closes
        const UiRoot* owner = nullptr;
        LocalizedText title;
    };

    UiContext context{tests::testContext()};
    std::vector<std::unique_ptr<Win>> windows;
    Point mouse;
    int captures = 0;
    std::optional<Point> preview;       // showWindowPreview's
    Size previewSize;

    Screen() { context.docking = this; }

    // A window at `position` showing `content` in a frame, with `titleCenter` in its title bar.
    Win& open(std::unique_ptr<Widget> content, Point position, Size size, std::unique_ptr<Widget> titleCenter = nullptr) {
        auto w = std::make_unique<Win>();
        w->root = std::make_unique<UiRoot>(context, &w->host);
        w->root->setSize(size);
        auto frame = std::make_unique<WindowFrame>(std::move(content));
        if (titleCenter)
            frame->titleBar().setCenter(std::move(titleCenter));
        w->root->setContent(std::move(frame));
        w->position = position;
        windows.insert(windows.begin(), std::move(w));
        windows.front()->root->layout();
        return *windows.front();
    }

    Win* find(const UiRoot& root) const {
        for (const auto& w : windows) {
            if (w->root.get() == &root)
                return w.get();
        }
        return nullptr;
    }

    bool closing(const Win& w) const { return w.closing || w.host.closed > 0; }

    // What the application does after the events: the windows closed are told, then go.
    void closeWindows() {
        for (bool told = true; told;) {
            told = false;
            for (std::size_t i = 0; i < windows.size(); ++i) {
                if (Win& w = *windows[i]; closing(w) && !w.told) {
                    w.told = true;
                    w.root->windowClosing();
                    told = true;
                }
            }
        }
        for (;;) {
            const auto it = std::find_if(windows.begin(), windows.end(), [&](const auto& w) { return closing(*w); });
            if (it == windows.end())
                break;
            std::unique_ptr<Win> closed = std::move(*it);
            windows.erase(it);
            closed.reset();
        }
    }

    void layout() {
        for (const auto& w : windows)
            w->root->layout();
    }

    // The mouse, at `screen`, given to `w` (the window it was pressed in).
    void press(Win& w, Point screen) {
        mouse = screen;
        w.root->dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, screen - w.position});
    }
    void moveTo(Win& w, Point screen) {
        mouse = screen;
        w.root->dispatch(platform::MouseMoved{1, screen - w.position});
    }
    void release(Win& w, Point screen) {
        mouse = screen;
        w.root->dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, screen - w.position});
    }
    void drag(Win& w, Point from, Point to) {
        press(w, from);
        moveTo(w, {from.x + 10, from.y});
        moveTo(w, {(from.x + to.x) / 2, (from.y + to.y) / 2});
        moveTo(w, to);
        release(w, to);
        closeWindows();
        layout();
    }

    // ---- DockingHost ----

    Point mouseOnScreen() const override { return mouse; }
    Point toScreen(const UiRoot& root, Point point) const override {
        const Win* w = find(root);
        return w ? point + w->position : point;
    }
    Hit windowAt(Point screen, const UiRoot* except) const override {
        for (const auto& w : windows) {
            const Size size = w->root->size();
            if (w->root.get() != except && w->visible && !closing(*w) &&
                Rect{w->position.x, w->position.y, size.width, size.height}.contains(screen))
                return {w->root.get(), screen - w->position};
        }
        return {};
    }
    void captureMouse(bool capture) override { captures += capture ? 1 : -1; }
    UiRoot& openWindow(NewWindow window) override {
        Win& w = open(std::move(window.content), window.position, window.size, std::move(window.titleCenter));
        if (!window.titleBar)
            static_cast<WindowFrame&>(*w.root->content()).titleBar().setVisible(false);
        w.owner = window.owner;
        w.title = window.title;
        return *w.root;
    }
    void closeWindow(const UiRoot& root) override {
        if (Win* w = find(root))
            w->closing = true;
    }
    void setWindowVisible(const UiRoot& root, bool visible) override {
        if (Win* w = find(root))
            w->visible = visible;
    }
    void setWindowOwner(const UiRoot& root, const UiRoot* owner) override {
        if (Win* w = find(root))
            w->owner = owner;
    }
    void raiseWindow(const UiRoot& root) override {
        const auto it = std::find_if(windows.begin(), windows.end(), [&](const auto& w) { return w->root.get() == &root; });
        if (it != windows.end())
            std::rotate(windows.begin(), it, it + 1);
    }
    Point windowPosition(const UiRoot& root) const override {
        const Win* w = find(root);
        return w ? w->position : Point{};
    }
    void moveWindow(const UiRoot& root, Point position) override {
        if (Win* w = find(root))
            w->position = position;
    }
    void setWindowOpacity(const UiRoot& root, float opacity) override {
        if (Win* w = find(root))
            w->opacity = opacity;
    }
    void showWindowPreview(std::optional<Point> position, Size size) override {
        preview = position;
        previewSize = size;
    }
};

// A window of documents: its major tabs, their strip in the title bar.
struct Documents {
    MajorTabs* tabs = nullptr;
    Screen::Win* window = nullptr;

    Documents(Screen& screen, Point position, std::initializer_list<const char*> titles) {
        auto t = std::make_unique<MajorTabs>();
        tabs = t.get();
        for (const char* title : titles)
            tabs->addTab(title, std::make_unique<Widget>());
        tabs->setActive(0);
        auto strip = tabs->makeStrip();
        window = &screen.open(std::move(t), position, {800, 600}, std::move(strip));
    }
};

// The screen point of the middle of major tab `index` of `tabs`' window.
Point majorTab(const Screen& screen, const MajorTabs& tabs, std::size_t index) {
    const UiRoot* r = tabs.root();
    // the strip: the title bar's center, found from the root down
    const MajorTabStrip* strip = nullptr;
    std::vector<const Widget*> todo{r->content()};
    while (!todo.empty() && !strip) {
        const Widget* w = todo.back();
        todo.pop_back();
        strip = dynamic_cast<const MajorTabStrip*>(w);
        for (const auto& child : w->children())
            todo.push_back(child.get());
    }
    const Widget& tab = *strip->children()[0]->children()[index];
    return tab.mapToRoot({tab.size().width / 2, tab.size().height / 2}) + screen.windowPosition(*r);
}

std::vector<std::string> titles(const MajorTabs& tabs) {
    std::vector<std::string> result;
    for (std::size_t i = 0; i < tabs.count(); ++i)
        result.push_back(tabs.title(i).source);
    return result;
}

DockPanel panel(const std::string& id) {
    return DockPanel{id, LocalizedText(id), std::make_unique<Widget>()};
}

// A window with a dock area: graph, palette at its start.
struct Tools {
    DockArea* area = nullptr;
    DockTabs* graph = nullptr;
    Screen::Win* window = nullptr;

    Tools(Screen& screen, Point position) {
        auto a = std::make_unique<DockArea>();
        area = a.get();
        graph = &area->addPanel(panel("graph"));
        area->addPanel(panel("palette"), graph, DockSide::Start, 0.3f);
        window = &screen.open(std::move(a), position, {800, 600});
    }
};

// The screen point of the tab of panel `id`.
Point panelTab(const Screen& screen, const DockArea& area, const std::string& id) {
    DockTabs* tabs = area.tabsOf(id);
    const Widget& tab = *tabs->children()[0]->children()[*tabs->indexOf(id)];
    return tab.mapToRoot({10, 10}) + screen.windowPosition(*tabs->root());
}

Point middleOf(const Screen& screen, const Widget& w) {
    return w.mapToRoot({w.size().width / 2, w.size().height / 2 + 15}) + screen.windowPosition(*w.root());
}

} // namespace

TEST(majorTabMovesToAnotherWindow) {
    Screen screen;
    Documents a(screen, {0, 0}, {"one", "two"});
    Documents b(screen, {1000, 0}, {"three"});

    // "two" onto the end of b's strip: it goes there, shown; a keeps "one"
    const Point end = majorTab(screen, *b.tabs, 0) + Point{60, 0};
    screen.drag(*a.window, majorTab(screen, *a.tabs, 1), end);
    CHECK((titles(*a.tabs) == std::vector<std::string>{"one"}));
    CHECK((titles(*b.tabs) == std::vector<std::string>{"three", "two"}));
    CHECK(b.tabs->active() == 1 && b.tabs->content(1)->visible());
    CHECK(screen.windows.front()->root.get() == b.tabs->root());     // raised
    CHECK(screen.windows.size() == 2 && screen.captures == 0);
}

TEST(windowDraggedByItsOnlyTabMerges) {
    Screen screen;
    Documents b(screen, {1000, 0}, {"three", "four"});
    Documents a(screen, {0, 0}, {"one"});

    // the only tab drags its window: it follows the mouse
    const Point from = majorTab(screen, *a.tabs, 0);
    screen.press(*a.window, from);
    screen.moveTo(*a.window, {from.x + 10, from.y});
    screen.moveTo(*a.window, {from.x + 200, from.y + 100});
    CHECK(a.window->position.x == 190 && a.window->position.y == 100);
    CHECK(a.window->opacity < 1 && !screen.preview);       // seen through while it moves; no window would open
    screen.release(*a.window, {from.x + 200, from.y + 100});
    screen.closeWindows();
    CHECK(a.window->opacity == 1);
    CHECK(screen.windows.size() == 2 && a.tabs->count() == 1);

    // let go over b's strip: its tab goes there, and the window, empty, closes
    screen.layout();
    const Point start = majorTab(screen, *a.tabs, 0);
    const Point onto = majorTab(screen, *b.tabs, 0);
    screen.drag(*a.window, start, onto);
    CHECK(screen.windows.size() == 1);
    CHECK((titles(*b.tabs) == std::vector<std::string>{"one", "three", "four"}) ||
          (titles(*b.tabs) == std::vector<std::string>{"three", "one", "four"}));
}

TEST(majorTabTornOffIntoANewWindow) {
    Screen screen;
    Documents a(screen, {0, 0}, {"one", "two", "three"});
    int added = 0;
    a.tabs->setOnNewTab([&](MajorTabs& into) {
        ++added;
        into.addTab("new", std::make_unique<Widget>());
    });
    screen.layout();        // the strip has a + now

    // let go far below the strip: a window of its own, with the same handlers
    screen.drag(*a.window, majorTab(screen, *a.tabs, 1), {300, 900});
    CHECK((titles(*a.tabs) == std::vector<std::string>{"one", "three"}));
    CHECK(screen.windows.size() == 2);
    Screen::Win& torn = *screen.windows.front();
    CHECK(torn.title.source == "two" && torn.owner == nullptr);
    CHECK(torn.position.y > 600);
    // its tabs: the one dragged, with a + that calls the handler with them
    auto* frame = dynamic_cast<WindowFrame*>(torn.root->content());
    auto* tornTabs = frame ? dynamic_cast<MajorTabs*>(frame->content()) : nullptr;
    CHECK(tornTabs && (titles(*tornTabs) == std::vector<std::string>{"two"}));
    const auto* strip = dynamic_cast<MajorTabStrip*>(frame->titleBar().center());
    CHECK(strip && strip->tabs() == tornTabs);
    const Widget& plus = *strip->children()[0]->children()[1];
    const Point p = plus.mapToRoot({plus.size().width / 2, plus.size().height / 2}) + torn.position;
    screen.press(torn, p);
    screen.release(torn, p);
    CHECK(added == 1 && tornTabs->count() == 2);

    // its last tab's × closes it
    screen.layout();
    tornTabs->removeTab(1);
    screen.layout();
    const Widget& tab = *strip->children()[0]->children()[0];
    const Point close = tab.mapToRoot({tab.size().width - 18, tab.size().height / 2}) + torn.position;
    screen.press(torn, close);
    screen.release(torn, close);
    screen.closeWindows();
    CHECK(screen.windows.size() == 1);
}

TEST(panelFloatsAndDocksBack) {
    Screen screen;
    Tools t(screen, {0, 0});

    // let go out of the window: a floating window above it, with the panel
    screen.drag(*t.window, panelTab(screen, *t.area, "palette"), {1200, 300});
    CHECK(screen.windows.size() == 2);
    Screen::Win& floating = *screen.windows.front();
    CHECK(floating.owner == t.window->root.get() && floating.title.source == "palette");
    CHECK(t.area->floatingAreas().size() == 1);
    DockArea* away = t.area->floatingAreas()[0];
    CHECK(away->floating() && away->home() == t.area && t.area->tabsOf("palette")->area() == away);
    CHECK(t.area->panelIds().size() == 2);      // the floating windows' panels too
    CHECK(t.area->tabsOf("graph")->parent() == t.area);     // the split it left gave way

    // no title bar: the tab bar is, with the buttons at its end (dragging the window itself, docking as it goes)
    auto& frame = static_cast<WindowFrame&>(*floating.root->content());
    CHECK(!frame.titleBar().visible());
    const DockTabs& stack = *away->tabsOf("palette");
    const Widget& firstTab = *stack.children()[0]->children()[0];
    const float barMiddle = firstTab.mapToRoot({}).y + firstTab.size().height / 2;
    const float afterTab = firstTab.mapToRoot({}).x + firstTab.size().width + 10;
    CHECK(floating.root->windowHitTest({afterTab, barMiddle}) == platform::HitArea::Client);
    CHECK(floating.root->windowHitTest(firstTab.mapToRoot({10, 10})) == platform::HitArea::Client);
    const Point close{floating.root->size().width - 20, barMiddle};
    CHECK(dynamic_cast<const WindowButton*>(floating.root->widgetAt(close)) != nullptr);
    CHECK(floating.root->windowHitTest(close) == platform::HitArea::Client);
    CHECK(firstTab.mapToRoot({}).x + firstTab.size().width < close.x - 40);      // the tabs leave the buttons room

    // its only tab drags the floating window
    const Point from = panelTab(screen, *t.area, "palette");
    const Point before = floating.position;
    screen.press(floating, from);
    screen.moveTo(floating, {from.x + 10, from.y});
    screen.moveTo(floating, {from.x + 50, from.y + 40});
    // (from where it was when the drag began, past the threshold)
    CHECK(floating.position.x == before.x + 40 && floating.position.y == before.y + 40);
    // over the graph's stack: shown there, and docked there when let go
    const Point onto = t.graph->mapToRoot(t.graph->guide(DockSide::Center).center()) + t.window->position;
    screen.moveTo(floating, onto);
    screen.release(floating, onto);
    screen.closeWindows();
    screen.layout();
    CHECK(screen.windows.size() == 1);
    CHECK(t.area->tabsOf("palette") == t.area->tabsOf("graph"));
    CHECK(t.area->floatingAreas().empty());
}

TEST(floatingPanelsGoHomeAndFollowTheirArea) {
    Screen screen;
    Tools t(screen, {0, 0});
    screen.drag(*t.window, panelTab(screen, *t.area, "palette"), {1200, 300});
    CHECK(screen.windows.size() == 2);

    // the area hidden (its major tab not shown): its floating windows hide; shown again, they show
    t.area->setVisible(false);
    CHECK(!screen.windows.front()->visible);
    t.area->setVisible(true);
    CHECK(screen.windows.front()->visible);

    // the floating window closed: its panel goes back to the area
    screen.windows.front()->host.closeWindow();
    screen.closeWindows();
    CHECK(screen.windows.size() == 1);
    CHECK(t.area->tabsOf("palette") && t.area->tabsOf("palette")->area() == t.area);

    // floated again, then the area goes: its floating windows close
    screen.layout();
    screen.drag(*t.window, panelTab(screen, *t.area, "palette"), {1200, 300});
    CHECK(screen.windows.size() == 2);
    auto* frame = dynamic_cast<WindowFrame*>(t.window->root->content());
    frame->setContent(nullptr);
    screen.closeWindows();
    CHECK(screen.windows.size() == 1);
}

TEST(outlinesWhereAWindowWouldOpen) {
    Screen screen;
    Documents a(screen, {0, 0}, {"one", "two"});

    // a major tab out of every strip: the outline of its window, the size of this one; back on the strip, none
    const Point from = majorTab(screen, *a.tabs, 1);
    screen.press(*a.window, from);
    screen.moveTo(*a.window, {from.x + 10, from.y});
    screen.moveTo(*a.window, {300, 900});
    CHECK(screen.preview && screen.preview->y > 600 && screen.previewSize == Size{800, 600});
    screen.moveTo(*a.window, from);
    CHECK(!screen.preview);
    screen.release(*a.window, from);
    CHECK(screen.windows.size() == 1 && !screen.preview);

    // a panel: no outline, its window itself
    Tools t(screen, {0, 1000});
    const Point tab = panelTab(screen, *t.area, "palette");
    screen.press(*t.window, tab);
    screen.moveTo(*t.window, {tab.x + 10, tab.y});
    screen.moveTo(*t.window, {1200, 1300});
    CHECK(!screen.preview && screen.windows.size() == 3);
    screen.release(*t.window, {1200, 1300});
    screen.closeWindows();
    CHECK(!screen.preview);
}

TEST(panelFloatsAsSoonAsItLeavesItsTabBar) {
    Screen screen;
    Tools t(screen, {0, 0});
    const std::size_t graphPanels = t.graph->count();

    // along its tab bar: still in its stack
    const Point tab = panelTab(screen, *t.area, "palette");
    screen.press(*t.window, tab);
    screen.moveTo(*t.window, {tab.x + 10, tab.y});
    screen.moveTo(*t.window, {tab.x + 12, tab.y + 10});
    CHECK(screen.windows.size() == 1 && t.area->tabsOf("palette")->area() == t.area);

    // down into its own area (another stack of it, the graph): it floats at once, in a window under the mouse, seen
    // through, without the focus; the drag goes on with that window
    const Point inGraph = t.graph->mapToRoot({60, 200});
    screen.moveTo(*t.window, inGraph);
    CHECK(screen.windows.size() == 2);
    Screen::Win& torn = *screen.windows.front();
    CHECK(t.area->tabsOf("palette")->area()->floating() && torn.opacity < 1);
    const Point at = torn.position;
    screen.moveTo(*t.window, {inGraph.x + 30, inGraph.y + 20});
    CHECK(torn.position.x == at.x + 30 && torn.position.y == at.y + 20);

    // over the graph's End guide: shown there; let go, it docks: the floating window goes
    const Point guide = t.graph->mapToRoot(t.graph->guide(DockSide::End).center());
    screen.moveTo(*t.window, guide);
    screen.release(*t.window, guide);
    screen.closeWindows();
    screen.layout();
    CHECK(screen.windows.size() == 1 && screen.captures == 0);
    DockTabs* palette = t.area->tabsOf("palette");
    CHECK(palette && palette->area() == t.area && palette != t.graph);
    CHECK(palette->mapToRoot({}).x > t.graph->mapToRoot({}).x);      // at its end
    CHECK(t.graph->count() == graphPanels);

    // let go on no guide: it stays floating where it is, raised
    screen.drag(*t.window, panelTab(screen, *t.area, "palette"), {500, 300});
    CHECK(screen.windows.size() == 2 && screen.windows.front()->root.get() == t.area->tabsOf("palette")->root());
    CHECK(screen.windows.front()->opacity == 1);
}

TEST(floatingWindowDraggedByItsTitleDocksAllItsPanels) {
    Screen screen;
    Tools t(screen, {0, 0});
    // a floating window with two panels: inspector floated, then palette dropped on its middle guide
    t.area->addPanel(panel("inspector"), t.graph, DockSide::End, 0.6f);      // wide enough for two tabs and the buttons
    screen.layout();
    screen.drag(*t.window, panelTab(screen, *t.area, "inspector"), {1200, 300});
    Screen::Win& floating = *screen.windows.front();
    screen.layout();
    DockTabs& inspector = *t.area->tabsOf("inspector");
    const Point middle = inspector.mapToRoot(inspector.guide(DockSide::Center).center()) + floating.position;
    screen.drag(*t.window, panelTab(screen, *t.area, "palette"), middle);
    CHECK(screen.windows.size() == 2 && t.area->tabsOf("palette") == t.area->tabsOf("inspector"));

    // its title (its tab bar after the tabs): a double click maximizes it
    screen.layout();
    DockTabs& stack = *t.area->tabsOf("palette");
    const Widget& lastTab = *stack.children()[0]->children()[1];
    const Point title = lastTab.mapToRoot({lastTab.size().width + 15, lastTab.size().height / 2}) + floating.position;
    CHECK(dynamic_cast<const WindowButton*>(floating.root->widgetAt(title - floating.position)) == nullptr);
    floating.root->dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 2, title - floating.position});
    floating.root->dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 2, title - floating.position});
    CHECK(floating.host.toggled == 1);

    // dragged by it: the window follows, seen through; let go on the graph's middle guide, both panels dock there
    const Point before = floating.position;
    screen.press(floating, title);
    screen.moveTo(floating, {title.x + 10, title.y});
    screen.moveTo(floating, {title.x + 40, title.y + 30});
    CHECK(floating.position.x == before.x + 30 && floating.position.y == before.y + 30 && floating.opacity < 1);
    const Point graphMiddle = t.graph->mapToRoot(t.graph->guide(DockSide::Center).center()) + t.window->position;
    screen.moveTo(floating, graphMiddle);
    screen.release(floating, graphMiddle);
    screen.closeWindows();
    CHECK(screen.windows.size() == 1);
    CHECK(t.area->tabsOf("palette") == t.graph && t.area->tabsOf("inspector") == t.graph && t.graph->count() == 3);
}

TEST(floatButtonFloatsAndDocksBack) {
    Screen screen;
    Tools t(screen, {0, 0});
    // the float button: last on the palette's tab bar
    auto floatButton = [&](DockTabs& tabs) -> Point {
        const Widget& tools = *tabs.children()[1];
        const Widget& button = *tools.children().back();
        return button.mapToRoot({12, 12}) + screen.windowPosition(*tabs.root());
    };
    screen.layout();
    const Point p = floatButton(*t.area->tabsOf("palette"));
    screen.press(*t.window, p);
    screen.release(*t.window, p);
    screen.closeWindows();
    CHECK(screen.windows.size() == 2 && t.area->tabsOf("palette")->area()->floating());

    // in its floating window, the same button docks it back
    screen.layout();
    Screen::Win& floating = *screen.windows.front();
    const Point back = floatButton(*t.area->tabsOf("palette"));
    screen.press(floating, back);
    screen.release(floating, back);
    screen.closeWindows();
    CHECK(screen.windows.size() == 1 && t.area->tabsOf("palette")->area() == t.area);
}
