// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// How long the widget layer takes for what an interface does often, with many widgets: laid out the first time, painted,
// scrolled, one text changed. No window and no GPU: a root, its draw list, as an engine would have them.
//
//   glw_benchmarks [--count N]...     (1000, 10000 and 100000 labels by default)
#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Canvas.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <compare>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// What it takes to run `f`, in milliseconds: the best of `runs` (the others are the machine's noise).
template <class F>
double best(int runs, F&& f) {
    double fastest = 1e30;
    for (int i = 0; i < runs; ++i) {
        const auto start = Clock::now();
        f();
        fastest = std::min(fastest, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    }
    return fastest;
}

double once(auto&& f) {
    return best(1, f);
}

void labelsInAScrollArea(glw::UiContext& ui, int count) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto list = std::make_unique<glw::Widget>();
    list->setLayout(std::make_unique<glw::StackLayout>(glw::Orientation::Vertical, 2.0f));
    std::vector<glw::Label*> labels;
    const double build = once([&] {
        for (int i = 0; i < count; ++i)
            labels.push_back(&list->add(std::make_unique<glw::Label>("Item " + std::to_string(i))));
    });
    auto scroll = std::make_unique<glw::ScrollArea>();
    glw::ScrollArea& area = *scroll;
    scroll->setContent(std::move(list));
    root.setContent(std::move(scroll));

    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    const std::size_t vertices = drawn.vertices.size();
    // a step of the wheel: moved, laid out (as a frame does when it must), painted
    float at = 0;
    const double scroll1 = best(5, [&] {
        at += 40;
        area.setOffset({0, at});
        root.paint(drawn);
    });
    // one label's text changed (in view): laid out and painted again
    int n = 0;
    const double text = best(5, [&] {
        labels[static_cast<std::size_t>(at / 30)]->setText("Changed " + std::to_string(++n));
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.1f | %8.2f | %8.2f | %8.2f | %8zu\n", count, build, layout, paint, scroll1, text, vertices);
}

// Items made when asked: a model of any size costs nothing.
class Numbers final : public glw::ListModel {
public:
    explicit Numbers(std::size_t count) : count_(count) {}
    std::size_t count() const override { return count_; }
    void changed(std::size_t index) { notifyChanged(index, 1); }

private:
    std::size_t count_;
};

void itemsInAListView(glw::UiContext& ui, std::size_t count, bool uniform) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto numbers = std::make_shared<Numbers>(count);
    glw::ListView* list = nullptr;
    const double build = once([&] {
        auto made = std::make_unique<glw::ListView>();
        list = made.get();
        list->setModel(numbers);
        list->setDelegate(std::make_unique<glw::TextDelegate>([](std::size_t i) { return glw::LocalizedText("Item " + std::to_string(i)); }));
        if (!uniform)
            list->setRowHeight(0.0f);
        root.setContent(std::move(made));
    });
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 120;
        list->setOffset(at);
        root.paint(drawn);
    });
    // far: half way down
    const double jump = best(5, [&] {
        at = list->maximumOffset() / 2 + (at > list->maximumOffset() / 2 ? -1000 : 1000);
        list->setOffset(at);
        root.paint(drawn);
    });
    const double text = best(5, [&] {
        numbers->changed(list->shownItems().first);
        root.paint(drawn);
    });
    std::printf("%9zu | %-8s | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8zu\n", count, uniform ? "uniform" : "measured", build,
                layout, paint, scroll, jump, text, list->children().size());
}

// A thousand folders of a thousand files, made when asked.
class Folders final : public glw::TreeModel {
public:
    std::size_t childCount(std::optional<glw::TreeNode> parent) const override {
        return !parent ? 1000 : parent->id < 1000 ? 1000 : 0;
    }
    glw::TreeNode child(std::optional<glw::TreeNode> parent, std::size_t index) const override {
        return {!parent ? index : 1000 + parent->id * 1000 + index};
    }
    bool hasChildren(glw::TreeNode node) const override { return node.id < 1000; }
};

