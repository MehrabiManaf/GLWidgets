// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// A node editor on a Canvas: the ground a visual scripting editor (FlowGraph's) stands on. Nodes are widgets on the canvas
// (a header, pins in rows, fields and check boxes for the values of the inputs that have no link); links are Bezier curves
// the canvas draws under them. Drag a node by its header or body (the selected ones move together), a pin to another to
// link them (Alt and a click on a pin breaks its links), the empty canvas to select with a rectangle; the wheel zooms, the
// middle or right button pans; Delete removes the selection, F frames it (or everything), Ctrl+A selects all. A comment's
// header moves it with the nodes in it, its corner resizes it. Space or Tab searches the nodes to add one by its name;
// a link let go on the empty canvas searches those that have a pin for it, and links the one added.
//
//   node_editor [--nodes N] [--rtl] [--frames N] [--style studio|fluid|plain|plastic|metal]
//               [--colors dark|light|dusk|sand|turquoise]
//
// --nodes adds N more nodes linked in a chain (thousands of them: only those that show are painted).
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Canvas.hpp>
#include <GLWidgets/Widgets/CheckBox.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/SearchPopup.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace glw;

// ---- what nodes are ----

enum class Type { Exec, Bool, Int, Float, String };

Color typeColor(Type type) {
    switch (type) {
    case Type::Exec: return {0.92f, 0.92f, 0.94f, 1};
    case Type::Bool: return {0.80f, 0.18f, 0.20f, 1};
    case Type::Int: return {0.12f, 0.80f, 0.62f, 1};
    case Type::Float: return {0.56f, 0.86f, 0.25f, 1};
    case Type::String: return {0.93f, 0.25f, 0.76f, 1};
    }
    return {1, 1, 1, 1};
}

Color rgb(int r, int g, int b) {
    return {r / 255.0f, g / 255.0f, b / 255.0f, 1};
}

const Color eventHeader = rgb(150, 40, 40);
const Color functionHeader = rgb(45, 100, 165);
const Color pureHeader = rgb(55, 135, 85);
const Color flowHeader = rgb(105, 105, 112);
const Color variableHeader = rgb(150, 105, 45);

struct PinSpec {
    std::string name;
    Type type;
};

struct NodeSpec {
    std::string title;
    Color header;
    std::vector<PinSpec> inputs, outputs;
};

// What can be added, by category.
struct CatalogEntry {
    std::string category;
    NodeSpec spec;
};

