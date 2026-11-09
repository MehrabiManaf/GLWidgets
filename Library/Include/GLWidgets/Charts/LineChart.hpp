// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Lines through the points of series over an x axis of values or of time: a feed of measures as they come (a live
// chart), a history, a signal of millions of points.
//
// Its data: series of points appended as they come (LineSeries, a SeriesBuffer each: the oldest dropped past a capacity),
// drawn from what shows only: a series of more points than the plot has columns of pixels is drawn as what each column
// shows (its first and last points, its lowest and highest), which looks the same and takes as long for a million points
// as for a thousand. Under a line an area may fade down; a line may step (a value held until the next).
//
// What shows: all of the data, or (a live chart, setWindow) its last `window` of x, following what comes; the y axis
// fitted to what shows (ending on its ticks), or a range of its own. A change of what shows is eased to: the axes glide
// as data comes, and a live chart scrolls on smoothly.
//
// Areas (AreaSeries, as Qt's QAreaSeries): of a color of their own, under a series's line down to the axis, or between
// two series's lines (a band: a range, a spread around a mean), drawn under the lines; a series may show its area only
// (Props::line false).
//
// With the mouse: the wheel zooms the x axis at the mouse (a live chart: its window, still following), with Ctrl the y
// axis; with Shift (or a wheel turned sideways) it pans along x. A drag pans (a live chart stops following, and follows
// again when dragged back to its newest data); the right button drags a rectangle to zoom into. A double click shows what
// it showed first again (fitted, following). The mouse over it shows a line at the nearest x, a dot on each series there
// and their values by it.
//
// The y axis is at the start side (on the right in right to left); x grows to the right in either direction, as time
// does on a clock's chart.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Charts/SeriesBuffer.hpp>
#include <GLWidgets/Export.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace glw {

class LineChart;

enum class ChartAxisKind : std::uint8_t {
    Value,      // numbers
    Time,       // seconds since 1970 (chartTime): labels at round local times
};

class GLW_EXPORT LineSeries {
public:
    struct Props {
        LocalizedText name;             // none: not in the legend (an area's edge)
        std::optional<Color> color;     // none: the chart's style's, in turn
        float width = 0;                // of its line; 0: the style's
        bool area = false;              // an area under its line, of its color, fading down to the axis (or to 0)
        bool steps = false;             // a step at each point: a value held until the next
        bool markers = false;           // a dot at each point, when they are far enough apart to be told apart
        bool line = true;               // its line drawn (false: an area's edge only, its area if it has one)
        std::size_t capacity = 0;       // the newest kept, the oldest dropped as more come; 0: all
    };

    ~LineSeries();

    // A point at the end: its x not less than the last's for the series to stay sorted (a time series), quick for any
    // number of points then. A y that is NaN: a gap.
    void append(double x, double y);
    void append(std::span<const ChartPoint> points);
    void clear();
    const SeriesBuffer& data() const { return data_; }

    void setName(LocalizedText name);
    const LocalizedText& name() const { return props_.name; }
    void setColor(std::optional<Color> color);
    // Its color: its own, or the chart's style's for its place.
    Color color() const;
    void setArea(bool area);
    void setSteps(bool steps);
    void setWidth(float width);
    const Props& props() const { return props_; }
    // Hidden: faded out, and out of the y axis's fit (its legend item dimmed); what a click on its legend item does.
    void setVisible(bool visible);
    bool visible() const { return visible_; }

private:
    friend class LineChart;
    LineSeries(LineChart& chart, Props props);
    void changed();

    LineChart& chart_;
    Props props_;
    SeriesBuffer data_;
    bool visible_ = true;
    Animation shown_;               // 1 shown, 0 hidden: eased between
};

// An area between a series's line and another's, or down to the axis: of its own color, under the lines.
class GLW_EXPORT AreaSeries {
public:
    struct Props {
        LocalizedText name;
        const LineSeries* upper = nullptr;      // its top edge
        const LineSeries* lower = nullptr;      // its bottom edge; none: the axis (0, or the side of the plot its data is past)
        std::optional<Color> color;             // none: its upper series's
        float opacity = 0.35f;                  // of its color
        bool gradient = false;                  // fading down from its top edge
    };

    ~AreaSeries();

    const Props& props() const { return props_; }
    void setColor(std::optional<Color> color);
    void setOpacity(float opacity);
    // Hidden: faded out (its legend item dimmed); what a click on its legend item does.
    void setVisible(bool visible);
    bool visible() const { return visible_; }

private:
    friend class LineChart;
    AreaSeries(LineChart& chart, Props props);

    LineChart& chart_;
    Props props_;
    bool visible_ = true;
    Animation shown_;
};

