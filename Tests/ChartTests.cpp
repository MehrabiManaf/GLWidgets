// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Charts: ticks at round values and round times, a series's points kept in chunks (what shows found by halves, the
// extents of a million points quick), line charts fitted, following, zoomed and panned and drawn from what shows; pies,
// treemaps (squarified, zoomed into) and bars (stacked, grouped, in percent, appended live).
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/BarRace.hpp>
#include <GLWidgets/Charts/BoxChart.hpp>
#include <GLWidgets/Charts/CandleChart.hpp>
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Charts/FlameGraph.hpp>
#include <GLWidgets/Charts/Gauge.hpp>
#include <GLWidgets/Charts/HeatMap.hpp>
#include <GLWidgets/Charts/HistogramChart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/PieChart.hpp>
#include <GLWidgets/Charts/SankeyChart.hpp>
#include <GLWidgets/Charts/ScatterChart.hpp>
#include <GLWidgets/Charts/SeriesBuffer.hpp>
#include <GLWidgets/Charts/Sparkline.hpp>
#include <GLWidgets/Charts/TimelineChart.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <array>
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

// A slice of a value, a slice of parts.
PieSlice leaf(std::string label, double value) {
    return {.label = std::move(label), .value = value};
}
PieSlice group(std::string label, std::vector<PieSlice> parts) {
    return {.label = std::move(label), .children = std::move(parts)};
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
    // asked for two at most where a round step is about as long as the range: two still (a narrow axis)
    const ChartTicks two = niceTicks({0, 20495}, 2);
    CHECK(two.values.size() == 2 && two.step == 20000);
    CHECK(niceTicks({0, 4.1}, 2).values.size() >= 2 && niceTicks({0, 4.1}, 3).values.size() >= 2);
}

TEST(aLogarithmicAxisHasItsTicksAtRoundValuesOfDecades) {
    // three decades, room for ten: 1, 2, 5 of each
    const ChartTicks few = logTicks({1, 1000}, 10);
    CHECK(few.values.size() == 10 && few.values.front() == 1 && near(few.values[1], 2) && near(few.values[2], 5) && near(few.values.back(), 1000));
    // nine decades, room for five: every other power of ten (or every third)
    const ChartTicks many = logTicks({1, 1e9}, 5);
    CHECK(many.values.size() >= 3 && many.values.size() <= 5 && many.values.front() == 1);
    for (const double v : many.values)
        CHECK(near(std::log10(v), std::round(std::log10(v)), 1e-9));
    // room for two only: still two powers of ten, not one
    const ChartTicks two = logTicks({5, 1000}, 2);
    CHECK(two.values.size() >= 2 && near(two.values.front(), 10));
    // within a decade: round values, as along a linear axis
    CHECK(logTicks({20, 80}, 6).values.size() >= 2);
    // small values: their decimals
    CHECK(logTicks({0.001, 1}, 4).decimals == 3);
    // fitted: out to 1, 2 or 5 times a power of ten
    CHECK((niceLogRange({3, 700}) == ChartRange{2, 1000}));
    CHECK((niceLogRange({0.012, 0.9}) == ChartRange{0.01, 1}));
}

TEST(aLineChartOnALogarithmicAxisSpacesDecadesAlike) {
    Shown<LineChart> c(LineChart::Props{.logY = true});
    LineSeries& s = c.chart->addSeries({.name = "a"});
    for (int i = 0; i <= 6; ++i)
        s.append(i, std::pow(10.0, i));
    c.paint();
    // fitted to its decades, in its values
    const ChartRange y = c.chart->yView();
    CHECK(y.low <= 1 && y.low > 0 && y.high >= 1e6);
    // 1 to 10 as far as 10 to 100; a value back from where it is drawn
    const float a = c.chart->pointOf({0, 1}).y, b = c.chart->pointOf({0, 10}).y, d = c.chart->pointOf({0, 100}).y;
    CHECK(near(a - b, b - d, 0.01));
    CHECK(near(c.chart->valueAt(c.chart->pointOf({0, 3000})).y, 3000, 1e-6 * 3000));
    // stacked: linear again
    c.chart->addSeries({.name = "s", .stacked = true}).append(0, 1);
    CHECK(!c.chart->logY());
}

TEST(aLineChartsSecondAxisFitsItsOwnSeries) {
    Shown<LineChart> c;
    LineSeries& small = c.chart->addSeries({.name = "percent"});
    LineSeries& large = c.chart->addSeries({.name = "volume", .secondAxis = true});
    for (int i = 0; i <= 10; ++i) {
        small.append(i, 10 + i);
        large.append(i, 10000 + i * 1000);
    }
    c.paint();
    // each axis its own series's range
    const ChartRange y = c.chart->yView(), y2 = c.chart->y2View();
    CHECK(y.high < 100 && y2.low >= 5000 && y2.high >= 20000);
    // room for its labels at the end side: a plot narrower than one's without it
    const Rect plot = c.chart->plotArea();
    Shown<LineChart> one;
    one.chart->addSeries({.name = "percent"}).append(0, 10);
    one.paint();
    CHECK(plot.right() < one.chart->plotArea().right() - 10);
    // zoomed in y with the wheel (Ctrl): the first only
    c.root.dispatch(platform::KeyChanged{1, platform::Key::Control, platform::KeyModifiers::Control, true});
    c.root.dispatch(platform::MouseWheelScrolled{1, {0, 2}, plot.center()});
    c.paint();
    CHECK(c.chart->yView().span() < y.span() && near(c.chart->y2View().low, y2.low) && near(c.chart->y2View().high, y2.high));
}

TEST(annotationsAreDrawnWhereTheirValuesAre) {
    Shown<LineChart> c;
    LineSeries& s = c.chart->addSeries({.name = "a"});
    for (int i = 0; i <= 10; ++i)
        s.append(i, i * 10);
    const std::size_t before = c.paint();
    // a band under the data, a line and a point over it: more drawn
    const std::size_t band = c.chart->addAnnotation({.kind = ChartAnnotation::Kind::YBand, .from = 20, .to = 40, .label = "normal"});
    c.chart->addAnnotation({.kind = ChartAnnotation::Kind::XLine, .from = 5, .label = "deploy", .dashed = true});
    c.chart->addAnnotation({.kind = ChartAnnotation::Kind::Point, .from = 7, .to = 70, .label = "peak"});
    CHECK(c.chart->annotationCount() == 3);
    const std::size_t with = c.paint();
    CHECK(with > before);
    // taken off, all of them
    c.chart->removeAnnotation(band);
    CHECK(c.chart->annotationCount() == 2);
    c.chart->clearAnnotations();
    CHECK(c.chart->annotationCount() == 0 && c.paint() == before);
}

TEST(chartsShareACrosshair) {
    // two charts over the same x, one of them shown
    auto first = std::make_unique<LineChart>(), second = std::make_unique<LineChart>();
    LineChart* a = first.get();
    LineChart* b = second.get();
    for (LineChart* c : {a, b}) {
        LineSeries& s = c->addSeries({.name = "s"});
        for (int i = 0; i <= 10; ++i)
            s.append(i, i);
    }
    auto cursor = std::make_shared<ChartCursor>();
    a->setCursor(cursor);
    b->setCursor(cursor);
    CHECK(cursor.use_count() == 3);
    paint::DrawList list;
    {
        UiRoot one{tests::testContext()};
        one.setSize({300, 400});
        one.setContent(std::move(first));
        one.paint(list);
        const std::size_t alone = list.vertices.size();
        // the mouse over the other at x 4: a line and dots here at 4
        cursor->set(b, 4.0);
        CHECK(cursor->from() == b && cursor->x() == 4.0);
        one.paint(list);
        CHECK(list.vertices.size() > alone);
        // it left: none
        cursor->set(b, std::nullopt);
        one.paint(list);
        CHECK(list.vertices.size() == alone && !cursor->x());
        // a chart gone leaves it
        one.setContent(nullptr);
    }
    CHECK(cursor.use_count() == 2);         // (this one, and the other's)
    b->setCursor(nullptr);
    CHECK(cursor.use_count() == 1);
}

