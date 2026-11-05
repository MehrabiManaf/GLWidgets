// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Charts fed live: a time series of three measures (an area under the first) appended `--rate` points a second each,
// its last minute shown and following; a donut of shares that drift; stacked bars of each second's counts, appended as
// they come; a treemap of a project's folders and files, their sizes changing now and then. Above them, the style and the
// colors of everything, whether the charts ease, and how the bars are laid out.
//
//   chart_demo [--rate N] [--style studio|fluid|plain|plastic|metal] [--colors dark|light|dusk|sand|turquoise] [--frames N]
//
// On a chart: the wheel zooms (with Ctrl, the y axis of the line chart), a drag pans (back to the newest, it follows
// again), the right button drags a rectangle to zoom into, a double click shows all again; a legend's item hides its
// series; a click on a treemap's tile goes into it, a right click out.
#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/PieChart.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace ui = glw::ui;
using glw::tr;

namespace {

struct Demo {
    glw::Application& app;
    double rate = 200;                      // points a second, each series
    glw::LineChart* line = nullptr;
    glw::LineSeries* cpu = nullptr;
    glw::LineSeries* memory = nullptr;
    glw::LineSeries* network = nullptr;
    glw::PieChart* pie = nullptr;
    glw::BarChart* bars = nullptr;
    glw::TreeMap* tree = nullptr;
    glw::Label* status = nullptr;
    glw::Timer feed, drift, second, disk, count;
    std::mt19937 random{42};
    double fed = 0;                         // the time of the last point
    double cpuValue = 30, memoryValue = 55, networkValue = 10;
    glw::TreeMapNode project;

    double uniform(double low, double high) { return std::uniform_real_distribution<double>(low, high)(random); }
    double normal(double sigma) { return std::normal_distribution<double>(0, sigma)(random); }

    std::unique_ptr<glw::Widget> build() {
        auto made = std::make_unique<glw::LineChart>(glw::LineChart::Props{
            .title = tr("CPU, memory and network, the last minute"), .xAxis = glw::ChartAxisKind::Time, .window = 60, .includeZero = true});
        line = made.get();
        const auto capacity = static_cast<std::size_t>(rate * 600);     // ten minutes of each
        cpu = &line->addSeries({.name = tr("CPU %"), .area = true, .capacity = capacity});
        memory = &line->addSeries({.name = tr("Memory %"), .capacity = capacity});
        network = &line->addSeries({.name = tr("Network MB/s"), .steps = false, .capacity = capacity});

        auto donut = std::make_unique<glw::PieChart>(glw::PieChart::Props{.title = tr("Requests by region"), .hole = 0.58f, .totalLabel = tr("requests")});
        pie = donut.get();
        pie->setSlices({{tr("Europe"), 420}, {tr("Asia"), 380}, {tr("North America"), 300}, {tr("South America"), 120},
                        {tr("Africa"), 90}, {tr("Oceania"), 45}});

        auto stacked = std::make_unique<glw::BarChart>(glw::BarChart::Props{.title = tr("Events a second"), .capacity = 600, .window = 24});
        bars = stacked.get();
        bars->addSeries(tr("Reads"));
        bars->addSeries(tr("Writes"));
        bars->addSeries(tr("Errors"));

        auto map = std::make_unique<glw::TreeMap>(glw::TreeMap::Props{.title = tr("A project's files, kB")});
        tree = map.get();
        project = makeProject();
        tree->setRoot(project);

        const auto card = [](std::unique_ptr<glw::Widget> chart) {
            chart->setPadding(glw::Insets::all(14));
            return ui::Card(std::move(chart)).stretch(1).build();
        };
        return ui::VStack(
                   {.spacing = 10},
                   ui::HStack({.spacing = 10},
                              ui::Label({.text = tr("Style")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {"Studio", "Fluid", "Plain", "Plastic", "Metal"},
                                            .current = static_cast<std::size_t>(app.style()),
                                            .onChanged = [this](std::size_t i) { app.setStyle(static_cast<glw::ThemeStyle>(i)); }}),
                              ui::Label({.text = tr("Colors")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Dark"), tr("Light"), tr("Dusk"), tr("Sand"), tr("Turquoise")},
                                            .current = static_cast<std::size_t>(app.colorScheme()),
                                            .onChanged = [this](std::size_t i) { app.setColorScheme(static_cast<glw::ColorScheme>(i)); }}),
                              ui::CheckBox({.text = tr("Eased"), .checked = true, .onToggled = [this](bool on) { animate(on); }})
                                  .alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::Label({.text = tr("Bars")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Stacked"), tr("Grouped"), tr("Percent")},
                                            .onChanged = [this](std::size_t i) { bars->setLayout(static_cast<glw::BarLayout>(i)); }}),
                              ui::CheckBox({.text = tr("Lying"),
                                            .onToggled = [this](bool on) {
                                                bars->setOrientation(on ? glw::Orientation::Horizontal : glw::Orientation::Vertical);
                                            }})
                                  .alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::Spacer(),
                              ui::Label({.muted = true}).bind(status).alignment(glw::Alignment::End, glw::Alignment::Center)),
                   ui::Splitter({.orientation = glw::Orientation::Vertical},
                                ui::Splitter({}, card(std::move(made)), card(std::move(donut))).stretch(1),
                                ui::Splitter({}, card(std::move(stacked)), card(std::move(map))).stretch(1))
                       .stretch(1))
            .padding(glw::Insets::all(12));
    }

