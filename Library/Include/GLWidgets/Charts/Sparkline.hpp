// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A small chart without axes: a measure's recent values at a glance, beside a number or in a table's cell. A line (an
// area fading under it, its last value a dot) or columns (those under 0 below its line), its lowest and highest dots too
// if asked, a level across it (a target, a limit). Its values as they come (append: the oldest dropped past a capacity;
// until it has as many, they fill it from the left),
// its range fitted to them (eased) or its own; the value under the mouse by it. More values than it has columns of pixels:
// each column what it shows, as a line chart's are.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Charts/SeriesBuffer.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

class GLW_EXPORT Sparkline : public Chart {
public:
    enum class Kind : std::uint8_t { Line, Columns };

    struct Props {
        Kind kind = Kind::Line;
        std::size_t capacity = 60;                  // the newest values kept; 0: all
        std::optional<Color> color;                 // none: the style's color of the `series`th series
        std::size_t series = 0;
        bool area = true;                           // a line's: an area fading under it
        bool last = true;                           // a line's: a dot on its last value
        bool extremes = false;                      // dots on its lowest and its highest
        std::optional<ChartRange> range;            // its own; none: fitted to its values (columns: with 0)
        std::optional<double> level;                // a line across it at this value: a target, a limit
    };

    Sparkline();
    explicit Sparkline(Props props);
    ~Sparkline() override;

    void append(double value);
    void setValues(std::span<const double> values);
    void clear();
    const SeriesBuffer& data() const { return data_; }
    // Its last value; NaN when it has none.
    double last() const;
    void setColor(std::optional<Color> color);
    void setLevel(std::optional<double> level);
    // How a value shows by the mouse; none: formatNumber.
    void setFormatter(std::function<std::string(double)> format);
    // What its height shows now (eased: on its way to fit its values).
    ChartRange shownRange() const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    Size measureContent(Size available) override;

private:
    ChartRange fit() const;
    std::string format(double value) const;

    Props props_;
    SeriesBuffer data_;
    double next_ = 0;                               // the next value's x: its count
    ChartRange from_, to_;
    bool fitted_ = false;
    Animation progress_;
    std::function<std::string(double)> format_;
    std::optional<Point> mouse_;
    Rect plot_;
    std::vector<Point> points_;                     // scratch
};

} // namespace glw