void nodesInATreeView(glw::UiContext& ui) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto made = std::make_unique<glw::TreeView>();
    glw::TreeView& tree = *made;
    tree.setModel(std::make_shared<Folders>());
    tree.setDelegate(std::make_unique<glw::TreeTextDelegate>([](glw::TreeNode n) { return glw::LocalizedText(std::to_string(n.id)); }));
    root.setContent(std::move(made));
    root.layout();
    glw::paint::DrawList drawn;
    const double expandAll = once([&] {
        tree.expandAll();
        root.paint(drawn);
    });
    const double paint = best(5, [&] { root.paint(drawn); });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 120;
        tree.list().setOffset(at);
        root.paint(drawn);
    });
    // a folder near the top collapsed and expanded again, a million rows after it
    bool open = true;
    const double toggle = best(6, [&] {
        open = !open;
        tree.setExpanded({1}, open);
        root.paint(drawn);
    });
    // the same, eased, the folder at the top (its thousand children and the million rows after them moving, its arrow
    // turning): every frame of it, 16 ms apart
    tree.list().setOffset(0);
    root.paint(drawn);
    glw::TimePoint now = Clock::now();
    glw::Scheduler scheduler([&now] { return now; });
    ui.scheduler = &scheduler;
    double total = 0, worst = 0;
    int frames = 0;
    bool first = true;
    for (int k = 0; k < 6; ++k) {
        first = !first;
        tree.setExpanded({0}, first);
        do {
            const double frame = once([&] {
                scheduler.tick();
                root.paint(drawn);
            });
            total += frame;
            worst = std::max(worst, frame);
            ++frames;
            now += std::chrono::milliseconds(16);
        } while (scheduler.animating());
    }
    ui.scheduler = nullptr;
    std::printf("%9zu | %10.1f | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %6d | %8zu\n", tree.shownCount(), expandAll, paint, scroll, toggle,
                total / frames, worst, frames / 6, tree.list().children().size());
}

// A table of rows x columns, made when asked: no memory per cell.
class Cells final : public glw::TableModel {
public:
    Cells(std::size_t rows, std::size_t columns) : rows_(rows), columns_(columns) {}
    std::size_t rowCount() const override { return rows_; }
    std::size_t columnCount() const override { return columns_; }
    glw::LocalizedText header(std::size_t column) const override { return glw::LocalizedText("Column " + std::to_string(column)); }
    // numbers that look random, to sort by
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(std::size_t a, std::size_t b, std::size_t column) const override {
        return value(a, column) <=> value(b, column);
    }
    static std::size_t value(std::size_t row, std::size_t column) { return (row * 7919 + column * 104729) % 100003; }

private:
    std::size_t rows_, columns_;
};

std::size_t widgetsIn(const glw::Widget& widget) {
    std::size_t count = 1;
    for (const auto& child : widget.children())
        count += widgetsIn(*child);
    return count;
}

void cellsInATableView(glw::UiContext& ui, std::size_t rows, std::size_t columns) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    const double build = once([&] {
        auto made = std::make_unique<glw::TableView>();
        made->setModel(std::make_shared<Cells>(rows, columns));
        made->setDelegate(std::make_unique<glw::TableTextDelegate>([](std::size_t r, std::size_t c) {
            return glw::LocalizedText(std::to_string(r) + ", " + std::to_string(c));
        }));
        root.setContent(std::move(made));
    });
    auto& table = static_cast<glw::TableView&>(*root.content());
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    double down = 0, across = 0;
    const double scroll = best(5, [&] {
        down += 120;
        table.list().setOffset(down);
        root.paint(drawn);
    });
    const double scrollAcross = best(5, [&] {
        across += 90;
        table.setHorizontalOffset(across);
        root.paint(drawn);
    });
    const double jump = best(5, [&] {
        down += 1e6;
        across += 1500;
        table.list().setOffset(down);
        table.setHorizontalOffset(across);
        root.paint(drawn);
    });
    // sorted by a column, and a filter (half the rows), each with its first paint; a million rows only
    double sort = 0, filter = 0;
    if (rows <= 1'000'000) {
        sort = once([&] {
            table.sortBy(1);
            root.paint(drawn);
        });
        filter = once([&] {
            table.setFilter([](std::size_t row) { return Cells::value(row, 2) % 2 == 0; });
            root.paint(drawn);
        });
        table.setFilter({});
        table.sortBy(std::nullopt);
    }
    std::printf("%9zu | %7zu | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8.1f | %8.1f | %8zu\n", rows, columns, build, layout, paint,
                scroll, scrollAcross, jump, sort, filter, widgetsIn(table));
}