const std::vector<CatalogEntry>& catalog() {
    const PinSpec exec{"", Type::Exec};
    static const std::vector<CatalogEntry> entries{
        {"Flow", {"Branch", flowHeader, {exec, {"Condition", Type::Bool}}, {{"True", Type::Exec}, {"False", Type::Exec}}}},
        {"Flow", {"Sequence", flowHeader, {exec}, {{"Then 0", Type::Exec}, {"Then 1", Type::Exec}}}},
        {"Flow", {"Delay", functionHeader, {exec, {"Seconds", Type::Float}}, {exec}}},
        {"Math", {"Add", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Sum", Type::Float}}}},
        {"Math", {"Subtract", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Difference", Type::Float}}}},
        {"Math", {"Multiply", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Result", Type::Float}}}},
        {"Math", {"Greater", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Result", Type::Bool}}}},
        {"Math", {"Increment", functionHeader, {exec, {"Value", Type::Int}, {"By", Type::Int}}, {exec, {"Value", Type::Int}}}},
        {"Logic", {"And", pureHeader, {{"A", Type::Bool}, {"B", Type::Bool}}, {{"Result", Type::Bool}}}},
        {"Logic", {"Not", pureHeader, {{"Value", Type::Bool}}, {{"Result", Type::Bool}}}},
        {"Text", {"Print String", functionHeader, {exec, {"In String", Type::String}, {"Print to Screen", Type::Bool}}, {exec}}},
        {"Text", {"To Text", pureHeader, {{"Value", Type::Int}}, {{"Text", Type::String}}}},
        {"Text", {"Append", pureHeader, {{"A", Type::String}, {"B", Type::String}}, {{"Result", Type::String}}}},
        {"Text", {"Write Log", functionHeader, {exec, {"Message", Type::String}}, {exec}}},
        {"Variables", {"Get Speed", variableHeader, {}, {{"Speed", Type::Float}}}},
        {"Variables", {"Set Speed", variableHeader, {exec, {"Speed", Type::Float}}, {exec, {"Speed", Type::Float}}}},
    };
    return entries;
}

constexpr float connectorRoom = 22;     // a pin's connector, on the node's side of its row

class Graph;
class NodeView;

// ---- a pin: its connector, its name, a field for its value while it has no link ----

class PinRow final : public Widget {
public:
    PinRow(Graph& graph, NodeView& node, PinSpec spec, bool input);

    NodeView& node() const { return node_; }
    Type type() const { return spec_.type; }
    bool input() const { return input_; }
    // The middle of its connector (its coordinates): at the start side for an input, the end side for an output.
    Point connector() const {
        const bool atStart = input_ != rightToLeft();
        return {atStart ? connectorRoom / 2 : size().width - connectorRoom / 2, size().height / 2};
    }
    void setLinked(bool linked) {
        linked_ = linked;
        if (editor_)
            editor_->setVisible(!linked);
        invalidatePaint();
    }

protected:
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool) override { invalidatePaint(); }

private:
    Graph& graph_;
    NodeView& node_;
    PinSpec spec_;
    bool input_;
    bool linked_ = false;
    Widget* editor_ = nullptr;
    glw::Animation glow_{[this] { invalidatePaint(); }};    // lit while a link dragged could end on it
};

// ---- a node ----

class NodeView final : public Widget {
public:
    NodeView(Graph& graph, const NodeSpec& spec);

    const std::vector<PinRow*>& pins() const { return pins_; }
    bool selected() const { return selected_; }
    void setSelected(bool selected) {
        selected_ = selected;
        invalidatePaint();
    }

protected:
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;

private:
    Graph& graph_;
    Color header_;
    Widget* title_ = nullptr;
    std::vector<PinRow*> pins_;
    bool selected_ = false;
};

// ---- a comment: a frame behind a group of nodes ----

class CommentView final : public Widget {
public:
    CommentView(Graph& graph, std::string text, Size size, Color color);

protected:
    Size measureContent(Size) override { return size_; }
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;

private:
    static constexpr float headerHeight = 30;
    static constexpr float grip = 16;
    Graph& graph_;
    Size size_;
    Color color_;
    bool resizing_ = false;
    Point grabbed_;                     // where the corner was pressed, from the corner
};

// ---- the graph ----

class Graph final : public Canvas {
public:
    Graph() {
        setOnViewChanged([this] { changed(); });
        setOnContextRequested([this](Point at) { contextMenu(at); });
    }

    // What the canvas offers where it was clicked: nodes to add there, and what is done to the selection.
    void contextMenu(Point at) {
        bool any = false;
        for (NodeView* node : nodes_)
            any = any || node->selected();
        // a submenu for each category, in the catalog's order
        std::vector<MenuItem> categories;
        for (const CatalogEntry& entry : catalog()) {
            if (categories.empty() || categories.back().text.source != entry.category)
                categories.push_back({.text = LocalizedText(entry.category)});
            categories.back().items.push_back(
                {.text = LocalizedText(entry.spec.title), .onTriggered = [this, at, spec = entry.spec] { select(addNode(spec, at), false); }});
        }
        showContextMenu(*this, fromCanvas(at),
                        {{.text = "Add Node", .items = std::move(categories)},
                         {.text = "Search Nodes…", .onTriggered = [this, at] { openPalette(at, nullptr); }, .shortcut = "Space"},
                         MenuItem::separatorItem(),
                         {.text = "Frame", .onTriggered = [this] { frameSelection(); }, .shortcut = "F"},
                         {.text = "Select All", .onTriggered = [this] { selectAll(true); }, .shortcut = "Ctrl+A"},
                         {.text = "Delete", .onTriggered = [this] { removeSelected(); }, .shortcut = "Del", .enabled = any},
                         MenuItem::separatorItem(),
                         {.text = "Show Grid", .onTriggered = [this] { setGridVisible(!grid()); }, .checked = grid()}});
    }

    // The nodes of the catalog to add at `at` (the canvas's), searched by name. Those with a pin `from` (let go on the empty
    // canvas) links to, when there is one: the node added is linked to it.
    void openPalette(Point at, PinRow* from) {
        std::vector<SearchItem> items;
        std::vector<std::size_t> which;
        for (std::size_t i = 0; i < catalog().size(); ++i) {
            const NodeSpec& spec = catalog()[i].spec;
            const auto& pins = from && !from->input() ? spec.inputs : spec.outputs;
            if (from && std::none_of(pins.begin(), pins.end(), [&](const PinSpec& p) { return p.type == from->type(); }))
                continue;
            items.push_back({LocalizedText(spec.title), LocalizedText(catalog()[i].category)});
            which.push_back(i);
        }
        // the pin by its node and its place, not a pointer: the node may go while the palette is open
        NodeView* node = from ? &from->node() : nullptr;
        const std::size_t pin = node ? std::size_t(std::find(node->pins().begin(), node->pins().end(), from) - node->pins().begin()) : 0;
        const Point shown = fromCanvas(at);
        showSearchPopup(*this, {shown.x, shown.y, 0, 0},
                        {.items = std::move(items),
                         .placeholder = "Search nodes",
                         .onChosen = [this, at, which, node, pin](std::size_t i) {
                             const NodeSpec& spec = catalog()[which[i]].spec;
                             const bool linked = node && std::find(nodes_.begin(), nodes_.end(), node) != nodes_.end();
                             // from an input: the node at the start side of the point, its output toward it
                             const bool toInput = linked && node->pins()[pin]->input();
                             NodeView& added = addNode(spec, toInput ? at - Point{rightToLeft() ? -40.0f : 180.0f, 20} : at - Point{0, 20});
                             select(added, false);
                             if (linked) {
                                 for (PinRow* p : added.pins()) {
                                     if (link(*node->pins()[pin], *p))
                                         break;
                                 }
                             }
                         }});
    }

    NodeView& addNode(const NodeSpec& spec, Point at) {
        NodeView& node = addItem(std::make_unique<NodeView>(*this, spec), at);
        nodes_.push_back(&node);
        changed();
        return node;
    }

    CommentView& addComment(std::string text, const Rect& area, Color color) {
        CommentView& comment = addItem(std::make_unique<CommentView>(*this, std::move(text), area.size(), color), {area.x, area.y});
        lowerItem(comment);
        return comment;
    }

    // The pin of a node by its place: its inputs first, then its outputs.
    static PinRow& pin(NodeView& node, std::size_t index) { return *node.pins()[index]; }

    // `a` and `b` linked, if they can be: an output and an input of one type on two nodes. An input has one link of data,
    // an output one of execution: the one it had goes.
    bool link(PinRow& a, PinRow& b) {
        PinRow& out = a.input() ? b : a;
        PinRow& in = a.input() ? a : b;
        if (out.input() || !in.input() || &out.node() == &in.node() || out.type() != in.type())
            return false;
        if (in.type() != Type::Exec)
            unlinkIf([&](const Link& l) { return l.in == &in; });
        else
            unlinkIf([&](const Link& l) { return l.out == &out; });
        links_.push_back({&out, &in, appearing()});
        out.setLinked(true);
        in.setLinked(true);
        changed();
        return true;
    }

    void unlink(PinRow& pin) {
        unlinkIf([&](const Link& l) { return l.out == &pin || l.in == &pin; });
    }

    void removeSelected() {
        std::vector<NodeView*> gone;
        for (NodeView* node : nodes_) {
            if (node->selected())
                gone.push_back(node);
        }
        for (NodeView* node : gone) {
            unlinkIf([&](const Link& l) { return &l.out->node() == node || &l.in->node() == node; });
            std::erase(nodes_, node);
            removeItem(*node);
        }
        changed();
    }

    // ---- choosing ----

    void select(NodeView& node, bool toggle) {
        if (toggle) {
            node.setSelected(!node.selected());
            return;
        }
        if (node.selected())
            return;
        for (NodeView* other : nodes_)
            other->setSelected(other == &node);
    }
    void selectAll(bool selected) {
        for (NodeView* node : nodes_)
            node->setSelected(selected);
    }
    void frameSelection() {
        Rect area;
        for (NodeView* node : nodes_) {
            if (node->selected())
                area = area.united(itemRect(*node));
        }
        if (area.empty())
            frameItems();
        else
            frame(area, 64);
    }

    // ---- moving ----

    // `items` follow the mouse from `at` (the canvas's coordinates).
    void beginMove(std::vector<Widget*> items, Point at) {
        moving_.clear();
        for (Widget* item : items)
            moving_.push_back({item, itemPosition(*item)});
        moveFrom_ = at;
    }
    void beginMoveSelection(NodeView& pressed, Point at) {
        std::vector<Widget*> items;
        for (NodeView* node : nodes_) {
            if (node->selected())
                items.push_back(node);
        }
        raiseItem(pressed);
        beginMove(std::move(items), at);
    }
    void moveTo(Point at) {
        const Point by = at - moveFrom_;
        for (const auto& [item, from] : moving_)
            setItemPosition(*item, from + by);
    }
    void endMove() { moving_.clear(); }

    // ---- linking ----

    void beginLink(PinRow& from, Point at) {
        linkFrom_ = &from;
        linkTo_ = at;
        invalidatePaint();
    }
    void linkTo(Point at) {
        linkTo_ = at;
        invalidatePaint();
    }
    // Let go at `rootPoint`: on a pin, or a node (its first pin that fits).
    void endLink(Point rootPoint) {
        PinRow* from = std::exchange(linkFrom_, nullptr);
        invalidatePaint();
        if (!from || !root())
            return;
        for (Widget* w = root()->widgetAt(rootPoint); w; w = w->parent()) {
            if (auto* pin = dynamic_cast<PinRow*>(w)) {
                link(*from, *pin);
                return;
            }
            if (auto* node = dynamic_cast<NodeView*>(w)) {
                for (PinRow* pin : node->pins()) {
                    if (link(*from, *pin))
                        return;
                }
                return;
            }
        }
        // on the empty canvas: what to link it to, to add there
        openPalette(linkTo_, from);
    }

    // A point of `widget` (in a node, a comment) on the canvas.
    Point inCanvas(const Widget& widget, Point local) const {
        const Widget* w = &widget;
        for (; w && w->parent() != this; w = w->parent())
            local = local + Point{w->bounds().x, w->bounds().y};
        return w ? local + Point{w->bounds().x, w->bounds().y} : local;
    }

    // While a link is dragged: whether it could end on `pin` (an output and an input of one type, on two nodes).
    bool linkFits(const PinRow& pin) const {
        if (!linkFrom_ || &pin == linkFrom_)
            return false;
        const PinRow& out = pin.input() ? *linkFrom_ : pin;
        const PinRow& in = pin.input() ? pin : *linkFrom_;
        return !out.input() && in.input() && &out.node() != &in.node() && out.type() == in.type();
    }

    void setOnChanged(std::function<void()> handler) { onChanged_ = std::move(handler); }
    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t linkCount() const { return links_.size(); }
    const std::vector<NodeView*>& nodes() const { return nodes_; }

protected:
    void paintUnderItems(PaintContext& context) override {
        const Rect shown = visibleArea();
        for (const Link& l : links_)
            drawLink(context.painter, inCanvas(*l.out, l.out->connector()), inCanvas(*l.in, l.in->connector()), l.out->type(), shown,
                     l.shown ? l.shown->value() : 1);
        // those gone, fading where they were
        std::erase_if(fading_, [](const Fading& f) { return !f.shown->running(); });
        for (const Fading& f : fading_)
            drawLink(context.painter, f.from, f.to, f.type, shown, f.shown->value());
    }

    void paintOverItems(PaintContext& context) override {
        paint::Painter& p = context.painter;
        if (linkFrom_) {
            const Point at = inCanvas(*linkFrom_, linkFrom_->connector());
            const Point from = linkFrom_->input() ? linkTo_ : at, to = linkFrom_->input() ? at : linkTo_;
            drawLink(p, from, to, linkFrom_->type(), visibleArea(), 0.7f);
        }
        if (band_) {
            const Color accent = context.ui.theme.palette.accent;
            const Rect r = bandRect();
            p.fillRect(r, {accent.r, accent.g, accent.b, 0.12f});
            p.strokeRoundedRect(r, 0, 1 / zoom(), accent);
        }
    }

    void onPreviewMouse(MouseEvent& event) override {
        if (event.type == MouseEventType::Move)
            mouse_ = event.position;         // where Space opens the palette
    }

    void onMouse(MouseEvent& event) override {
        // the left button on the empty canvas: a rectangle that selects what it touches
        if (event.button == platform::MouseButton::Left && event.type != MouseEventType::Wheel) {
            const Point at = toCanvas(event.position);
            switch (event.type) {
            case MouseEventType::Press:
                focus();
                bandAdds_ = platform::hasAny(event.modifiers, platform::shortcutModifier | platform::KeyModifiers::Shift);
                if (!bandAdds_)
                    selectAll(false);
                bandBefore_.clear();
                for (NodeView* node : nodes_)
                    bandBefore_.push_back(node->selected());
                band_ = {at, at};
                event.handled = true;
                return;
            case MouseEventType::Move:
                if (!band_)
                    break;
                band_->second = at;
                for (std::size_t i = 0; i < nodes_.size(); ++i)
                    nodes_[i]->setSelected(bandBefore_[i] || itemRect(*nodes_[i]).intersects(bandRect()));
                invalidatePaint();
                event.handled = true;
                return;
            case MouseEventType::Release:
                if (band_) {
                    band_.reset();
                    invalidatePaint();
                    event.handled = true;
                    return;
                }
                break;
            default: break;
            }
        }
        Canvas::onMouse(event);
    }

    void onKey(KeyEvent& event) override {
        if (!event.pressed)
            return;
        const bool shortcut = platform::hasAny(event.modifiers, platform::shortcutModifier);
        if (event.key == platform::Key::Delete) {
            removeSelected();
        } else if (event.key == platform::Key::F && !shortcut) {
            frameSelection();
        } else if ((event.key == platform::Key::Space || event.key == platform::Key::Tab) && !shortcut) {
            openPalette(toCanvas(mouse_), nullptr);
        } else if (event.key == platform::Key::A && shortcut) {
            selectAll(true);
        } else {
            return;
        }
        event.handled = true;
    }

private:
    struct Link {
        PinRow* out;
        PinRow* in;
        std::unique_ptr<glw::Animation> shown;      // from 0 to 1 as it comes (the theme's normal motion)
    };
    // a link gone, fading out where it was (its pins may be gone too)
    struct Fading {
        Point from;
        Point to;
        Type type;
        std::unique_ptr<glw::Animation> shown;
    };

    std::unique_ptr<glw::Animation> appearing() {
        auto a = std::make_unique<glw::Animation>([this] { invalidatePaint(); });
        a->set(0);
        a->animateTo(scheduler(), 1, context() ? Duration(context()->theme.motion.normal) : Duration::zero());
        return a;
    }

    template <class F>
    void unlinkIf(F&& gone) {
        std::vector<PinRow*> touched;
        std::erase_if(links_, [&](const Link& l) {
            if (!gone(l))
                return false;
            touched.push_back(l.out);
            touched.push_back(l.in);
            // faded out where it was
            auto fade = std::make_unique<glw::Animation>([this] { invalidatePaint(); });
            fade->set(l.shown ? l.shown->value() : 1.0f);
            fade->animateTo(scheduler(), 0, context() ? Duration(context()->theme.motion.normal) : Duration::zero());
            if (fade->running())
                fading_.push_back({inCanvas(*l.out, l.out->connector()), inCanvas(*l.in, l.in->connector()), l.out->type(), std::move(fade)});
            return true;
        });
        for (PinRow* pin : touched) {
            const bool still = std::any_of(links_.begin(), links_.end(), [&](const Link& l) { return l.out == pin || l.in == pin; });
            pin->setLinked(still);
        }
        if (!touched.empty())
            changed();
    }

    // From an output at `from` to an input at `to`: leaving the one and coming into the other along their sides.
    void drawLink(paint::Painter& p, Point from, Point to, Type type, const Rect& shown, float alpha) const {
        const float side = rightToLeft() ? -1.0f : 1.0f;
        const float pull = std::max(40.0f, std::abs(to.x - from.x) * 0.5f);
        const Point c1{from.x + side * pull, from.y}, c2{to.x - side * pull, to.y};
        const float left = std::min({from.x, to.x, c1.x, c2.x}), right = std::max({from.x, to.x, c1.x, c2.x});
        const float top = std::min(from.y, to.y), bottom = std::max(from.y, to.y);
        if (!Rect{left, top, right - left + 1, bottom - top + 1}.intersects(shown.inflated(8)))
            return;
        Color color = typeColor(type);
        color.a *= alpha;
        p.drawBezier(from, c1, c2, to, type == Type::Exec ? 3.0f : 2.0f, color);
    }

    Rect bandRect() const {
        const Point a = band_->first, b = band_->second;
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y)};
    }

    void changed() {
        if (onChanged_)
            onChanged_();
    }

    std::vector<NodeView*> nodes_;
    std::vector<Link> links_;
    std::vector<Fading> fading_;
    std::vector<std::pair<Widget*, Point>> moving_;
    Point moveFrom_;
    PinRow* linkFrom_ = nullptr;
    Point linkTo_;
    std::optional<std::pair<Point, Point>> band_;
    bool bandAdds_ = false;
    std::vector<bool> bandBefore_;
    Point mouse_;                           // its own coordinates
    std::function<void()> onChanged_;
};

// ---- pins ----

PinRow::PinRow(Graph& graph, NodeView& node, PinSpec spec, bool input) : graph_(graph), node_(node), spec_(std::move(spec)), input_(input) {
    setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 6.0f));
    setPadding(input ? Insets{connectorRoom, 2, 4, 2} : Insets{4, 2, connectorRoom, 2});
    setMinimumSize({0, 24});
    auto& name = add(std::make_unique<Label>(Label::Props{.text = LocalizedText(spec_.name), .muted = true}));
    name.setAlignment(Alignment::Start, Alignment::Center);
    // the value of an input without a link, typed in the node
    if (input && spec_.type != Type::Exec) {
        if (spec_.type == Type::Bool) {
            editor_ = &add(std::make_unique<CheckBox>());
        } else {
            const char* initial = spec_.type == Type::String ? "Hello" : "0";
            auto& field = add(std::make_unique<LineEdit>(LineEdit::Props{.text = initial}));
            field.setMinimumSize({spec_.type == Type::String ? 110.0f : 56.0f, 0});
            field.setMaximumSize({spec_.type == Type::String ? 110.0f : 56.0f, unbounded});
            editor_ = &field;
        }
        editor_->setAlignment(Alignment::Start, Alignment::Center);
    }
    if (!input)
        setAlignment(Alignment::End, Alignment::Start);
}

void PinRow::paint(PaintContext& context) {
    paint::Painter& p = context.painter;
    const Point c = connector();
    const Color color = typeColor(spec_.type);
    const bool lit = hovered();
    // where the link dragged could go: a soft glow of its color, eased in and out
    glow_.animateTo(scheduler(), graph_.linkFits(*this) ? 1.0f : 0.0f, context.ui.theme.motion.normal);
    if (const float g = glow_.value(); g > 0)
        p.drawShadow({c.x - 7, c.y - 7, 14, 14}, 7, 4, {color.r, color.g, color.b, 0.6f * g});
    if (spec_.type == Type::Exec) {
        // an arrow toward where execution goes
        const float d = rightToLeft() ? -1.0f : 1.0f;
        const Point points[] = {{c.x - 5 * d, c.y - 6}, {c.x + 1 * d, c.y - 6}, {c.x + 6 * d, c.y}, {c.x + 1 * d, c.y + 6}, {c.x - 5 * d, c.y + 6}};
        if (linked_ || lit) {
            p.fillConvexPolygon(points, color);
        } else {
            const Point closed[] = {points[0], points[1], points[2], points[3], points[4], points[0]};
            p.drawPolyline(closed, 1.5f, color);
        }
        return;
    }
    if (linked_ || lit)
        p.fillCircle(c, 5, color);
    else
        p.strokeCircle(c, 5, 1.5f, color);
}

void PinRow::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left)
        return;
    switch (event.type) {
    case MouseEventType::Press:
        event.handled = true;
        if (platform::hasAny(event.modifiers, platform::KeyModifiers::Alt)) {
            graph_.unlink(*this);
            return;
        }
        graph_.beginLink(*this, graph_.inCanvas(*this, event.position));
        break;
    case MouseEventType::Move:
        graph_.linkTo(graph_.inCanvas(*this, event.position));
        event.handled = true;
        break;
    case MouseEventType::Release:
        graph_.endLink(mapToRoot(event.position));
        event.handled = true;
        break;
    default: break;
    }
}

// ---- nodes ----

NodeView::NodeView(Graph& graph, const NodeSpec& spec) : graph_(graph), header_(spec.header) {
    setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 0.0f));
    setMinimumSize({150, 0});
    auto title = std::make_unique<Widget>();
    title->setPadding({12, 7, 12, 7});
    title->add(std::make_unique<Label>(Label::Props{.text = LocalizedText(spec.title), .color = Color{1, 1, 1, 0.95f}}));
    title_ = &add(std::move(title));

    auto body = std::make_unique<Widget>();
    body->setLayout(std::make_unique<StackLayout>(Orientation::Horizontal, 16.0f));
    body->setPadding({0, 6, 0, 10});
    auto column = [](float stretch) {
        auto c = std::make_unique<Widget>();
        c->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 2.0f));
        c->setStretch(stretch);
        return c;
    };
    auto inputs = column(1), outputs = column(0);
    for (const PinSpec& pin : spec.inputs)
        pins_.push_back(&inputs->add(std::make_unique<PinRow>(graph, *this, pin, true)));
    for (const PinSpec& pin : spec.outputs)
        pins_.push_back(&outputs->add(std::make_unique<PinRow>(graph, *this, pin, false)));
    body->add(std::move(inputs));
    body->add(std::move(outputs));
    add(std::move(body));
}

