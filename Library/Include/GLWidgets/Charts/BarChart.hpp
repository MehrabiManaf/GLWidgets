// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Bars of the values of series in categories: stacked (each category's bar of its series' values on each other, the
// negative ones below 0), grouped (side by side), or stacked as shares of the category's whole (Percent). Standing up
// (categories along x) or lying (categories down the side, for long names): setOrientation.
//
// Categories may come as they happen (appendCategory: a minute's, a day's), the oldest dropped past a capacity; a live
// chart shows its last `window` of them, following, and scrolls on smoothly as one comes. Values may change at any rate:
// the bars are eased to their new lengths, a series hidden (its legend item clicked) shrinks out of the stacks, and the
// value axis is fitted to what shows, ending on its ticks.
//
// With the mouse: the wheel zooms along the categories at the mouse (a live chart: its window, still following), a drag
// pans along them (a live chart follows again when dragged back to its newest), a double click shows what it showed
// first. The bar under the mouse is lit, its category's values and total by the mouse.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Layout.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

enum class BarLayout : std::uint8_t { Stacked, Grouped, Percent };

class GLW_EXPORT BarChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        BarLayout layout = BarLayout::Stacked;
        Orientation orientation = Orientation::Vertical;    // Vertical: bars standing up; Horizontal: lying
        std::size_t capacity = 0;       // the newest categories kept, the oldest dropped as more come; 0: all
        std::size_t window = 0;         // its last `window` categories shown, following (a live chart); 0: all
    };

    explicit BarChart(Props props = {});
    ~BarChart() override;

    // A series (its values 0 in every category): its index.
    std::size_t addSeries(LocalizedText name, std::optional<Color> color = std::nullopt);
    std::size_t seriesCount() const { return series_.size(); }
    void setSeriesVisible(std::size_t series, bool visible);
    bool seriesVisible(std::size_t series) const { return series < series_.size() && series_[series].visible; }
    Color seriesColorOf(std::size_t series) const;

    // Its categories (the values of those it did not have 0, of those it keeps kept).
    void setCategories(std::vector<LocalizedText> categories);
    // A category at the end with its series' values (in their order; those not given 0).
    void appendCategory(LocalizedText category, std::span<const double> values = {});
    std::size_t categoryCount() const { return categories_.size(); }
    const LocalizedText& category(std::size_t index) const { return categories_[index]; }
    void setValue(std::size_t series, std::size_t category, double value);
    void setValues(std::size_t series, std::span<const double> values);
    double value(std::size_t series, std::size_t category) const;

    void setLayout(BarLayout layout);
    BarLayout layout() const { return props_.layout; }
    void setOrientation(Orientation orientation);
    void setWindow(std::size_t window);
    // How a value shows (a tick, a step apart from the next; a tooltip's: step 0); none: formatNumber.
    void setFormatter(std::function<std::string(double value, double step)> format);

    // ---- the view ----

    // The categories shown: from `first` (fractions: a part of one), `count` of them (no longer following).
    void setCategoryView(double first, double count);
    void resetView();
    bool following() const { return mode_ == Mode::Window; }
    // What shows now (eased), in categories (0: the first kept) / in values.
    ChartRange categoryView() const;
    ChartRange valueView() const;
    // The bar under a point (its own coordinates), as it was last painted: its series and category.
    std::optional<std::pair<std::size_t, std::size_t>> barAt(Point at) const;
    Rect plotArea() const { return plot_; }

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    enum class Mode : std::uint8_t { Fit, Window, Manual };
    struct Series {
        LocalizedText name;
        std::optional<Color> color;
        bool visible = true;
        std::deque<double> values;
        std::deque<double> from, to;            // eased between
        std::unique_ptr<Animation> shown;       // 1 shown, 0 hidden (grouped: its bars' width)
    };
    double shownValue(const Series& s, std::size_t category) const;
    double target(const Series& s, std::size_t category) const;
    void changed();
    void settle();                              // what changed eased to, from what shows (once a frame)
    ChartRange fitCategories() const;           // in absolute categories (dropped_ on)
    ChartRange fitValues(ChartRange categories) const;
    void updateView();
    ChartRange absoluteView() const;
    std::string format(double value, double step) const;
    void zoom(double factor, double anchor);

    Props props_;
    std::vector<Series> series_;
    std::deque<LocalizedText> categories_;
    std::size_t dropped_ = 0;                   // categories dropped before the first kept
    std::function<std::string(double, double)> format_;
    bool dirty_ = false;
    Animation progress_;                        // values: from 0 (from) to 1 (to)

    Mode mode_ = Mode::Fit;
    double window_ = 0;
    ChartRange user_;                           // absolute categories
    struct View {
        ChartRange fromC, toC, fromV, toV;
    };
    View view_;
    bool hasView_ = false;
    bool immediate_ = false;
    bool quick_ = false;
    Animation viewProgress_;
    int valueTickCount_ = 6;

    Rect plot_;
    ChartTicks ticks_;
    struct Bar {
        Rect rect;
        std::size_t series, category;
    };
    std::vector<Bar> bars_;                     // as last painted
    std::optional<std::pair<std::size_t, std::size_t>> hovered_;
    std::optional<Point> mouse_;
    bool dragging_ = false;
    Point dragFrom_;
    ChartRange dragView_;
    bool dragFollowing_ = false;
};

} // namespace glw
