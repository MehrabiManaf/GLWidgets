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
#include <GLWidgets/Paint/Painter.hpp>
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
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glw {

namespace text {
class TextLayout;
} // namespace text

// Something marked on a chart's plot at values of its axes (Chart::addAnnotation): a line across it at an x (an event:
// a deploy, a release) or at a y (a threshold, a target), a band between two of them (a maintenance window, a normal
// range), or a point with a label. Bands lie under the data, lines and points over it; each has its label by it. They
// move with the view (zoomed, panned, eased) as the data does. Line, scatter, candle (x: times), histogram (x: values,
// y: counts) and bar charts (x: a category's place among those it holds, 0 the first's start; y: values) show them.
struct ChartAnnotation {
    enum class Kind : std::uint8_t { XLine, YLine, XBand, YBand, Point };
    Kind kind = Kind::YLine;
    double from = 0;                // a line's value; a band's first; a point's x
    double to = 0;                  // a band's last; a point's y
    LocalizedText label;
    std::optional<Color> color;     // none: the style's text color (a band: faint)
    bool dashed = false;            // a line's
    bool secondAxis = false;        // its y on the second y axis (a line chart's)
};

class Chart;

// A crosshair shared by charts over the same x (a dashboard's panels over the same time): the mouse over one of them
// shows a line at its x in the others too, a dot on each of their series there (its tooltip in the one under the mouse
// only). Charts join one by Chart::setCursor; line, candle and histogram charts show it (a bar chart: by category,
// shared with bar charts of the same categories only).
//
//   auto cursor = std::make_shared<glw::ChartCursor>();
//   cpu->setCursor(cursor);
//   traffic->setCursor(cursor);
class GLW_EXPORT ChartCursor {
public:
    ChartCursor() = default;
    ChartCursor(const ChartCursor&) = delete;
    ChartCursor& operator=(const ChartCursor&) = delete;

    // Where the mouse is along x over a chart of it, and that chart; none: over none of them.
    std::optional<double> x() const { return x_; }
    const Chart* from() const { return from_; }
    // ... set by the chart the mouse is over (none: it left): the others painted again.
    void set(const Chart* from, std::optional<double> x);

private:
    friend class Chart;
    std::vector<Chart*> charts_;
    const Chart* from_ = nullptr;
    std::optional<double> x_;
};

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

    // Marks on its plot (ChartAnnotation): added (its id, to take it off again), taken off, all of them.
    std::size_t addAnnotation(ChartAnnotation annotation);
    void removeAnnotation(std::size_t id);
    void clearAnnotations();
    std::size_t annotationCount() const { return annotations_.size(); }

    // The crosshair it shares with other charts (ChartCursor); none: its own only.
    void setCursor(std::shared_ptr<ChartCursor> cursor);
    const std::shared_ptr<ChartCursor>& cursor() const { return cursor_; }

    // Its pieces (bars, tiles, slices) coming in again, as when it first showed them.
    void playEntrance();

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
        bool log = false;           // logarithmic (base 10): its range above 0, its ticks at 1, 2, 5 of each decade
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
        const Locale* locale_ = &Locale::english();         // its chart's, as it was fitted
        float widest_ = 0;
        float height_ = 0;
    };

    // Its annotations over `plot` by its axes `x` and `y` (`y2`: the second y axis's, when it has one): the bands
    // (`above` false: under the data) or the lines and points (true: over it), with their labels; cut to the plot.
    void paintAnnotations(PaintContext& context, const Rect& plot, const Axis& x, const Axis& y, bool above, const Axis* y2 = nullptr) const;
    // ... where a chart places values itself: `x` an x's place, `y` a y's (on the second axis when it says so); `lying`:
    // its x down the plot and its y across it (a bar chart lying on its side).
    void paintAnnotations(PaintContext& context, const Rect& plot, const std::function<float(double)>& x,
                          const std::function<float(double, bool)>& y, bool above, bool lying = false) const;

    // A sector of a ring raised as the style's bars are (a pie's slice, a gauge's arc): a relief (Plastic, Metal, Fluid;
    // `lift` times as high: the slice under the mouse; its corners as round as the bars'), lit from above with a light
    // line along its top (Studio), flat (Plain). Painter::fillArc's angles and gap.
    void paintArc(paint::Painter& painter, Point center, float inner, float outer, float start, float sweep, float inset, Color color,
                  float lift = 1) const;

    // The mouse along x over it (none: it left), for the charts that share its cursor; where the mouse is over another
    // of them (none: over none, or over this one).
    void shareCursor(std::optional<double> x) const;
    std::optional<double> sharedX() const;

    // How numbers and times are written in it: its window's language's (Translator::locale), English's outside one.
    const Locale& locale() const;
    // The axes' formats (ChartAxis.hpp) in its locale: its own, so that a chart's code calls them by their names.
    std::string formatNumber(double value, int decimals, double step = 0) const { return glw::formatNumber(value, decimals, step, locale()); }
    std::string formatTime(double seconds, const TimeTicks& ticks) const { return glw::formatTime(seconds, ticks, locale()); }
    std::string formatTimeFully(double seconds, TimeUnit unit, bool date) const { return glw::formatTimeFully(seconds, unit, date, locale()); }
    TimeTicks timeTicks(ChartRange seconds, int count) const { return glw::timeTicks(seconds, count, locale()); }

    // How long a change is eased over: the theme's normal motion (fast: its fast one), none when it is not animated or
    // not shown.
    Duration transition(bool fast = false) const;

    // ---- its pieces coming in ----
    // The first time it shows something (and again at playEntrance) its pieces come in one after another, one of three
    // ways (at random where the theme varies its motions, MotionStyle::varied; the first otherwise), over the style's
    // entrance time: where its pieces are solids (the theme's relief bricks), in space: dropping in from in front,
    // swinging down onto the plot from standing toward the one looking, rising from behind it; otherwise in its plane:
    // growing out of where they stand, sliding in, settling from a little larger.
    // Begun by the chart's paint once it has something to show (nothing when it is not animated); once.
    void beginEntrance();
    bool entering() const { return entering_; }
    // How far all of it has come (0 to 1; 1 when it is not coming in), and which of the ways it comes this time (0 to 2).
    float entranceProgress() const;
    int entranceWay() const { return entranceWay_; }
    // How far a piece whose turn is at `order` (0 the first, 1 the last) has come, from 0 to 1 (1 when none comes).
    float arrived(float order) const;
    // Its pieces are solids (they come in space).
    bool solidPieces() const;
    // What was drawn of a piece since `mark` (Painter::markHere) placed as it comes, `u` of its way (arrived): `rect` its
    // bounds; `base` where it stands (a bar's middle on its axis, a slice's on its inner edge; a tile: none, its top);
    // `out` the way out of there (a bar's up from its axis, a slice's out from the middle).
    void placeArriving(paint::Painter& painter, const paint::Painter::Mark& mark, float u, const Rect& rect, std::optional<Point> base = {},
                       Point out = {}) const;

    // ---- the piece under the mouse ----
    // Where its pieces are solids, the one under the mouse (`key`, its own; none: none) is lifted toward the one looking,
    // eased up, catching the light; the one that was, eased down. Told by the chart's paint, each time.
    // `press`: where the mouse is on it, for a piece that leans there (placeLifted's).
    void liftPiece(std::optional<std::string> key, std::optional<Point> press = {});
    // Some piece is lifted (or coming down): pieces are looked up (pieceLift).
    bool lifting() const { return !lifted_.empty() || lowering_.value() > 0; }
    // How far `key` is lifted (0 to 1); where the mouse pressed it (the last it was, while it comes down).
    float pieceLift(std::string_view key) const;
    std::optional<Point> piecePress(std::string_view key) const;
    // What was drawn of a piece since `mark` (Painter::markHere), `rect` its bounds, lifted `amount` (pieceLift); with
    // `press`, leaning there, as if the mouse were a weight on it (that side lower, the far one higher), the more the
    // further out it is from the middle.
    void placeLifted(paint::Painter& painter, const paint::Painter::Mark& mark, float amount, const Rect& rect,
                     std::optional<Point> press = {}) const;

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
    std::vector<std::pair<std::size_t, ChartAnnotation>> annotations_;      // by their ids
    std::size_t nextAnnotation_ = 1;
    std::shared_ptr<ChartCursor> cursor_;
    mutable std::unordered_map<std::string, std::unique_ptr<text::TextLayout>> texts_;
    std::string lifted_;
    std::string lowered_;
    std::optional<Point> liftedPress_;
    std::optional<Point> loweredPress_;
    Animation lifting_{[this] { invalidatePaint(); }};
    Animation lowering_{[this] { invalidatePaint(); }};
    bool entered_ = false;
    bool entering_ = false;
    int entranceWay_ = 0;
    Animation entrance_{[this] {
        if (entrance_.value() >= 1)
            entering_ = false;
        invalidatePaint();
    }};
};

} // namespace glw
