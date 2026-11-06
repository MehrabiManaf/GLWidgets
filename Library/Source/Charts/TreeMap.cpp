// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/TreeMap.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace glw {

namespace {

constexpr float inset = 3;                      // a parent's children in from its sides
constexpr const char* separator = " \xE2\x80\xBA ";     // " › ": between the names of a path
constexpr const char* arrow = "\xE2\x80\xBA";           // ... along the top, a little apart from them
constexpr float crumbGap = 5;

// The rectangles of `values` (largest first, none 0) filling `area`, squarified: rows along its shorter side, each as
// long as keeps the worst of its tiles' aspects falling.
std::vector<Rect> squarify(std::span<const double> values, Rect area) {
    std::vector<Rect> out(values.size(), Rect{area.x, area.y, 0, 0});
    const double total = std::accumulate(values.begin(), values.end(), 0.0);
    if (!(total > 0) || area.empty())
        return out;
    const double scale = static_cast<double>(area.width) * area.height / total;
    double fx = area.x, fy = area.y, fw = area.width, fh = area.height;
    std::size_t i = 0;
    while (i < values.size()) {
        const double side = std::min(fw, fh);
        if (side <= 0)
            break;
        // the worst aspect of a row of `sum` with its largest `most` and smallest `least`, along `side`
        const auto worst = [&](double sum, double most, double least) {
            return std::max(side * side * most / (sum * sum), sum * sum / (side * side * least));
        };
        std::size_t j = i;
        double sum = 0, aspect = std::numeric_limits<double>::infinity();
        while (j < values.size()) {
            const double next = sum + values[j] * scale;
            const double w = worst(next, values[i] * scale, values[j] * scale);
            if (j > i && w > aspect)
                break;
            aspect = w;
            sum = next;
            ++j;
        }
        // the row: across the shorter side, as thick as its area takes
        const double thickness = sum / side;
        double along = 0;
        for (std::size_t k = i; k < j; ++k) {
            const double length = values[k] * scale / thickness;
            if (fw >= fh)
                out[k] = {static_cast<float>(fx), static_cast<float>(fy + along), static_cast<float>(thickness), static_cast<float>(length)};
            else
                out[k] = {static_cast<float>(fx + along), static_cast<float>(fy), static_cast<float>(length), static_cast<float>(thickness)};
            along += length;
        }
        if (fw >= fh) {
            fx += thickness;
            fw -= thickness;
        } else {
            fy += thickness;
            fh -= thickness;
        }
        i = j;
    }
    return out;
}

Rect between(Rect a, Rect b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.width + (b.width - a.width) * t, a.height + (b.height - a.height) * t};
}

} // namespace

double TreeMapNode::total() const {
    if (children.empty())
        return std::max(0.0, value);
    double sum = 0;
    for (const TreeMapNode& child : children)
        sum += child.total();
    return sum;
}

TreeMap::TreeMap() : TreeMap(Props{}) {}

TreeMap::TreeMap(Props props) : props_(std::move(props)), progress_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setFocusable(true);
    progress_.set(1);
}

TreeMap::~TreeMap() = default;

void TreeMap::setRoot(TreeMapNode root) {
    root_ = std::move(root);
    // as far down the focus as the new data goes
    std::size_t valid = 0;
    for (const TreeMapNode* node = &root_; valid < focus_.size() && focus_[valid] < node->children.size(); ++valid)
        node = &node->children[focus_[valid]];
    focus_.resize(valid);
    changed(true);
}

void TreeMap::setDepth(int depth) {
    props_.depth = std::max(1, depth);
    changed(true);
}

void TreeMap::setFocus(std::vector<std::size_t> path) {
    if (!nodeOf(path) || path == focus_)
        return;
    focus_ = std::move(path);
    changed(true);
}

bool TreeMap::zoomOut() {
    if (focus_.empty())
        return false;
    focus_.pop_back();
    changed(true);
    return true;
}

void TreeMap::setFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string TreeMap::format(double value) const {
    if (format_)
        return format_(value);
    return formatNumber(value, value == std::floor(value) ? 0 : 1);
}

