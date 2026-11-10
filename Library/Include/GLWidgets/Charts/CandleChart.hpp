// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A price over time as candles: for each period its open, high, low and close (a body from the open to the close, a wick
// from the low to the high; of the style's rise color when it closed higher than it opened, of its fall color when lower),
// or as bars (OHLC: a tick to the left at the open, one to the right at the close); its volume as bars along the bottom,
// on a scale of their own. Prices come as they come: a candle at a time (append), or trade by trade (trade: into the
// candle of its period, a new one when its period begins), the last one growing as they do.
//
// Its period (CandlePeriod): seconds, minutes or hours counted from local midnight, or days, weeks (from a day of one's
// own), months or years of the local calendar. The one its candles come in (Props::period: kept as they are given), or a
// longer one made of them (setPeriod; Props::periods, buttons on its title's row to choose one): its candles merged into
// those of the longer one, at once and as they come, so that a minute's candles and a month's are of the same data. Averages of its closes over its candles (addAverage: of the
// period that shows, made again when it changes); lines of one's own over it (addLine: of times). Each, and its volume,
// an item of its legend: a click hides it and shows it again, eased.
//
// What shows: its last `window` candles, following the newest (a live chart), or all of them; the price axis at the end
// side (as traders read it), fitted to the highs and lows that show and eased there. The last close a line across it, its
// value on the axis in its candle's color. More candles than pixels: each column of pixels one candle of those in it (the
// first's open, the highest high, the lowest low, the last's close, their volumes summed), as one of a longer period
// would be. Its x is the candles' places, not their times: a night without trades takes no room.
//
// With the mouse, as a line chart: the wheel zooms at the mouse (a live one: its window, still following; with Shift it
// pans), a drag pans (a live one follows again when dragged back to its newest), a double click shows what it showed first
// again. The mouse over it: a crosshair, the price at it on the axis, and the candle under it (its open, high, low, close,
// change and volume) in a tooltip.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/SeriesBuffer.hpp>
#include <GLWidgets/Export.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace glw {

struct Candle {
    double time = 0;            // its period's start: seconds since 1970 (chartTime), or any value
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
};

// A candle's period: so many seconds, minutes or hours (counted from local midnight, so that an hour's candles begin on
// the hour wherever one is), or days, weeks, months or years of the local calendar.
struct GLW_EXPORT CandlePeriod {
    enum class Unit : std::uint8_t { Second, Minute, Hour, Day, Week, Month, Year };
    Unit unit = Unit::Minute;
    int count = 1;

    constexpr CandlePeriod() = default;
    constexpr CandlePeriod(Unit unit, int count) : unit(unit), count(std::max(1, count)) {}
    // So many seconds, rounded: in the largest unit they are a whole number of (604800: a week; 86400: a day; 3600: an
    // hour).
    CandlePeriod(double seconds);
    static constexpr CandlePeriod seconds(int n) { return {Unit::Second, n}; }
    static constexpr CandlePeriod minutes(int n) { return {Unit::Minute, n}; }
    static constexpr CandlePeriod hours(int n) { return {Unit::Hour, n}; }
    static constexpr CandlePeriod days(int n) { return {Unit::Day, n}; }
    static constexpr CandlePeriod weeks(int n) { return {Unit::Week, n}; }
    static constexpr CandlePeriod months(int n) { return {Unit::Month, n}; }
    static constexpr CandlePeriod years(int n) { return {Unit::Year, n}; }

    // About how long it is, in seconds (a month: a mean one's).
    double length() const;
    // The start of the one `time` is in (seconds since 1970), of the local calendar, its weeks from `weekStart` (0:
    // Sunday, 1: Monday, 6: Saturday), its months and years those of `calendar` (a Solar Hijri month from its first);
    // the start of the one after the one that starts at `start`.
    double start(double time, int weekStart = 1, Calendar calendar = Calendar::Gregorian) const;
    double next(double start, Calendar calendar = Calendar::Gregorian) const;
    // Whether candles of `base` make it up: each of its candles all of some of theirs (minutes of seconds, a month of
    // days; not a month of weeks).
    bool madeOf(const CandlePeriod& base) const;
    // Its name on its button: 5s, 15m, 4h, 1D, 1W, 1M, 1Y.
    std::string name() const;

    friend bool operator==(const CandlePeriod&, const CandlePeriod&) = default;
};

class GLW_EXPORT CandleChart : public Chart {
public:
    enum class Kind : std::uint8_t { Candles, Bars };

    struct Props {
        LocalizedText title;
        Kind kind = Kind::Candles;
        ChartAxisKind xAxis = ChartAxisKind::Time;
        CandlePeriod period = CandlePeriod::minutes(1);     // of the candles it is given: the shortest it shows
        // Periods to choose from (each made of `period`'s candles), buttons on its title's row; none: no buttons.
        std::vector<CandlePeriod> periods;
        // the day a week begins on (0 Sunday, 1 Monday, 6 Saturday), and the calendar of its months and years; none: its
        // locale's (Chart::locale: the window's language's)
        std::optional<int> weekStart;
        std::optional<Calendar> calendar;
        std::size_t capacity = 0;           // the newest candles of `period` kept; 0: all
        std::size_t window = 0;             // its last `window` candles shown, following the newest; 0: all
        bool volume = true;                 // its volume as bars along the bottom
        std::optional<Color> rise;          // none: the style's
        std::optional<Color> fall;
    };

