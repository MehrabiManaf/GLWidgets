// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/SankeyChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace glw {

namespace {

constexpr int segments = 24;            // of a link's band: its curve, and its colors from its source's to its target's
constexpr int relaxations = 8;          // how many times over the nodes are drawn toward what they are linked to

float between(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

SankeyChart::SankeyChart() : SankeyChart(Props{}) {}

SankeyChart::SankeyChart(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setLegendVisible(false);
    progress_.set(1);
}

SankeyChart::~SankeyChart() = default;

void SankeyChart::changed() {
    arranged_ = false;
    ++version_;
    invalidatePaint();
}

std::size_t SankeyChart::addNode(LocalizedText name, std::optional<Color> color) {
    nodes_.push_back({.name = std::move(name), .color = color});
    changed();
    return nodes_.size() - 1;
}

void SankeyChart::setLink(std::size_t source, std::size_t target, double value) {
    const auto found = std::find_if(links_.begin(), links_.end(), [&](const Link& l) { return l.source == source && l.target == target; });
    if (found != links_.end()) {
        if (value > 0)
            found->value = value;
        else
            links_.erase(found);
    } else if (value > 0) {
        links_.push_back({source, target, value});
    } else {
        return;
    }
    changed();
}

void SankeyChart::setLinks(std::vector<SankeyLink> links) {
    links_.clear();
    for (const SankeyLink& l : links)
        if (l.value > 0)
            links_.push_back({l.source, l.target, l.value});
    changed();
}

void SankeyChart::clear() {
    nodes_.clear();
    links_.clear();
    changed();
}

double SankeyChart::linkValue(std::size_t source, std::size_t target) const {
    for (const Link& l : links_)
        if (l.source == source && l.target == target)
            return l.value;
    return 0;
}

bool SankeyChart::linkShown(std::size_t source, std::size_t target) const {
    arrange();
    for (const Link& l : links_)
        if (l.source == source && l.target == target)
            return !l.back;
    return false;
}

std::size_t SankeyChart::columnOf(std::size_t node) const {
    arrange();
    return node < nodes_.size() ? nodes_[node].column : 0;
}

std::size_t SankeyChart::columnCount() const {
    arrange();
    return columns_;
}

double SankeyChart::nodeValue(std::size_t node) const {
    arrange();
    return node < nodes_.size() ? std::max(nodes_[node].in, nodes_[node].out) : 0;
}

void SankeyChart::setValueFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

Color SankeyChart::colorOf(std::size_t node) const {
    return nodes_[node].color ? *nodes_[node].color : seriesColor(node);
}

std::string SankeyChart::format(double value) const {
    if (format_)
        return format_(value);
    return formatNumber(value, value == std::floor(value) ? 0 : 1) + props_.unit;
}

// ---- its columns ----

void SankeyChart::arrange() const {
    if (arranged_)
        return;
    arranged_ = true;
    const std::size_t n = nodes_.size();
    for (Node& node : nodes_) {
        node.column = 0;
        node.in = node.out = 0;
    }
    columns_ = 0;
    std::vector<std::vector<std::size_t>> out(n);
    for (std::size_t i = 0; i < links_.size(); ++i) {
        Link& l = links_[i];
        l.back = l.source == l.target || l.source >= n || l.target >= n;
        if (!l.back)
            out[l.source].push_back(i);
    }
    // the links back to a node they came through (found going down from each node, in order): left out
    std::vector<std::uint8_t> state(n, 0);              // 0: not seen, 1: on the way, 2: done
    std::vector<std::pair<std::size_t, std::size_t>> way;      // a node, and which of its links is next
    for (std::size_t root = 0; root < n; ++root) {
        if (state[root] != 0)
            continue;
        state[root] = 1;
        way.emplace_back(root, 0);
        while (!way.empty()) {
            const std::size_t u = way.back().first, k = way.back().second;
            if (k >= out[u].size()) {
                state[u] = 2;
                way.pop_back();
                continue;
            }
            ++way.back().second;
            Link& l = links_[out[u][k]];
            if (state[l.target] == 1) {
                l.back = true;
            } else if (state[l.target] == 0) {
                state[l.target] = 1;
                way.emplace_back(l.target, 0);
            }
        }
    }
    // a node's column: the longest way to it from a source
    std::vector<std::size_t> coming(n, 0);
    for (const Link& l : links_) {
        if (l.back)
            continue;
        ++coming[l.target];
        nodes_[l.source].out += l.value;
        nodes_[l.target].in += l.value;
    }
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i)
        if (coming[i] == 0)
            ready.push_back(i);
    for (std::size_t at = 0; at < ready.size(); ++at) {
        const std::size_t u = ready[at];
        for (std::size_t i : out[u]) {
            const Link& l = links_[i];
            if (l.back)
                continue;
            nodes_[l.target].column = std::max(nodes_[l.target].column, nodes_[u].column + 1);
            if (--coming[l.target] == 0)
                ready.push_back(l.target);
        }
    }
    for (const Node& node : nodes_)
        if (node.in > 0 || node.out > 0)
            columns_ = std::max(columns_, node.column + 1);
    // those nothing flows out of: in the last column
    if (props_.justify)
        for (Node& node : nodes_)
            if (node.in > 0 && node.out <= 0)
                node.column = columns_ - 1;
}

// ---- where things go ----

void SankeyChart::layout(const Rect& plot, std::vector<NodeBox>& boxes, std::vector<LinkBand>& bands) const {
    const std::size_t n = nodes_.size();
    boxes.assign(n, {});
    bands.assign(links_.size(), {});
    if (columns_ == 0)
        return;
    const auto value = [&](std::size_t i) { return std::max(nodes_[i].in, nodes_[i].out); };
    std::vector<std::vector<std::size_t>> columns(columns_);
    for (std::size_t i = 0; i < n; ++i)
        if (value(i) > 0)
            columns[nodes_[i].column].push_back(i);
    std::size_t most = 1;
    for (const auto& column : columns)
        most = std::max(most, column.size());
    // as many dips a unit of value as the fullest column has room for, its nodes a padding apart
    const float pad = most > 1 ? std::min(props_.nodePadding, plot.height * 0.4f / static_cast<float>(most - 1)) : 0.0f;
    double scale = std::numeric_limits<double>::infinity();
    for (const auto& column : columns) {
        double sum = 0;
        for (std::size_t i : column)
            sum += value(i);
        if (sum > 0)
            scale = std::min(scale, (static_cast<double>(plot.height) - pad * static_cast<float>(column.size() - 1)) / sum);
    }
    if (!(scale > 0) || !std::isfinite(scale))
        return;
    // the columns across it (from the right, right to left); each one's nodes stacked in its middle, in their order
    const bool rtl = rightToLeft();
    const float usable = std::max(0.0f, plot.width - props_.nodeWidth);
    for (std::size_t c = 0; c < columns_; ++c) {
        const float along = columns_ > 1 ? usable * static_cast<float>(c) / static_cast<float>(columns_ - 1) : 0.0f;
        float total = pad * static_cast<float>(columns[c].empty() ? 0 : columns[c].size() - 1);
        for (std::size_t i : columns[c]) {
            boxes[i].height = std::max(1.0f, static_cast<float>(value(i) * scale));
            total += boxes[i].height;
        }
        float y = plot.y + std::max(0.0f, (plot.height - total) / 2);
        for (std::size_t i : columns[c]) {
            boxes[i].x = rtl ? plot.right() - props_.nodeWidth - along : plot.x + along;
            boxes[i].y = y;
            y += boxes[i].height + pad;
        }
    }
    std::vector<std::vector<std::size_t>> out(n), in(n);
    for (std::size_t i = 0; i < links_.size(); ++i) {
        if (links_[i].back)
            continue;
        out[links_[i].source].push_back(i);
        in[links_[i].target].push_back(i);
    }
    const auto center = [&](std::size_t i) { return boxes[i].y + boxes[i].height / 2; };
    // a column's nodes apart again, in the plot: pushed down from the top, then up from the bottom
    const auto resolve = [&](std::vector<std::size_t>& column) {
        std::stable_sort(column.begin(), column.end(), [&](std::size_t a, std::size_t b) { return boxes[a].y < boxes[b].y; });
        float y = plot.y;
        for (std::size_t i : column) {
            boxes[i].y = std::max(boxes[i].y, y);
            y = boxes[i].y + boxes[i].height + pad;
        }
        float bottom = plot.bottom();
        for (std::size_t k = column.size(); k-- > 0;) {
            NodeBox& b = boxes[column[k]];
            b.y = std::min(b.y, bottom - b.height);
            bottom = b.y - pad;
        }
    };
    // each node toward the middle of what it is linked to (by the links' values): by what it flows into, from the last
    // column back; then by what flows into it, from the first on; a few times over
    for (int pass = 0; pass < relaxations; ++pass) {
        const float alpha = std::pow(0.99f, static_cast<float>(pass));
        const auto pull = [&](std::vector<std::size_t>& column, bool toTargets) {
            for (std::size_t i : column) {
                double sum = 0, weight = 0;
                for (std::size_t li : toTargets ? out[i] : in[i]) {
                    const Link& l = links_[li];
                    sum += center(toTargets ? l.target : l.source) * l.value;
                    weight += l.value;
                }
                if (weight > 0)
                    boxes[i].y += (static_cast<float>(sum / weight) - center(i)) * alpha;
            }
            resolve(column);
        };
        for (std::size_t c = columns_ - 1; c-- > 0;)
            pull(columns[c], true);
        for (std::size_t c = 1; c < columns_; ++c)
            pull(columns[c], false);
    }
    // a node's links leave it in the order of where they go, and arrive in the order of where they come from
    for (std::size_t i = 0; i < n; ++i) {
        std::stable_sort(out[i].begin(), out[i].end(), [&](std::size_t a, std::size_t b) { return center(links_[a].target) < center(links_[b].target); });
        std::stable_sort(in[i].begin(), in[i].end(), [&](std::size_t a, std::size_t b) { return center(links_[a].source) < center(links_[b].source); });
        float y = boxes[i].y;
        for (std::size_t li : out[i]) {
            bands[li].width = static_cast<float>(links_[li].value * scale);
            bands[li].sourceY = y;
            y += bands[li].width;
        }
        y = boxes[i].y;
        for (std::size_t li : in[i]) {
            bands[li].targetY = y;
            y += static_cast<float>(links_[li].value * scale);
        }
    }
}

// ---- painting ----

void SankeyChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    const bool rtl = rightToLeft();
    nodeRects_.clear();
    placed_.clear();
    arrange();
    if (columns_ == 0)
        return;
    // every node's name and value after it (toward the end side): room kept past the last column for its nodes'
    float margin = 0;
    for (const Node& node : nodes_)
        if (node.column + 1 == columns_ && (node.in > 0 || node.out > 0))
            margin = std::max(margin, 6 + textSize(translate(node.name)).width + 6 + textSize(format(std::max(node.in, node.out))).width);
    margin = std::ceil(std::min(margin, area.width * 0.35f));
    Rect plot = area;
    plot.width = std::max(0.0f, plot.width - margin);
    if (rtl)
        plot.x += margin;
    if (plot.width < 40 || plot.height < 20)
        return;

