// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Charts: ticks at round values and round times, a series's points kept in chunks (what shows found by halves, the
// extents of a million points quick), line charts fitted, following, zoomed and panned and drawn from what shows; pies,
// treemaps (squarified, zoomed into) and bars (stacked, grouped, in percent, appended live).
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/PieChart.hpp>
#include <GLWidgets/Charts/SeriesBuffer.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <random>

using namespace glw;

namespace {

bool near(double a, double b, double within = 1e-6) {
    return std::abs(a - b) <= within;
}

// A chart alone in a root 600 x 400, painted (laid out) once.
template <class T>
struct Shown {
    UiRoot root{tests::testContext()};
    T* chart = nullptr;

    template <class... Args>
    explicit Shown(Args&&... args) {
        auto made = std::make_unique<T>(std::forward<Args>(args)...);
        chart = made.get();
        root.setSize({600, 400});
        root.setContent(std::move(made));
        paint();
    }
    std::size_t paint() {
        paint::DrawList list;
        root.paint(list);
        return list.vertices.size();
    }
    void button(platform::MouseButton b, bool down, Point at, int clicks = 1) {
        root.dispatch(platform::MouseButtonChanged{1, b, down, clicks, at});
    }
    void move(Point to) { root.dispatch(platform::MouseMoved{1, to}); }
    void wheel(Point at, float steps) { root.dispatch(platform::MouseWheelScrolled{1, {0, steps}, at}); }
    void drag(Point from, Point to, platform::MouseButton b = platform::MouseButton::Left) {
        move(from);
        button(b, true, from);
        move({(from.x + to.x) / 2, (from.y + to.y) / 2});
        move(to);
        button(b, false, to);
        paint();
    }
};

} // namespace

// ---- axes ----

TEST(ticksFallOnRoundValues) {
    const ChartTicks t = niceTicks({0, 97}, 6);
    CHECK(t.step == 20 && t.values.size() == 5 && t.values.front() == 0 && t.values.back() == 80 && t.decimals == 0);
    const ChartTicks f = niceTicks({0.013, 0.071}, 4);
    CHECK(near(f.step, 0.02) && f.decimals == 2 && near(f.values.front(), 0.02));
    // a fitted axis ends on its ticks
    const ChartRange r = niceRange({3.2, 96.5}, 6);
    CHECK(r.low == 0 && r.high == 100);
    CHECK(niceRange({-7, 42}, 5).low == -20 && niceRange({-7, 42}, 5).high == 60);
    // a range of no width: one tick, no crash
    CHECK(niceTicks({5, 5}, 6).values.size() == 1);
}