    void animate(bool on) {
        for (glw::Chart* chart : std::initializer_list<glw::Chart*>{line, pie, bars, tree})
            chart->setAnimated(on);
    }

    // Folders of folders and files, their sizes made up.
    glw::TreeMapNode makeProject() {
        const auto files = [&](std::initializer_list<const char*> names, double scale) {
            std::vector<glw::TreeMapNode> out;
            for (const char* name : names)
                out.push_back({.label = name, .value = std::round(uniform(0.2, 1.0) * scale)});
            return out;
        };
        glw::TreeMapNode root{.label = "GLWidgets"};
        glw::TreeMapNode library{.label = "Library"};
        library.children.push_back({.label = "Charts", .children = files({"BarChart.cpp", "LineChart.cpp", "PieChart.cpp", "TreeMap.cpp", "Chart.cpp", "ChartAxis.cpp", "SeriesBuffer.cpp"}, 30)});
        library.children.push_back({.label = "Widgets", .children = files({"Docking.cpp", "TableView.cpp", "TextEdit.cpp", "ListView.cpp", "TreeView.cpp", "Theme.cpp", "Application.cpp", "UiRoot.cpp", "Widget.cpp", "Menu.cpp"}, 90)});
        library.children.push_back({.label = "Paint", .children = files({"Painter.cpp", "Renderer.cpp", "PainterShaders.hpp"}, 120)});
        library.children.push_back({.label = "Text", .children = files({"TextLayout.cpp", "Font.cpp", "GlyphAtlas.cpp", "DrawText.cpp"}, 60)});
        library.children.push_back({.label = "Platform", .children = files({"SdlWindow.cpp", "SdlPlatform.cpp", "SdlKeys.cpp"}, 50)});
        root.children.push_back(library);
        root.children.push_back({.label = "Examples", .children = files({"WidgetsDemo", "DockingDemo", "NodeEditor", "CodeEditor", "ChartDemo", "PaintDemo"}, 40)});
        root.children.push_back({.label = "Tests", .children = files({"DockingTests.cpp", "TableViewTests.cpp", "ChartTests.cpp", "TextEditTests.cpp", "ItemViewTests.cpp", "StyleTests.cpp"}, 45)});
        root.children.push_back({.label = "Docs", .children = files({"Architecture.md", "README.md", "ThirdPartyNotices.md"}, 70)});
        root.children.push_back({.label = "Fonts", .children = files({"Vazirmatn-Regular.ttf", "Vazirmatn-Bold.ttf"}, 200)});
        return root;
    }

