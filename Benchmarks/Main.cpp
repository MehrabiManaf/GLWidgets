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
#include <GLWidgets/Widgets/ScrollArea.hpp>
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
    return 0;
}