TEST(candlesHistogramsAndBarsShowAnnotations) {
    // each: more drawn with a line, a band and a point marked than without
    const auto marks = [](Chart& chart, double x, double y) {
        chart.addAnnotation({.kind = ChartAnnotation::Kind::XLine, .from = x, .label = "x"});
        chart.addAnnotation({.kind = ChartAnnotation::Kind::YBand, .from = y * 0.9, .to = y * 1.1, .label = "band"});
        chart.addAnnotation({.kind = ChartAnnotation::Kind::Point, .from = x, .to = y, .label = "point"});
    };
    Shown<CandleChart> candles(CandleChart::Props{.period = 60});
    for (int i = 0; i < 30; ++i)
        candles.chart->trade(i * 60.0, 100 + i, 1);
    const std::size_t c0 = candles.paint();
    marks(*candles.chart, 15 * 60.0, 115);
    CHECK(candles.paint() > c0);

    Shown<HistogramChart> histogram(HistogramChart::Props{.bins = 10, .range = ChartRange{0, 100}});
    HistogramSeries& h = histogram.chart->addSeries({.name = "a"});
    for (int i = 0; i < 100; ++i)
        h.add(i + 0.5);
    const std::size_t h0 = histogram.paint();
    marks(*histogram.chart, 50, 5);
    CHECK(histogram.paint() > h0);

    for (const Orientation o : {Orientation::Vertical, Orientation::Horizontal}) {
        Shown<BarChart> bars(BarChart::Props{.orientation = o});
        const std::size_t a = bars.chart->addSeries("a");
        bars.chart->setCategories({"x", "y", "z"});
        bars.chart->setValues(a, std::vector<double>{10, 20, 30});
        const std::size_t b0 = bars.paint();
        marks(*bars.chart, 1.5, 20);
        CHECK(bars.paint() > b0);
    }
}

TEST(aHistogramAndCandlesShareACrosshair) {
    auto cursor = std::make_shared<ChartCursor>();
    Shown<HistogramChart> histogram(HistogramChart::Props{.bins = 10, .range = ChartRange{0, 100}});
    HistogramSeries& h = histogram.chart->addSeries({.name = "a"});
    for (int i = 0; i < 100; ++i)
        h.add(i + 0.5);
    Shown<CandleChart> candles(CandleChart::Props{.period = 60});
    for (int i = 0; i < 30; ++i)
        candles.chart->trade(i * 60.0, 100 + i, 1);
    histogram.chart->setCursor(cursor);
    candles.chart->setCursor(cursor);
    const std::size_t h0 = histogram.paint(), c0 = candles.paint();
    // the mouse over the histogram: its value shared, a line in the candles' chart
    histogram.move(histogram.chart->plotArea().center());
    CHECK(cursor->from() == histogram.chart && cursor->x() && *cursor->x() > 30 && *cursor->x() < 70);
    CHECK(candles.paint() > c0);
    // over the candles: the histogram's bar there lit, its line
    histogram.move({-10, -10});
    candles.move(candles.chart->plotArea().center());
    candles.paint();            // (its time found as it paints)
    CHECK(cursor->from() == candles.chart);
    cursor->set(candles.chart, 40.0);
    CHECK(histogram.paint() > h0);
}

