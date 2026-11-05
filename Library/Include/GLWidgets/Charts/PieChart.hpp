// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Parts of a whole as slices of a circle (a pie), or of a ring (a donut: setHole). It is as large as its room allows;
// each slice shows its share inside it when it fits there, a donut its total in its hole (or the slice under the mouse's
// value); the legend names them, and a click on an item hides its slice (and shows it again). Slices are a gap apart
// (ChartStyle::gap), all along.
//
// Its values may change as often as they come: the slices are eased to their new sizes (a slice that comes grows from
// nothing, one hidden shrinks to nothing), and the first time it shows, they sweep around into place. The slice under
// the mouse comes out of it a little, with its value and share by the mouse.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

struct PieSlice {
    LocalizedText label;
    double value = 0;                   // less than 0: as 0
    std::optional<Color> color;         // none: the style's, in turn
};

class GLW_EXPORT PieChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        float hole = 0;                 // of the radius: a donut's hole; 0: a pie
        float startAngle = -90;         // where the first slice starts, degrees (0: to the right; -90: at the top), clockwise
        bool shares = true;             // each slice's share in it, when it fits
        // What a donut shows in its hole under its total; none: nothing.
        LocalizedText totalLabel;
    };

    explicit PieChart(Props props = {});
    ~PieChart() override;

    // Its slices (matched to those it had by their place): eased to.
    void setSlices(std::vector<PieSlice> slices);
    void setValue(std::size_t index, double value);
    std::size_t sliceCount() const { return slices_.size(); }
    const PieSlice& slice(std::size_t index) const { return slices_[index]; }
    void setSliceVisible(std::size_t index, bool visible);
    bool sliceVisible(std::size_t index) const { return index < hidden_.size() && !hidden_[index]; }
    void setHole(float hole);
    void setStartAngle(float degrees);
    // How a value shows (in a tooltip, in the hole); none: formatNumber.
    void setFormatter(std::function<std::string(double)> format);
    // A slice clicked.
    void setOnSliceClicked(std::function<void(std::size_t index)> handler) { onSliceClicked_ = std::move(handler); }

    // The slice at a point (its own coordinates), as it was last painted.
    std::optional<std::size_t> sliceAt(Point at) const;
    // A slice's value as it shows now (eased: on the way to its value).
    double shownValue(std::size_t index) const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    void valuesChanged();
    double target(std::size_t index) const;
    std::string format(double value) const;
    Color colorOf(std::size_t index) const;

    Props props_;
    std::vector<PieSlice> slices_;
    std::vector<bool> hidden_;
    std::vector<double> from_, to_;                 // the values shown eased from, to
    Animation progress_;
    Animation sweep_;                               // the first time it shows: from 0 around to 1
    bool swept_ = false;
    std::vector<std::unique_ptr<Animation>> out_;   // how far each slice is out (the one under the mouse)
    std::optional<std::size_t> hovered_;
    std::optional<Point> mouse_;
    std::function<std::string(double)> format_;
    std::function<void(std::size_t)> onSliceClicked_;
    Point center_;                                  // as last painted
    float radius_ = 0;
    float inner_ = 0;
};

} // namespace glw