void NodeView::paint(PaintContext& context) {
    // as the theme's nodes are (NodeStyle): a shadow under it, its body, its header in its kind's color over it
    paint::Painter& p = context.painter;
    const NodeStyle& s = context.ui.theme.node;
    const Rect r{0, 0, size().width, size().height};
    const float radius = s.cornerRadius;
    if (s.shadow.a > 0)
        p.drawShadow({r.x, r.y + 3, r.width, r.height}, radius, 12, s.shadow);
    paintRaised(p, r, radius, s.body, {0, 0, 0, 0}, s.bodyBevel);
    // the header: rounded at the top only (cut out of a taller box)
    const float h = title_->bounds().bottom();
    p.pushClip({0, 0, r.width, h});
    paintRaised(p, {0, 0, r.width, h + radius}, radius, header_, {0, 0, 0, 0}, s.bevel);
    p.popClip();
    p.strokeRoundedRect(r, radius, 1, s.border);
}

void NodeView::paintOver(PaintContext& context) {
    const NodeStyle& s = context.ui.theme.node;
    if (selected_)
        context.painter.strokeRoundedRect({-2, -2, size().width + 4, size().height + 4}, s.cornerRadius + 2, 2, s.selection);
}

void NodeView::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left)
        return;
    const Point at = graph_.inCanvas(*this, event.position);
    switch (event.type) {
    case MouseEventType::Press:
        event.handled = true;
        graph_.focus();
        graph_.select(*this, platform::hasAny(event.modifiers, platform::shortcutModifier | platform::KeyModifiers::Shift));
        if (selected_)
            graph_.beginMoveSelection(*this, at);
        break;
    case MouseEventType::Move:
        graph_.moveTo(at);
        event.handled = true;
        break;
    case MouseEventType::Release:
        graph_.endMove();
        event.handled = true;
        break;
    default: break;
    }
}

