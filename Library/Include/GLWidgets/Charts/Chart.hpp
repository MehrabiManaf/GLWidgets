// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What every chart has: a title over it, a legend under that (an item for each series, slice or group, which a click
// hides and shows again, eased), its colors and sizes from the theme (ChartStyle, Theme::chart: the series' colors from
// the color scheme, its bars and tiles raised as the style's controls are), a tooltip by the mouse, and motion: new data,
// a new view, a series hidden are eased to (the theme's motions, none in Plain), or shown at once when it is not
// animated (setAnimated).
//
// A chart takes the room it gets and uses all of it: its axes as wide as their labels, its ticks as many as fit, its
// legend in as many rows as it needs. Data may change as often as it comes (a live feed): a change only marks it to be
// painted again, and what shows is made of the data once a frame, from what of it shows.
#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace glw {

namespace text {
class TextLayout;
} // namespace text

class GLW_EXPORT Chart : public Widget {
public:
    ~Chart() override;

    void setTitle(LocalizedText title);
    const LocalizedText& title() const { return title_; }
    void setLegendVisible(bool visible);
    bool legendVisible() const { return legendVisible_; }
    // Eased to new data and to a new view (the theme's motions); at once when not. On by default.
    void setAnimated(bool animated) { animated_ = animated; }
    bool animated() const { return animated_; }
    // Its own; the theme's (Theme::chart) when none.
    void setStyle(std::optional<ChartStyle> style);
    const ChartStyle& style() const;
    // The color of the `index`th series (slice, group): the style's, in turn.
    Color seriesColor(std::size_t index) const;

protected:
    Chart();

    // ---- for the charts ----

    // What its legend shows, in order: a text, its color, whether it shows (dimmed when it does not). None: no legend.
    struct LegendItem {
        std::string text;
        Color color;
        bool shown = true;
    };
    virtual std::vector<LegendItem> legendItems() const { return {}; }
    virtual void legendItemClicked(std::size_t index) {}

    // Its background, its title and its legend painted; what is left of it for the data (its own coordinates).
    Rect paintFrame(PaintContext& context);
    // A legend's item clicked (a press on it): handled. What the charts' onMouse ask first.
    bool legendMouse(MouseEvent& event);

    // A text of one line `size` dips high (the style's font size when 0), laid out once and kept.
    const text::TextLayout* textLayout(const std::string& text, float size = 0) const;
    Size textSize(const std::string& text, float size = 0) const;
    // ... drawn with its point `align` of the way across and down it at `at` (0, 0: its top left; 0.5, 0.5: its middle).
    void drawText(PaintContext& context, const std::string& text, Point at, Color color, float size = 0, Point align = {}) const;

    // A tooltip by `at` (kept in its bounds): a title, and rows of a swatch of a color (none: none), a name and a value.
    struct ToolTipRow {
        std::optional<Color> swatch;
        std::string name;
        std::string value;
    };
    void paintToolTip(PaintContext& context, Point at, const std::string& title, std::span<const ToolTipRow> rows) const;

    // An axis of values or of times (a line chart's x and y, a bar chart's values, a race's): its ticks at round values for
    // a range and a length, as many as their labels fit (along a horizontal one, apart by their widths); their labels;
    // its grid across the plot and its labels beside it; a value as a tooltip shows it. What the charts' axes are made
    // with, so that they all look and fit alike.
    class GLW_EXPORT Axis {
    public:
        enum class Side : std::uint8_t { Left, Right, Top, Bottom };       // where its labels are, beside the plot

        Orientation orientation = Orientation::Horizontal;  // along x (Horizontal), or along y
        bool time = false;                                  // seconds since 1970: round local times (TimeTicks)
        bool reversed = false;      // a horizontal one growing to the left (right to left); a vertical one downward
        std::string suffix;                                 // after each label ("%")
        std::function<std::string(double value, double step)> format;     // none: formatNumber, formatTime

        // Its ticks for `range` along `length` dips.
        void fit(const Chart& chart, ChartRange range, float length);
        const std::vector<double>& values() const { return values_; }
        const std::vector<std::string>& labels() const { return labels_; }
        double step() const { return step_; }
        int decimals() const { return decimals_; }
        // How far its labels reach across it: the widest (a vertical one's), or a line's height.
        float labelWidth() const { return widest_; }
        float labelHeight() const { return height_; }
        // Where a value is from `start` over `length` dips.
        float at(double value, float start, float length) const;
        // Lines across `plot` at its ticks, sharp on the pixels; its labels beside `plot` at `side`, those that would go out
        // of `bounds` left out.
        void paintGrid(paint::Painter& painter, const Rect& plot, Color color) const;
        void paintLabels(PaintContext& context, const Chart& chart, const Rect& plot, Side side, Color color, const Rect& bounds) const;
        // A value as a tooltip shows it: a decimal more than its ticks; a time to the second (to the millisecond when its
        // ticks are), its day too when more than a day shows.
        std::string precise(double value) const;

    private:
        std::string label(double value) const;
        ChartRange range_;
        std::vector<double> values_;
        std::vector<std::string> labels_;
        double step_ = 1;
        int decimals_ = 0;
        TimeTicks timeTicks_;
        float widest_ = 0;
        float height_ = 0;
    };

    // A sector of a ring raised as the style's bars are (a pie's slice, a gauge's arc): a relief (Plastic, Metal, Fluid;
    // `lift` times as high: the slice under the mouse; its corners as round as the bars'), lit from above with a light
    // line along its top (Studio), flat (Plain). Painter::fillArc's angles and gap.
    void paintArc(paint::Painter& painter, Point center, float inner, float outer, float start, float sweep, float inset, Color color,
                  float lift = 1) const;

    // How long a change is eased over: the theme's normal motion (fast: its fast one), none when it is not animated or
    // not shown.
    Duration transition(bool fast = false) const;

    // A pixel's edge near `v`, at `pixelsPerDip`: thin lines drawn there are sharp.
    static float snap(float v, float pixelsPerDip);
    // Black or white, the one that reads on `background`.
    static Color textOn(Color background);

    void onStyleChanged() override;
    void onRootChanged() override;
    void onLanguageChanged() override;
    Size measureContent(Size available) override;

private:
    LocalizedText title_;
    bool legendVisible_ = true;
    bool animated_ = true;
    std::optional<ChartStyle> style_;
    struct Item {
        Rect rect;
        std::size_t index;
    };
    std::vector<Item> legendRects_;                     // where its legend's items were painted
    mutable std::unordered_map<std::string, std::unique_ptr<text::TextLayout>> texts_;
};

} // namespace glw
