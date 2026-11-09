// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A matrix of values as colors: rows (named: servers, endpoints, cores; the first at the top) across columns (of times, of
// values, or named), each cell of its value's color on a color scale (ColorScale: from the theme, or its own stops), the
// scale beside it as a bar with its values. Columns come as they come (appendColumn: the oldest dropped past a capacity;
// a live one shows its last window, following), or all at once (setValues: a matrix).
//
// As many cells as it has: large cells are drawn one by one, round as the style's bars, their values in them when they
// fit, eased to new values; cells smaller than a few dips are an image (ImageStore) of buckets of columns and rows, a
// pixel each at least, of the highest of their values (a spike stays seen; or their mean), kept from frame to frame:
// only the columns that came or changed are made again, the view moves and scales the image. Its cost is its pixels',
// not its cells'.
//
// With the mouse, as a line chart: the wheel zooms the columns at the mouse (with Ctrl the rows; with Shift it pans), a
// drag pans (a live one follows again when dragged back to its newest), the right button drags a rectangle to zoom into, a
// double click shows all again; the cell under it outlined (its row and column), its value by it; a click on a cell is
// told (setOnCellClicked). Over the scale's bar: only the cells of the values there stand out.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/ImageStore.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

// How values become colors: from its range's low end to its high end, through stops evenly apart.
struct ColorScale {
    enum class Kind : std::uint8_t {
        Sequential,     // from the plot's color, faintly of the accent, through the accent, to a light (dark: a dark) end
        Diverging,      // from the second series color through a neutral one (at `center`) to the accent
        Custom,         // `stops`
    };
    Kind kind = Kind::Sequential;
    std::vector<Color> stops;               // Custom's, from low to high
    std::optional<ChartRange> range;        // its own; none: fitted to the values, out to round values
    double center = 0;                      // Diverging's neutral value: its range as far below it as above
    bool logarithmic = false;               // by powers of ten (values at or under 0: its low end)
    float clip = 1;                         // a fitted range up to this quantile of the values (0.99: the top hundredth at its end)
};

class GLW_EXPORT HeatMap : public Chart {
public:
    // Cells under a pixel: the highest of them (a spike stays seen), or their mean.
    enum class Reduce : std::uint8_t { Max, Mean };
    // How the last paint drew its cells.
    enum class Drawn : std::uint8_t { Nothing, Cells, Image };

    struct Props {
        LocalizedText title;
        std::vector<LocalizedText> rows;        // their names, from the top; none: as many as a column's values, numbered
        // The columns' x: `start + index * step` (Value, Time: seconds since 1970); named columns (setColumnNames): their
        // names.
        ChartAxisKind xAxis = ChartAxisKind::Value;
        double start = 0;
        double step = 1;
        std::size_t capacity = 0;               // the newest columns kept; 0: all
        double window = 0;                      // its last `window` of x shown, following the newest column; 0: all
        ColorScale scale;
        Reduce reduce = Reduce::Max;
        bool values = true;                     // a cell's value in it, when it fits
    };

    HeatMap();
    explicit HeatMap(Props props);
    ~HeatMap() override;

    // ---- data ----

    void setRows(std::vector<LocalizedText> rows);
    std::size_t rowCount() const { return rows_; }
    void setColumnNames(std::vector<LocalizedText> names);
    // A column after the last: a value for each row (NaN: none there; missing ones none, more than its rows left out).
    void appendColumn(std::span<const double> values);
    // All of it: `columns` columns, column after column (values[column * rows + row]).
    void setValues(std::size_t columns, std::span<const double> values);
    // A cell of a column it has (its index: counted from the first ever appended, firstColumn() the oldest kept).
    void setValue(std::size_t column, std::size_t row, double value);
    void clear();
    std::size_t firstColumn() const { return first_; }
    std::size_t columnCount() const { return columns_.size(); }
    double value(std::size_t column, std::size_t row) const;
    // A column's x (its start; a fraction of a column: within it).
    double xOf(double column) const { return props_.start + column * props_.step; }

    // ---- its look ----

    void setScale(ColorScale scale);
    const ColorScale& scale() const { return props_.scale; }
    void setReduce(Reduce reduce);
    // What its colors go from and to now; a value's color.
    ChartRange colorRange() const;
    Color colorOf(double value) const;
    void setXFormatter(std::function<std::string(double value, double step)> format);
    void setValueFormatter(std::function<std::string(double value)> format);
    void setOnCellClicked(std::function<void(std::size_t column, std::size_t row)> clicked);