// ---- comments ----

CommentView::CommentView(Graph& graph, std::string text, Size size, Color color) : graph_(graph), size_(size), color_(color) {
    auto& label = add(std::make_unique<Label>(Label::Props{.text = LocalizedText(std::move(text)), .fontSize = 15, .color = Color{1, 1, 1, 0.9f}}));
    label.setAlignment(Alignment::Start, Alignment::Start);
    label.setMargin({12, 6, 12, 0});
}

void CommentView::paint(PaintContext& context) {
    paint::Painter& p = context.painter;
    const Rect r{0, 0, size().width, size().height};
    const float radius = context.ui.theme.node.cornerRadius;
    p.fillRoundedRect(r, radius, {color_.r, color_.g, color_.b, 0.16f});
    p.pushClip({0, 0, r.width, headerHeight});
    p.fillRoundedRect({0, 0, r.width, headerHeight + radius}, radius, {color_.r, color_.g, color_.b, 0.55f});
    p.popClip();
    p.strokeRoundedRect(r, radius, 1, {color_.r, color_.g, color_.b, 0.7f});
    // the grip that resizes it, at its bottom end corner
    const float x = rightToLeft() ? 4.0f : r.width - 4, d = rightToLeft() ? 1.0f : -1.0f;
    for (int i = 1; i <= 3; ++i)
        p.drawLine({x + d * 4.0f * i, r.height - 4}, {x, r.height - 4 - 4.0f * i}, 1, {1, 1, 1, 0.35f});
}

