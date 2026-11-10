// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/FlameGraph.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace glw {

namespace {

// A color of a hue (degrees), a saturation and a lightness (0 to 1).
Color hsl(float h, float s, float l) {
    const float c = (1 - std::abs(2 * l - 1)) * s;
    const float hp = std::fmod(h / 60.0f, 6.0f);
    const float x = c * (1 - std::abs(std::fmod(hp, 2.0f) - 1));
    float r = 0, g = 0, b = 0;
    if (hp < 1)
        r = c, g = x;
    else if (hp < 2)
        r = x, g = c;
    else if (hp < 3)
        g = c, b = x;
    else if (hp < 4)
        g = x, b = c;
    else if (hp < 5)
        r = x, b = c;
    else
        r = c, b = x;
    const float m = l - c / 2;
    return {r + m, g + m, b + m, 1};
}

} // namespace

FlameGraph::FlameGraph() : FlameGraph(Props{}) {}

FlameGraph::FlameGraph(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setLegendVisible(false);
    clear();
    progress_.set(1);
}

FlameGraph::~FlameGraph() = default;

void FlameGraph::clear() {
    nodes_.clear();
    nodes_.push_back({.name = std::string()});
    zoom_ = 0;
    from_ = to_ = {0, 1};
    invalidatePaint();
}

void FlameGraph::addStack(std::span<const std::string> frames, double weight) {
    if (!(weight > 0))
        return;
    int at = 0;
    nodes_[0].value += weight;
    for (const std::string& frame : frames) {
        const auto found = nodes_[static_cast<std::size_t>(at)].children.find(frame);
        int next;
        if (found != nodes_[static_cast<std::size_t>(at)].children.end()) {
            next = found->second;
        } else {
            next = static_cast<int>(nodes_.size());
            const std::uint32_t depth = nodes_[static_cast<std::size_t>(at)].depth + 1;
            nodes_[static_cast<std::size_t>(at)].children.emplace(frame, next);
            nodes_.push_back({.name = frame, .parent = at, .depth = depth});
        }
        at = next;
        nodes_[static_cast<std::size_t>(at)].value += weight;
    }
    nodes_[static_cast<std::size_t>(at)].self += weight;
    invalidatePaint();
}

double FlameGraph::total() const {
    return nodes_[0].value;
}

std::size_t FlameGraph::depth() const {
    std::uint32_t most = 0;
    for (const Node& n : nodes_)
        most = std::max(most, n.depth);
    return most;
}

void FlameGraph::setHighlight(std::string text) {
    highlight_ = std::move(text);
    invalidatePaint();
}

bool FlameGraph::matches(const Node& node) const {
    return !highlight_.empty() && node.name.find(highlight_) != std::string::npos;
}

double FlameGraph::highlighted() const {
    if (highlight_.empty() || nodes_[0].value <= 0)
        return 0;
    // the frames that have it, but for those called by one that has it too (counted in it)
    double sum = 0;
    for (std::size_t i = 1; i < nodes_.size(); ++i) {
        if (!matches(nodes_[i]))
            continue;
        bool counted = false;
        for (int p = nodes_[i].parent; p > 0 && !counted; p = nodes_[static_cast<std::size_t>(p)].parent)
            counted = matches(nodes_[static_cast<std::size_t>(p)]);
        if (!counted)
            sum += nodes_[i].value;
    }
    return sum / nodes_[0].value;
}

std::pair<double, double> FlameGraph::span(int node) const {
    const double all = nodes_[0].value;
    if (node <= 0 || all <= 0)
        return {0, 1};
    const Node& n = nodes_[static_cast<std::size_t>(node)];
    const auto [start, end] = span(n.parent);
    (void)end;
    // after its siblings before it (in the order of their names)
    double before = 0;
    for (const auto& [name, child] : nodes_[static_cast<std::size_t>(n.parent)].children) {
        if (child == node)
            break;
        before += nodes_[static_cast<std::size_t>(child)].value;
    }
    const double low = start + before / all;
    return {low, low + n.value / all};
}

