// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Where the time goes (Brendan Gregg's flame graphs): stacks of a sampling profiler (or of anything nested: a disk's
// folders, a budget), merged into a tree, each frame a bar as wide as its share of the samples, the frames it called on it
// (in a row above; or, an icicle, under it), in the order of their names. Stacks come as they come (addStack: a weight
// each, a sample's 1). Frames of warm colors by their names (the same name the same color), or of the style's series
// colors.
//
// A click on a frame zooms into it: it as wide as the graph, eased there; the frames under it (its callers) as wide too, a
// click on one of them zooms out to it; a double click shows all again. A search (setHighlight) leaves the frames whose
// names have it in their colors, the rest faint, and says how much of all of it they are. The mouse over a frame: its
// name, its samples, its share of all and of what shows.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace glw {

class GLW_EXPORT FlameGraph : public Chart {
public:
    enum class Colors : std::uint8_t {
        Warm,           // reds, oranges and yellows (a flame's), by a frame's name
        Series,         // the style's series colors, by a frame's name
    };

    struct Props {
        LocalizedText title;
        bool icicle = false;                // its root at the top, the frames called under their callers
        Colors colors = Colors::Warm;
        float rowHeight = 18;
        LocalizedText rootName{"all"};      // its root frame's name
    };

    FlameGraph();
    explicit FlameGraph(Props props);
    ~FlameGraph() override;

    // A stack, from its outermost frame to its innermost, `weight` more of its samples.
    void addStack(std::span<const std::string> frames, double weight = 1);
    void clear();
    double total() const;
    // How many frames it has (its root among them), how deep its deepest stack is.
    std::size_t frameCount() const { return nodes_.size(); }
    std::size_t depth() const;

    // The frames whose names have `text` in them stand out (none: all); how much of all of it they are, 0 to 1.
    void setHighlight(std::string text);
    double highlighted() const;
    // Zoomed into the frame of this stack (its outermost frame to it); none: all.
    void zoomTo(std::span<const std::string> frames);
    void zoomOut();
    // What shows across, of all of it (0 to 1; eased).
    std::pair<double, double> zoomRange() const;

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    std::optional<platform::Cursor> cursorAt(Point point) const override;

private:
    struct Node {
        std::string name;
        double value = 0;                   // its samples and its callees'
        double self = 0;                    // its own
        int parent = -1;
        std::uint32_t depth = 0;
        std::map<std::string, int, std::less<>> children;   // in the order of their names
    };
    // A frame where it was painted.
    struct Placed {
        Rect rect;
        int node;
    };
    // Where a node is across all of it (0 to 1).
    std::pair<double, double> span(int node) const;
    void zoomInto(int node);
    Color colorOf(const Node& node) const;
    bool matches(const Node& node) const;

    Props props_;
    std::vector<Node> nodes_;
    std::string highlight_;
    int zoom_ = 0;
    std::pair<double, double> from_{0, 1}, to_{0, 1};
    Animation progress_;
    std::vector<Placed> placed_;
    std::optional<Point> mouse_;
};

} // namespace glw