class GLW_EXPORT LineChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        ChartAxisKind xAxis = ChartAxisKind::Value;
        // Its last `window` of x shown, following the newest data (a live chart); 0: all of it.
        double window = 0;
        // A y range of its own; none: fitted to what shows.
        std::optional<ChartRange> yRange;
        bool includeZero = false;           // a fitted y axis shows 0 too
    };

    LineChart();
    explicit LineChart(Props props);
    ~LineChart() override;

    LineSeries& addSeries(LineSeries::Props props);
    // A series taken off it, and the areas along it.
    void removeSeries(LineSeries& series);
    // An area along series of it (its upper one required).
    AreaSeries& addArea(AreaSeries::Props props);
    void removeArea(AreaSeries& area);
    std::span<const std::unique_ptr<AreaSeries>> areas() const { return areas_; }
    std::span<const std::unique_ptr<LineSeries>> series() const { return series_; }

    void setXAxisKind(ChartAxisKind kind);
    ChartAxisKind xAxisKind() const { return props_.xAxis; }
    void setWindow(double window);
    double window() const { return props_.window; }
    void setYRange(std::optional<ChartRange> range);
    void setIncludeZero(bool include);
    // How a tick's label and a tooltip show a value (a step apart from the next tick); none: formatNumber, formatTime.
    void setXFormatter(std::function<std::string(double value, double step)> format);
    void setYFormatter(std::function<std::string(double value, double step)> format);

    // ---- the view ----

    // Zoomed to `x` of x (no longer following), eased there.
    void setXView(ChartRange x);
    // What it showed first again: all of its data or its window, following; its y axis fitted (or its range).
    void resetView();
    // It shows its last window, following what comes.
    bool following() const;
    // What it shows now (eased: on the way to targetX); what it goes to.
    ChartRange xView() const;
    ChartRange yView() const;
    ChartRange targetXView() const;
    ChartRange targetYView() const;
    // Where its data is drawn (its own coordinates), as it was last painted.
    Rect plotArea() const { return plot_; }
    // From a point of the plot to values, and back.
    ChartPoint valueAt(Point at) const;
    Point pointOf(ChartPoint value) const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    friend class LineSeries;
    friend class AreaSeries;
    enum class XMode : std::uint8_t { Fit, Window, Manual };
    void dataChanged();
    // The x and y it goes to; the view eased there when it changed (at once when `immediate_`).
    ChartRange fitX() const;
    ChartRange fitY(ChartRange x) const;
    void updateView();
    double newestX() const;
    void paintSeries(PaintContext& context, const LineSeries& series, std::size_t index);
    void paintArea(PaintContext& context, const AreaSeries& area);
    // A series's line as it shows, in the plot's coordinates, left to right, its gaps left out: what each column of pixels
    // shows of it when it has more points than columns.
    void project(const LineSeries& series, float pixelsPerDip, std::vector<Point>& out) const;
    Color colorOf(const LineSeries& series) const;
    void paintHover(PaintContext& context);
    void zoomX(double factor, double anchor);
    void zoomY(double factor, double anchor);

    Props props_;
    std::vector<std::unique_ptr<LineSeries>> series_;
    std::vector<std::unique_ptr<AreaSeries>> areas_;

    XMode xMode_ = XMode::Fit;
    double window_ = 0;                 // following: what of x shows (the wheel zooms it)
    ChartRange userX_;
    std::optional<ChartRange> userY_;
    struct View {
        ChartRange fromX, toX, fromY, toY;
    };
    View view_;
    bool hasView_ = false;
    bool immediate_ = false;            // the next change of the view at once (a drag)
    bool quick_ = false;                // ... eased quickly (the wheel)
    Animation progress_;                // from 0 (view_.from) to 1 (view_.to)
    int yTickCount_ = 6;                // what fitY rounds to: as many as the plot is high for

    Rect plot_;
    Axis xAxis_, yAxis_;                // as they were last painted
    std::optional<Point> mouse_;
    enum class Drag : std::uint8_t { None, Pan, Box };
    Drag drag_ = Drag::None;
    Point dragFrom_;
    Point dragTo_;
    ChartRange dragX_, dragY_;
    bool dragFollowing_ = false;
    std::vector<Point> points_;         // scratch: a series as it is drawn
    std::vector<Rect> spans_;           // ... its columns' strokes, from their lowest to their highest
    std::vector<Point> lower_;          // ... an area's lower edge
    // What its legend's items are, in order: an area (true) or a series, and its place.
    mutable std::vector<std::pair<bool, std::size_t>> legendTargets_;
};

} // namespace glw
