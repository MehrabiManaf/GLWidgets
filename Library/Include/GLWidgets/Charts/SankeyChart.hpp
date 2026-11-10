// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What flows where (Sankey diagrams): nodes (sources, stages, sinks) and links between them, each link a band as wide as
// its value, from its source's end side to its target's start side, of its source's color turning into its target's. Nodes
// are in columns by how far they are from the sources (the longest way to them; those nothing flows out of in the last
// column, if asked), each as high as what flows through it (the more of what comes in and what goes out), raised as the
// style's bars are, its name and value after it (toward the end side: room is kept there for the last column's). Within a column they are placed where their links are shortest (each
// drawn toward the middle of what it is linked to, column after column, a few times over), and a node's links leave and
// arrive in the order of where they go, so that bands cross as little as they can. A link back to a node before it (a
// cycle) is left out.
//
// Values change as they come (setLink): the bands and nodes are eased to their new sizes and places. The mouse over a
// link: it stands out, the rest faint, its value and its share of its source by it; over a node: its links stand out,
// what comes into it and goes out of it by it. Right to left, it flows from the right.
#include <GLWidgets/Charts/Chart.hpp>
#include <GLWidgets/Export.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace glw {

struct SankeyLink {
    std::size_t source = 0;
    std::size_t target = 0;
    double value = 0;
};

class GLW_EXPORT SankeyChart : public Chart {
public:
    struct Props {
        LocalizedText title;
        float nodeWidth = 14;
        float nodePadding = 10;             // between the nodes of a column, at least
        bool justify = true;                // the nodes nothing flows out of in the last column
        float linkOpacity = 0.4f;
        std::string unit;                   // after a value ("k", " req/s")
    };

    SankeyChart();
    explicit SankeyChart(Props props);
    ~SankeyChart() override;

    // A node; none colored: the style's series colors, in turn.
    std::size_t addNode(LocalizedText name, std::optional<Color> color = {});
    std::size_t nodeCount() const { return nodes_.size(); }
    // A link from a node to another: made, or its value changed (0: gone).
    void setLink(std::size_t source, std::size_t target, double value);
    void setLinks(std::vector<SankeyLink> links);
    void clear();
    double linkValue(std::size_t source, std::size_t target) const;
    // Whether a link is drawn: not one back to a node before it.
    bool linkShown(std::size_t source, std::size_t target) const;
    // A node's column (0: the sources'), how many columns there are, what flows through a node.
    std::size_t columnOf(std::size_t node) const;
    std::size_t columnCount() const;
    double nodeValue(std::size_t node) const;
    // How a value shows; none: formatNumber and the unit.
    void setValueFormatter(std::function<std::string(double)> format);

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    struct Node {
        LocalizedText name;
        std::optional<Color> color;
        // found by arrange(): its column, what comes in and goes out
        std::size_t column = 0;
        double in = 0, out = 0;
    };
    struct Link {
        std::size_t source, target;
        double value;
        bool back = false;                  // to a node before it: left out
    };
    // Where things are (dips): a node's box; a link's band, its top at its source and at its target, its width.
    struct NodeBox {
        float x = 0, y = 0, height = 0;
    };
    struct LinkBand {
        float sourceY = 0, targetY = 0, width = 0;
    };
    void changed();
    // Its columns and the nodes' flows found again (when its links changed).
    void arrange() const;
    // Where its nodes and links go in `plot`.
    void layout(const Rect& plot, std::vector<NodeBox>& boxes, std::vector<LinkBand>& bands) const;
    Color colorOf(std::size_t node) const;
    std::string format(double value) const;

    Props props_;
    mutable std::vector<Node> nodes_;
    mutable std::vector<Link> links_;
    mutable bool arranged_ = false;
    mutable std::size_t columns_ = 0;
    std::function<std::string(double)> format_;

    // eased: from what showed to where they go
    std::vector<NodeBox> fromBoxes_, toBoxes_;
    std::vector<LinkBand> fromBands_, toBands_;
    std::uint64_t version_ = 1, laidOut_ = 0;
    Rect laidOutIn_;
    float laidOutMargin_ = 0;
    Animation progress_;

    // as it was last painted: each node's box, each link's upper edge and width
    std::vector<Rect> nodeRects_;
    struct PlacedLink {
        std::vector<Point> upper;
        float width = 0;
        std::size_t link = 0;
    };
    std::vector<PlacedLink> placed_;
    std::optional<Point> mouse_;
};

} // namespace glw