std::pair<double, double> FlameGraph::zoomRange() const {
    const auto target = span(zoom_);
    const float t = progress_.value();
    if (t >= 1)
        return target;
    return {from_.first + (target.first - from_.first) * t, from_.second + (target.second - from_.second) * t};
}

void FlameGraph::zoomInto(int node) {
    if (node == zoom_ || node < 0 || static_cast<std::size_t>(node) >= nodes_.size())
        return;
    from_ = zoomRange();
    zoom_ = node;
    if (transition() > Duration::zero()) {
        progress_.set(0);
        progress_.animateTo(scheduler(), 1, transition(), Easing::OutCubic);
    } else {
        progress_.set(1);
    }
    invalidatePaint();
}

void FlameGraph::zoomTo(std::span<const std::string> frames) {
    int at = 0;
    for (const std::string& frame : frames) {
        const auto& children = nodes_[static_cast<std::size_t>(at)].children;
        const auto found = children.find(frame);
        if (found == children.end())
            return;
        at = found->second;
    }
    zoomInto(at);
}

void FlameGraph::zoomOut() {
    zoomInto(0);
}

Color FlameGraph::colorOf(const Node& node) const {
    const std::size_t h = std::hash<std::string>{}(node.name);
    if (props_.colors == Colors::Series)
        return seriesColor(h % 10);
    // a flame's: red to yellow, a little more or less saturated, by its name
    const UiContext* ui = context();
    const bool dark = ui ? ui->theme.isDark() : true;
    const float hue = static_cast<float>(h % 997) / 997.0f * 52.0f;
    const float saturation = 0.70f + static_cast<float>((h / 997) % 100) / 100.0f * 0.2f;
    return hsl(hue, saturation, dark ? 0.50f : 0.60f);
}

// ---- painting ----