float TreeMap::headerHeight() const {
    return textSize("Ag").height + 8;
}

const TreeMapNode* TreeMap::nodeOf(std::span<const std::size_t> path) const {
    const TreeMapNode* node = &root_;
    for (std::size_t i : path) {
        if (i >= node->children.size())
            return nullptr;
        node = &node->children[i];
    }
    return node;
}

void TreeMap::changed(bool eased) {
    // from where they show now
    from_.clear();
    if (eased)
        for (const Tile& t : tiles_)
            from_[t.key] = shownRect(t);
    eased_ = eased;
    stale_ = true;
    hovered_.reset();
    invalidatePaint();
}

Rect TreeMap::shownRect(const Tile& tile) const {
    const float t = progress_.value();
    if (t >= 1)
        return tile.rect;
    if (const auto from = from_.find(tile.key); from != from_.end())
        return between(from->second, tile.rect, t);
    // a tile that is new: out of its middle
    const Point c = tile.rect.center();
    return between({c.x, c.y, 0, 0}, tile.rect, t);
}

void TreeMap::layOut(const TreeMapNode& node, std::vector<std::size_t>& path, const std::string& key, Rect area, int level,
                     Color color, std::vector<Tile>& out) const {
    // its children with something in them, the largest first
    std::vector<std::size_t> order;
    std::vector<double> totals(node.children.size());
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        totals[i] = node.children[i].total();
        if (totals[i] > 0)
            order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return totals[a] > totals[b]; });
    std::vector<double> values;
    for (std::size_t i : order)
        values.push_back(totals[i]);
    const std::vector<Rect> rects = squarify(values, area);
    const ChartStyle& s = style();
    const float header = headerHeight();
    for (std::size_t k = 0; k < order.size(); ++k) {
        const std::size_t index = order[k];
        const TreeMapNode& child = node.children[index];
        // a gap between tiles: half of it on each side
        const Rect& r0 = rects[k];
        const float g = std::min({s.gap / 2, r0.width / 4, r0.height / 4});
        const Rect r{r0.x + g, r0.y + g, r0.width - 2 * g, r0.height - 2 * g};
        if (r.width < 1 || r.height < 1)
            continue;
        Color c;
        if (child.color) {
            c = *child.color;
        } else if (level == 0) {
            c = seriesColor(index);
        } else {
            // a shade of its parent's: lighter or darker by its place
            const float shade = (static_cast<float>(index % 4) - 1.5f) * 0.11f;
            c = shade >= 0 ? mix(color, Color{1, 1, 1, color.a}, shade) : mix(color, Color{0, 0, 0, color.a}, -shade);
        }
        std::string childKey = key;
        childKey += '\x1f';
        childKey += child.label.source.empty() ? "#" + std::to_string(index) : translate(child.label);
        path.push_back(index);
        const bool parent = !child.children.empty() && level + 1 < props_.depth && r.width > 48 && r.height > header + 24;
        out.push_back({childKey, path, r, c, level, parent, &child});
        if (parent)
            layOut(child, path, childKey, {r.x + inset, r.y + header, r.width - 2 * inset, r.height - header - inset}, level + 1, c, out);
        path.pop_back();
    }
}