    // where everything goes: eased from where it showed when its values changed (at once when its room did)
    if (version_ != laidOut_ || !(area == laidOutIn_) || margin != laidOutMargin_) {
        std::vector<NodeBox> boxes;
        std::vector<LinkBand> bands;
        layout(plot, boxes, bands);
        const bool ease = version_ != laidOut_ && area == laidOutIn_ && boxes.size() == toBoxes_.size() && bands.size() == toBands_.size() &&
                          transition() > Duration::zero();
        if (ease) {
            const float t = progress_.value();
            for (std::size_t i = 0; i < boxes.size(); ++i) {
                const NodeBox &a = fromBoxes_[i], &b = toBoxes_[i];
                fromBoxes_[i] = {between(a.x, b.x, t), between(a.y, b.y, t), between(a.height, b.height, t)};
                // one that did not show: out of its middle
                if (fromBoxes_[i].height <= 0)
                    fromBoxes_[i] = {boxes[i].x, boxes[i].y + boxes[i].height / 2, 0};
            }
            for (std::size_t i = 0; i < bands.size(); ++i) {
                const LinkBand &a = fromBands_[i], &b = toBands_[i];
                fromBands_[i] = {between(a.sourceY, b.sourceY, t), between(a.targetY, b.targetY, t), between(a.width, b.width, t)};
                if (fromBands_[i].width <= 0)
                    fromBands_[i] = {bands[i].sourceY + bands[i].width / 2, bands[i].targetY + bands[i].width / 2, 0};
            }
            progress_.set(0);
            progress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
        } else {
            fromBoxes_ = boxes;
            fromBands_ = bands;
            progress_.set(1);
        }
        toBoxes_ = std::move(boxes);
        toBands_ = std::move(bands);
        laidOut_ = version_;
        laidOutIn_ = area;
        laidOutMargin_ = margin;
    }
    const float t = progress_.value();
    const auto box = [&](std::size_t i) {
        const NodeBox &a = fromBoxes_[i], &b = toBoxes_[i];
        return NodeBox{between(a.x, b.x, t), between(a.y, b.y, t), between(a.height, b.height, t)};
    };