    // A line over it: of points appended as they come (of times), or the average of its last `count` closes.
    class GLW_EXPORT Line {
    public:
        void append(double x, double y);
        void clear();
        const SeriesBuffer& data() const { return data_; }
        const LocalizedText& name() const { return name_; }
        std::size_t averageOf() const { return count_; }
        // Hidden: faded out (its legend item dimmed); what a click on its legend item does.
        void setVisible(bool visible);
        bool visible() const { return visible_; }

    private:
        friend class CandleChart;
        Line(CandleChart& chart, LocalizedText name, std::optional<Color> color, std::size_t capacity, std::size_t count);
        CandleChart& chart_;
        LocalizedText name_;
        std::optional<Color> color_;
        SeriesBuffer data_;
        std::size_t count_ = 0;
        bool visible_ = true;
        Animation shown_;
    };

    CandleChart();
    explicit CandleChart(Props props);
    ~CandleChart() override;

    // ---- data (of `Props::period`) ----

    // A candle after the last (its time after the last's); the oldest dropped past its capacity.
    void append(const Candle& candle);
    // The last one again (a period still going on).
    void setLast(const Candle& candle);
    // A trade: into the candle of its period (its high, low, close and volume), a new candle when its period has begun.
    void trade(double time, double price, double volume = 0);
    void clear();
    // The candles of the period that shows: how many, each, the last.
    std::size_t count() const { return candles_.size(); }
    const Candle& candle(std::size_t index) const { return candles_[index]; }
    const Candle* last() const { return candles_.empty() ? nullptr : &candles_.back(); }
    // The average of the last `count` closes of the period that shows; none named: "MA count".
    Line& addAverage(std::size_t count, std::optional<Color> color = {}, LocalizedText name = {});
    // A line of points of its own (times and values).
    Line& addLine(LocalizedText name, std::optional<Color> color = {}, std::size_t capacity = 0);

    // ---- its look ----

    // The period that shows (made of Props::period's candles, merged; one that is not: left as it is), what it shows
    // following the newest again.
    void setPeriod(CandlePeriod period);
    CandlePeriod period() const { return period_; }
    void setKind(Kind kind);
    Kind kind() const { return props_.kind; }
    void setVolumeVisible(bool visible);
    bool volumeVisible() const { return props_.volume; }
    // How a price and a volume show (a step apart from the next tick); none: formatNumber.
    void setPriceFormatter(std::function<std::string(double value, double step)> format);
    void setVolumeFormatter(std::function<std::string(double value)> format);

    // ---- what shows ----

    void setWindow(std::size_t candles);
    // In candles' indices (eased: on its way); the prices it shows.
    ChartRange indexView() const;
    ChartRange priceView() const;
    void setIndexView(ChartRange view);
    void resetView();
    bool following() const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    // Its weeks' first day and its calendar: its own, or its locale's.
    int weekStart() const { return props_.weekStart.value_or(locale().firstDayOfWeek); }
    Calendar calendar() const { return props_.calendar.value_or(locale().calendar); }
    enum class XMode : std::uint8_t { Fit, Window, Manual };
    // The last of the period that shows made again of the given candles in it; the oldest given one dropped (the first
    // that shows made again of those left in it, or gone); all of those that show made again (a new period).
    void refreshLast();
    void dropFront();
    void rebuild();
    ChartRange fitIndex() const;
    ChartRange fitPrice(ChartRange index) const;
    void updateView();
    std::string price(double value) const;
    std::string volume(double value) const;
    Color riseColor() const;
    Color fallColor() const;
    Color lineColor(std::size_t index) const;
    // The candles that show, a column of pixels at a time when there are more than columns: (index, candle) each.
    void gather(ChartRange view, double columns);
    void paintPeriods(PaintContext& context);
    void zoom(double factor, double anchor);

    Props props_;
    std::deque<Candle> given_;              // of Props::period
    std::deque<Candle> candles_;            // of the period that shows
    CandlePeriod period_;
    std::size_t first_ = 0;                 // the index of the oldest that shows (those before dropped)
    std::vector<std::unique_ptr<Line>> lines_;
    std::function<std::string(double, double)> priceFormat_;
    std::function<std::string(double)> volumeFormat_;
    Animation volumeShown_;

    XMode xMode_ = XMode::Fit;
    std::size_t window_ = 0;
    ChartRange userX_;
    struct View {
        ChartRange fromX, toX, fromY, toY;
    };
    View view_;
    bool hasView_ = false, immediate_ = false, quick_ = false;
    Animation progress_;

    struct Shown {
        double index;           // where it is (a column's middle when it stands for several)
        double width;           // how many candles' room it has
        Candle candle;
    };
    std::vector<Shown> shown_;
    struct Button {
        Rect rect;
        CandlePeriod period;
    };
    std::vector<Button> buttons_;
    Rect plot_, volumes_;
    Axis timeAxis_, priceAxis_;
    std::optional<Point> mouse_;
    bool dragging_ = false;
    Point dragFrom_;
    ChartRange dragX_;
    std::vector<Point> points_;
    std::vector<double> averages_;          // scratch
};

} // namespace glw