void TreeMap::paint(PaintContext& context) {
    Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    const Palette& palette = context.ui.theme.palette;
    const bool rtl = rightToLeft();
    const float header = headerHeight();

    // the path to the node zoomed into, along the top: a way back to each node on it
    crumbs_.clear();
    if (!focus_.empty()) {
        float x = 0;
        const TreeMapNode* node = &root_;
        for (std::size_t depth = 0; depth <= focus_.size(); ++depth) {
            std::string name = node->label.source.empty() ? std::string("\xE2\x8C\x82") : translate(node->label);     // ⌂
            const Size size = textSize(name);
            const Rect rect{rtl ? area.right() - x - size.width : area.x + x, area.y, size.width, header};
            const bool last = depth == focus_.size();
            drawText(context, name, {rect.x, rect.y + header / 2}, last ? s.title : s.text, 0, {0, 0.5f});
            if (!last) {
                crumbs_.push_back({rect.inflated(3), depth});
                // (a text's spaces at its ends are not measured: apart by hand)
                const Size sep = textSize(arrow);
                drawText(context, arrow, {rtl ? rect.x - crumbGap - sep.width : rect.right() + crumbGap, rect.y + header / 2}, s.text, 0, {0, 0.5f});
                x += size.width + sep.width + 2 * crumbGap;
                node = &node->children[focus_[depth]];
            }
        }
        area.y += header + 2;
        area.height = std::max(0.0f, area.height - header - 2);
    }

    // laid out again when it changed, or when its room did (at once then)
    if (stale_ || !(laidOutIn_ == area)) {
        if (!stale_)
            from_.clear();
        const TreeMapNode* node = nodeOf(focus_);
        if (!node) {
            focus_.clear();
            node = &root_;
        }
        std::string key;
        for (std::size_t depth = 0; depth < focus_.size(); ++depth) {
            const TreeMapNode* on = nodeOf(std::span(focus_).first(depth + 1));
            key += '\x1f';
            key += on->label.source.empty() ? "#" + std::to_string(focus_[depth]) : translate(on->label);
        }
        tiles_.clear();
        std::vector<std::size_t> path = focus_;
        layOut(*node, path, key, area, 0, palette.accent, tiles_);
        if (rtl)
            for (Tile& t : tiles_)
                t.rect.x = area.x + area.right() - t.rect.right();
        laidOutIn_ = area;
        const Duration duration = stale_ && eased_ ? transition() * 2 : Duration::zero();
        stale_ = eased_ = false;
        if (duration > Duration::zero()) {
            progress_.set(0);
            progress_.animateTo(scheduler(), 1, duration, Easing::InOutCubic);
        } else {
            from_.clear();
            progress_.set(1);
        }
    }

    const float radius = s.barRadius;
    const float pad = 6;
    for (std::size_t i = 0; i < tiles_.size(); ++i) {
        const Tile& t = tiles_[i];
        const Rect r = shownRect(t);
        if (r.width < 0.5f || r.height < 0.5f)
            continue;
        const bool lit = hovered_ == i;
        const float rr = std::min({radius, r.width / 2, r.height / 2});
        if (t.parent) {
            // its name along its top, its children under it, on a dim shade of its color
            const Color body = mix(t.color, palette.window, 0.62f);
            p.fillRoundedRect(r, rr, lit ? mix(body, t.color, 0.25f) : body);
            const std::string name = translate(t.node->label);
            const Size size = textSize(name);
            if (progress_.value() >= 1 && size.width + 2 * pad < r.width) {
                drawText(context, name, {rtl ? r.right() - pad : r.x + pad, r.y + header / 2}, s.title, 0, {rtl ? 1.0f : 0.0f, 0.5f});
                const std::string value = format(t.node->total());
                if (size.width + textSize(value).width + 4 * pad < r.width)
                    drawText(context, value, {rtl ? r.x + pad : r.right() - pad, r.y + header / 2}, s.text, 0, {rtl ? 0.0f : 1.0f, 0.5f});
            }
            continue;
        }
        const Color color = lit ? mix(t.color, Color{1, 1, 1, t.color.a}, 0.14f) : t.color;
        if (r.width > 4 && r.height > 4)
            paintRaised(p, r, rr, color, {0, 0, 0, 0}, s.barBevel);
        else
            p.fillRect(r, color);
        // its name and its value in it, where they fit
        // (too small for any name: not measured, a treemap of thousands has many)
        if (progress_.value() < 1 || r.width < 2 * pad + s.fontSize || r.height < s.fontSize + pad)
            continue;
        const std::string name = translate(t.node->label);
        const Size size = textSize(name);
        const Color ink = textOn(color);
        if (size.width + 2 * pad <= r.width && size.height + pad <= r.height) {
            const float x = rtl ? r.right() - pad : r.x + pad;
            drawText(context, name, {x, r.y + pad - 2}, ink, 0, {rtl ? 1.0f : 0.0f, 0});
            const std::string value = format(t.node->total());
            const Size vs = textSize(value);
            if (vs.width + 2 * pad <= r.width && size.height + vs.height + pad <= r.height)
                drawText(context, value, {x, r.y + pad - 2 + size.height}, withAlpha(ink, ink.a * 0.72f), 0, {rtl ? 1.0f : 0.0f, 0});
        }
    }
    if (hovered_ && *hovered_ < tiles_.size() && mouse_) {
        const Tile& t = tiles_[*hovered_];
        std::string title;
        const TreeMapNode* node = &root_;
        for (std::size_t i = 0; i < t.path.size(); ++i) {
            node = &node->children[t.path[i]];
            if (i >= focus_.size() || i + 1 == t.path.size()) {
                if (!title.empty())
                    title += separator;
                title += translate(node->label);
            }
        }
        const TreeMapNode* parent = nodeOf(std::span(t.path).first(t.path.size() - 1));
        const double whole = parent ? parent->total() : 0;
        const double value = t.node->total();
        const ToolTipRow row{t.color, format(value), whole > 0 ? formatNumber(value / whole * 100, 1) + "%" : std::string()};
        paintToolTip(context, *mouse_, title, std::span(&row, 1));
    }
}