    // ---- what shows ----

    void setWindow(double window);
    // In columns' and rows' indices (eased: on its way to what it shows next).
    ChartRange columnView() const;
    ChartRange rowView() const;
    void setColumnView(ChartRange columns);
    void resetView();
    bool following() const;
    Drawn drawn() const { return drawn_; }

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;

private:
    enum class XMode : std::uint8_t { Fit, Window, Manual };
    enum class Drag : std::uint8_t { None, Pan, Box };

    void snapshot();                            // before a change: what its large cells show, to ease from
    void dataChanged(std::size_t column);       // a column changed (SIZE_MAX: all of it)
    void fitColors() const;
    void buildColors(const PaintContext& context);
    double fraction(double value) const;        // where a value is on the scale, 0 to 1
    ChartRange fitX() const;
    ChartRange fitY() const;
    void updateView();
    std::string format(double value, bool precise) const;
    std::string columnLabel(std::size_t column) const;
    std::optional<std::pair<std::size_t, std::size_t>> cellAt(Point at) const;
    void paintCells(PaintContext& context, ChartRange xv, ChartRange yv);
    void paintImage(PaintContext& context, ChartRange xv, ChartRange yv);
    void paintColumns(PaintContext& context, ChartRange xv, const Rect& area);
    void paintRowLabels(PaintContext& context, ChartRange yv, const Rect& labels);
    void paintScale(PaintContext& context, const Rect& bar, const Rect& area);
    void paintHover(PaintContext& context, ChartRange xv, ChartRange yv);
    void zoomX(double factor, double anchor);
    void zoomY(double factor, double anchor);

    Props props_;
    std::size_t rows_ = 0;
    std::vector<LocalizedText> columnNames_;
    std::deque<std::vector<float>> columns_;
    std::size_t first_ = 0;
    std::function<std::string(double, double)> xFormat_;
    std::function<std::string(double)> valueFormat_;
    std::function<void(std::size_t, std::size_t)> clicked_;

    // its colors: the range fitted (exactly: the lowest and highest kept as they come), a table of 256
    mutable bool extremesKnown_ = false;
    mutable double low_ = 0, high_ = 0;
    mutable bool rangeKnown_ = false;
    mutable ChartRange range_;
    std::vector<Color> table_;
    std::vector<std::uint32_t> pixels_;
    std::uint64_t tableKey_ = 0;                // what the table was made of (its range, its scale, the theme, the band)
    std::optional<std::pair<float, float>> band_;   // the scale's part under the mouse: only its cells stand out

    // the view (as a line chart's)
    struct View {
        ChartRange fromX, toX, fromY, toY;
    };
    XMode xMode_ = XMode::Fit;
    ChartRange userX_;
    std::optional<ChartRange> userY_;
    double window_ = 0;
    View view_;
    bool hasView_ = false, immediate_ = false, quick_ = false;
    Animation progress_;

    // large cells eased to new values: what they showed when the values changed
    std::vector<float> from_;
    std::size_t fromFirst_ = 0, fromColumns_ = 0;
    Animation cellsProgress_;
    bool easing_ = false;

    // small cells: an image of buckets of `bucketColumns` columns (a ring of them: bucket b at b % width) and of
    // `bucketRows` rows from `rowBase`; which bucket each of its columns holds
    ImageStore::Image image_;
    std::int64_t bucketColumns_ = 0, bucketRows_ = 0, rowBase_ = 0;
    std::uint64_t imageKey_ = 0;
    std::vector<std::int64_t> held_;
    std::vector<std::size_t> changed_;          // the columns that changed since the last paint
    bool allChanged_ = true;
    std::vector<float> sums_;                   // scratch
    std::vector<std::uint32_t> counts_;

    Drawn drawn_ = Drawn::Nothing;
    Rect plot_, bar_;
    Axis xAxis_, scaleAxis_;
    std::optional<Point> mouse_;
    Drag drag_ = Drag::None;
    Point dragFrom_, dragTo_;
    ChartRange dragX_, dragY_;
    bool dragged_ = false;
};

} // namespace glw
