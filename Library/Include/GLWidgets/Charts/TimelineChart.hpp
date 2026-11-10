// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What happened when, lane by lane (a profiler's trace, a scheduler's jobs, a service's requests): lanes (threads,
// machines, services) of spans of time, each a bar from its start to its end, those within another a row under it (its
// depth: a call within a call), named, of its category's color (by default its name's: the same name the same color).
// Spans come as they come (addSpan: the oldest of a lane dropped past a capacity; a live one shows its last window,
// following). Only what shows is drawn: spans found by halves in each row, those narrower than a pixel run together
// into one bar, so that a million spans cost what their pixels do; a span's name in it, when it fits.
//
// With the mouse: the wheel zooms the time at the mouse (with Shift it pans; with Ctrl it scrolls the lanes, which the
// style's scroll bar at the end side scrolls too, when there are more than fit), a drag pans both ways (a live one follows
// again when dragged back to its newest); a double click on a span zooms to it, elsewhere shows all again. The span under
// it outlined, its name, start, length and lane by it.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ScrollBar.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace glw {

struct TimelineSpan {
    double start = 0;
    double end = 0;
    std::string name;
    std::uint32_t depth = 0;                    // within the spans of the rows over it (0: the lane's first row)
    std::optional<std::size_t> category;        // its color: the style's `category`th series color; none: its name's
};

class GLW_EXPORT TimelineChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        ChartAxisKind xAxis = ChartAxisKind::Value;
        std::vector<LocalizedText> lanes;
        std::vector<LocalizedText> categories;  // their names, in its legend (in order: the series colors)
        float rowHeight = 18;
        std::size_t capacity = 0;               // each lane's newest spans kept; 0: all
        double window = 0;                      // its last `window` of time shown, following the newest; 0: all
    };

    TimelineChart();
    explicit TimelineChart(Props props);
    ~TimelineChart() override;

    std::size_t addLane(LocalizedText name);
    std::size_t laneCount() const { return lanes_.size(); }
    // A span of a lane (spans of a row in the order of their starts, not over one another); more lanes made when it is
    // of one past the last.
    void addSpan(std::size_t lane, TimelineSpan span);
    void clearLane(std::size_t lane);
    // How many spans a lane has; its rows.
    std::size_t spanCount(std::size_t lane) const;
    std::size_t rowCount(std::size_t lane) const;
    // How a time and a length show (a step apart from the next tick); none: formatNumber, formatTime, and lengths in
    // seconds (of a time axis: s, ms, µs).
    void setXFormatter(std::function<std::string(double value, double step)> format);
    void setLengthFormatter(std::function<std::string(double length)> format);

    // What shows of time (eased); zoomed to a time of one's own; all (or its window) again.
    ChartRange timeView() const;
    void setTimeView(ChartRange view);
    void resetView();
    bool following() const;
    // How far down the lanes are scrolled, in dips.
    double scrollPosition() const { return scroll_; }
    void scrollTo(double offset, bool animated = true);

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;

private:
    struct Lane {
        LocalizedText name;
        std::vector<std::deque<TimelineSpan>> rows;
        std::size_t count = 0;
        std::deque<std::pair<std::uint32_t, double>> order;     // its spans' rows and starts as they came: the oldest first
    };
    enum class XMode : std::uint8_t { Fit, Window, Manual };
    ChartRange fitX() const;
    void updateView();
    float laneHeight(const Lane& lane) const;
    float contentHeight() const;
    double lastScroll() const;
    void placeBar();
    Color colorOf(const TimelineSpan& span) const;
    std::string length(double seconds) const;
    // The span at a point: its lane, row and place in it.
    struct Hit {
        std::size_t lane, row, index;
    };
    std::optional<Hit> spanAt(Point at) const;

    Props props_;
    std::vector<Lane> lanes_;
    std::function<std::string(double, double)> xFormat_;
    std::function<std::string(double)> lengthFormat_;

    XMode xMode_ = XMode::Fit;
    double window_ = 0;
    ChartRange userX_;
    struct View {
        ChartRange from, to;
    };
    View view_;
    bool hasView_ = false, immediate_ = false, quick_ = false;
    Animation progress_;

    double scroll_ = 0;                         // dips scrolled down the lanes
    SmoothScroll scrolling_;
    ScrollBar bar_;
    Rect barTrack_;

    Rect plot_, lanesArea_;
    float labelWidth_ = 0;
    Axis axis_;
    std::optional<Point> mouse_;
    bool dragging_ = false;
    Point dragFrom_;
    ChartRange dragX_;
    double dragScroll_ = 0;
};

} // namespace glw