void CommentView::onMouse(MouseEvent& event) {
    if (event.button != platform::MouseButton::Left)
        return;
    const Point at = graph_.inCanvas(*this, event.position);
    const bool rtl = rightToLeft();
    switch (event.type) {
    case MouseEventType::Press: {
        const Rect corner{rtl ? 0.0f : size_.width - grip, size_.height - grip, grip, grip};
        if (corner.contains(event.position)) {
            resizing_ = true;
            grabbed_ = {event.position.x - (rtl ? 0.0f : size_.width), event.position.y - size_.height};
            event.handled = true;
        } else if (event.position.y < headerHeight) {
            // with the nodes in it
            std::vector<Widget*> items{this};
            const Rect area = graph_.itemRect(*this);
            for (NodeView* node : graph_.nodes()) {
                const Rect n = graph_.itemRect(*node);
                if (n.x >= area.x && n.y >= area.y && n.right() <= area.right() && n.bottom() <= area.bottom())
                    items.push_back(node);
            }
            graph_.beginMove(std::move(items), at);
            event.handled = true;
        }
        break;
    }
    case MouseEventType::Move:
        if (resizing_) {
            const float width = rtl ? size_.width - (event.position.x - grabbed_.x) : event.position.x - grabbed_.x;
            const Size to{std::max(160.0f, width), std::max(80.0f, event.position.y - grabbed_.y)};
            if (rtl)        // its end side is its left: its position moves with it
                graph_.setItemPosition(*this, graph_.itemPosition(*this) - Point{to.width - size_.width, 0});
            size_ = to;
            invalidateLayout();
        } else {
            graph_.moveTo(at);
        }
        event.handled = true;
        break;
    case MouseEventType::Release:
        resizing_ = false;
        graph_.endMove();
        event.handled = true;
        break;
    default: break;
    }
}