// A live feed: prices that change all the time, rows coming and going, sorted by price.
class Ticks final : public glw::TableModel {
public:
    explicit Ticks(std::size_t rows) : prices(rows) {
        for (std::size_t i = 0; i < rows; ++i)
            prices[i] = next();
    }
    std::size_t rowCount() const override { return prices.size(); }
    std::size_t columnCount() const override { return 3; }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(std::size_t a, std::size_t b, std::size_t) const override { return prices[a] <=> prices[b]; }

    void change(std::size_t row) {
        prices[row] = next();
        notifyRowsChanged(row, 1);
    }
    void insert(std::size_t at) {
        prices.insert(prices.begin() + static_cast<std::ptrdiff_t>(at), next());
        notifyRowsInserted(at, 1);
    }
    void remove(std::size_t at) {
        prices.erase(prices.begin() + static_cast<std::ptrdiff_t>(at));
        notifyRowsRemoved(at, 1);
    }
    std::size_t any() { return static_cast<std::size_t>(random_() % prices.size()); }
    std::deque<double> prices;      // inserted and erased at the ends in no time: what is timed is the table's

private:
    double next() { return static_cast<double>(random_() % 1'000'000) / 100; }
    std::uint64_t random_() { return seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull, seed_ >> 33; }
    std::uint64_t seed_ = 1;
};

// The feed in a table, sorted, a thousand of its changes at a time (then a frame painted): what each costs, in microseconds.
// A column fitted to its cells (a double click on its header's edge): the rows that show and a thousand across them, or
// every row.
void aColumnFittedInATableView(glw::UiContext& ui, std::size_t rows) {
    glw::UiRoot root(ui);
    root.setSize({1200, 800});
    auto made = std::make_unique<glw::TableView>();
    glw::TableView& table = *made;
    table.setModel(std::make_shared<Cells>(rows, 10));
    table.setDelegate(std::make_unique<glw::TableTextDelegate>(
        [](std::size_t row, std::size_t column) { return glw::LocalizedText(std::to_string(Cells::value(row, column) * row)); }));
    root.setContent(std::move(made));
    root.layout();
    float width = 0;
    const double sampled = best(5, [&] { width = table.contentWidth(1); });
    table.setFitRows(std::nullopt);
    float every = 0;
    const double all = once([&] { every = table.contentWidth(1); });
    std::printf("%9zu | %8.2f | %10.1f | %7.0f | %6.0f\n", rows, sampled, all, width, every);
}