std::optional<std::vector<std::size_t>> TreeMap::nodeAt(Point at) const {
    for (std::size_t i = tiles_.size(); i-- > 0;)
        if (shownRect(tiles_[i]).contains(at))
            return tiles_[i].path;
    return std::nullopt;
}

std::optional<Rect> TreeMap::tileRect(std::span<const std::size_t> path) const {
    for (const Tile& t : tiles_)
        if (std::equal(t.path.begin(), t.path.end(), path.begin(), path.end()))
            return shownRect(t);
    return std::nullopt;
}

void TreeMap::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    // the tile of the top level under a point: what a click goes into
    const auto topAt = [&]() -> std::optional<std::vector<std::size_t>> {
        const auto path = nodeAt(at);
        if (!path || path->size() <= focus_.size())
            return std::nullopt;
        return std::vector<std::size_t>(path->begin(), path->begin() + static_cast<std::ptrdiff_t>(focus_.size() + 1));
    };
    switch (event.type) {
    case MouseEventType::Move: {
        mouse_ = at;
        std::optional<std::size_t> over;
        for (std::size_t i = tiles_.size(); i-- > 0;) {
            if (shownRect(tiles_[i]).contains(at)) {
                over = i;
                break;
            }
        }
        hovered_ = over;
        invalidatePaint();
        return;
    }
    case MouseEventType::Press:
        focus();
        if (event.button == platform::MouseButton::Right || event.button == platform::MouseButton::Back) {
            zoomOut();
            event.handled = true;
            return;
        }
        if (event.button != platform::MouseButton::Left)
            return;
        for (const Crumb& crumb : crumbs_) {
            if (crumb.rect.contains(at)) {
                setFocus(std::vector<std::size_t>(focus_.begin(), focus_.begin() + static_cast<std::ptrdiff_t>(crumb.depth)));
                event.handled = true;
                return;
            }
        }
        if (const auto top = topAt()) {
            const TreeMapNode* node = nodeOf(*top);
            if (node && !node->children.empty()) {
                setFocus(*top);
            } else if (onLeafClicked_) {
                const auto path = nodeAt(at);
                onLeafClicked_(*path);
            }
            event.handled = true;
        }
        return;
    case MouseEventType::Wheel:
        event.handled = true;
        if (progress_.running())        // a level at a time: not again while it goes there
            return;
        if (event.wheelDelta.y > 0) {
            if (const auto top = topAt())
                if (const TreeMapNode* node = nodeOf(*top); node && !node->children.empty())
                    setFocus(*top);
        } else if (event.wheelDelta.y < 0) {
            zoomOut();
        }
        event.handled = true;
        return;
    case MouseEventType::Release:
        return;
    }
}

void TreeMap::onKey(KeyEvent& event) {
    if (event.pressed && (event.key == platform::Key::Backspace || event.key == platform::Key::Escape) && zoomOut())
        event.handled = true;
}

void TreeMap::onHoverChanged(bool hovered) {
    if (!hovered) {
        hovered_.reset();
        mouse_.reset();
        invalidatePaint();
    }
}

} // namespace glw