    // the nodes' boxes; the links' bands: a curve from each one's source to its target, its points from the left
    nodeRects_.resize(nodes_.size());
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const NodeBox b = box(i);
        nodeRects_[i] = toBoxes_[i].height > 0 ? Rect{b.x, b.y, props_.nodeWidth, std::max(b.height, 1.0f / ppd)} : Rect{};
    }
    for (std::size_t li = 0; li < links_.size(); ++li) {
        const Link& l = links_[li];
        if (l.back || toBands_[li].width <= 0)
            continue;
        const LinkBand &a = fromBands_[li], &b = toBands_[li];
        const float width = std::max(between(a.width, b.width, t), 1.0f / ppd);
        const float sourceY = between(a.sourceY, b.sourceY, t), targetY = between(a.targetY, b.targetY, t);
        const Rect &from = nodeRects_[l.source], &to = nodeRects_[l.target];
        // (right to left: its target at its left)
        const float x0 = rtl ? to.right() : from.right(), x1 = rtl ? from.x : to.x;
        const float y0 = rtl ? targetY : sourceY, y1 = rtl ? sourceY : targetY;
        PlacedLink placed{.width = width, .link = li};
        placed.upper.reserve(segments + 1);
        for (int k = 0; k <= segments; ++k) {
            // a cubic, its handles level at the middle between them
            const float u = static_cast<float>(k) / segments, v = 1 - u;
            const float middle = (x0 + x1) / 2;
            const float x = v * v * v * x0 + 3 * v * v * u * middle + 3 * v * u * u * middle + u * u * u * x1;
            placed.upper.push_back({x, y0 + (y1 - y0) * (3 * u * u - 2 * u * u * u)});
        }
        placed_.push_back(std::move(placed));
    }

    // what the mouse is over: a node, or a link (the last drawn first)
    std::optional<std::size_t> overNode, overLink;
    if (mouse_) {
        for (std::size_t i = 0; i < nodeRects_.size() && !overNode; ++i)
            if (!nodeRects_[i].empty() && nodeRects_[i].inflated(2).contains(*mouse_))
                overNode = i;
        for (std::size_t k = placed_.size(); k-- > 0 && !overNode && !overLink;) {
            const PlacedLink& link = placed_[k];
            const std::vector<Point>& u = link.upper;
            if (mouse_->x < u.front().x || mouse_->x > u.back().x)
                continue;
            const auto next = std::upper_bound(u.begin(), u.end(), mouse_->x, [](float x, const Point& point) { return x < point.x; });
            if (next == u.begin() || next == u.end())
                continue;
            const Point a = *(next - 1), b = *next;
            const float y = a.y + (b.y - a.y) * (b.x > a.x ? (mouse_->x - a.x) / (b.x - a.x) : 0.0f);
            const float reach = std::max(0.0f, (4 - link.width) / 2);       // a thin one: found a little around it
            if (mouse_->y >= y - reach && mouse_->y <= y + link.width + reach)
                overLink = link.link;
        }
    }
    const auto picked = [&](const Link& l, std::size_t li) {
        return (overLink && *overLink == li) || (overNode && (l.source == *overNode || l.target == *overNode));
    };

    // the bands: of their source's color turning into their target's; those the mouse picks out over the rest, which fade
    const bool picking = overNode || overLink;
    for (int pass = 0; pass < 2; ++pass) {
        for (const PlacedLink& link : placed_) {
            const Link& l = links_[link.link];
            const bool stands = picking && picked(l, link.link);
            if (stands != (pass == 1))
                continue;
            const float alpha = !picking ? props_.linkOpacity : stands ? std::min(1.0f, props_.linkOpacity * 1.9f) : props_.linkOpacity * 0.3f;
            const Color left = colorOf(rtl ? l.target : l.source), right = colorOf(rtl ? l.source : l.target);
            for (int k = 0; k < segments; ++k) {
                const Point upper[2] = {link.upper[static_cast<std::size_t>(k)], link.upper[static_cast<std::size_t>(k) + 1]};
                const Point lower[2] = {{upper[0].x, upper[0].y + link.width}, {upper[1].x, upper[1].y + link.width}};
                Color color = mix(left, right, (static_cast<float>(k) + 0.5f) / segments);
                color.a *= alpha;
                p.fillBand(upper, lower, color, color);
            }
        }
    }

    // the nodes, raised as the style's bars are; their names and values after them
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const Rect& r = nodeRects_[i];
        if (r.empty())
            continue;
        Color color = colorOf(i);
        if (overNode && *overNode == i)
            color = mix(color, Color{1, 1, 1, color.a}, 0.18f);
        if (r.height >= 3)
            paintRaised(p, r, std::min(s.barRadius, r.width / 3), color, {0, 0, 0, 0}, s.barBevel);
        else
            p.fillRect(r, color);
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const Rect& r = nodeRects_[i];
        const bool hovered = overNode && *overNode == i;
        if (r.empty() || (r.height < 9 && !hovered))
            continue;
        const bool atRight = !rtl;
        const std::string name = translate(nodes_[i].name);
        const std::string value = format(std::max(nodes_[i].in, nodes_[i].out));
        const float y = r.y + r.height / 2;
        const float x = atRight ? r.right() + 6 : r.x - 6;
        const float align = atRight ? 0.0f : 1.0f;
        drawText(context, name, {x, y}, s.title, 0, {align, 0.5f});
        const float past = textSize(name).width + 6;
        drawText(context, value, {atRight ? x + past : x - past, y}, s.text, 0, {align, 0.5f});
    }

    // by the mouse: a link's ends, its value and its share of its source; a node's flows
    if (overLink) {
        const Link& l = links_[*overLink];
        const double share = nodes_[l.source].out > 0 ? l.value / nodes_[l.source].out * 100 : 0;
        const std::vector<ToolTipRow> rows = {
            {colorOf(l.source), translate(LocalizedText("From")), translate(nodes_[l.source].name)},
            {colorOf(l.target), translate(LocalizedText("To")), translate(nodes_[l.target].name)},
            {std::nullopt, translate(LocalizedText("Share")), formatNumber(share, 1) + "%"},
        };
        paintToolTip(context, *mouse_, format(l.value), rows);
    } else if (overNode) {
        const Node& node = nodes_[*overNode];
        std::vector<ToolTipRow> rows;
        if (node.in > 0)
            rows.push_back({std::nullopt, translate(LocalizedText("In")), format(node.in)});
        if (node.out > 0)
            rows.push_back({std::nullopt, translate(LocalizedText("Out")), format(node.out)});
        paintToolTip(context, *mouse_, translate(node.name), rows);
    }
}

// ---- the mouse ----

void SankeyChart::onMouse(MouseEvent& event) {
    if (event.type == MouseEventType::Move) {
        mouse_ = event.position;
        invalidatePaint();
    }
}

void SankeyChart::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

} // namespace glw