void aLiveFeedInATableView(glw::UiContext& ui, std::size_t rows) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto ticks = std::make_shared<Ticks>(rows);
    auto made = std::make_unique<glw::TableView>();
    glw::TableView& table = *made;
    table.setModel(ticks);
    table.setDelegate(std::make_unique<glw::TableTextDelegate>([t = ticks.get()](std::size_t r, std::size_t) {
        return glw::LocalizedText(std::to_string(t->prices[r]));
    }));
    root.setContent(std::move(made));
    root.layout();
    glw::paint::DrawList drawn;
    const double sort = once([&] {
        table.sortBy(0);
        root.paint(drawn);
    });
    const auto each = [&](int n, auto&& op) {
        return once([&] {
            for (int i = 0; i < n; ++i)
                op();
            root.paint(drawn);
        }) * 1000.0 / n;
    };
    const double changed = each(1000, [&] { ticks->change(ticks->any()); });
    const double appended = each(1000, [&] { ticks->insert(ticks->prices.size()); });
    const double insertedFirst = each(1000, [&] { ticks->insert(0); });
    const double removed = each(1000, [&] { ticks->remove(0); });          // the oldest: a window that moves on
    // filtered too: a price under 5000
    table.setFilter([t = ticks.get()](std::size_t r) { return t->prices[r] < 5000; });
    const double changedFiltered = each(1000, [&] { ticks->change(ticks->any()); });
    std::printf("%9zu | %8.1f | %8.1f | %8.1f | %8.1f | %8.1f | %8.1f\n", rows, sort, changed, appended, insertedFirst, removed,
                changedFiltered);
}

// An order book's orders, as a table model: in a vector in no order (the view sorts them by price, then time), an order
// removed by moving the last one into its place, so that a change costs the model nothing either.
class Orders final : public glw::TableModel {
public:
    struct Order {
        double price;
        std::uint64_t time;
    };
    explicit Orders(std::size_t count) {
        for (std::size_t i = 0; i < count; ++i)
            orders.push_back(make());
    }
    std::size_t rowCount() const override { return orders.size(); }
    std::size_t columnCount() const override { return 3; }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(std::size_t a, std::size_t b, std::size_t) const override {
        if (const auto o = orders[a].price <=> orders[b].price; o != 0)
            return o;
        return orders[a].time <=> orders[b].time;
    }

    void add() {
        orders.push_back(make());
        notifyRowsInserted(orders.size() - 1, 1);
    }
    void remove(std::size_t at) {
        // the last one into its place: it changed there, and the last place went
        const std::size_t last = orders.size() - 1;
        if (at != last) {
            orders[at] = orders[last];
            notifyRowsChanged(at, 1);
        }
        orders.pop_back();
        notifyRowsRemoved(last, 1);
    }
    void reprice(std::size_t at) {
        orders[at] = make();
        notifyRowsChanged(at, 1);
    }
    std::size_t any() { return static_cast<std::size_t>(random_() % orders.size()); }
    std::vector<Order> orders;

private:
    Order make() { return {static_cast<double>(random_() % 20000) / 100, ++time_}; }
    std::uint64_t random_() { return seed_ = seed_ * 6364136223846793005ull + 1442695040888963407ull, seed_ >> 33; }
    std::uint64_t seed_ = 3;
    std::uint64_t time_ = 0;
};

// What came since the last frame: a thousand orders gone, a thousand new, two thousand repriced; then the frame.
void anOrderBookInATableView(glw::UiContext& ui, std::size_t count) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto orders = std::make_shared<Orders>(count);
    auto made = std::make_unique<glw::TableView>();
    glw::TableView& table = *made;
    table.setModel(orders);
    table.setDelegate(std::make_unique<glw::TableTextDelegate>([o = orders.get()](std::size_t r, std::size_t) {
        return glw::LocalizedText(std::to_string(o->orders[r].price));
    }));
    root.setContent(std::move(made));
    table.sortBy(0);
    glw::paint::DrawList drawn;
    root.paint(drawn);
    const auto changes = [&] {
        for (int i = 0; i < 1000; ++i)
            orders->remove(orders->any());
        for (int i = 0; i < 1000; ++i)
            orders->add();
        for (int i = 0; i < 2000; ++i)
            orders->reprice(orders->any());
    };
    const double each = best(3, [&] {
        changes();
        root.paint(drawn);
    });
    // told as a batch: placed once, at its end
    const double batch = best(3, [&] {
        orders->beginChanges();
        changes();
        orders->endChanges();
        root.paint(drawn);
    });
    // the same told as one reset: all of it sorted again
    const double reset = best(3, [&] {
        table.reorder();
        root.paint(drawn);
    });
    std::printf("%9zu | %8.2f | %8.2f | %8.2f\n", count, each, batch, reset);
}