TEST(aScatterChartOnLogarithmicAxes) {
    Shown<ScatterChart> c(ScatterChart::Props{.logX = true, .logY = true});
    ScatterSeries& s = c.chart->addSeries({.name = "a", .trend = true});
    for (int i = 1; i <= 1000; ++i)
        s.append(i, i * 0.5);
    s.append(-3, 0);           // (off its plot)
    c.paint();
    const ChartRange x = c.chart->xView(), y = c.chart->yView();
    CHECK(x.low > 0 && x.low <= 1 && x.high >= 1000 && y.low > 0 && y.low <= 0.5 && y.high >= 500);
    // zoomed by the wheel at its middle: still above 0, narrower in decades
    c.wheel({300, 200}, 2);
    c.paint();
    const ChartRange z = c.chart->xView();
    CHECK(z.low > 0 && std::log10(z.high / z.low) < std::log10(x.high / x.low));
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
    c.chart->setSlices({leaf("a", 1), leaf("b", 1), leaf("c", 2)});
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
        c.chart->setSlices({leaf("a", 1), leaf("b", 1)});
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

// ---- what charts are drawn with ----

TEST(aJaggedLineIsAsWideAtItsSharpTurns) {
    // up and straight down again a pixel on: a turn of more than 170 degrees
    paint::DrawList list;
    {
        paint::Painter p(list, {0, 0, 100, 100}, 1);
        const Point points[] = {{10, 50}, {11, 10}, {12, 50}};
        p.drawPolyline(points, 3, {1, 1, 1, 1});
    }
    // the solid vertices at the turn: as far apart as the line is wide (less its fringe), not pinched together
    float widest = 0;
    for (const paint::Vertex& a : list.vertices)
        for (const paint::Vertex& b : list.vertices) {
            const bool solid = (a.color >> 24) == 255 && (b.color >> 24) == 255;
            const bool atTurn = std::abs(a.position.y - 10) < 3 && std::abs(b.position.y - 10) < 3;
            if (solid && atTurn)
                widest = std::max(widest, std::hypot(a.position.x - b.position.x, a.position.y - b.position.y));
        }
    CHECK(widest >= 1.9f);
    // a gentle turn: one miter, four vertices at each point
    paint::DrawList gentle;
    {
        paint::Painter p(gentle, {0, 0, 100, 100}, 1);
        const Point points[] = {{10, 50}, {50, 40}, {90, 50}};
        p.drawPolyline(points, 3, {1, 1, 1, 1});
    }
    CHECK(gentle.vertices.size() == 12);
}

TEST(aSliceOfAPieIsARingRelief) {
    paint::DrawList list;
    {
        paint::Painter p(list, {0, 0, 400, 400}, 1);
        p.fillArcRelief({200, 200}, 60, 100, -1.5708f, 1.0f, 1, 1.5f, 2, {1, 0, 0, 1});
    }
    CHECK(list.vertices.size() == 4 && list.indices.size() == 6);
    const paint::Vertex& v = list.vertices[0];
    CHECK(v.kind == static_cast<float>(paint::VertexKind::RingRelief) && v.halfSize.x == 60 && v.halfSize.y == 100);
    // its half angle and its sides' inset (a dip: 16 sixteenths) packed
    CHECK(near(v.radius, 0.5 + 8 * 16, 1e-4) && v.blur == 1.5f && v.border == 2);
    // its quad around it: every corner of it within a pixel or so of the sector's reach
    for (const paint::Vertex& c : list.vertices)
        CHECK(std::hypot(c.position.x - 200, c.position.y - 200) <= 100 * 1.2f + 2);
    // round corners (3 dips: 24 eighths) packed beside its bevel
    list = {};
    {
        paint::Painter p(list, {0, 0, 400, 400}, 1);
        p.fillArcRelief({200, 200}, 60, 100, -1.5708f, 1.0f, 1, 1.5f, 2, {1, 0, 0, 1}, 3);
    }
    CHECK(list.vertices.size() == 4 && list.vertices[0].border == 2 + 256 * 24);
}

TEST(aPieChartsPartsAreRingsFurtherOut) {
    Shown<PieChart> c(PieChart::Props{.hole = 0.3f});
    // two halves, the first of two parts (a quarter each), one of them of two (an eighth each)
    c.chart->setLegendVisible(false);
    c.chart->setSlices({group("a", {group("a1", {leaf("x", 1), leaf("y", 1)}), leaf("a2", 2)}), leaf("b", 4)});
    c.paint();
    const Point center{300, 200};
    const float radius = 200 - 7 - 2, inner = radius * 0.3f, ring = (radius - inner) / 3;
    const auto at = [&](float degrees, float r) {
        const float a = degrees * 3.14159265f / 180;
        return Point{center.x + std::cos(a) * r, center.y + std::sin(a) * r};
    };
    // from the top, clockwise: a's half, then b's; a1 and a2 in the next ring, x and y in the third
    CHECK(c.chart->nodeAt(at(-90 + 45, inner + ring * 0.5f)) == std::optional(std::vector<std::size_t>{0}));
    CHECK(c.chart->nodeAt(at(-90 + 45, inner + ring * 1.5f)) == std::optional(std::vector<std::size_t>{0, 0}));
    CHECK(c.chart->nodeAt(at(-90 + 135, inner + ring * 1.5f)) == std::optional(std::vector<std::size_t>{0, 1}));
    CHECK(c.chart->nodeAt(at(-90 + 70, inner + ring * 2.5f)) == std::optional(std::vector<std::size_t>{0, 0, 1}));
    CHECK(c.chart->nodeAt(at(180, inner + ring * 0.5f)) == std::optional(std::vector<std::size_t>{1}));
    // b has no parts: nothing further out there
    CHECK(!c.chart->nodeAt(at(180, inner + ring * 1.5f)));
    CHECK(c.chart->sliceAt(at(-90 + 70, inner + ring * 2.5f)) == std::optional<std::size_t>(0));
    // a click on a: it is the whole, its parts the first ring (a1 half of it now)
    const Point onA = at(-90 + 45, inner + ring * 0.5f);
    c.button(platform::MouseButton::Left, true, onA);
    c.button(platform::MouseButton::Left, false, onA);
    c.paint();
    CHECK(c.chart->focus() == std::vector<std::size_t>{0});
    const float rings2 = (radius - inner) / 2;
    CHECK(c.chart->nodeAt(at(-90 + 90, inner + rings2 * 0.5f)) == std::optional(std::vector<std::size_t>{0, 0}));
    CHECK(c.chart->nodeAt(at(90 + 45, inner + rings2 * 0.5f)) == std::optional(std::vector<std::size_t>{0, 1}));
    CHECK(c.chart->sliceAt(at(90 + 45, inner + rings2 * 0.5f)) == std::optional<std::size_t>(1));
    CHECK(c.chart->shownValue(0) == 2 && c.chart->shownValue(1) == 2);
    // a click in the hole: up again
    c.button(platform::MouseButton::Left, true, center);
    c.button(platform::MouseButton::Left, false, center);
    c.paint();
    CHECK(c.chart->focus().empty() && c.chart->nodeAt(at(180, inner + ring * 0.5f)));
    // a leaf hidden from the legend of a slice zoomed into: out of its whole
    c.chart->setFocus({0});
    c.chart->setSliceVisible(1, false);
    c.paint();
    CHECK(c.chart->shownValue(1) == 0 && c.chart->nodeAt(at(90, inner + rings2 * 0.5f)) == std::optional(std::vector<std::size_t>{0, 0}));
}

TEST(aLineChartsAreasLieUnderAndBetweenItsLines) {
    Shown<LineChart> c;
    LineSeries& high = c.chart->addSeries({.name = "high"});
    LineSeries& low = c.chart->addSeries({.line = false});
    for (int i = 0; i <= 100; ++i) {
        high.append(i, 50 + (i % 7));
        low.append(i + 0.5, 20 + (i % 5));        // its own x: the band between them still
    }
    const std::size_t lines = c.paint();
    AreaSeries& band = c.chart->addArea({.name = "range", .upper = &high, .lower = &low});
    const std::size_t banded = c.paint();
    CHECK(banded > lines + 300);
    // its legend: the series with a name, then the area (the nameless series not in it); a click hides the area
    CHECK(band.visible());
    const std::size_t hidden = [&] {
        band.setVisible(false);
        return c.paint();
    }();
    CHECK(hidden < banded);
    // a series taken off: its areas too
    c.chart->removeSeries(low);
    CHECK(c.chart->areas().empty());
    c.chart->addArea({.upper = &high, .gradient = true});
    CHECK(c.paint() > lines);
}

// ---- bar races ----

TEST(aBarRaceRanksItsItemsAndMovesThemAsTheyChange) {
    Shown<BarRace> c(BarRace::Props{.shown = 3});
    for (const char* name : {"a", "b", "c", "d"})
        c.chart->addItem(name);
    const double values[] = {10, 40, 30, 20};
    for (std::size_t i = 0; i < 4; ++i)
        c.chart->setValue(i, values[i]);
    c.paint();
    CHECK(c.chart->ranking() == (std::vector<std::size_t>{1, 2, 3, 0}));
    CHECK(c.chart->shownPlace(1) == 0 && c.chart->shownPlace(2) == 1 && c.chart->shownPlace(3) == 2 && c.chart->shownPlace(0) == 3);
    // a passes them all: the top; d under the last row that shows
    c.chart->setValue(0, 100);
    c.paint();
    CHECK(c.chart->shownPlace(0) == 0 && c.chart->shownPlace(1) == 1 && c.chart->shownPlace(3) >= 3);
    // the bar under a point
    const Rect plot = Rect{0, 0, 600, 400};
    CHECK(c.chart->itemAt({plot.center().x, 60}) == std::optional<std::size_t>(0));
    // scrolled a row down: b at the top
    c.chart->scrollTo(1, false);
    c.paint();
    CHECK(c.chart->scrollPosition() == 1 && c.chart->itemAt({plot.center().x, 60}) == std::optional<std::size_t>(1));
    // no further than its last rows
    c.chart->scrollTo(10, false);
    CHECK(c.chart->scrollPosition() == 1);
    // more shown: all of them, from the top
    c.chart->setShown(4);
    c.paint();
    CHECK(c.chart->scrollPosition() == 0 && c.chart->shownPlace(3) == 3);
}

TEST(aBarRaceOfPartsRanksByThePartsThatShow) {
    Shown<BarRace> c(BarRace::Props{.shown = 2, .parts = {"x", "y"}});
    const std::size_t a = c.chart->addItem("a"), b = c.chart->addItem("b");
    c.chart->setValues(a, std::vector<double>{10, 50});
    c.chart->setValues(b, std::vector<double>{40, 5});
    c.paint();
    CHECK(c.chart->ranking() == (std::vector<std::size_t>{a, b}) && c.chart->shownTotal(a) == 60);
    // y hidden: b leads
    c.chart->setPartVisible(1, false);
    c.paint();
    CHECK(c.chart->ranking() == (std::vector<std::size_t>{b, a}) && c.chart->shownTotal(a) == 10 && c.chart->shownPlace(b) == 0);
}

TEST(aBarRaceEasesItsBarsToTheirPlaces) {
    tests::testScheduler().setAnimationsEnabled(true);
    {
        Shown<BarRace> c(BarRace::Props{.shown = 2});
        const std::size_t a = c.chart->addItem("a"), b = c.chart->addItem("b");
        c.chart->setValue(a, 10);
        c.chart->setValue(b, 5);
        c.paint();
        tests::advance(std::chrono::seconds(3));
        c.paint();
        CHECK(c.chart->shownPlace(a) == 0);
        c.chart->setValue(b, 20);
        c.paint();
        tests::advance(std::chrono::milliseconds(100));
        CHECK(c.chart->shownPlace(b) > 0 && c.chart->shownPlace(b) < 1 && c.chart->shownTotal(b) > 5 && c.chart->shownTotal(b) < 20);
        tests::advance(std::chrono::seconds(3));
        CHECK(c.chart->shownPlace(b) == 0 && c.chart->shownTotal(b) == 20);
    }
    tests::testScheduler().setAnimationsEnabled(false);
}

TEST(aBarRaceOfThousandsRanksThemAllAndScrollsThroughThem) {
    Shown<BarRace> c(BarRace::Props{.shown = 12, .parts = {"p", "q", "r"}});
    std::mt19937 random(5);
    std::uniform_real_distribution<double> value(0, 1000);
    for (int i = 0; i < 20'000; ++i)
        c.chart->addItem("item " + std::to_string(i));
    for (int round = 0; round < 3; ++round) {
        for (std::size_t i = 0; i < c.chart->itemCount(); ++i) {
            const double parts[] = {value(random), value(random), value(random)};
            c.chart->setValues(i, parts);
        }
        c.paint();
    }
    const std::vector<std::size_t> all = c.chart->ranking();
    CHECK(all.size() == 20'000);
    bool ordered = true, placed = true;
    for (std::size_t k = 1; k < all.size(); ++k)
        ordered = ordered && c.chart->total(all[k - 1]) >= c.chart->total(all[k]);
    for (std::size_t k = 0; k < all.size(); ++k)
        placed = placed && c.chart->shownPlace(all[k]) == static_cast<float>(k);
    CHECK(ordered && placed);
    // far down: the 15000th at the top row
    c.chart->scrollTo(15'000, false);
    c.paint();
    const Rect plot{0, 0, 600, 400};
    const auto top = c.chart->itemAt({plot.x + 200, 70});
    CHECK(top == std::optional<std::size_t>(all[15'000]));
}


TEST(aPieChartShowsItsTopPartsAndTheOthersAsOne) {
    Shown<PieChart> c(PieChart::Props{.top = 3});
    c.chart->setLegendVisible(false);
    std::vector<PieSlice> slices;
    for (int i = 0; i < 10; ++i)
        slices.push_back(leaf("s" + std::to_string(i), i == 7 ? 100 : 10 - i));
    c.chart->setSlices(slices);
    c.paint();
    // the top three (s7, s0, s1) in their order, then the others (s2... the rest: 8 + 7 + 6 + 5 + 4 + 2 + 1 = 33)
    const Point center{300, 200};
    const float radius = 200 - 7 - 2;
    const auto at = [&](double turn) {
        const double a = (turn * 360 - 90) * 3.14159265 / 180;
        return Point{static_cast<float>(center.x + std::cos(a) * radius * 0.7), static_cast<float>(center.y + std::sin(a) * radius * 0.7)};
    };
    const double total = 100 + 10 + 9 + 33;
    CHECK(c.chart->sliceAt(at(5 / total)) == std::optional<std::size_t>(0));
    CHECK(c.chart->sliceAt(at((10 + 4.5) / total)) == std::optional<std::size_t>(1));
    CHECK(c.chart->sliceAt(at((19 + 50) / total)) == std::optional<std::size_t>(7));
    CHECK(c.chart->nodeAt(at((119 + 16) / total)) == std::optional(std::vector<std::size_t>{PieChart::othersIndex}));
    // s7 drops: s2 into the top three, s7 among the others
    slices[7].value = 0.5;
    c.chart->setSlices(slices);
    c.paint();
    CHECK(c.chart->sliceAt(at(0.01)) == std::optional<std::size_t>(0) && c.chart->shownValue(7) == 0 && c.chart->shownValue(2) == 8);
    // all of them again
    c.chart->setTop(0);
    c.paint();
    CHECK(c.chart->shownValue(7) == 0.5 && c.chart->shownValue(9) == 1);
}

TEST(aBarRaceShowsItsTopOnesAndTheOthersAsOneBar) {
    Shown<BarRace> c(BarRace::Props{.shown = 3, .top = 5});
    for (int i = 0; i < 100; ++i)
        c.chart->setValue(c.chart->addItem("i" + std::to_string(i)), 1 + i);
    c.paint();
    CHECK(c.chart->othersCount() == 95);
    const std::vector<std::size_t> top = c.chart->ranking();
    CHECK(top[0] == 99 && top[4] == 95 && c.chart->shownPlace(99) == 0 && c.chart->shownPlace(10) == 5);
    // six rows (five and the others'), three shown: its scroll bar's thumb dragged to the end of its track
    const Rect area{0, 0, 600, 400};
    const Point thumb{area.right() - 4, 30};
    c.move(thumb);
    c.button(platform::MouseButton::Left, true, thumb);
    c.move({thumb.x, 200});
    c.move({thumb.x, area.bottom() - 1});
    c.button(platform::MouseButton::Left, false, {thumb.x, area.bottom() - 1});
    c.paint();
    CHECK(c.chart->scrollPosition() == 3);
    // the others' bar in its row: the last that shows
    CHECK(c.chart->itemAt({200, area.bottom() - 40}) == std::optional<std::size_t>(BarRace::othersItem));
    // an item leaps into the top: out of the others
    c.chart->setValue(3, 1000);
    c.paint();
    CHECK(c.chart->ranking()[0] == 3 && c.chart->shownPlace(3) == 0 && c.chart->othersCount() == 95);
}

TEST(aTreeMapsLabelsStayAsItEases) {
    tests::testScheduler().setAnimationsEnabled(true);
    {
        Shown<TreeMap> c(TreeMap::Props{.depth = 1});
        TreeMapNode root{.label = "all"};
        for (int i = 0; i < 6; ++i)
            root.children.push_back({.label = "node " + std::to_string(i), .value = 10.0 + i});
        c.chart->setRoot(root);
        c.paint();          // laid out, its tiles growing in
        tests::advance(std::chrono::seconds(2));
        // the glyphs drawn: its labels'
        const auto glyphs = [&] {
            paint::DrawList list;
            c.root.paint(list);
            int count = 0;
            for (const paint::Vertex& v : list.vertices)
                count += v.kind == static_cast<float>(paint::VertexKind::Mask) ? 1 : 0;
            return count;
        };
        const int settled = glyphs();
        CHECK(settled > 0);
        // one value changed: the tiles eased to their new sizes, their labels with them, not gone meanwhile
        root.children[2].value = 30;
        c.chart->setRoot(root);
        glyphs();
        tests::advance(std::chrono::milliseconds(100));
        CHECK(glyphs() >= settled * 3 / 4);
        tests::advance(std::chrono::seconds(2));
    }
    tests::testScheduler().setAnimationsEnabled(false);
}

// ---- sparklines, gauges ----

TEST(aSparklineKeepsItsNewestAndFitsThem) {
    Shown<Sparkline> c(Sparkline::Props{.capacity = 5});
    for (int i = 0; i < 10; ++i)
        c.chart->append(i);
    CHECK(c.chart->data().size() == 5);
    CHECK(c.chart->last() == 9);
    const std::size_t drawn = c.paint();
    CHECK(drawn > 0);
    CHECK(c.chart->shownRange() == (ChartRange{5, 9}));
    // the value under the mouse: a tooltip more
    c.move({300, 200});
    CHECK(c.paint() > drawn);
    // columns: from 0
    Shown<Sparkline> columns(Sparkline::Props{.kind = Sparkline::Kind::Columns});
    const double values[] = {3, 5, 4};
    columns.chart->setValues(values);
    columns.paint();
    CHECK(columns.chart->shownRange() == (ChartRange{0, 5}));
}

TEST(aSparklineOfAMillionValuesDrawsWhatItsPixelsShow) {
    Shown<Sparkline> c(Sparkline::Props{.capacity = 0, .area = false});
    std::mt19937 random(7);
    std::normal_distribution<double> noise;
    for (int i = 0; i < 1'000'000; ++i)
        c.chart->append(noise(random));
    // a point for each column of pixels at most: as many vertices as a line of 600
    CHECK(c.paint() < 600 * 24);
}

TEST(aGaugeEasesToItsValueColoredByItsBand) {
    const Color good{0, 1, 0, 1}, high{1, 0, 0, 1};
    tests::testScheduler().setAnimationsEnabled(true);
    {
        Shown<Gauge> c(Gauge::Props{.bands = {{.to = 70, .color = good}, {.to = 100, .color = high}}, .unit = "%"});
        CHECK(c.chart->shownValue() == 0);
        c.chart->setValue(90);
        tests::advance(std::chrono::milliseconds(60));
        CHECK(c.chart->shownValue() > 0 && c.chart->shownValue() < 90);
        tests::advance(std::chrono::seconds(1));
        CHECK(c.chart->shownValue() == 90);
        CHECK(c.chart->colorOf(50) == good);
        CHECK(c.chart->colorOf(90) == high);
        CHECK(c.paint() > 0);
    }
    tests::testScheduler().setAnimationsEnabled(false);
    // a needle, a half circle: drawn too
    Shown<Gauge> half(Gauge::Props{.sweep = 180, .needle = true});
    half.chart->setValue(40);
    CHECK(half.chart->shownValue() == 40);
    CHECK(half.paint() > 0);
}

// ---- heat maps ----

TEST(aHeatMapColorsItsCellsByTheirValues) {
    Shown<HeatMap> c(HeatMap::Props{.rows = {"a", "b"}});
    // three columns of two rows: a few large cells, one by one, their values in them
    const double values[] = {1, 2, 3, 4, 5, std::nan("")};
    c.chart->setValues(3, values);
    c.paint();
    CHECK(c.chart->drawn() == HeatMap::Drawn::Cells && c.chart->columnCount() == 3 && c.chart->rowCount() == 2);
    CHECK(c.chart->value(1, 0) == 3 && std::isnan(c.chart->value(2, 1)) && std::isnan(c.chart->value(7, 0)));
    // its colors from its lowest to its highest value, out to round values
    const ChartRange r = c.chart->colorRange();
    CHECK(r.low <= 1 && r.high >= 5 && r.span() <= 10);
    CHECK(c.chart->colorOf(1) != c.chart->colorOf(5) && c.chart->colorOf(std::nan("")).a == 0);
    // its own range; a diverging one as far below its center as above; a logarithmic one by powers of ten
    c.chart->setScale({.range = ChartRange{0, 100}});
    CHECK(c.chart->colorRange() == (ChartRange{0, 100}));
    c.chart->setScale({.kind = ColorScale::Kind::Diverging});
    const ChartRange d = c.chart->colorRange();
    CHECK(near(d.low, -d.high) && d.high >= 5);
    const double wide[] = {3, 2000};
    c.chart->setValues(1, wide);
    c.chart->setScale({.logarithmic = true});
    CHECK(c.chart->colorRange() == (ChartRange{1, 10000}));
}

TEST(aLiveHeatMapKeepsItsNewestColumnsAndFollowsThem) {
    Shown<HeatMap> c(HeatMap::Props{.rows = {"a", "b", "c"}, .capacity = 100, .window = 50});
    for (int i = 0; i < 200; ++i) {
        const double column[] = {double(i), double(i % 7), 1};
        c.chart->appendColumn(column);
    }
    c.paint();
    CHECK(c.chart->columnCount() == 100 && c.chart->firstColumn() == 100 && c.chart->value(199, 0) == 199);
    CHECK(c.chart->following() && c.chart->columnView() == (ChartRange{150, 200}));
    // its colors fitted to what it keeps: the dropped ones' values gone from it
    CHECK(c.chart->colorRange().low >= 0 && c.chart->colorRange().low <= 1 && c.chart->colorRange().high >= 199);
    // dragged back from the newest, then to it again: following again
    c.drag({300, 200}, {500, 200});
    CHECK(!c.chart->following());
    c.drag({500, 200}, {100, 200});
    CHECK(c.chart->following());
}

TEST(aHeatMapOfManyCellsIsAnImageOfThem) {
    Shown<HeatMap> c(HeatMap::Props{.capacity = 4000, .window = 4000});
    std::vector<double> column(500);
    for (int i = 0; i < 4000; ++i) {
        for (std::size_t r = 0; r < column.size(); ++r)
            column[r] = std::sin(i * 0.01 + static_cast<double>(r) * 0.05);
        c.chart->appendColumn(column);
    }
    // two million cells: an image (of buckets of them), drawn with a texture of its own, a few quads
    const std::size_t vertices = c.paint();
    CHECK(c.chart->drawn() == HeatMap::Drawn::Image && c.chart->rowCount() == 500 && tests::testContext().images->count() >= 1);
    CHECK(vertices < 20000);
    paint::DrawList list;
    c.root.paint(list);
    CHECK(std::any_of(list.commands.begin(), list.commands.end(), [](const paint::DrawCommand& d) { return d.texture != paint::whiteTexture; }));
    // a column more: still an image; zoomed into ten columns (their rows still under a pixel): an image of them
    c.chart->appendColumn(column);
    c.paint();
    CHECK(c.chart->drawn() == HeatMap::Drawn::Image && c.chart->firstColumn() == 1);
    c.chart->setColumnView({100, 110});
    c.paint();
    CHECK(c.chart->columnView() == (ChartRange{100, 110}) && !c.chart->following() && c.chart->drawn() == HeatMap::Drawn::Image);
}

TEST(aClickOnAHeatMapsCellTellsWhichItIs) {
    Shown<HeatMap> c(HeatMap::Props{.rows = {"a", "b"}});
    const double values[] = {1, 2, 3, 4};
    c.chart->setValues(2, values);
    c.paint();
    std::optional<std::pair<std::size_t, std::size_t>> clicked;
    c.chart->setOnCellClicked([&](std::size_t column, std::size_t row) { clicked = std::pair{column, row}; });
    // the lower right cell: of the second column, the second row
    const Rect bounds = c.chart->bounds();
    c.move({bounds.width * 0.6f, bounds.height * 0.75f});
    c.button(platform::MouseButton::Left, true, {bounds.width * 0.6f, bounds.height * 0.75f});
    c.button(platform::MouseButton::Left, false, {bounds.width * 0.6f, bounds.height * 0.75f});
    CHECK(clicked && clicked->first == 1 && clicked->second == 1);
}

// ---- stacked areas ----

TEST(stackedSeriesLieOnTheOnesUnderThem) {
    Shown<LineChart> c;
    // three series of 1, 2 and 3, stacked: their sum 6, the y axis from 0 to it; one of them at other x than the others
    std::array<LineSeries*, 3> series{};
    for (int k = 0; k < 3; ++k) {
        series[k] = &c.chart->addSeries({.name = std::string(1, static_cast<char>('a' + k)), .stacked = true});
        for (int x = 0; x <= 10; ++x)
            series[k]->append(k == 1 ? x + 0.5 : x, k + 1.0);
    }
    c.paint();
    ChartRange y = c.chart->targetYView();
    CHECK(y.low == 0 && y.high >= 6 && y.high < 8);
    // the middle one hidden: the others down onto what is left (4)
    series[1]->setVisible(false);
    c.paint();
    c.paint();
    y = c.chart->targetYView();
    CHECK(y.low == 0 && y.high >= 4 && y.high < 5.5);
    // in shares: 0 to 100 %
    series[1]->setVisible(true);
    c.chart->setPercent(true);
    c.paint();
    CHECK(c.chart->targetYView() == (ChartRange{0, 100}));
    // the mouse over it: a tooltip of each one's value and share, and their sum
    const std::size_t drawn = c.paint();
    c.move({300, 200});
    CHECK(c.paint() > drawn);
}

// ---- candles ----

TEST(aCandleChartMakesCandlesOfTrades) {
    Shown<CandleChart> c(CandleChart::Props{.period = 60});
    // a minute's trades: its first price its open, its highest and lowest, its last its close, their volumes summed
    c.chart->trade(0, 10, 1);
    c.chart->trade(30, 12, 2);
    c.chart->trade(59, 9, 1);
    c.chart->trade(61, 11, 5);
    c.chart->trade(20, 50, 9);          // of a minute gone: left out
    CHECK(c.chart->count() == 2);
    const Candle& first = c.chart->candle(0);
    CHECK(first.time == 0 && first.open == 10 && first.high == 12 && first.low == 9 && first.close == 9 && first.volume == 4);
    CHECK(c.chart->last()->time == 60 && c.chart->last()->open == 11 && c.chart->last()->volume == 5);
    // the last one again, as its period goes on
    c.chart->setLast({60, 11, 14, 10, 13, 8});
    CHECK(c.chart->count() == 2 && c.chart->last()->high == 14);
    // the mouse over a candle: a crosshair, its values by it
    const std::size_t drawn = c.paint();
    CHECK(drawn > 0);
    c.chart->setWindow(2);
    c.paint();
    const std::size_t still = c.paint();
    c.move({200, 200});
    CHECK(c.paint() > still);
}

TEST(aLiveCandleChartFollowsItsNewestCandles) {
    Shown<CandleChart> c(CandleChart::Props{.capacity = 500, .window = 100});
    for (int i = 0; i < 1000; ++i)
        c.chart->append({i * 60.0, 100.0 + i, 101.0 + i, 99.0 + i, 100.5 + i, 10});
    c.paint();
    CHECK(c.chart->count() == 500 && c.chart->following());
    CHECK(c.chart->indexView() == (ChartRange{900, 1001}));
    // its prices fitted to the candles that show
    const ChartRange y = c.chart->priceView();
    CHECK(y.low < 999 && y.low > 950 && y.high > 1100 && y.high < 1110);
    // dragged back, then to the newest again: following again
    c.drag({300, 150}, {500, 150});
    CHECK(!c.chart->following());
    c.drag({500, 150}, {100, 150});
    CHECK(c.chart->following());
}

TEST(aCandleChartOfManyCandlesDrawsWhatItsPixelsShow) {
    Shown<CandleChart> c;
    double price = 100;
    std::mt19937 random(5);
    std::normal_distribution<double> step(0, 0.5);
    for (int i = 0; i < 200'000; ++i) {
        const double open = price;
        price += step(random);
        c.chart->append({i * 60.0, open, std::max(open, price) + 0.2, std::min(open, price) - 0.2, price, 1});
    }
    // two hundred thousand candles in 600 dips: a candle a column of pixels, of those in it
    CHECK(c.paint() < 600 * 40);
}

TEST(aCandleChartShowsItsCandlesInLongerPeriods) {
    Shown<CandleChart> c(CandleChart::Props{.period = 1, .periods = {1, 5, 60}});
    // a minute of candles of a second, rising by one each
    for (int i = 0; i < 60; ++i)
        c.chart->append({double(i), 100.0 + i, 101.0 + i, 99.0 + i, 100.5 + i, 1});
    CHECK(c.chart->count() == 60);
    // of five seconds: twelve, each its first second's open, its last's close, their highest and lowest, their volumes
    c.chart->setPeriod(5);
    CHECK(c.chart->period() == 5 && c.chart->count() == 12);
    const Candle& first = c.chart->candle(0);
    CHECK(first.time == 0 && first.open == 100 && first.close == 104.5 && first.high == 105 && first.low == 99 && first.volume == 5);
    // a second more: a new candle of five seconds begun, its trades into it as they come
    c.chart->append({60, 160, 161, 159, 160.5, 1});
    c.chart->trade(61.5, 170, 2);
    CHECK(c.chart->count() == 13 && c.chart->last()->time == 60 && c.chart->last()->high == 170 && c.chart->last()->volume == 3);
    // a period not a whole number of its own: the nearest that is; a minute: all of them in one and the new one
    c.chart->setPeriod(4.6);
    CHECK(c.chart->period() == 5);
    c.chart->setPeriod(60);
    CHECK(c.chart->count() == 2 && c.chart->candle(0).volume == 60);
    // its buttons at the end of its top row (of a second, five, a minute): a click on the first shows candles of a second
    c.paint();
    c.button(platform::MouseButton::Left, true, {c.chart->bounds().right() - 70, 10});
    CHECK(c.chart->period() == 1 && c.chart->count() == 62);
}

TEST(aCandleChartsLegendHidesItsAveragesAndItsVolume) {
    Shown<CandleChart> c(CandleChart::Props{.period = 1});
    for (int i = 0; i < 40; ++i)
        c.chart->append({double(i), 100.0 + i, 101.0 + i, 99.0 + i, 100.0 + i, 10});
    CandleChart::Line& average = c.chart->addAverage(10);
    CHECK(average.averageOf() == 10 && average.name().source == "MA 10" && average.visible());
    c.paint();
    // the legend: its volume, then the average; a click on each hides it
    c.button(platform::MouseButton::Left, true, {c.chart->bounds().x + 20, 8});
    CHECK(!c.chart->volumeVisible());
    c.chart->setVolumeVisible(true);
    average.setVisible(false);
    CHECK(!average.visible());
    CHECK(c.paint() > 0);
}

TEST(aCandleChartsPeriodsAreOfTheLocalCalendar) {
    // a local date's midnight
    const auto midnight = [](int year, int month, int day) {
        std::tm t{};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_isdst = -1;
        return static_cast<double>(std::mktime(&t));
    };
    // their names; seconds in the largest unit they are a whole number of
    CHECK(CandlePeriod(86400).unit == CandlePeriod::Unit::Day && CandlePeriod(604800) == CandlePeriod::weeks(1));
    CHECK(CandlePeriod(300).name() == "5m" && CandlePeriod::days(1).name() == "1D" && CandlePeriod::months(3).name() == "3M");
    // what is made of what: minutes of seconds, a month of days; not a month of weeks, nor 7 seconds of 5
    CHECK(CandlePeriod::months(1).madeOf(CandlePeriod::days(1)) && CandlePeriod::hours(1).madeOf(CandlePeriod::seconds(5)));
    CHECK(!CandlePeriod::months(1).madeOf(CandlePeriod::weeks(1)) && !CandlePeriod::seconds(7).madeOf(CandlePeriod::seconds(5)));
    // an hour's start: on the local hour; a week's: on its first day (a Saturday), a month's: on its first
    CHECK(CandlePeriod::hours(1).start(midnight(2025, 3, 10) + 5000) == midnight(2025, 3, 10) + 3600);
    CHECK(CandlePeriod::weeks(1).start(midnight(2025, 1, 1) + 600, 6) == midnight(2024, 12, 28));
    CHECK(CandlePeriod::months(1).start(midnight(2025, 2, 17)) == midnight(2025, 2, 1));
    CHECK(CandlePeriod::months(1).next(midnight(2025, 1, 1)) == midnight(2025, 2, 1));
    CHECK(CandlePeriod::years(1).start(midnight(2025, 7, 4)) == midnight(2025, 1, 1));

    // days of three months (as they were given: their times kept), in weeks from Saturday, in months
    Shown<CandleChart> c(CandleChart::Props{.period = CandlePeriod::days(1),
                                            .periods = {CandlePeriod::days(1), CandlePeriod::weeks(1), CandlePeriod::months(1)},
                                            .weekStart = 6});
    for (int day = 1; day <= 90; ++day) {
        const double at = midnight(2025, 1, day);       // (the 32nd of January: the 1st of February)
        c.chart->append({at, 100.0 + day, 101.0 + day, 99.0 + day, 100.5 + day, 1});
    }
    CHECK(c.chart->count() == 90 && c.chart->candle(0).time == midnight(2025, 1, 1));
    c.chart->setPeriod(CandlePeriod::weeks(1));
    CHECK(c.chart->count() == 14 && c.chart->candle(0).time == midnight(2024, 12, 28) && c.chart->candle(0).volume == 3);
    CHECK(c.chart->candle(0).open == 101 && c.chart->candle(1).time == midnight(2025, 1, 4) && c.chart->candle(1).volume == 7);
    c.chart->setPeriod(CandlePeriod::months(1));
    CHECK(c.chart->count() == 3 && c.chart->candle(1).time == midnight(2025, 2, 1) && c.chart->candle(1).volume == 28);
    // not of days: left as it was
    c.chart->setPeriod(CandlePeriod::hours(5));
    CHECK(c.chart->period() == CandlePeriod::months(1));
    CHECK(c.paint() > 0);
}

// ---- distributions ----

TEST(aHistogramCountsItsValuesIntoBins) {
    Shown<HistogramChart> c(HistogramChart::Props{.bins = 10, .range = ChartRange{0, 100}});
    HistogramSeries& s = c.chart->addSeries({.name = "a"});
    for (int i = 0; i < 100; ++i)
        s.add(i + 0.5);
    // ten bins of ten, ten values in each; its quantiles within them
    CHECK(c.chart->binCount() == 10 && c.chart->binWidth() == 10 && c.chart->binStart() == 0 && c.chart->binOf(55) == 5u);
    CHECK(std::all_of(s.counts().begin(), s.counts().end(), [](std::uint32_t n) { return n == 10; }));
    CHECK(near(s.quantile(0.5), 50, 1e-9) && near(s.quantile(0.9), 90, 1e-9) && near(s.mean(), 50, 1e-9));
    // the mouse over a bin: its count by it
    const std::size_t drawn = c.paint();
    c.move({300, 250});
    CHECK(c.paint() > drawn);
}

TEST(aHistogramFindsRoundBinsForWhatComes) {
    Shown<HistogramChart> c;
    HistogramSeries& s = c.chart->addSeries({.name = "latency", .capacity = 5000});
    std::mt19937 random(9);
    std::lognormal_distribution<double> latency(std::log(20.0), 0.5);
    for (int i = 0; i < 8000; ++i)
        s.add(latency(random));
    // the newest 5000, all of them in its bins, of a round width (1, 2 or 5 times a power of ten)
    CHECK(s.size() == 5000);
    const std::size_t bins = c.chart->binCount();
    CHECK(bins >= 5 && bins <= 200);
    std::size_t total = 0;
    for (std::uint32_t n : s.counts())
        total += n;
    CHECK(total == 5000);
    const double w = c.chart->binWidth(), mantissa = w / std::pow(10.0, std::floor(std::log10(w)));
    CHECK(near(mantissa, 1, 1e-9) || near(mantissa, 2, 1e-9) || near(mantissa, 5, 1e-9));
    // its bins from the 1st percentile to the 99th: the farthest values in its first and last; one far past them too
    CHECK(c.chart->binStart() + c.chart->binWidth() * static_cast<double>(bins) < 200);
    const std::uint32_t last = s.counts().back();
    s.add(1000);
    CHECK(!c.chart->binOf(1000).has_value() && s.counts().back() == last + 1 && c.chart->binCount() == bins);
    total = 0;
    for (std::uint32_t n : s.counts())
        total += n;
    CHECK(total == 5000);
    CHECK(c.paint() > 0);
}

TEST(aBoxPlotsQuartilesWhiskersAndOutliers) {
    std::vector<double> values{9, 1, 8, 2, 7, 3, 6, 4, 5};
    BoxSummary b = BoxChart::summarize(values, BoxChart::Whiskers::Tukey);
    CHECK(b.q1 == 3 && b.median == 5 && b.q3 == 7 && b.low == 1 && b.high == 9 && b.outliers.empty() && b.count == 9 && b.mean == 5);
    // one far out: an outlier, the whisker at the last value within reach
    values.push_back(100);
    b = BoxChart::summarize(values, BoxChart::Whiskers::Tukey);
    CHECK(b.outliers.size() == 1 && b.outliers[0] == 100 && b.high == 9);
    b = BoxChart::summarize(values, BoxChart::Whiskers::Range);
    CHECK(b.high == 100 && b.outliers.empty());

    Shown<BoxChart> c(BoxChart::Props{.categories = {"a", "b"}, .capacity = 4});
    const double a[] = {1, 2, 3, 4, 5, 6};
    c.chart->setValues(0, a);           // its newest four: 3 to 6
    CHECK(c.chart->summary(0).count == 4 && c.chart->summary(0).low == 3 && c.chart->summary(0).median == 4.5);
    c.chart->setSummary(1, {.low = 0, .q1 = 1, .median = 2, .q3 = 3, .high = 4, .mean = 2, .count = 100});
    CHECK(c.chart->summary(1).count == 100);
    const std::size_t drawn = c.paint();
    c.move({200, 200});
    CHECK(c.paint() > drawn);
}

// ---- scatter ----

TEST(aScatterChartFitsItsPointsAndALineThroughThem) {
    Shown<ScatterChart> c;
    ScatterSeries& s = c.chart->addSeries({.name = "a", .trend = true});
    for (int i = 0; i < 50; ++i)
        s.append(i, 2.0 * i + 1);
    // y = 2x + 1 exactly: its fit
    const auto f = s.fit();
    CHECK(f && near(f->slope, 2, 1e-9) && near(f->intercept, 1, 1e-9) && near(f->r2, 1, 1e-9));
    c.paint();
    CHECK(c.chart->drawn() == ScatterChart::Drawn::Markers);
    const ChartRange x = c.chart->xView(), y = c.chart->yView();
    CHECK(x.low <= 0 && x.high >= 49 && y.low <= 1 && y.high >= 99);
    // the oldest dropped past a capacity: its fit and extents of those kept
    ScatterSeries& live = c.chart->addSeries({.capacity = 10});
    for (int i = 0; i < 100; ++i)
        live.append(1000.0 + i, i % 2 == 0 ? 5.0 : -5.0);
    CHECK(live.size() == 10 && live.x(0) == 1090);
    // the mouse near a point: it ringed, its values by it
    const std::size_t drawn = c.paint();
    c.chart->setView({0, 49}, {1, 99});
    c.paint();
    const std::size_t still = c.paint();
    const Rect plot = c.chart->bounds();
    c.move({plot.width / 2, plot.height / 2});
    CHECK(c.paint() >= still && drawn > 0);
}

TEST(aScatterChartOfAMillionPointsDrawsTheirDensity) {
    Shown<ScatterChart> c(ScatterChart::Props{.markers = 20000});
    ScatterSeries& s = c.chart->addSeries({.name = "many"});
    std::mt19937 random(11);
    std::normal_distribution<double> value;
    for (int i = 0; i < 1'000'000; ++i)
        s.append(value(random), value(random));
    // an image of where they are: a quad, not a million dots
    CHECK(c.paint() < 4000 && c.chart->drawn() == ScatterChart::Drawn::Density);
    // zoomed into a corner of few of them: dots again
    c.chart->setView({3.5, 4.5}, {3.5, 4.5});
    c.paint();
    CHECK(c.chart->drawn() == ScatterChart::Drawn::Markers);
}

// ---- timelines, flame graphs ----

TEST(aTimelineLaysItsSpansInLanesAndRows) {
    Shown<TimelineChart> c(TimelineChart::Props{.lanes = {"main", "worker"}, .capacity = 3});
    c.chart->addSpan(0, {.start = 0, .end = 10, .name = "frame"});
    c.chart->addSpan(0, {.start = 1, .end = 4, .name = "update", .depth = 1});
    c.chart->addSpan(0, {.start = 5, .end = 9, .name = "render", .depth = 1});
    c.chart->addSpan(1, {.start = 2, .end = 6, .name = "load"});
    CHECK(c.chart->laneCount() == 2 && c.chart->spanCount(0) == 3 && c.chart->rowCount(0) == 2 && c.chart->rowCount(1) == 1);
    // a lane's oldest dropped past its capacity; a span of a lane past the last: lanes made for it
    c.chart->addSpan(0, {.start = 11, .end = 12, .name = "frame"});
    CHECK(c.chart->spanCount(0) == 3);
    c.chart->addSpan(3, {.start = 0, .end = 1, .name = "late"});
    CHECK(c.chart->laneCount() == 4);
    c.paint();
    const ChartRange t = c.chart->timeView();
    CHECK(t.low <= 0 && t.high >= 12);
    // the mouse over a span: its name, start and length by it
    const std::size_t drawn = c.paint();
    c.move({320, 60});
    CHECK(c.paint() >= drawn);
}

TEST(aTimelineOfAMillionSpansDrawsWhatItsPixelsShow) {
    Shown<TimelineChart> c;
    for (int i = 0; i < 1'000'000; ++i)
        c.chart->addSpan(static_cast<std::size_t>(i % 4), {.start = i * 1.0, .end = i + 0.6, .name = "task"});
    // spans under a pixel run together: about a bar a pixel a row, not a million
    CHECK(c.paint() < 4 * 600 * 8);
    // zoomed into a few of them: each one, its name in it
    c.chart->setTimeView({1000, 1010});
    CHECK(c.paint() > 0);
}

TEST(aFlameGraphMergesStacksAndZoomsIntoAFrame) {
    Shown<FlameGraph> c;
    const std::vector<std::string> ab{"main", "a", "b"}, ac{"main", "a", "c"}, d{"main", "d"};
    c.chart->addStack(ab);
    c.chart->addStack(ac, 2);
    c.chart->addStack(d);
    // its root, main, a, b, c, d; three deep
    CHECK(c.chart->total() == 4 && c.chart->frameCount() == 6 && c.chart->depth() == 3);
    // a search: the frames with it, how much of all they are
    c.chart->setHighlight("c");
    CHECK(near(c.chart->highlighted(), 0.5));
    c.chart->setHighlight("");
    // into a: it across the graph (its siblings in the order of their names: a, then d)
    const std::vector<std::string> a{"main", "a"};
    c.chart->zoomTo(a);
    CHECK(near(c.chart->zoomRange().first, 0) && near(c.chart->zoomRange().second, 0.75));
    c.chart->zoomOut();
    CHECK(near(c.chart->zoomRange().second, 1));
    // a click on a frame (d: the last quarter of the third row from the bottom) zooms into it; the mouse over one: its
    // share by it
    c.paint();
    c.button(platform::MouseButton::Left, true, {500, 355});
    CHECK(c.chart->zoomRange().first > 0.7);
    const std::size_t drawn = c.paint();
    c.move({300, 375});
    CHECK(c.paint() > drawn);
}

// ---- sankey diagrams ----

TEST(aSankeyPutsItsNodesInColumnsByHowFarTheyAreFromTheSources) {
    Shown<SankeyChart> c;
    const std::size_t a = c.chart->addNode("a"), b = c.chart->addNode("b"), d = c.chart->addNode("c"), e = c.chart->addNode("d");
    c.chart->setLink(a, b, 6);
    c.chart->setLink(b, d, 4);
    c.chart->setLink(a, d, 3);
    c.chart->setLink(a, e, 1);
    // a, then b, then c (the longest way to it: through b); d, which nothing flows out of, in the last column too
    CHECK(c.chart->columnCount() == 3 && c.chart->columnOf(a) == 0 && c.chart->columnOf(b) == 1 && c.chart->columnOf(d) == 2);
    CHECK(c.chart->columnOf(e) == 2);
    // what flows through a node: the more of what comes in and what goes out
    CHECK(c.chart->nodeValue(a) == 10 && c.chart->nodeValue(b) == 6 && c.chart->nodeValue(d) == 7 && c.chart->nodeValue(e) == 1);
    // a link back to a node before it: left out, the columns as they were
    c.chart->setLink(d, a, 5);
    CHECK(!c.chart->linkShown(d, a) && c.chart->linkShown(a, b) && c.chart->columnOf(a) == 0 && c.chart->nodeValue(a) == 10);
    // a value changed, a link gone
    c.chart->setLink(a, b, 8);
    c.chart->setLink(a, e, 0);
    CHECK(c.chart->linkValue(a, b) == 8 && c.chart->linkValue(a, e) == 0 && c.chart->nodeValue(a) == 11);
    // drawn; the mouse over its first node (at the start side, its middle): its flows by it
    const std::size_t drawn = c.paint();
    CHECK(drawn > 0);
    c.move({c.chart->bounds().x + 6, c.chart->bounds().height / 2});
    CHECK(c.paint() > drawn);
}

TEST(aSankeyWithoutJustifyLeavesItsSinksWhereTheyAre) {
    Shown<SankeyChart> c(SankeyChart::Props{.justify = false});
    const std::size_t a = c.chart->addNode("a"), b = c.chart->addNode("b"), d = c.chart->addNode("c"), e = c.chart->addNode("d");
    c.chart->setLinks({{a, b, 2}, {b, d, 2}, {a, e, 1}});
    CHECK(c.chart->columnCount() == 3 && c.chart->columnOf(e) == 1 && c.chart->columnOf(d) == 2);
    CHECK(c.paint() > 0);
}