// ---- a script ----

void buildScript(Graph& g) {
    const PinSpec exec{"", Type::Exec};
    NodeView& tick = g.addNode({"Event Tick", eventHeader, {}, {exec, {"Delta Seconds", Type::Float}}}, {0, 0});
    NodeView& speed = g.addNode({"Get Speed", variableHeader, {}, {{"Speed", Type::Float}}}, {0, 230});
    NodeView& multiply = g.addNode({"Multiply", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Result", Type::Float}}}, {260, 170});
    NodeView& greater = g.addNode({"Greater", pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Result", Type::Bool}}}, {520, 210});
    NodeView& branch = g.addNode({"Branch", flowHeader, {exec, {"Condition", Type::Bool}}, {{"True", Type::Exec}, {"False", Type::Exec}}}, {780, 0});
    NodeView& print = g.addNode({"Print String", functionHeader, {exec, {"In String", Type::String}, {"Print to Screen", Type::Bool}}, {exec}}, {1060, -60});
    NodeView& count = g.addNode({"Increment", functionHeader, {exec, {"Value", Type::Int}, {"By", Type::Int}}, {exec, {"Value", Type::Int}}}, {1060, 160});
    NodeView& text = g.addNode({"To Text", pureHeader, {{"Value", Type::Int}}, {{"Text", Type::String}}}, {1340, 220});
    NodeView& log = g.addNode({"Write Log", functionHeader, {exec, {"Message", Type::String}}, {exec}}, {1600, 160});

    g.link(Graph::pin(tick, 0), Graph::pin(branch, 0));
    g.link(Graph::pin(tick, 1), Graph::pin(multiply, 0));
    g.link(Graph::pin(speed, 0), Graph::pin(multiply, 1));
    g.link(Graph::pin(multiply, 2), Graph::pin(greater, 0));
    g.link(Graph::pin(greater, 2), Graph::pin(branch, 1));
    g.link(Graph::pin(branch, 2), Graph::pin(print, 0));
    g.link(Graph::pin(branch, 3), Graph::pin(count, 0));
    g.link(Graph::pin(count, 3), Graph::pin(log, 0));
    g.link(Graph::pin(count, 4), Graph::pin(text, 0));
    g.link(Graph::pin(text, 1), Graph::pin(log, 1));
    g.addComment("How fast is too fast", {-30, 120, 790, 240}, rgb(70, 120, 160));
    g.addComment("Count the slow frames", {1030, 110, 830, 260}, rgb(120, 90, 150));
}

// `count` more nodes in a chain, under the script.
void addChain(Graph& g, int count) {
    const int columns = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(count))));
    NodeView* previous = nullptr;
    for (int i = 0; i < count; ++i) {
        const Point at{static_cast<float>(i % columns) * 280.0f, 600.0f + static_cast<float>(i / columns) * 200.0f};
        NodeView& node = g.addNode({"Add " + std::to_string(i + 1), pureHeader, {{"A", Type::Float}, {"B", Type::Float}}, {{"Sum", Type::Float}}}, at);
        if (previous)
            g.link(Graph::pin(*previous, 2), Graph::pin(node, 0));
        previous = &node;
    }
}

} // namespace

