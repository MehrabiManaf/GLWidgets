// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A measure against its range: an arc (270 degrees, open at the bottom, or as much as it is given) filled to its value,
// raised as the style's bars are (Chart::paintArc), the value large in its middle with its unit under it, the range's
// ends by the arc's; at the top of its room. Bands along its inside (what is good, what is near a limit, what is past it)
// color the value as the band it is in; a needle, if asked, points at it. A new value is eased to (the theme's motion).
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace glw {

class GLW_EXPORT Gauge : public Chart {
public:
    // A part of its range, from the band before it (its range's low for the first) to `to`.
    struct Band {
        double to = 0;
        Color color;
    };

    struct Props {
        ChartRange range{0, 100};
        float sweep = 270;                  // degrees of its arc, open at the bottom (180: a half circle; 360: a ring)
        float thickness = 0.2f;             // of its arc, of its radius
        std::vector<Band> bands;
        std::optional<Color> color;         // its value's, in no band; none: the style's color of the `series`th series
        std::size_t series = 0;
        std::string unit;                   // under its value ("%", "MB/s")
        bool needle = false;
        bool ends = true;                   // its range's ends by the arc's
    };

    Gauge();
    explicit Gauge(Props props);
    ~Gauge() override;

    void setValue(double value);
    double value() const { return value_; }
    // What it shows now (eased: on its way to its value).
    double shownValue() const;
    void setRange(ChartRange range);
    ChartRange range() const { return props_.range; }
    void setBands(std::vector<Band> bands);
    void setUnit(std::string unit);
    // How its value shows; none: formatNumber (as many decimals as its range wants).
    void setFormatter(std::function<std::string(double)> format);
    // The color of what `value` shows: its band's, else its own.
    Color colorOf(double value) const;

protected:
    void paint(PaintContext& context) override;
    Size measureContent(Size available) override;

private:
    std::string format(double value) const;
    // A needle from `at` at `angle` (radians), `length` dips long, as wide as a gauge of `radius` wants, of `color`, and
    // the hub over its root: how far that reaches from `at`.
    float paintNeedle(PaintContext& context, Point at, float angle, float length, float radius, Color color) const;

    Props props_;
    double value_ = 0;
    double from_ = 0;
    Animation progress_;
    std::function<std::string(double)> format_;
};

} // namespace glw
