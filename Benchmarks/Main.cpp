// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// How long the widget layer takes for what an interface does often, with many widgets: laid out the first time, painted,
// scrolled, one text changed. No window and no GPU: a root, its draw list, as an engine would have them.
//
//   glw_benchmarks [--count N]...     (1000, 10000 and 100000 labels by default)
#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/ScrollArea.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <chrono>
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
    return 0;
}