// Nodes of a canvas: a box with a title and two lines each, `count` of them in a grid.
void nodesOnACanvas(glw::UiContext& ui, int count) {
    glw::UiRoot root(ui);
    root.setSize({1280, 720});
    auto made = std::make_unique<glw::Canvas>();
    glw::Canvas& canvas = *made;
    const int columns = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(count))));
    const double build = once([&] {
        for (int i = 0; i < count; ++i) {
            auto node = std::make_unique<glw::Widget>();
            node->setLayout(std::make_unique<glw::StackLayout>(glw::Orientation::Vertical, 4.0f));
            node->setPadding(glw::Insets::all(8));
            node->add(std::make_unique<glw::Label>("Node " + std::to_string(i)));
            node->add(std::make_unique<glw::Label>("A value"));
            node->add(std::make_unique<glw::Label>("Another value"));
            canvas.addItem(std::move(node), {static_cast<float>(i % columns) * 220.0f, static_cast<float>(i / columns) * 140.0f});
        }
    });
    root.setContent(std::move(made));
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    float x = 0;
    const double pan = best(5, [&] {
        x -= 60;
        canvas.setOrigin({x, 0});
        root.paint(drawn);
    });
    // all of them in view: as far out as it zooms
    canvas.setZoomRange(0.02f, 4);
    canvas.frameItems();
    root.paint(drawn);
    const double all = best(5, [&] { root.paint(drawn); });
    // a step of the wheel each time, in and out again
    int step = 0;
    canvas.setZoom(1);
    const double zoom = best(6, [&] {
        canvas.setZoom(step++ % 2 == 0 ? 1.15f : 1.0f, glw::Point{640, 360});
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.1f | %8.2f | %8.2f | %8.2f | %8.1f | %8zu\n", count, build, layout, paint, pan, zoom, all,
                drawn.vertices.size());
}

// A line chart of `series` series of `count` points each (a noisy signal, x sorted): filled, painted all of it, zoomed to
// a tenth, then live (a thousand points appended to each, a frame).
void pointsInALineChart(glw::UiContext& ui, std::size_t count, int series) {
    glw::UiRoot root(ui);
    root.setSize({1280, 720});
    auto made = std::make_unique<glw::LineChart>();
    glw::LineChart& chart = *made;
    root.setContent(std::move(made));
    std::vector<glw::LineSeries*> lines;
    for (int i = 0; i < series; ++i)
        lines.push_back(&chart.addSeries({.name = "s" + std::to_string(i), .area = i == 0, .capacity = count}));
    std::mt19937 random(1);
    std::normal_distribution<double> noise(0, 1);
    double x = 0;
    const double fill = once([&] {
        for (std::size_t k = 0; k < count; ++k, ++x)
            for (int i = 0; i < series; ++i)
                lines[i]->append(x, std::sin(x * 1e-4 + i) * 10 + noise(random));
    });
    glw::paint::DrawList drawn;
    root.paint(drawn);
    const double all = best(5, [&] { root.paint(drawn); });
    const std::size_t vertices = drawn.vertices.size();
    chart.setXView({x * 0.45, x * 0.55});
    root.paint(drawn);
    const double tenth = best(5, [&] { root.paint(drawn); });
    chart.resetView();
    root.paint(drawn);
    const double live = best(5, [&] {
        for (int k = 0; k < 1000; ++k, ++x)
            for (int i = 0; i < series; ++i)
                lines[i]->append(x, std::sin(x * 1e-4 + i) * 10 + noise(random));
        root.paint(drawn);
    });
    std::printf("%9zu | %6d | %8.1f | %8.2f | %8.2f | %8.2f | %8zu\n", count, series, fill, all, tenth, live, vertices);
}

