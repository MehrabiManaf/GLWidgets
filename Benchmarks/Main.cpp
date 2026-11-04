// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// How long the widget layer takes for what an interface does often, with many widgets: laid out the first time, painted,
// scrolled, one text changed. No window and no GPU: a root, its draw list, as an engine would have them.
//
//   glw_benchmarks [--count N]...     (1000, 10000 and 100000 labels by default)
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/Canvas.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// What it takes to run `f`, in milliseconds: the best of `runs` (the others are the machine's noise).
template <class F>
double best(int runs, F&& f) {
    double fastest = 1e30;
    for (int i = 0; i < runs; ++i) {
        const auto start = Clock::now();
        f();
        fastest = std::min(fastest, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
    }
    return fastest;
}

double once(auto&& f) {
    return best(1, f);
}

void labelsInAScrollArea(glw::UiContext& ui, int count) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto list = std::make_unique<glw::Widget>();
    list->setLayout(std::make_unique<glw::StackLayout>(glw::Orientation::Vertical, 2.0f));
    std::vector<glw::Label*> labels;
    const double build = once([&] {
        for (int i = 0; i < count; ++i)
            labels.push_back(&list->add(std::make_unique<glw::Label>("Item " + std::to_string(i))));
    });
    auto scroll = std::make_unique<glw::ScrollArea>();
    glw::ScrollArea& area = *scroll;
    scroll->setContent(std::move(list));
    root.setContent(std::move(scroll));

    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    const std::size_t vertices = drawn.vertices.size();
    // a step of the wheel: moved, laid out (as a frame does when it must), painted
    float at = 0;
    const double scroll1 = best(5, [&] {
        at += 40;
        area.setOffset({0, at});
        root.paint(drawn);
    });
    // one label's text changed (in view): laid out and painted again
    int n = 0;
    const double text = best(5, [&] {
        labels[static_cast<std::size_t>(at / 30)]->setText("Changed " + std::to_string(++n));
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.1f | %8.2f | %8.2f | %8.2f | %8zu\n", count, build, layout, paint, scroll1, text, vertices);
}

// Items made when asked: a model of any size costs nothing.
class Numbers final : public glw::ListModel {
public:
    explicit Numbers(std::size_t count) : count_(count) {}
    std::size_t count() const override { return count_; }
    void changed(std::size_t index) { notifyChanged(index, 1); }

private:
    std::size_t count_;
};

void itemsInAListView(glw::UiContext& ui, std::size_t count, bool uniform) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto numbers = std::make_shared<Numbers>(count);
    glw::ListView* list = nullptr;
    const double build = once([&] {
        auto made = std::make_unique<glw::ListView>();
        list = made.get();
        list->setModel(numbers);
        list->setDelegate(std::make_unique<glw::TextDelegate>([](std::size_t i) { return glw::LocalizedText("Item " + std::to_string(i)); }));
        if (!uniform)
            list->setRowHeight(0.0f);
        root.setContent(std::move(made));
    });
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 120;
        list->setOffset(at);
        root.paint(drawn);
    });
    // far: half way down
    const double jump = best(5, [&] {
        at = list->maximumOffset() / 2 + (at > list->maximumOffset() / 2 ? -1000 : 1000);
        list->setOffset(at);
        root.paint(drawn);
    });
    const double text = best(5, [&] {
        numbers->changed(list->shownItems().first);
        root.paint(drawn);
    });
    std::printf("%9zu | %-8s | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8zu\n", count, uniform ? "uniform" : "measured", build,
                layout, paint, scroll, jump, text, list->children().size());
}

// A thousand folders of a thousand files, made when asked.
class Folders final : public glw::TreeModel {
public:
    std::size_t childCount(std::optional<glw::TreeNode> parent) const override {
        return !parent ? 1000 : parent->id < 1000 ? 1000 : 0;
    }
    glw::TreeNode child(std::optional<glw::TreeNode> parent, std::size_t index) const override {
        return {!parent ? index : 1000 + parent->id * 1000 + index};
    }
    bool hasChildren(glw::TreeNode node) const override { return node.id < 1000; }
};

