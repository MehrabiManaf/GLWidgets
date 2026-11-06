// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Charts/PieChart.hpp>

#include <GLWidgets/Charts/ChartAxis.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>

namespace glw {

namespace {

constexpr float pi = 3.14159265358979f;
constexpr float turn = 2 * pi;
constexpr float popOut = 7;             // how far the slice under the mouse comes out, dips (rings: less)
constexpr int mostRings = 6;
constexpr char between = '\x1f';        // between the labels of a key
constexpr const char* arrow = " \xE2\x80\xBA ";     // " › ": between the labels of a path shown
constexpr const char* othersPart = "\x1e";              // the others' slice in a key

// What a slice is called in a key: its label, or its place when it has none.
std::string part(const PieSlice& slice, std::size_t index) {
    return slice.label.source.empty() ? "#" + std::to_string(index) : slice.label.source;
}

} // namespace

double PieSlice::total() const {
    if (children.empty())
        return std::max(0.0, value);
    double sum = 0;
    for (const PieSlice& child : children)
        sum += child.total();
    return sum;
}

PieChart::PieChart() : PieChart(Props{}) {}

PieChart::PieChart(Props props)
    : props_(std::move(props)), progress_([this] { invalidatePaint(); }), sweep_([this] { invalidatePaint(); }),
      zoom_([this] { invalidatePaint(); }) {
    setTitle(props_.title);
    setFocusable(true);
    progress_.set(1);
    sweep_.set(0);
    zoom_.set(1);
}

PieChart::~PieChart() = default;

// ---- its slices ----

const std::vector<PieSlice>* PieChart::partsOf(std::span<const std::size_t> path) const {
    const std::vector<PieSlice>* parts = &slices_;
    for (std::size_t i : path) {
        if (i >= parts->size())
            return nullptr;
        parts = &(*parts)[i].children;
    }
    return parts;
}

std::string PieChart::keyOf(std::span<const std::size_t> path) const {
    std::string key;
    const std::vector<PieSlice>* parts = &slices_;
    for (std::size_t i : path) {
        key += between;
        if (i >= parts->size()) {
            key += othersPart;      // the others' slice (it has no parts)
            break;
        }
        key += part((*parts)[i], i);
        parts = &(*parts)[i].children;
    }
    return key;
}

double PieChart::target(const PieSlice& slice, const std::string& key, bool hidden) const {
    hidden = hidden || hidden_.contains(key);
    if (slice.children.empty())
        return hidden ? 0.0 : std::max(0.0, slice.value);
    double sum = 0;
    for (std::size_t i = 0; i < slice.children.size(); ++i)
        sum += target(slice.children[i], key + between + part(slice.children[i], i), hidden);
    return sum;
}

std::vector<PieChart::Item> PieChart::items(std::unordered_map<std::string, double>* leaves) const {
    std::vector<Item> out;
    std::vector<std::size_t> path;
    const float t = progress_.value();
    others_.label = props_.others;
    const Color othersColor = style().text;
    // a slice without parts: eased from where it was
    const auto leaf = [&](const PieSlice& slice, const std::string& key, bool gone) {
        const double to = gone ? 0.0 : std::max(0.0, slice.value);
        const auto from = from_.find(key);
        const double was = from != from_.end() ? from->second : 0.0;
        const double value = was + (to - was) * t;
        if (leaves)
            (*leaves)[key] = value;
        return value;
    };
    // ... one with parts, in the others' slice: its value, no items for it
    const auto quiet = [&](auto& self, const PieSlice& slice, const std::string& key, bool gone) -> double {
        gone = gone || hidden_.contains(key);
        if (slice.children.empty())
            return leaf(slice, key, gone);
        double sum = 0;
        for (std::size_t i = 0; i < slice.children.size(); ++i)
            sum += self(self, slice.children[i], key + between + part(slice.children[i], i), gone);
        return sum;
    };
    // each slice and its parts, a slice's value the sum of its parts'
    const auto walk = [&](auto& self, const std::vector<PieSlice>& parts, const std::string& parentKey, int depth, Color parentColor,
                          bool hidden) -> double {
        // the largest only, the others in one slice (two or more of them; their order kept)
        std::vector<char> merged(parts.size(), 0);
        std::size_t mergedCount = 0;
        if (props_.top > 0 && parts.size() > props_.top + 1) {
            std::vector<std::pair<double, std::size_t>> by(parts.size());
            // by their sizes, hidden or not (one hidden stays where its legend item can show it again)
            for (std::size_t i = 0; i < parts.size(); ++i)
                by[i] = {parts[i].total(), i};
            std::partial_sort(by.begin(), by.begin() + static_cast<std::ptrdiff_t>(props_.top), by.end(),
                              [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
            for (std::size_t k = props_.top; k < by.size(); ++k)
                merged[by[k].second] = 1;
            mergedCount = by.size() - props_.top;
        }
        double sum = 0;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (merged[i])
                continue;
            const PieSlice& slice = parts[i];
            const std::string key = parentKey + between + part(slice, i);
            const bool gone = hidden || hidden_.contains(key);
            Color color;
            if (slice.color) {
                color = *slice.color;
            } else if (depth == 0) {
                color = seriesColor(i);
            } else {
                // a shade of its parent's, lighter out, a little apart from its siblings
                color = mix(parentColor, Color{1, 1, 1, parentColor.a}, 0.12f + 0.08f * static_cast<float>(i % 3));
            }
            path.push_back(i);
            const std::size_t at = out.size();
            out.push_back({&slice, key, path, depth, 0, 0, 0, color});
            const double value = slice.children.empty() ? leaf(slice, key, gone) : self(self, slice.children, key, depth + 1, color, gone);
            out[at].value = value;
            sum += value;
            path.pop_back();
        }
        if (mergedCount > 0) {
            // the others, after the top ones
            const std::string key = parentKey + between + othersPart;
            const bool gone = hidden || hidden_.contains(key);
            path.push_back(othersIndex);
            const std::size_t at = out.size();
            out.push_back({&others_, key, path, depth, 0, 0, 0, othersColor, mergedCount});
            double value = 0;
            for (std::size_t i = 0; i < parts.size(); ++i)
                if (merged[i])
                    value += quiet(quiet, parts[i], parentKey + between + part(parts[i], i), gone);
            out[at].value = value;
            sum += value;
            path.pop_back();
        }
        return sum;
    };
    const double total = walk(walk, slices_, std::string(), 0, Color{}, false);
    // their places in the whole, in turn at each depth: a slice's parts from where it starts
    std::vector<double> cursor(1, 0.0);
    for (Item& item : out) {
        const auto d = static_cast<std::size_t>(item.depth);
        if (cursor.size() < d + 2)
            cursor.resize(d + 2, 0.0);
        item.from = cursor[d];
        item.to = item.from + (total > 0 ? item.value / total : 0.0);
        cursor[d] = item.to;
        cursor[d + 1] = item.from;
    }
    return out;
}

void PieChart::snapshot() {
    // every slice without parts, those among the others too: what each eases from
    std::unordered_map<std::string, double> now;
    items(&now);
    from_ = std::move(now);
}

void PieChart::eased() {
    progress_.set(0);
    progress_.animateTo(scheduler(), 1, transition());
    invalidatePaint();
}

void PieChart::setSlices(std::vector<PieSlice> slices) {
    snapshot();
    slices_ = std::move(slices);
    // as far down the focus as the new slices go
    std::size_t valid = 0;
    for (const std::vector<PieSlice>* parts = &slices_; valid < focus_.size() && focus_[valid] < parts->size() &&
                                                         !(*parts)[focus_[valid]].children.empty();
         ++valid)
        parts = &(*parts)[focus_[valid]].children;
    focus_.resize(valid);
    eased();
}

void PieChart::setValue(std::size_t index, double value) {
    if (index >= slices_.size() || slices_[index].value == value)
        return;
    snapshot();
    slices_[index].value = value;
    eased();
}

void PieChart::setSliceVisible(std::size_t index, bool visible) {
    const std::vector<PieSlice>* parts = partsOf(focus_);
    if (!parts || index >= parts->size() || sliceVisible(index) == visible)
        return;
    std::vector<std::size_t> path = focus_;
    path.push_back(index);
    snapshot();
    if (visible)
        hidden_.erase(keyOf(path));
    else
        hidden_.insert(keyOf(path));
    eased();
}

bool PieChart::sliceVisible(std::size_t index) const {
    std::vector<std::size_t> path = focus_;
    path.push_back(index);
    return !hidden_.contains(keyOf(path));
}

double PieChart::shownValue(std::size_t index) const {
    std::vector<std::size_t> path = focus_;
    path.push_back(index);
    for (const Item& item : items())
        if (item.path == path)
            return item.value;
    return 0;
}

void PieChart::setHole(float hole) {
    props_.hole = hole;
    invalidatePaint();
}

void PieChart::setStartAngle(float degrees) {
    props_.startAngle = degrees;
    invalidatePaint();
}

void PieChart::setTop(std::size_t top) {
    snapshot();
    props_.top = top;
    eased();
}

void PieChart::setRings(int rings) {
    props_.rings = std::max(0, rings);
    invalidatePaint();
}

void PieChart::setFormatter(std::function<std::string(double)> format) {
    format_ = std::move(format);
    invalidatePaint();
}

std::string PieChart::format(double value) const {
    if (format_)
        return format_(value);
    const int decimals = value == std::floor(value) ? 0 : std::abs(value) < 10 ? 2 : 1;
    return formatNumber(value, decimals);
}

std::string PieChart::pathText(std::span<const std::size_t> path) const {
    // from the slice zoomed into down to it
    std::string text;
    const std::vector<PieSlice>* parts = &slices_;
    for (std::size_t depth = 0; depth < path.size(); ++depth) {
        if (path[depth] >= parts->size()) {
            // the others' slice
            if (!text.empty())
                text += arrow;
            text += translate(props_.others);
            break;
        }
        const PieSlice& slice = (*parts)[path[depth]];
        if (depth >= focus_.size() || depth + 1 == path.size()) {
            if (!text.empty())
                text += arrow;
            text += translate(slice.label);
        }
        parts = &slice.children;
    }
    return text;
}

int PieChart::ringsUnder(const std::vector<PieSlice>& parts) const {
    int rings = parts.empty() ? 0 : 1;
    for (const PieSlice& slice : parts)
        if (!slice.children.empty())
            rings = std::max(rings, 1 + ringsUnder(slice.children));
    return rings;
}

// ---- zooming ----

void PieChart::setFocus(std::vector<std::size_t> path) {
    const std::vector<PieSlice>* parts = partsOf(path);
    if (!parts || parts->empty() || path == focus_)
        return;
    // from what shows now
    fromLow_ = lastLow_;
    fromHigh_ = lastHigh_;
    fromDepth_ = lastDepth_;
    fromRings_ = lastRings_;
    focus_ = std::move(path);
    // the slice under the mouse back in its place: it is somewhere else now
    if (hovered_)
        if (const auto found = out_.find(*hovered_); found != out_.end())
            found->second->animateTo(scheduler(), 0, transition(true));
    hovered_.reset();
    zoom_.set(0);
    zoom_.animateTo(scheduler(), 1, transition() * 2, Easing::InOutCubic);
    invalidatePaint();
}

bool PieChart::zoomOut() {
    if (focus_.empty())
        return false;
    std::vector<std::size_t> up = focus_;
    up.pop_back();
    setFocus(std::move(up));
    return true;
}

// ---- painting ----

void PieChart::paint(PaintContext& context) {
    const Rect area = paintFrame(context);
    paint::Painter& p = context.painter;
    const ChartStyle& s = style();
    painted_ = items();
    shown_.clear();
    double total = 0;
    for (const Item& item : painted_)
        if (item.depth == 0)
            total += item.value;
    // the first time it shows with something to show: around into place
    if (!swept_ && total > 0) {
        swept_ = true;
        const Duration duration = transition() * 3;
        if (duration > Duration::zero())
            sweep_.animateTo(scheduler(), 1, duration, Easing::InOutCubic);
        else
            sweep_.set(1);
    }
    center_ = area.center();
    radius_ = std::max(0.0f, std::min(area.width, area.height) / 2 - popOut - 2);
    inner_ = radius_ * std::clamp(props_.hole, 0.0f, 0.95f);
    if (radius_ < 8)
        return;

    // the view: the part of the turn of the slice zoomed into, its parts in the first ring; eased there
    const Item* focused = nullptr;
    for (const Item& item : painted_)
        if (!focus_.empty() && item.path == focus_)
            focused = &item;
    const double toLow = focused ? focused->from : 0.0, toHigh = focused ? focused->to : 1.0;
    const std::vector<PieSlice>* parts = partsOf(focus_);
    const float toRings = static_cast<float>(props_.rings > 0 ? props_.rings : std::clamp(parts ? ringsUnder(*parts) : 1, 1, mostRings));
    const float z = zoom_.value();
    const double low = fromLow_ + (toLow - fromLow_) * z, high = fromHigh_ + (toHigh - fromHigh_) * z;
    const float depth = fromDepth_ + (static_cast<float>(focus_.size()) - fromDepth_) * z;
    const float rings = fromRings_ + (toRings - fromRings_) * z;
    lastLow_ = low;
    lastHigh_ = high;
    lastDepth_ = depth;
    lastRings_ = rings;
    const double whole = focused ? focused->value : total;
    if (total <= 0 || high - low <= 1e-12) {
        p.fillArc(center_, inner_, radius_, 0, turn, 0, s.grid);
        return;
    }

    const float thickness = (radius_ - inner_) / std::max(rings, 1.0f);
    const float start = props_.startAngle * pi / 180;
    const float full = turn * sweep_.value();
    const auto angleOf = [&](double f) { return start + static_cast<float>((std::clamp(f, low, high) - low) / (high - low)) * full; };
    const float out = popOut * (rings > 1.5f ? 0.6f : 1.0f);
    const float gap = s.gap / 2;
    // rings further apart than a slice comes out under the mouse (along its middle: a wide one's far side comes in as
    // much), so that it never goes into the next ring, nor into the one before it
    const float ringGap = std::max(gap, (out + s.gap) / 2);
    const bool settled = sweep_.value() >= 1 && zoom_.value() >= 1;
    // its slices raised as the style's bars are: reliefs (Plastic, Metal), lit from above with a light line along their top
    // (Studio), flat (Fluid, Plain)
    const Bevel& bevel = s.barBevel;
    for (std::size_t k = 0; k < painted_.size(); ++k) {
        const Item& item = painted_[k];
        const float ring = static_cast<float>(item.depth) - depth;      // 0: the first ring out of the hole
        if (item.value <= 0 || ring + 1 <= 0.001f || ring >= rings - 0.001f)
            continue;
        float r0 = std::max(inner_, inner_ + ring * thickness), r1 = inner_ + (ring + 1) * thickness;
        // rings a gap apart too
        if (ring > 0.01f)
            r0 += ringGap;
        if (ring + 1 < rings - 0.01f)
            r1 -= ringGap;
        const float a0 = angleOf(item.from), sweep = angleOf(item.to) - a0;
        if (r1 - r0 < 0.5f || sweep < 1e-3f)
            continue;
        const float middle = a0 + sweep / 2;
        const Point direction{std::cos(middle), std::sin(middle)};
        const auto moved = out_.find(item.key);
        const Point at = center_ + direction * (moved != out_.end() ? moved->second->value() * out : 0.0f);
        const bool raised = hovered_ == item.key;
        Color color = item.color;
        if (raised)
            color = mix(color, Color{1, 1, 1, color.a}, 0.10f);
        if (bevel.relief > 0) {
            // its shadow a little down (what shows of it: along its lower edges), then the relief (higher under the mouse)
            if (bevel.shadow.a > 0)
                p.fillArc({at.x, at.y + bevel.relief * 0.7f}, r0, r1, a0, sweep, gap, withAlpha(bevel.shadow, bevel.shadow.a * 0.6f));
            p.fillArcRelief(at, r0, r1, a0, sweep, gap, bevel.relief * (raised ? 1.6f : 1.0f), bevel.roundness * 1.5f, color);
        } else if (bevel.gradient > 0 || bevel.highlight.a > 0) {
            p.fillArc(at, r0, r1, a0, sweep, gap, lit(color, bevel.gradient * 1.5f), lit(color, -bevel.gradient * 0.75f),
                      paint::Painter::ArcShading::Vertical);
            if (bevel.highlight.a > 0)
                p.fillArc(at, r1 - 1.5f, r1, a0, sweep, gap, bevel.highlight, withAlpha(bevel.highlight, 0), paint::Painter::ArcShading::Vertical);
        } else {
            p.fillArc(at, r0, r1, a0, sweep, gap, color);
        }
        shown_.push_back({k, r0, r1, a0, sweep, at});
        // its share of what shows in it, where it fits
        if (props_.shares && settled && whole > 0) {
            const double share = item.value / whole;
            const std::string text = formatNumber(share * 100, share < 0.1 ? 1 : 0) + "%";
            const Size size = textSize(text);
            const float r = r0 > 0.5f ? (r0 + r1) / 2 : r1 * 0.64f;
            if (r * sweep > size.width + 8 && r1 - r0 > size.height + 6)
                drawText(context, text, at + direction * r, textOn(color), 0, {0.5f, 0.5f});
        }
    }
    // the hole: the total of what shows (or the slice zoomed into's), or the value of the slice under the mouse
    const Item* under = nullptr;
    for (const Item& item : painted_)
        if (hovered_ && item.key == *hovered_)
            under = &item;
    if (inner_ > 28) {
        const std::string value = format(under ? under->value : whole);
        const std::string label = under      ? translate(under->slice->label)
                                  : focused ? translate(focused->slice->label)
                                            : translate(props_.totalLabel);
        const float big = std::min(inner_ * 0.42f, s.titleSize * 1.6f);
        const float valueHeight = textSize(value, big).height;
        const float labelHeight = label.empty() ? 0.0f : textSize(label).height;
        const float top = center_.y - (valueHeight + labelHeight) / 2;
        if (textSize(value, big).width < inner_ * 1.8f)
            drawText(context, value, {center_.x, top}, s.title, big, {0.5f, 0});
        if (!label.empty() && textSize(label).width < inner_ * 1.8f)
            drawText(context, label, {center_.x, top + valueHeight}, s.text, 0, {0.5f, 0});
    }
    if (under && mouse_) {
        const ToolTipRow row{under->color, format(under->value), whole > 0 ? formatNumber(under->value / whole * 100, 1) + "%" : std::string()};
        std::string title = pathText(under->path);
        if (under->merged > 0)
            title += " (" + std::to_string(under->merged) + ")";
        paintToolTip(context, *mouse_, title, std::span(&row, 1));
    }
}

// ---- the mouse ----

std::optional<std::vector<std::size_t>> PieChart::nodeAt(Point at) const {
    for (std::size_t i = shown_.size(); i-- > 0;) {
        const Shown& s = shown_[i];
        const Point d = at - s.center;
        const float r = std::sqrt(d.x * d.x + d.y * d.y);
        if (r < s.inner || r > s.outer)
            continue;
        float a = std::fmod(std::atan2(d.y, d.x) - s.start, turn);
        if (a < 0)
            a += turn;
        if (a <= s.sweep && s.item < painted_.size())
            return painted_[s.item].path;
    }
    return std::nullopt;
}

std::optional<std::size_t> PieChart::sliceAt(Point at) const {
    const auto path = nodeAt(at);
    if (!path || path->size() <= focus_.size() || !std::equal(focus_.begin(), focus_.end(), path->begin()))
        return std::nullopt;
    return (*path)[focus_.size()];
}

void PieChart::onMouse(MouseEvent& event) {
    if (legendMouse(event))
        return;
    const Point at = event.position;
    switch (event.type) {
    case MouseEventType::Move: {
        mouse_ = at;
        std::optional<std::string> over;
        if (const auto path = nodeAt(at))
            over = keyOf(*path);
        if (over != hovered_) {
            if (hovered_)
                if (const auto found = out_.find(*hovered_); found != out_.end())
                    found->second->animateTo(scheduler(), 0, transition(true));
            if (over) {
                auto& moved = out_[*over];
                if (!moved)
                    moved = std::make_unique<Animation>([this] { invalidatePaint(); });
                moved->animateTo(scheduler(), 1, transition(true));
            }
            hovered_ = over;
        }
        invalidatePaint();
        return;
    }
    case MouseEventType::Press: {
        focus();
        if (event.button == platform::MouseButton::Right || event.button == platform::MouseButton::Back) {
            event.handled = zoomOut();
            return;
        }
        if (event.button != platform::MouseButton::Left)
            return;
        const Point d = at - center_;
        if (std::sqrt(d.x * d.x + d.y * d.y) < inner_) {
            // the hole: back up
            event.handled = zoomOut();
            return;
        }
        if (const auto path = nodeAt(at)) {
            const std::vector<PieSlice>* parts = partsOf(*path);
            if (parts && !parts->empty())
                setFocus(*path);
            else if (onSliceClicked_)
                onSliceClicked_(*path);
            event.handled = true;
        }
        return;
    }
    case MouseEventType::Wheel:
    case MouseEventType::Release:
        return;
    }
}

void PieChart::onKey(KeyEvent& event) {
    if (event.pressed && (event.key == platform::Key::Backspace || event.key == platform::Key::Escape) && zoomOut())
        event.handled = true;
}

void PieChart::onHoverChanged(bool hovered) {
    if (hovered)
        return;
    if (hovered_)
        if (const auto found = out_.find(*hovered_); found != out_.end())
            found->second->animateTo(scheduler(), 0, transition(true));
    hovered_.reset();
    mouse_.reset();
    invalidatePaint();
}

std::vector<Chart::LegendItem> PieChart::legendItems() const {
    // the parts of the slice zoomed into that show (the top ones and the others' slice), and those hidden
    std::vector<LegendItem> items;
    legendKeys_.clear();
    const std::vector<PieSlice>* parts = partsOf(focus_);
    if (!parts)
        return items;
    for (const Item& item : this->items()) {
        if (item.path.size() != focus_.size() + 1 || !std::equal(focus_.begin(), focus_.end(), item.path.begin()))
            continue;
        const std::string label = item.merged > 0 ? translate(props_.others) + " (" + std::to_string(item.merged) + ")" : translate(item.slice->label);
        items.push_back({label, item.color, !hidden_.contains(item.key)});
        legendKeys_.push_back(item.key);
    }
    return items;
}

void PieChart::legendItemClicked(std::size_t index) {
    if (index >= legendKeys_.size())
        return;
    const std::string key = legendKeys_[index];
    snapshot();
    if (hidden_.contains(key))
        hidden_.erase(key);
    else
        hidden_.insert(key);
    eased();
}

} // namespace glw