// A bar chart of `categories` categories of 5 series, stacked; a treemap of `nodes` leaves in folders of 20.
void barsAndTiles(glw::UiContext& ui, std::size_t categories, std::size_t nodes) {
    glw::paint::DrawList drawn;
    double bars = 0, appended = 0, tiles = 0, retiled = 0;
    {
        glw::UiRoot root(ui);
        root.setSize({1280, 720});
        auto made = std::make_unique<glw::BarChart>(glw::BarChart::Props{.capacity = categories, .window = 200});
        glw::BarChart& chart = *made;
        root.setContent(std::move(made));
        for (int i = 0; i < 5; ++i)
            chart.addSeries("s" + std::to_string(i));
        const double values[] = {3, 5, 2, 7, 1};
        for (std::size_t c = 0; c < categories; ++c)
            chart.appendCategory(std::to_string(c), values);
        root.paint(drawn);
        bars = best(5, [&] { root.paint(drawn); });
        appended = best(5, [&] {
            chart.appendCategory("new", values);
            root.paint(drawn);
        });
    }
    {
        glw::UiRoot root(ui);
        root.setSize({1280, 720});
        auto made = std::make_unique<glw::TreeMap>();
        glw::TreeMap& chart = *made;
        root.setContent(std::move(made));
        glw::TreeMapNode tree{.label = "root"};
        for (std::size_t i = 0; i < nodes; ++i) {
            if (i % 20 == 0)
                tree.children.push_back({.label = "folder " + std::to_string(i / 20)});
            tree.children.back().children.push_back({.label = "file " + std::to_string(i), .value = 1.0 + static_cast<double>(i % 37)});
        }
        chart.setRoot(tree);
        root.paint(drawn);
        tiles = best(5, [&] { root.paint(drawn); });
        retiled = best(5, [&] {
            tree.children[3].children[2].value += 5;
            chart.setRoot(tree);
            root.paint(drawn);
        });
    }
    std::printf("%10zu | %8.2f | %8.2f | %8zu | %8.2f | %8.2f\n", categories, bars, appended, nodes, tiles, retiled);
}

// A text of `count` lines of C++, highlighted.
void linesInATextEdit(glw::UiContext& ui, int count) {
    std::string code;
    for (int i = 0; i < count; ++i)
        code += "    int value" + std::to_string(i) + " = compute(" + std::to_string(i) + ", \"text\"); // a comment\n";
    glw::UiRoot root(ui);
    root.setSize({1000, 700});
    auto made = std::make_unique<glw::TextEdit>();
    glw::TextEdit& edit = *made;
    edit.setHighlighter(std::make_unique<glw::CppHighlighter>());
    root.setContent(std::move(made));
    const double load = once([&] { edit.setText(code); });
    glw::paint::DrawList drawn;
    const double first = once([&] { root.paint(drawn); });
    const double paint = best(5, [&] {
        root.invalidatePaint();
        root.paint(drawn);
    });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 600;
        edit.setVerticalOffset(at);
        root.paint(drawn);
    });
    // typing in the middle: the line laid out and colored again, those after it colored again as they show
    edit.select({static_cast<std::size_t>(count / 2), 10}, {static_cast<std::size_t>(count / 2), 10});
    root.paint(drawn);
    const double type = best(5, [&] {
        edit.replace(edit.selection(), "x");
        root.paint(drawn);
    });
    // to the end: highlighted from where it was known to there, once
    const double jump = once([&] {
        edit.select({static_cast<std::size_t>(count), 0}, {static_cast<std::size_t>(count), 0});
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.2f | %8.2f | %8.2f | %8.2f | %8.1f | %8zu\n", count, load, first, paint, scroll, type, jump,
                edit.laidOutLines());
}

} // namespace

