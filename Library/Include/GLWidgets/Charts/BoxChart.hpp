// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// How the values of each category fall, side by side (Tukey's box plots): a box from the first quartile to the third, a
// line across it at the median, whiskers out to the last values within one and a half times the box's height of it (or to
// the lowest and highest), the values past them as dots (outliers), the mean a mark. Of each category's values (setValues,
// addValues: the oldest dropped past a capacity), found again when they change, or of a summary of one's own
// (setSummary: what a database or a service computed). Boxes raised as the style's bars are, each category of a series
// color in turn (or all of one); new values are eased to. The mouse over a box: it lit, its numbers by it.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

// A box's numbers: its whiskers' ends, its quartiles and median, its mean, the values past its whiskers, how many values.
struct BoxSummary {
    double low = 0;
    double q1 = 0;
    double median = 0;
    double q3 = 0;
    double high = 0;
    double mean = 0;
    std::vector<double> outliers;
    std::size_t count = 0;
};

class GLW_EXPORT BoxChart : public Chart {
public:
    enum class Whiskers : std::uint8_t {
        Tukey,          // to the last values within 1.5 times the box's height of it; those past them outliers
        Range,          // to the lowest and the highest
    };

    struct Props {
        LocalizedText title;
        std::vector<LocalizedText> categories;
        Whiskers whiskers = Whiskers::Tukey;
        std::size_t capacity = 0;           // each category's newest values kept; 0: all
        bool outliers = true;
        bool mean = true;
        std::optional<Color> color;         // all of one color; none: each category of a series color in turn
        bool includeZero = false;           // its axis shows 0 too
    };

    BoxChart();
    explicit BoxChart(Props props);
    ~BoxChart() override;

    void setCategories(std::vector<LocalizedText> categories);
    std::size_t categoryCount() const { return categories_.size(); }
    void setValues(std::size_t category, std::span<const double> values);
    void addValues(std::size_t category, std::span<const double> values);
    void setSummary(std::size_t category, BoxSummary summary);
    // A category's numbers now (found again, if its values changed).
    const BoxSummary& summary(std::size_t category) const;
    void setWhiskers(Whiskers whiskers);
    // How a value shows (a step apart from the next tick); none: formatNumber.
    void setValueFormatter(std::function<std::string(double value, double step)> format);

    // The numbers of `values` (sorted in place): its quartiles (linearly between values), its whiskers, its outliers.
    static BoxSummary summarize(std::span<double> values, Whiskers whiskers);

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    struct Category {
        LocalizedText name;
        std::deque<double> values;
        bool own = false;                   // a summary of one's own, not of values
        mutable bool dirty = false;
        mutable BoxSummary summary;
        // what it shows, eased: from, to (low, q1, median, q3, high, mean)
        std::array<double, 6> from{}, to{};
        bool shown = false;
    };
    void changed(Category& category);
    Color colorOf(std::size_t index) const;

    Props props_;
    std::vector<Category> categories_;
    std::function<std::string(double, double)> format_;
    Animation progress_;
    ChartRange fromRange_, toRange_;
    bool hasRange_ = false;
    Rect plot_;
    Axis axis_;
    std::vector<Rect> boxes_;               // where each category's box was painted
    std::optional<Point> mouse_;
    mutable std::vector<double> scratch_;
};

} // namespace glw