    void start() {
        glw::Scheduler& scheduler = *app.context().scheduler;
        fed = glw::chartNow();
        // the measures: a point each `1 / rate` seconds, all that came since the last frame
        feed.start(scheduler, std::chrono::milliseconds(16), [this] {
            const double now = glw::chartNow();
            const double step = 1 / rate;
            for (; fed + step <= now; fed += step) {
                cpuValue = std::clamp(cpuValue + normal(1.2) + (35 - cpuValue) * 0.002, 0.0, 100.0);
                memoryValue = std::clamp(memoryValue + normal(0.15), 20.0, 95.0);
                networkValue = std::max(0.0, networkValue * 0.995 + normal(0.4) + (uniform(0, 1) < 0.002 ? uniform(20, 60) : 0.0));
                cpu->append(fed + step, cpuValue);
                memory->append(fed + step, memoryValue);
                network->append(fed + step, networkValue);
            }
        }, true);
        // the shares drift
        drift.start(scheduler, std::chrono::milliseconds(800), [this] {
            for (std::size_t i = 0; i < pie->sliceCount(); ++i)
                pie->setValue(i, std::max(5.0, std::round(pie->slice(i).value * (1 + normal(0.06)))));
        }, true);
        // a second's counts
        second.start(scheduler, std::chrono::seconds(1), [this] {
            const std::time_t now = std::time(nullptr);
            std::tm t{};
#ifdef _WIN32
            localtime_s(&t, &now);
#else
            localtime_r(&now, &t);
#endif
            char label[16];
            std::snprintf(label, sizeof label, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
            const double values[] = {std::round(uniform(40, 120)), std::round(uniform(10, 60)), std::round(uniform(0, 1) < 0.2 ? uniform(1, 25) : 0)};
            bars->appendCategory(std::string(label), values);
        }, true);
        // a file grows or shrinks now and then
        disk.start(scheduler, std::chrono::milliseconds(2500), [this] {
            glw::TreeMapNode* node = &project;
            while (!node->children.empty())
                node = &node->children[std::uniform_int_distribution<std::size_t>(0, node->children.size() - 1)(random)];
            node->value = std::max(1.0, std::round(node->value * uniform(0.6, 1.7)));
            tree->setRoot(project);
        }, true);
        count.start(scheduler, std::chrono::milliseconds(500), [this] {
            const std::size_t points = cpu->data().size() + memory->data().size() + network->data().size();
            status->setText(tr("{n} points, {rate} a second").arg("n", std::to_string(points)).arg("rate", std::to_string(static_cast<int>(rate * 3))));
        }, true);
        // what came before it opened: the last ten minutes, so that there is a history to pan into
        const double from = fed - 600;
        for (double t = from; t < fed; t += 1 / rate) {
            cpuValue = std::clamp(cpuValue + normal(1.2) + (35 - cpuValue) * 0.002, 0.0, 100.0);
            memoryValue = std::clamp(memoryValue + normal(0.15), 20.0, 95.0);
            networkValue = std::max(0.0, networkValue * 0.995 + normal(0.4) + (uniform(0, 1) < 0.002 ? uniform(20, 60) : 0.0));
            cpu->append(t, cpuValue);
            memory->append(t, memoryValue);
            network->append(t, networkValue);
        }
        for (int i = 30; i > 0; --i) {
            const double values[] = {std::round(uniform(40, 120)), std::round(uniform(10, 60)), std::round(uniform(0, 1) < 0.2 ? uniform(1, 25) : 0)};
            bars->appendCategory("-" + std::to_string(i) + "s", values);
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    double rate = 200;
    std::optional<glw::ThemeStyle> style;
    std::optional<glw::ColorScheme> colors;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--rate" && i + 1 < argc)
            rate = std::clamp(std::atof(argv[++i]), 1.0, 100000.0);
        else if (arg == "--style" && i + 1 < argc)
            style = glw::themeStyleNamed(argv[++i]);
        else if (arg == "--colors" && i + 1 < argc)
            colors = glw::colorSchemeNamed(argv[++i]);
    }
    try {
        glw::Application app;
        if (style)
            app.setStyle(*style, false);
        if (colors)
            app.setColorScheme(*colors, false);
        Demo demo{app, rate};
        app.openWindow({.title = tr("Charts"), .size = {1280, 820}}, demo.build());
        demo.start();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chart_demo: %s\n", e.what());
        return 1;
    }
}