void FlameGraph::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const float ppd = p.pixelsPerDip();
    placed_.clear();
    if (nodes_[0].value <= 0 || area.width < 8 || area.height < 8)
        return;
    const float line = textSize("0").height;
    Rect plot = area;
    // how much the search finds: at the end of its title's row (without a title, a row of its own at the top)
    if (!highlight_.empty()) {
        const std::string matched = translate(LocalizedText("Matched")) + " " + formatNumber(highlighted() * 100, 1) + "%";
        const bool rtl = rightToLeft();
        if (!title().source.empty()) {
            const Rect content = contentRect();
            drawText(context, matched, {rtl ? content.x : content.right(), content.y + 1}, s.text, 0, {rtl ? 0.0f : 1.0f, 0});
        } else {
            drawText(context, matched, {rtl ? area.x : area.right(), area.y}, s.text, 0, {rtl ? 0.0f : 1.0f, 0});
            plot.y += line + 6;
            plot.height = std::max(0.0f, plot.height - line - 6);
        }
    }
    const auto [a, b] = zoomRange();
    const double across = std::max(b - a, 1e-12);
    const auto levels = static_cast<float>(depth() + 1);
    const float rh = std::max(6.0f, std::min(props_.rowHeight, plot.height / levels));
    const float hair = 1.0f / ppd;
    const double all = nodes_[0].value;
    const Color faint = s.plot.a > 0.5f ? s.plot : context.ui.theme.palette.layer;
    // each frame where it is across what shows, its callees on it (or under it), those narrower than half a pixel left out
    p.pushClip(plot);
    const std::function<void(int, double)> place = [&](int index, double start) {
        const Node& n = nodes_[static_cast<std::size_t>(index)];
        const double end = start + n.value / all;
        const float x0 = static_cast<float>(plot.x + (start - a) / across * plot.width);
        const float x1 = static_cast<float>(plot.x + (end - a) / across * plot.width);
        if (x1 - x0 < 0.5f * hair || x1 < plot.x || x0 > plot.right())
            return;
        const float y = props_.icicle ? plot.y + static_cast<float>(n.depth) * rh : plot.bottom() - static_cast<float>(n.depth + 1) * rh;
        if (y > plot.bottom() || y + rh < plot.y)
            return;
        const Rect rect{std::max(x0, plot.x - 2), y, std::min(x1, plot.right() + 2) - std::max(x0, plot.x - 2) - (x1 - x0 >= 3 ? hair : 0.0f), rh - hair};
        Color color = index == 0 ? withAlpha(s.text, s.text.a * 0.35f) : colorOf(n);
        if (!highlight_.empty() && !matches(n))
            color = mix(color, faint, 0.78f);
        const bool hovered = mouse_ && rect.contains(*mouse_);
        if (hovered)
            color = mix(color, Color{1, 1, 1, color.a}, 0.2f);
        if (rect.width >= 3)
            p.fillRoundedRect(rect, std::min(s.barRadius, rect.height / 4), color);
        else
            p.fillRect(rect, color);
        placed_.push_back({rect, index});
        // its name in it, as much as fits
        if (rect.width >= 24 && rh >= s.fontSize) {
            const Rect visible = rect.intersected(plot);
            p.pushClip({visible.x + 3, visible.y, std::max(0.0f, visible.width - 6), visible.height});
            drawText(context, index == 0 ? translate(props_.rootName) : n.name, {visible.x + 4, rect.y + rect.height / 2}, textOn(color), 0,
                     {0, 0.5f});
            p.popClip();
        }
        double at = start;
        for (const auto& [name, child] : n.children) {
            place(child, at);
            at += nodes_[static_cast<std::size_t>(child)].value / all;
        }
    };
    place(0, 0);
    p.popClip();

    // the frame under the mouse: its samples, its share of all and of what shows
    if (mouse_) {
        for (auto it = placed_.rbegin(); it != placed_.rend(); ++it) {
            if (!it->rect.contains(*mouse_))
                continue;
            const Node& n = nodes_[static_cast<std::size_t>(it->node)];
            const double shown = nodes_[static_cast<std::size_t>(zoom_)].value;
            const int decimals = n.value == std::floor(n.value) ? 0 : 2;
            std::vector<ToolTipRow> rows = {
                {colorOf(n), translate(LocalizedText("Samples")), formatNumber(n.value, decimals)},
                {std::nullopt, translate(LocalizedText("Of all")), formatNumber(n.value / all * 100, 1) + "%"},
            };
            if (zoom_ != 0 && shown > 0)
                rows.push_back({std::nullopt, translate(LocalizedText("Of what shows")), formatNumber(n.value / shown * 100, 1) + "%"});
            if (n.self > 0 && !n.children.empty())
                rows.push_back({std::nullopt, translate(LocalizedText("Its own")), formatNumber(n.self / all * 100, 1) + "%"});
            paintToolTip(context, *mouse_, it->node == 0 ? translate(props_.rootName) : n.name, rows);
            break;
        }
    }
}

// ---- the mouse ----

void FlameGraph::onMouse(MouseEvent& event) {
    switch (event.type) {
    case MouseEventType::Move:
        mouse_ = event.position;
        invalidatePaint();
        return;
    case MouseEventType::Press:
        if (event.button != platform::MouseButton::Left)
            return;
        if (event.clicks == 2) {
            zoomOut();
        } else {
            for (auto it = placed_.rbegin(); it != placed_.rend(); ++it) {
                if (it->rect.contains(event.position)) {
                    zoomInto(it->node);
                    break;
                }
            }
        }
        event.handled = true;
        return;
    default:
        return;
    }
}

void FlameGraph::onHoverChanged(bool hovered) {
    if (!hovered) {
        mouse_.reset();
        invalidatePaint();
    }
}

std::optional<platform::Cursor> FlameGraph::cursorAt(Point point) const {
    for (const Placed& placed : placed_)
        if (placed.rect.contains(point))
            return platform::Cursor::Pointer;
    return Widget::cursorAt(point);
}

} // namespace glw