int main(int argc, char** argv) {
    std::vector<int> counts;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--count" && i + 1 < argc)
            counts.push_back(std::max(1, std::atoi(argv[++i])));
    }
    if (counts.empty())
        counts = {1000, 10000, 100000};

    glw::text::FontLibrary fonts;
    glw::text::GlyphAtlas atlas(1, 1024);
    glw::Translator translator;
    glw::UiContext ui{fonts, atlas, translator, glw::Theme::dark(glw::ThemeStyle::Studio)};

    std::printf("Labels in a scroll area, milliseconds (paint and after: the best of 5)\n");
    std::printf("   labels |    build |   layout |    paint |   scroll |     text | vertices\n");
    for (int count : counts)
        labelsInAScrollArea(ui, count);

    std::printf("\nItems in a list view (a widget for each that shows), milliseconds\n");
    std::printf("    items | rows     |    build |   layout |    paint |   scroll |     jump |     text |  widgets\n");
    for (std::size_t count : {std::size_t{1'000'000}, std::size_t{100'000'000}}) {
        itemsInAListView(ui, count, true);
        if (count <= 1'000'000)
            itemsInAListView(ui, count, false);
    }

    std::printf("\nA tree view, all expanded, milliseconds\n");
    std::printf("    nodes | expand all |    paint |   scroll |   toggle |    eased |    worst | frames |  widgets\n");
    nodesInATreeView(ui);

    std::printf("\nA table view (a widget for each cell that shows), milliseconds\n");
    std::printf("     rows | columns |    build |   layout |    paint |   scroll |   across |     jump |     sort |   filter |  widgets\n");
    cellsInATableView(ui, 1'000'000, 100);
    cellsInATableView(ui, 100'000'000, 1000);

    std::printf("\nA column fitted to its cells in a table view: a thousand rows and those that show, or all; milliseconds, "
                "then the widths\n");
    std::printf("     rows |  sampled |        all | sampled |    all\n");
    for (std::size_t count : {std::size_t{10'000}, std::size_t{100'000}, std::size_t{1'000'000}})
        aColumnFittedInATableView(ui, count);

    std::printf("\nA live feed in a table view, sorted by price: sorting in milliseconds, then each change in microseconds\n");
    std::printf("     rows |     sort |   change |   append | at first |   oldest | filtered\n");
    aLiveFeedInATableView(ui, 1'000'000);

    std::printf("\nAn order book in a table view, sorted by price then time: 1000 removed, 1000 added, 2000 repriced, then a frame;\n"
                "or all of it sorted again; milliseconds\n");
    std::printf("   orders |     each |    batch | sort all\n");
    for (std::size_t count : {std::size_t{10'000}, std::size_t{100'000}, std::size_t{1'000'000}})
        anOrderBookInATableView(ui, count);

    std::printf("\nNodes on a canvas, milliseconds (all: every one in view, zoomed out)\n");
    std::printf("    nodes |    build |   layout |    paint |      pan |     zoom |      all | vertices\n");
    for (int count : {1000, 10000})
        nodesOnACanvas(ui, count);

    std::printf("\nPoints in a line chart, milliseconds: filled, painted all, a tenth of it, live (1000 more each, a frame)\n");
    std::printf("   points | series |     fill |      all |    tenth |     live | vertices\n");
    pointsInALineChart(ui, 1'000'000, 1);
    pointsInALineChart(ui, 1'000'000, 4);
    pointsInALineChart(ui, 10'000'000, 1);

    std::printf("\nBars and tiles, milliseconds: a bar chart's last 200 painted, one appended and painted; a treemap painted, a value changed\n");
    std::printf("categories |     bars | appended |    nodes |    tiles |  retiled\n");
    barsAndTiles(ui, 10'000, 10'000);

    std::printf("\nLines of C++ in a text edit, highlighted, milliseconds\n");
    std::printf("    lines |     load |    first |    paint |   scroll |     type | to the end |     laid\n");
    for (int count : {10'000, 1'000'000})
        linesInATextEdit(ui, count);
    return 0;
}