void nodesInATreeView(glw::UiContext& ui) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    auto made = std::make_unique<glw::TreeView>();
    glw::TreeView& tree = *made;
    tree.setModel(std::make_shared<Folders>());
    tree.setDelegate(std::make_unique<glw::TreeTextDelegate>([](glw::TreeNode n) { return glw::LocalizedText(std::to_string(n.id)); }));
    root.setContent(std::move(made));
    root.layout();
    glw::paint::DrawList drawn;
    const double expandAll = once([&] {
        tree.expandAll();
        root.paint(drawn);
    });
    const double paint = best(5, [&] { root.paint(drawn); });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 120;
        tree.list().setOffset(at);
        root.paint(drawn);
    });
    // a folder near the top collapsed and expanded again, a million rows after it
    bool open = true;
    const double toggle = best(6, [&] {
        open = !open;
        tree.setExpanded({1}, open);
        root.paint(drawn);
    });
    std::printf("%9zu | %10.1f | %8.2f | %8.2f | %8.2f | %8zu\n", tree.shownCount(), expandAll, paint, scroll, toggle,
                tree.list().children().size());
}

// A table of rows x columns, made when asked: no memory per cell.
class Cells final : public glw::TableModel {
public:
    Cells(std::size_t rows, std::size_t columns) : rows_(rows), columns_(columns) {}
    std::size_t rowCount() const override { return rows_; }
    std::size_t columnCount() const override { return columns_; }
    glw::LocalizedText header(std::size_t column) const override { return glw::LocalizedText("Column " + std::to_string(column)); }

private:
    std::size_t rows_, columns_;
};

std::size_t widgetsIn(const glw::Widget& widget) {
    std::size_t count = 1;
    for (const auto& child : widget.children())
        count += widgetsIn(*child);
    return count;
}

void cellsInATableView(glw::UiContext& ui, std::size_t rows, std::size_t columns) {
    glw::UiRoot root(ui);
    root.setSize({800, 600});
    const double build = once([&] {
        auto made = std::make_unique<glw::TableView>();
        made->setModel(std::make_shared<Cells>(rows, columns));
        made->setDelegate(std::make_unique<glw::TableTextDelegate>([](std::size_t r, std::size_t c) {
            return glw::LocalizedText(std::to_string(r) + ", " + std::to_string(c));
        }));
        root.setContent(std::move(made));
    });
    auto& table = static_cast<glw::TableView&>(*root.content());
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    double down = 0, across = 0;
    const double scroll = best(5, [&] {
        down += 120;
        table.list().setOffset(down);
        root.paint(drawn);
    });
    const double scrollAcross = best(5, [&] {
        across += 90;
        table.setHorizontalOffset(across);
        root.paint(drawn);
    });
    const double jump = best(5, [&] {
        down += 1e6;
        across += 1500;
        table.list().setOffset(down);
        table.setHorizontalOffset(across);
        root.paint(drawn);
    });
    std::printf("%9zu | %7zu | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8.2f | %8zu\n", rows, columns, build, layout, paint, scroll,
                scrollAcross, jump, widgetsIn(table));
}

// Nodes of a canvas: a box with a title and two lines each, `count` of them in a grid.
void nodesOnACanvas(glw::UiContext& ui, int count) {
    glw::UiRoot root(ui);
    root.setSize({1280, 720});
    auto made = std::make_unique<glw::Canvas>();
    glw::Canvas& canvas = *made;
    const int columns = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(count))));
    const double build = once([&] {
        for (int i = 0; i < count; ++i) {
            auto node = std::make_unique<glw::Widget>();
            node->setLayout(std::make_unique<glw::StackLayout>(glw::Orientation::Vertical, 4.0f));
            node->setPadding(glw::Insets::all(8));
            node->add(std::make_unique<glw::Label>("Node " + std::to_string(i)));
            node->add(std::make_unique<glw::Label>("A value"));
            node->add(std::make_unique<glw::Label>("Another value"));
            canvas.addItem(std::move(node), {static_cast<float>(i % columns) * 220.0f, static_cast<float>(i / columns) * 140.0f});
        }
    });
    root.setContent(std::move(made));
    glw::paint::DrawList drawn;
    const double layout = once([&] { root.layout(); });
    const double paint = best(5, [&] { root.paint(drawn); });
    float x = 0;
    const double pan = best(5, [&] {
        x -= 60;
        canvas.setOrigin({x, 0});
        root.paint(drawn);
    });
    // all of them in view: as far out as it zooms
    canvas.setZoomRange(0.02f, 4);
    canvas.frameItems();
    root.paint(drawn);
    const double all = best(5, [&] { root.paint(drawn); });
    // a step of the wheel each time, in and out again
    int step = 0;
    canvas.setZoom(1);
    const double zoom = best(6, [&] {
        canvas.setZoom(step++ % 2 == 0 ? 1.15f : 1.0f, glw::Point{640, 360});
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.1f | %8.2f | %8.2f | %8.2f | %8.1f | %8zu\n", count, build, layout, paint, pan, zoom, all,
                drawn.vertices.size());
}

