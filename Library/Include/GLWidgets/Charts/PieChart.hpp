// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Parts of a whole as slices of a circle (a pie), or of a ring (a donut: setHole); and parts of parts as rings further
// out (a sunburst, as a disk usage analyzer shows folders): a slice's children share its angle in the next ring, theirs in
// the one after. It is as large as its room allows, its rings as thick as they all fit; each slice shows its share inside
// it when it fits there, a donut its total in its hole (or the slice under the mouse's value). Slices are a gap apart
// (ChartStyle::gap), all along, and rings too.
//
// Zooming is going into a slice: a click on one with parts makes it the whole, its angle opening to a full turn and its
// rings moving in, eased (its siblings squeezed out of the way); a click in the hole or a right click goes back up a
// level. The legend names the parts of what it shows, and a click on an item hides one (and shows it again).
//
// Its values may change as often as they come: the slices are eased to their new sizes (a slice that comes grows from
// nothing, one hidden shrinks to nothing), and the first time it shows, they sweep around into place. The slice under the
// mouse comes out of it a little, with its path, value and share by the mouse. Slices are matched with those they were
// by the labels on their paths.
//
// Many parts (heavy data): only the `top` largest parts of each slice show (and of the whole), the rest together as one
// slice of others, a neutral gray, in the legend too (a click hides them all). Which are the top ones is found again as the
// values change (a partial sort of each slice's parts), the others' slice growing and shrinking with them.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace glw {

struct PieSlice {
    LocalizedText label;
    double value = 0;                   // less than 0: as 0
    std::optional<Color> color;         // none: the style's, in turn (at the top), its parent's shade (in a ring further out)
    std::vector<PieSlice> children;     // its parts: in the next ring out, within its angle (it is as large as all of them)

    // Its value, or its parts' together.
    double total() const;
};

class GLW_EXPORT PieChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        float hole = 0;                 // of the radius: a donut's hole; 0: a pie
        float startAngle = -90;         // where the first slice starts, degrees (0: to the right; -90: at the top), clockwise
        bool shares = true;             // each slice's share in it, when it fits
        // What a donut shows in its hole under its total, at the top; none: nothing. Zoomed into a slice: its label.
        LocalizedText totalLabel;
        int rings = 0;                  // the rings shown at once, out from the slice zoomed into; 0: all it has (6 at most)
        std::size_t top = 0;            // the largest parts of each slice shown, the rest as one of others; 0: all of them
        LocalizedText others = "Others";
    };

    // The place of the others' slice in a path (what nodeAt and a click on it give).
    static constexpr std::size_t othersIndex = static_cast<std::size_t>(-1);

    PieChart();
    explicit PieChart(Props props);
    ~PieChart() override;

    // Its slices (matched to those it had by their labels' paths): eased to. What it was zoomed into stays, as far down as
    // the new slices have it.
    void setSlices(std::vector<PieSlice> slices);
    // The value of one of its top slices.
    void setValue(std::size_t index, double value);
    std::size_t sliceCount() const { return slices_.size(); }
    const PieSlice& slice(std::size_t index) const { return slices_[index]; }
    // Of the parts of the slice zoomed into (the top slices at first): hidden (out of the whole) or shown.
    void setSliceVisible(std::size_t index, bool visible);
    bool sliceVisible(std::size_t index) const;
    void setHole(float hole);
    void setStartAngle(float degrees);
    void setRings(int rings);
    void setTop(std::size_t top);
    // How a value shows (in a tooltip, in the hole); none: formatNumber.
    void setFormatter(std::function<std::string(double)> format);
    // A slice without parts clicked (one with parts is zoomed into): its path, the places of the slices on the way to it.
    void setOnSliceClicked(std::function<void(std::span<const std::size_t> path)> handler) { onSliceClicked_ = std::move(handler); }

    // The slice that is the whole, as its path from the top (empty: the top).
    void setFocus(std::vector<std::size_t> path);
    const std::vector<std::size_t>& focus() const { return focus_; }
    // A level up; false at the top.
    bool zoomOut();

    // The slice at a point (its own coordinates), as it was last painted: of the parts of the slice zoomed into, the one
    // it is in (none in the hole); its whole path.
    std::optional<std::size_t> sliceAt(Point at) const;
    std::optional<std::vector<std::size_t>> nodeAt(Point at) const;
    // A part of the slice zoomed into: its value as it shows now (eased: on the way to its value).
    double shownValue(std::size_t index) const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::vector<LegendItem> legendItems() const override;
    void legendItemClicked(std::size_t index) override;

private:
    // A slice as it shows: its place in the whole (fractions of the top's turn), its ring, its color.
    struct Item {
        const PieSlice* slice = nullptr;
        std::string key;                    // the labels on its path
        std::vector<std::size_t> path;
        int depth = 0;                      // 0: a top slice
        double value = 0;                   // as it shows (eased)
        double from = 0, to = 0;            // of the top's turn
        Color color;
        std::size_t merged = 0;             // the others' slice: how many it is of
    };
    // Painted: where it is, to find it under the mouse.
    struct Shown {
        std::size_t item;
        float inner, outer, start, sweep;
        Point center;
    };
    // The parts of the slice at `path` (the top slices for an empty one); none when it is not there.
    const std::vector<PieSlice>* partsOf(std::span<const std::size_t> path) const;
    std::string keyOf(std::span<const std::size_t> path) const;
    double target(const PieSlice& slice, const std::string& key, bool hidden) const;
    // The slices as they show now, the parents before their parts; each one without parts's value as it shows into
    // `leaves` (those in the others' slice too).
    std::vector<Item> items(std::unordered_map<std::string, double>* leaves = nullptr) const;
    // What shows now kept as where it eases from, before a change; then eased to what changed.
    void snapshot();
    void eased();
    int ringsUnder(const std::vector<PieSlice>& parts) const;
    std::string format(double value) const;
    std::string pathText(std::span<const std::size_t> path) const;

    Props props_;
    std::vector<PieSlice> slices_;
    std::unordered_set<std::string> hidden_;            // keys
    mutable PieSlice others_;                           // what the others' slices show (their label)
    mutable std::vector<std::string> legendKeys_;       // what its legend's items are
    std::unordered_map<std::string, double> from_;      // the shown values of the slices without parts, eased from
    Animation progress_;
    Animation sweep_;                                   // the first time it shows: from 0 around to 1
    bool swept_ = false;
    // zooming: the part of the top's turn shown and the ring the focus's parts are in, eased between
    std::vector<std::size_t> focus_;
    double fromLow_ = 0, fromHigh_ = 1;
    float fromDepth_ = 0, fromRings_ = 1;
    Animation zoom_;
    std::vector<Shown> shown_;                          // as last painted
    std::vector<Item> painted_;
    std::unordered_map<std::string, std::unique_ptr<Animation>> out_;   // how far each slice is out (the one under the mouse)
    std::optional<std::string> hovered_;
    std::optional<Point> mouse_;
    std::function<std::string(double)> format_;
    std::function<void(std::span<const std::size_t>)> onSliceClicked_;
    Point center_;
    float radius_ = 0;
    float inner_ = 0;
    double lastLow_ = 0, lastHigh_ = 1;                 // the view as last painted
    float lastDepth_ = 0, lastRings_ = 1;
};

} // namespace glw