TEST(numbersShowAsTicksDo) {
    CHECK(formatNumber(1234567.0, 0) == "1.2M" || formatNumber(1234567.0, 0) == "1M");
    CHECK(formatNumber(12345, 0) == "12,345");
    CHECK(formatNumber(-0.0001, 2) == "0.00");
    CHECK(formatNumber(2.5, 1) == "2.5");
    CHECK(formatNumber(15000, 0, 5000) == "15k");
    CHECK(formatNumber(2'500'000, 0, 500'000) == "2.5M");
}

TEST(timeTicksFallOnRoundLocalTimes) {
    // a minute and a half: every 15 seconds, at whole quarters of a minute of local time
    const double start = 1'762'400'000.3;       // 2025-11-06, some time in it
    const TimeTicks t = timeTicks({start, start + 90}, 8);
    CHECK(t.unit == TimeUnit::Second && t.step == 15 && t.values.size() >= 5);
    for (double v : t.values) {
        const std::time_t whole = static_cast<std::time_t>(v);
        std::tm l{};
#ifdef _WIN32
        localtime_s(&l, &whole);
#else
        localtime_r(&whole, &l);
#endif
        CHECK(l.tm_sec % 15 == 0 && near(v, std::floor(v)));
    }
    const std::string label = formatTime(t.values[0], t);
    CHECK(label.size() == 8 && label[2] == ':' && label[5] == ':');
    // a second: every 0.2 s, labels with a decimal
    const TimeTicks ms = timeTicks({start, start + 1}, 6);
    CHECK(ms.unit == TimeUnit::Millisecond && near(ms.step, 0.2));
    CHECK(formatTime(ms.values[0], ms).find('.') == 8);
    // a few days: days, labelled by the day
    const TimeTicks days = timeTicks({start, start + 6 * 86400.0}, 8);
    CHECK(days.unit == TimeUnit::Day && formatTime(days.values[0], days).find(' ') != std::string::npos);
    // two years: months, a year's first shows the year
    const TimeTicks months = timeTicks({start, start + 730 * 86400.0}, 10);
    CHECK(months.unit == TimeUnit::Month && months.step >= 2);
    CHECK(formatTimeFully(start, TimeUnit::Second, true).size() == 19);
}

// ---- a series's points ----

TEST(aSeriesBufferFindsWhatShowsByHalves) {
    SeriesBuffer b;
    for (int i = 0; i < 10'000; ++i)
        b.append(i * 0.5, std::sin(i * 0.01) * 100);
    CHECK(b.size() == 10'000 && b.sorted());
    CHECK(b.lowerBound(-1) == 0 && b.lowerBound(1000) == 2000 && b.lowerBound(1000.1) == 2001 && b.lowerBound(1e9) == 10'000);
    // the extents of any range, as a scan finds them
    std::mt19937 random(7);
    for (int k = 0; k < 200; ++k) {
        std::size_t a = random() % 10'000, c = random() % 10'000;
        if (a > c)
            std::swap(a, c);
        ++c;
        double low = 1e300, high = -1e300;
        for (std::size_t i = a; i < c; ++i) {
            low = std::min(low, b.y(i));
            high = std::max(high, b.y(i));
        }
        const auto e = b.yExtent(a, c);
        CHECK(e && e->low == low && e->high == high);
    }
    CHECK(b.xExtent()->low == 0 && b.xExtent()->high == 4999.5);
}

TEST(aSeriesBufferKeepsItsNewestAndSkipsGaps) {
    SeriesBuffer b(1000);
    for (int i = 0; i < 5000; ++i)
        b.append(i, i == 4500 ? NAN : static_cast<double>(i % 777));
    CHECK(b.size() == 1000 && b.x(0) == 4000 && b.x(999) == 4999);
    CHECK(b.lowerBound(4000) == 0 && b.lowerBound(4500) == 500);
    // the oldest dropped are in no extent; a gap neither
    const auto e = b.yExtent(0, 10);
    CHECK(e && e->low == 4000 % 777 && e->high == 4009 % 777);
    CHECK(!b.yExtent(500, 501));
    CHECK(b.yExtent()->high == 776 && b.yExtent()->low == 0);
    // x going down: no longer sorted, still extents
    b.append(10, 5);
    CHECK(!b.sorted() && b.xExtent()->low == 10);
    b.clear();
    CHECK(b.empty() && b.sorted() && !b.yExtent());
}

// ---- line charts ----

TEST(aLineChartFitsItsAxesToWhatShows) {
    Shown<LineChart> c;
    LineSeries& s = c.chart->addSeries({.name = "a"});
    for (int i = 0; i <= 100; ++i)
        s.append(i, 10 + i * 0.5);
    c.paint();
    CHECK(c.chart->xView() == (ChartRange{0, 100}));
    const ChartRange y = c.chart->yView();
    CHECK(y.low <= 10 && y.high >= 60 && y.low >= 0 && y.high <= 70);
    // with 0 in it
    c.chart->setIncludeZero(true);
    c.paint();
    CHECK(c.chart->yView().low == 0);
    // a range of its own
    c.chart->setYRange(ChartRange{-5, 5});
    c.paint();
    CHECK(c.chart->yView() == (ChartRange{-5, 5}));
    // a hidden series is out of the fit
    c.chart->setYRange(std::nullopt);
    LineSeries& big = c.chart->addSeries({.name = "b"});
    big.append(50, 1000);
    c.paint();
    CHECK(c.chart->yView().high >= 1000);
    big.setVisible(false);
    c.paint();
    CHECK(c.chart->yView().high < 100);
}

TEST(aLiveLineChartFollowsWhatComes) {
    Shown<LineChart> c(LineChart::Props{.xAxis = ChartAxisKind::Time, .window = 60});
    LineSeries& s = c.chart->addSeries({.name = "load", .capacity = 10'000});
    const double t0 = 1'762'400'000;
    for (int i = 0; i < 600; ++i)
        s.append(t0 + i * 0.5, i % 50);
    c.paint();
    CHECK(c.chart->following() && near(c.chart->xView().high, t0 + 299.5) && near(c.chart->xView().span(), 60));
    s.append(t0 + 400, 3);
    c.paint();
    CHECK(near(c.chart->xView().high, t0 + 400));
    // the wheel zooms its window: still following
    const Rect plot = c.chart->plotArea();
    c.wheel(plot.center(), 1);
    c.paint();
    CHECK(c.chart->following() && c.chart->xView().span() < 60 && near(c.chart->xView().high, t0 + 400));
    // a drag pans back into its history: no longer following
    const double span = c.chart->xView().span();
    c.drag(plot.center(), {plot.center().x + plot.width / 4, plot.center().y});
    CHECK(!c.chart->following() && near(c.chart->xView().span(), span, 1e-3) && c.chart->xView().high < t0 + 400 - span / 5);
    s.append(t0 + 401, 4);
    c.paint();
    CHECK(c.chart->xView().high < t0 + 400);
    // dragged back to the newest, it follows again
    c.drag(plot.center(), {plot.center().x - plot.width, plot.center().y});
    CHECK(c.chart->following());
    s.append(t0 + 410, 4);
    c.paint();
    CHECK(near(c.chart->xView().high, t0 + 410));
    // a double click: what it showed first
    c.button(platform::MouseButton::Left, true, plot.center(), 2);
    c.button(platform::MouseButton::Left, false, plot.center(), 2);
    c.paint();
    CHECK(c.chart->following() && near(c.chart->xView().span(), 60));
}

TEST(aLineChartZoomsAtTheMouseAndIntoARectangle) {
    Shown<LineChart> c;
    LineSeries& s = c.chart->addSeries({.name = "a"});
    for (int i = 0; i <= 1000; ++i)
        s.append(i, i % 100);
    c.paint();
    const Rect plot = c.chart->plotArea();
    // the value under the mouse stays under it
    const Point at{plot.x + plot.width * 0.25f, plot.center().y};
    const double before = c.chart->valueAt(at).x;
    c.wheel(at, 2);
    c.paint();
    CHECK(c.chart->xView().span() < 1000 * 0.8 && near(c.chart->valueAt(at).x, before, 1e-6 * 1000));
    // a rectangle dragged with the right button: zoomed to it, x and y
    c.chart->resetView();
    c.paint();
    const ChartPoint a = c.chart->valueAt({plot.x + 100, plot.y + 40}), b = c.chart->valueAt({plot.x + 300, plot.y + 140});
    c.drag({plot.x + 100, plot.y + 40}, {plot.x + 300, plot.y + 140}, platform::MouseButton::Right);
    CHECK(near(c.chart->xView().low, a.x, 1e-6) && near(c.chart->xView().high, b.x, 1e-6));
    CHECK(near(c.chart->yView().low, b.y, 1e-6) && near(c.chart->yView().high, a.y, 1e-6));
}

TEST(aLineChartOfAMillionPointsDrawsWhatItsPixelsShow) {
    Shown<LineChart> c;
    LineSeries& s = c.chart->addSeries({.name = "signal"});
    std::mt19937 random(3);
    std::normal_distribution<double> noise(0, 1);
    for (int i = 0; i < 1'000'000; ++i)
        s.append(i, std::sin(i * 1e-4) * 10 + noise(random));
    // about four points for each column of pixels, whatever their count
    const std::size_t vertices = c.paint();
    CHECK(vertices < 600 * 4 * 4 * 2 + 20'000);
    // the line goes through the extremes: the fitted axis holds them
    const ChartRange y = c.chart->yView();
    CHECK(y.low <= s.data().yExtent()->low && y.high >= s.data().yExtent()->high);
}

// ---- pies ----

TEST(aPieChartsSlicesAreTheirShares) {
    Shown<PieChart> c(PieChart::Props{.hole = 0.5f});
    c.chart->setSlices({{"a", 1}, {"b", 1}, {"c", 2}});
    c.paint();
    // from the top, clockwise: a (a quarter), b (a quarter), c (a half)
    const Rect r = Rect{0, 0, 600, 400};
    const Point center = r.center();
    const float radius = 150;
    const auto at = [&](float degrees) {
        const float a = degrees * 3.14159265f / 180;
        return Point{center.x + std::cos(a) * radius * 0.8f, center.y + std::sin(a) * radius * 0.8f};
    };
    CHECK(c.chart->sliceAt(at(-90 + 45)) == std::optional<std::size_t>(0));
    CHECK(c.chart->sliceAt(at(45)) == std::optional<std::size_t>(1));
    CHECK(c.chart->sliceAt(at(180)) == std::optional<std::size_t>(2));
    // the hole is no slice
    CHECK(!c.chart->sliceAt({center.x + 5, center.y}));
    // hidden: no share; a value changed: its share
    c.chart->setSliceVisible(2, false);
    c.paint();
    CHECK(c.chart->shownValue(2) == 0 && c.chart->sliceAt(at(180)) == std::optional<std::size_t>(1));
    c.chart->setSliceVisible(2, true);
    c.chart->setValue(0, 6);
    c.paint();
    CHECK(c.chart->sliceAt(at(90)) == std::optional<std::size_t>(0));
}

TEST(aPieChartEasesToNewValues) {
    tests::testScheduler().setAnimationsEnabled(true);
    {
        Shown<PieChart> c;
        c.chart->setSlices({{"a", 1}, {"b", 1}});
        tests::advance(std::chrono::seconds(2));
        c.chart->setValue(0, 3);
        CHECK(c.chart->shownValue(0) == 1);
        tests::advance(std::chrono::milliseconds(60));
        CHECK(c.chart->shownValue(0) > 1 && c.chart->shownValue(0) < 3);
        tests::advance(std::chrono::seconds(1));
        CHECK(c.chart->shownValue(0) == 3);
    }
    tests::testScheduler().setAnimationsEnabled(false);
}

// ---- treemaps ----

TEST(aTreeMapsTilesAreAsLargeAsTheirValues) {
    Shown<TreeMap> c(TreeMap::Props{.depth = 1});
    TreeMapNode root{.label = "all"};
    for (double v : {6.0, 6.0, 4.0, 3.0, 2.0, 2.0, 1.0})
        root.children.push_back({.label = "n" + std::to_string(root.children.size()), .value = v});
    c.chart->setRoot(root);
    c.paint();
    double area = 0;
    std::vector<Rect> rects;
    for (std::size_t i = 0; i < root.children.size(); ++i) {
        const auto r = c.chart->tileRect(std::vector<std::size_t>{i});
        CHECK(r.has_value());
        rects.push_back(*r);
        area += r->width * r->height;
    }
    // shares of the area as of the values (the gaps aside), side by side
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const double share = rects[i].width * rects[i].height / area;
        CHECK(near(share, root.children[i].value / 24, 0.03));
        for (std::size_t k = i + 1; k < rects.size(); ++k)
            CHECK(!rects[i].intersects(rects[k]));
        // squarified: none a sliver
        CHECK(std::max(rects[i].width, rects[i].height) / std::min(rects[i].width, rects[i].height) < 4);
    }
}

TEST(aTreeMapZoomsIntoANodeAndOut) {
    Shown<TreeMap> c;
    TreeMapNode root{.label = "disk"};
    for (int i = 0; i < 3; ++i) {
        TreeMapNode folder{.label = "folder " + std::to_string(i)};
        for (int k = 0; k < 4; ++k)
            folder.children.push_back({.label = "file " + std::to_string(k), .value = 1.0 + k});
        root.children.push_back(folder);
    }
    c.chart->setRoot(root);
    c.paint();
    // a click on a file in a folder goes into the folder
    const Rect file = *c.chart->tileRect(std::vector<std::size_t>{1, 2});
    c.button(platform::MouseButton::Left, true, file.center());
    c.button(platform::MouseButton::Left, false, file.center());
    c.paint();
    CHECK(c.chart->focus() == std::vector<std::size_t>{1});
    // its files fill it now
    const Rect big = *c.chart->tileRect(std::vector<std::size_t>{1, 3});
    CHECK(big.width * big.height > file.width * file.height * 2);
    CHECK(!c.chart->tileRect(std::vector<std::size_t>{0}));
    // a right click: up again
    c.button(platform::MouseButton::Right, true, big.center());
    c.button(platform::MouseButton::Right, false, big.center());
    c.paint();
    CHECK(c.chart->focus().empty() && c.chart->tileRect(std::vector<std::size_t>{0}));
    // new data keeps where it is, as far as it goes
    c.chart->setFocus({2});
    root.children.pop_back();
    c.chart->setRoot(root);
    CHECK(c.chart->focus().empty());
}

// ---- bars ----

TEST(aBarChartStacksGroupsAndShares) {
    Shown<BarChart> c;
    const std::size_t a = c.chart->addSeries("a"), b = c.chart->addSeries("b");
    c.chart->setCategories({"x", "y", "z"});
    c.chart->setValues(a, std::vector<double>{10, 20, 30});
    c.chart->setValues(b, std::vector<double>{5, -5, 40});
    c.paint();
    // stacked: the tallest stack, and the negative part below 0
    CHECK(c.chart->valueView().high >= 70 && c.chart->valueView().low <= -5 && c.chart->valueView().low > -50);
    CHECK(c.chart->categoryView() == (ChartRange{0, 3}));
    // the bar under a point: z's b above z's a
    const Rect plot = c.chart->plotArea();
    const float zx = plot.x + plot.width * 2.5f / 3;
    const auto top = c.chart->barAt({zx, plot.bottom() - plot.height * 0.05f - static_cast<float>(60 / c.chart->valueView().span() * plot.height)});
    CHECK(top && top->first == b && top->second == 2);
    // grouped: the tallest bar
    c.chart->setLayout(BarLayout::Grouped);
    c.paint();
    CHECK(c.chart->valueView().high >= 40 && c.chart->valueView().high < 70);
    // shares: to 100
    c.chart->setLayout(BarLayout::Percent);
    c.paint();
    CHECK(c.chart->valueView() == (ChartRange{-100, 100}));
    // a series hidden: out of the stacks and the fit
    c.chart->setLayout(BarLayout::Stacked);
    c.chart->setSeriesVisible(b, false);
    c.paint();
    CHECK(c.chart->valueView().high < 50 && c.chart->valueView().low == 0);
}

TEST(aLiveBarChartFollowsItsNewestCategories) {
    Shown<BarChart> c(BarChart::Props{.capacity = 50, .window = 10});
    c.chart->addSeries("v");
    for (int i = 0; i < 30; ++i) {
        const double v = i;
        c.chart->appendCategory(std::to_string(i), std::span(&v, 1));
    }
    c.paint();
    CHECK(c.chart->following() && c.chart->categoryView() == (ChartRange{20, 30}));
    // past its capacity, the oldest dropped: still its last ten
    for (int i = 30; i < 80; ++i) {
        const double v = i;
        c.chart->appendCategory(std::to_string(i), std::span(&v, 1));
    }
    c.paint();
    CHECK(c.chart->categoryCount() == 50 && c.chart->categoryView() == (ChartRange{40, 50}));
    CHECK(c.chart->valueView().high >= 79);
    // a drag back: its history, no longer following
    const Rect plot = c.chart->plotArea();
    c.drag(plot.center(), {plot.center().x + plot.width / 2, plot.center().y});
    CHECK(!c.chart->following() && c.chart->categoryView().high < 50);
}