int main(int argc, char** argv) {
    int frames = -1, nodes = 0;
    bool rtl = false;
    std::optional<glw::ThemeStyle> style;
    std::optional<glw::ColorScheme> colors;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--nodes" && i + 1 < argc)
            nodes = std::max(0, std::atoi(argv[++i]));
        else if (arg == "--rtl")
            rtl = true;
        else if (arg == "--style" && i + 1 < argc)
            style = glw::themeStyleNamed(argv[++i]);
        else if (arg == "--colors" && i + 1 < argc)
            colors = glw::colorSchemeNamed(argv[++i]);
    }
    try {
        Application app;
        if (style)
            app.setStyle(*style, false);
        if (colors)
            app.setColorScheme(*colors, false);
        auto content = std::make_unique<Widget>();
        content->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 0.0f));
        Graph& graph = content->add(std::make_unique<Graph>());
        graph.setStretch(1);
        auto& status = content->add(std::make_unique<Label>(Label::Props{.fontSize = 12, .muted = true}));
        status.setPadding({12, 6, 12, 6});
        if (rtl)
            content->setLayoutDirection(LayoutDirection::RightToLeft);
        graph.setOnChanged([&] {
            status.setText(LocalizedText(std::to_string(graph.nodeCount()) + " nodes, " + std::to_string(graph.linkCount()) + " links, " +
                                         std::to_string(static_cast<int>(std::lround(graph.zoom() * 100))) +
                                         "%. Wheel: zoom; middle or right button: pan; drag a pin to another: link (Alt+click: "
                                         "unlink); Delete, F, Ctrl+A."));
        });
        buildScript(graph);
        addChain(graph, nodes);
        graph.frameItems();
        app.openWindow({.title = "Node Editor", .size = {1280, 760}}, std::move(content));
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "node_editor: %s\n", e.what());
        return 1;
    }
}