// A text of `count` lines of C++, highlighted.
void linesInATextEdit(glw::UiContext& ui, int count) {
    std::string code;
    for (int i = 0; i < count; ++i)
        code += "    int value" + std::to_string(i) + " = compute(" + std::to_string(i) + ", \"text\"); // a comment\n";
    glw::UiRoot root(ui);
    root.setSize({1000, 700});
    auto made = std::make_unique<glw::TextEdit>();
    glw::TextEdit& edit = *made;
    edit.setHighlighter(std::make_unique<glw::CppHighlighter>());
    root.setContent(std::move(made));
    const double load = once([&] { edit.setText(code); });
    glw::paint::DrawList drawn;
    const double first = once([&] { root.paint(drawn); });
    const double paint = best(5, [&] {
        root.invalidatePaint();
        root.paint(drawn);
    });
    double at = 0;
    const double scroll = best(5, [&] {
        at += 600;
        edit.setVerticalOffset(at);
        root.paint(drawn);
    });
    // typing in the middle: the line laid out and colored again, those after it colored again as they show
    edit.select({static_cast<std::size_t>(count / 2), 10}, {static_cast<std::size_t>(count / 2), 10});
    root.paint(drawn);
    const double type = best(5, [&] {
        edit.replace(edit.selection(), "x");
        root.paint(drawn);
    });
    // to the end: highlighted from where it was known to there, once
    const double jump = once([&] {
        edit.select({static_cast<std::size_t>(count), 0}, {static_cast<std::size_t>(count), 0});
        root.paint(drawn);
    });
    std::printf("%9d | %8.1f | %8.2f | %8.2f | %8.2f | %8.2f | %8.1f | %8zu\n", count, load, first, paint, scroll, type, jump,
                edit.laidOutLines());
}

} // namespace

int main(int argc, char** argv) {
    std::vector<int> counts;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--count" && i + 1 < argc)
            counts.push_back(std::max(1, std::atoi(argv[++i])));
    }
    if (counts.empty())
        counts = {1000, 10000, 100000};

    glw::text::FontLibrary fonts;
    glw::text::GlyphAtlas atlas(1, 1024);
    glw::Translator translator;
    glw::UiContext ui{fonts, atlas, translator, glw::Theme::dark()};

    std::printf("Labels in a scroll area, milliseconds (paint and after: the best of 5)\n");
    std::printf("   labels |    build |   layout |    paint |   scroll |     text | vertices\n");
    for (int count : counts)
        labelsInAScrollArea(ui, count);

    std::printf("\nItems in a list view (a widget for each that shows), milliseconds\n");
    std::printf("    items | rows     |    build |   layout |    paint |   scroll |     jump |     text |  widgets\n");
    for (std::size_t count : {std::size_t{1'000'000}, std::size_t{100'000'000}}) {
        itemsInAListView(ui, count, true);
        if (count <= 1'000'000)
            itemsInAListView(ui, count, false);
    }

    std::printf("\nA tree view, all expanded, milliseconds\n");
    std::printf("    nodes | expand all |    paint |   scroll |   toggle |  widgets\n");
    nodesInATreeView(ui);

    std::printf("\nA table view (a widget for each cell that shows), milliseconds\n");
    std::printf("     rows | columns |    build |   layout |    paint |   scroll |   across |     jump |  widgets\n");
    cellsInATableView(ui, 1'000'000, 100);
    cellsInATableView(ui, 100'000'000, 1000);

    std::printf("\nNodes on a canvas, milliseconds (all: every one in view, zoomed out)\n");
    std::printf("    nodes |    build |   layout |    paint |      pan |     zoom |      all | vertices\n");
    for (int count : {1000, 10000})
        nodesOnACanvas(ui, count);

    std::printf("\nLines of C++ in a text edit, highlighted, milliseconds\n");
    std::printf("    lines |     load |    first |    paint |   scroll |     type | to the end |     laid\n");
    for (int count : {10'000, 1'000'000})
        linesInATextEdit(ui, count);
    return 0;
}
