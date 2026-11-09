// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Points of two values each (a request's size and its latency, a price and its volume): series of them, each point a dot
// of its series's color (a bubble as large as a third value, if it has one: its area by it), both axes fitted to the
// points that show and eased there. Points come as they come (append: the oldest dropped past a capacity).
//
// More points in view than it draws as dots (Props::markers): an image of them instead (ImageStore), each pixel of the
// color of the series in it, as opaque as how many of its points fall there (a density: where they are many shows
// darker, a million of them cost what their pixels do); zoomed into fewer, dots again.
//
// A series's straight line through its points (least squares: Props::trend), its r² by its end. With the mouse, as a line
// chart: the wheel zooms at the mouse (with Ctrl the y axis only, with Shift the x axis only), a drag pans, the right
// button drags a rectangle to zoom into, a double click fits all again; the point nearest to it ringed, its values by it.
// A series's legend item hides it.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ImageStore.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

class ScatterChart;

class GLW_EXPORT ScatterSeries {
public:
    struct Props {
        LocalizedText name;
        std::optional<Color> color;         // none: the style's, in turn
        float radius = 0;                   // of its dots (its largest bubble's: four times it); 0: 3.5 dips
        std::size_t capacity = 0;           // the newest points kept; 0: all
        bool trend = false;                 // its least-squares line drawn
    };

    // A straight line through its points (y = intercept + slope x, least squares) and how much of y it accounts for.
    struct Fit {
        double slope = 0;
        double intercept = 0;
        double r2 = 0;
    };

    ~ScatterSeries();

    // A point; `size` a bubble's (its area by it), none: a dot.
    void append(double x, double y, double size = 0);
    void append(std::span<const ChartPoint> points);
    void clear();
    std::size_t size() const { return xs_.size(); }
    double x(std::size_t i) const { return xs_[i]; }
    double y(std::size_t i) const { return ys_[i]; }
    std::optional<Fit> fit() const;
    void setVisible(bool visible);
    bool visible() const { return visible_; }
    const Props& props() const { return props_; }

private:
    friend class ScatterChart;
    ScatterSeries(ScatterChart& chart, Props props);
    void take(double x, double y, double size, int by);
    void dropOldest();

    ScatterChart& chart_;
    Props props_;
    std::deque<double> xs_, ys_, sizes_;    // sizes_: empty until a point has one
    // for its fit (about their first point, so that large values keep their precision)
    double ox_ = 0, oy_ = 0;
    bool origin_ = false;
    double sx_ = 0, sy_ = 0, sxx_ = 0, sxy_ = 0, syy_ = 0;
    // its extents (found again when one of them goes)
    mutable bool extentKnown_ = true;
    mutable double lowX_ = 0, highX_ = 0, lowY_ = 0, highY_ = 0, largest_ = 0;
    bool visible_ = true;
    Animation shown_;
};

class GLW_EXPORT ScatterChart : public Chart {
public:
    enum class Drawn : std::uint8_t { Nothing, Markers, Density };

    struct Props {
        LocalizedText title;
        ChartAxisKind xAxis = ChartAxisKind::Value;
        std::optional<ChartRange> xRange;   // their own; none: fitted to the points
        std::optional<ChartRange> yRange;
        float opacity = 0.75f;              // of a dot
        std::size_t markers = 50000;        // more points in view: an image of their density
    };

    ScatterChart();
    explicit ScatterChart(Props props);
    ~ScatterChart() override;

    ScatterSeries& addSeries(ScatterSeries::Props props);
    std::span<const std::unique_ptr<ScatterSeries>> series() const { return series_; }
    // How a value shows (a step apart from the next tick); none: formatNumber, formatTime.
    void setXFormatter(std::function<std::string(double value, double step)> format);
    void setYFormatter(std::function<std::string(double value, double step)> format);

    // What shows (eased: on its way); zoomed to a view of one's own; fitted to all again.
    ChartRange xView() const;
    ChartRange yView() const;
    void setView(ChartRange x, ChartRange y);
    void resetView();
    Drawn drawn() const { return drawn_; }

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    friend class ScatterSeries;
    void changed();
    ChartRange fitX() const;
    ChartRange fitY() const;
    void updateView();
    Color colorOf(std::size_t index) const;
    void paintDensity(PaintContext& context, ChartRange xv, ChartRange yv);
    void zoom(double fx, double fy, Point at);

    Props props_;
    std::vector<std::unique_ptr<ScatterSeries>> series_;
    std::optional<ChartRange> userX_, userY_;
    struct View {
        ChartRange fromX, toX, fromY, toY;
    };
    View view_;
    bool hasView_ = false, immediate_ = false, quick_ = false;
    Animation progress_;
    std::uint64_t version_ = 0;

    Drawn drawn_ = Drawn::Nothing;
    ImageStore::Image image_;
    std::uint64_t imageKey_ = 0;
    std::vector<std::uint32_t> counts_;     // scratch: a series's points in each pixel
    std::vector<float> composite_;          // ... the image's colors, premultiplied (r, g, b, a a pixel)

    Rect plot_;
    Axis xAxis_, yAxis_;
    std::optional<Point> mouse_;
    enum class Drag : std::uint8_t { None, Pan, Box };
    Drag drag_ = Drag::None;
    Point dragFrom_, dragTo_;
    ChartRange dragX_, dragY_;
};

} // namespace glw
