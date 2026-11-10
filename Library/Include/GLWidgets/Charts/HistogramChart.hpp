// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How values fall: series of values (latencies, sizes, scores) counted into bins of round widths, a bar for each bin, its
// height their count (or their share). Values come as they come (add: the oldest dropped past a capacity) and are counted
// as they come: a value costs the same however many there are. Its bins span its values but for the farthest of them
// (Props::clip: from the 1st percentile to the 99th, so that a long tail does not crowd the rest into a few bars): those
// past them are in its first and last bins (their tooltips say so). They are found again only when many more values fall
// past them, or there are many more or fewer values than when they were found (as many as their number and the plot's
// width want, of a round width); or they are its own (Props::bins, Props::range).
//
// Over the bars, lines at quantiles of its first series (its median, its 90th and 99th percentiles: what a latency's
// service levels are) and at its mean, their values by them; its first series's share at or under each value as a line
// (cumulative), on an axis of percent at the end side. Several series overlaid, translucent. New counts are eased to; a
// series hidden by its legend item fades out. The mouse over a bin: it lit, its range and each series's count (and share)
// by it.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

class HistogramChart;

class GLW_EXPORT HistogramSeries {
public:
    struct Props {
        LocalizedText name;
        std::optional<Color> color;         // none: the style's, in turn
        std::size_t capacity = 0;           // the newest values kept; 0: all
    };

    ~HistogramSeries();

    void add(double value);
    void add(std::span<const double> values);
    void clear();
    std::size_t size() const { return values_.size(); }
    double mean() const;
    // A quantile of its values (0.5: the median), from its bins: within its bin, as if its values were spread evenly there.
    double quantile(double q) const;
    // How many of its values are in each bin.
    const std::vector<std::uint32_t>& counts() const { return counts_; }
    void setVisible(bool visible);
    bool visible() const { return visible_; }
    const Props& props() const { return props_; }

private:
    friend class HistogramChart;
    HistogramSeries(HistogramChart& chart, Props props);
    void count(double value, int by);

    HistogramChart& chart_;
    Props props_;
    std::deque<double> values_;
    double sum_ = 0;
    std::vector<std::uint32_t> counts_;
    std::size_t outside_ = 0;               // values past its chart's bins (in its first and last: found again when many)
    bool visible_ = true;
    Animation shown_;
};

class GLW_EXPORT HistogramChart : public Chart {
public:
    enum class Scale : std::uint8_t { Count, Percent };

    struct Props {
        LocalizedText title;
        int bins = 0;                       // how many; 0: as many as its values and its width want
        std::optional<ChartRange> range;    // what its bins span; none: its values, out to round values
        float clip = 0.99f;                 // ... from this quantile of them at the low end to this one at the high end
        Scale scale = Scale::Count;
        std::vector<double> quantiles = {0.5, 0.9, 0.99};     // lines at these quantiles of its first series
        bool mean = false;                  // ... and at its mean
        bool cumulative = false;            // its first series's share at or under each value, a line (axis: 0 to 100 %)
    };

    HistogramChart();
    explicit HistogramChart(Props props);
    ~HistogramChart() override;

    HistogramSeries& addSeries(HistogramSeries::Props props);
    std::span<const std::unique_ptr<HistogramSeries>> series() const { return series_; }

    void setScale(Scale scale);
    void setBins(int bins);
    void setRange(std::optional<ChartRange> range);
    void setQuantiles(std::vector<double> quantiles);
    void setCumulative(bool cumulative);
    // How a value shows (a step apart from the next tick); none: formatNumber.
    void setValueFormatter(std::function<std::string(double value, double step)> format);

    // Its bins: where the first begins, how wide each is, how many (found again first, if they are to be).
    double binStart() const;
    double binWidth() const;
    std::size_t binCount() const;
    // Which bin a value is in; none: past them.
    std::optional<std::size_t> binOf(double value) const;
    // Where its bins are drawn (its own coordinates), as it was last painted.
    Rect plotArea() const { return plot_; }

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    friend class HistogramSeries;
    void changed();
    // The bins for its values (and its width), all of them counted again; whether they are to be first.
    void rebin() const;
    bool stale() const;
    Color colorOf(std::size_t index) const;
    double height(const HistogramSeries& series, std::size_t bin) const;

    Props props_;
    std::vector<std::unique_ptr<HistogramSeries>> series_;
    std::function<std::string(double, double)> format_;

    // its bins: [start + i * width, start + (i + 1) * width)
    mutable double start_ = 0, width_ = 1;
    mutable std::size_t bins_ = 0;
    mutable std::size_t binnedAt_ = 0;      // how many values there were when they were found
    mutable bool clipped_ = false;          // its first and last bins hold the values past them
    mutable bool rebin_ = true;
    float plotWidth_ = 400;                 // as it was last painted: how many bins it has room for

    // the bars' heights, eased: from, to (series after series, bin after bin), and the y axis's top
    std::vector<double> from_, to_;
    double fromTop_ = 1, toTop_ = 1;
    std::size_t easedBins_ = 0;
    double easedStart_ = 0, easedWidth_ = 0;
    std::uint64_t version_ = 0, easedVersion_ = ~std::uint64_t{0};
    Animation progress_;

    Rect plot_;
    Axis xAxis_, yAxis_;
    std::optional<Point> mouse_;
    std::vector<Point> points_;
};

} // namespace glw
