// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Charts fed live: a time series of three measures (an area under the first, a band of the range around the second)
// appended `--rate` points a second each, its last minute shown and following; a donut of shares that drift, in rings
// (regions, their countries, a few cities: a click goes into a slice, a click in the hole out); a race of `--race`
// packages' downloads (stacked by system), a month every 0.6 seconds; stacked bars of each second's counts, appended as
// they come; a treemap of a project's folders and files, their sizes changing now and then. Above them, the style and the
// colors of everything, whether the charts ease, how the bars are laid out, and whether the donut and the race show
// only their top ones (the rest as one of others).
//
//   chart_demo [--rate N] [--race N] [--style studio|fluid|plain|plastic|metal] [--colors dark|light|dusk|sand|turquoise]
//              [--frames N]
//
// On a chart: the wheel zooms (with Ctrl, the y axis of the line chart), a drag pans (back to the newest, it follows
// again), the right button drags a rectangle to zoom into, a double click shows all again; a legend's item hides its
// series; a click on a treemap's tile goes into it, a right click out.
#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/BarRace.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/PieChart.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <array>
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
#include <vector>

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
    glw::LineSeries* memoryHigh = nullptr;  // the edges of the band around memory: no lines, not in the legend
    glw::LineSeries* memoryLow = nullptr;
    double spread = 3;
    std::vector<glw::PieSlice> regions;
    glw::PieChart* pie = nullptr;
    glw::BarChart* bars = nullptr;
    glw::TreeMap* tree = nullptr;
    glw::Label* status = nullptr;
    glw::BarRace* race = nullptr;
    std::size_t racers = 500;
    std::vector<std::array<double, 3>> downloads;   // each package's, by system
    std::vector<double> momentum;                   // how fast each grows
    int month = 0;
    glw::Timer feed, drift, second, disk, count, months;
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
        cpu = &line->addSeries({.name = tr("CPU %"), .capacity = capacity});
        memory = &line->addSeries({.name = tr("Memory %"), .capacity = capacity});
        network = &line->addSeries({.name = tr("Network MB/s"), .capacity = capacity});
        memoryHigh = &line->addSeries({.line = false, .capacity = capacity});
        memoryLow = &line->addSeries({.line = false, .capacity = capacity});
        // an area under CPU, fading down; a band of memory's range around it
        line->addArea({.upper = cpu, .opacity = 0.32f, .gradient = true});
        line->addArea({.name = tr("Memory range"), .upper = memoryHigh, .lower = memoryLow, .color = memory->color(), .opacity = 0.22f});

        auto donut = std::make_unique<glw::PieChart>(
            glw::PieChart::Props{.title = tr("Requests by region, country and city"), .hole = 0.38f, .totalLabel = tr("requests")});
        pie = donut.get();
        // a slice of a value, a slice of parts
        const auto leaf = [](glw::LocalizedText label, double value) { return glw::PieSlice{.label = std::move(label), .value = value}; };
        const auto group = [](glw::LocalizedText label, std::vector<glw::PieSlice> parts) {
            return glw::PieSlice{.label = std::move(label), .children = std::move(parts)};
        };
        regions = {
            group(tr("Europe"), {leaf(tr("Germany"), 120), leaf(tr("France"), 90), leaf(tr("United Kingdom"), 85), leaf(tr("Italy"), 60),
                                 leaf(tr("Spain"), 45)}),
            group(tr("Asia"), {group(tr("Iran"), {leaf(tr("Tehran"), 70), leaf(tr("Isfahan"), 30), leaf(tr("Shiraz"), 22), leaf(tr("Tabriz"), 15)}),
                               leaf(tr("China"), 110), leaf(tr("India"), 80), leaf(tr("Japan"), 60), leaf(tr("Turkey"), 25)}),
            group(tr("North America"), {group(tr("United States"), {leaf(tr("California"), 90), leaf(tr("Texas"), 60), leaf(tr("New York"), 50)}),
                                        leaf(tr("Canada"), 50), leaf(tr("Mexico"), 40)}),
            group(tr("South America"), {leaf(tr("Brazil"), 70), leaf(tr("Argentina"), 30), leaf(tr("Chile"), 20)}),
            group(tr("Africa"), {leaf(tr("Nigeria"), 35), leaf(tr("Egypt"), 30), leaf(tr("South Africa"), 25)}),
            group(tr("Oceania"), {leaf(tr("Australia"), 35), leaf(tr("New Zealand"), 10)}),
        };
        pie->setSlices(regions);

        auto stacked = std::make_unique<glw::BarChart>(glw::BarChart::Props{.title = tr("Events a second"), .capacity = 600, .window = 24});
        bars = stacked.get();
        bars->addSeries(tr("Reads"));
        bars->addSeries(tr("Writes"));
        bars->addSeries(tr("Errors"));

        auto racing = std::make_unique<glw::BarRace>(glw::BarRace::Props{
            .title = tr("Downloads of {n} packages, by system").arg("n", std::to_string(racers)), .shown = 10,
            .parts = {tr("Windows"), tr("Linux"), tr("macOS")}});
        race = racing.get();
        makeRace();

        auto map = std::make_unique<glw::TreeMap>(glw::TreeMap::Props{.title = tr("A project's files, kB")});
        tree = map.get();
        project = makeProject();
        tree->setRoot(project);

        const auto card = [](std::unique_ptr<glw::Widget> chart, float stretch = 1) {
            chart->setPadding(glw::Insets::all(14));
            return ui::Card(std::move(chart)).stretch(stretch).build();
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
                              // only the top ones, the rest as one of others
                              ui::Label({.text = tr("Donut")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("All"), tr("Top 2"), tr("Top 3")},
                                            .onChanged = [this](std::size_t i) { pie->setTop(i == 0 ? 0 : i + 1); }}),
                              ui::Label({.text = tr("Race")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("All"), tr("Top 10"), tr("Top 50")},
                                            .onChanged = [this](std::size_t i) {
                                                // the top ten and the others' bar all in view; the top fifty scrolled
                                                race->setTop(i == 0 ? 0 : i == 1 ? 10 : 50);
                                                race->setShown(i == 1 ? 11 : 10);
                                            }}),
                              ui::Spacer(),
                              ui::Label({.muted = true}).bind(status).alignment(glw::Alignment::End, glw::Alignment::Center)),
                   ui::Splitter({.orientation = glw::Orientation::Vertical},
                                ui::Splitter({}, card(std::move(made), 3), card(std::move(donut), 2)).stretch(1),
                                ui::Splitter({}, card(std::move(racing)), card(std::move(stacked)), card(std::move(map))).stretch(1))
                       .stretch(1))
            .padding(glw::Insets::all(12));
    }

    // Packages with names made up, each with a share of each system and a pace of its own.
    void makeRace() {
        static constexpr const char* first[] = {"aurora", "basalt", "cedar", "delta", "ember", "fjord", "garnet", "harbor", "indigo",
                                                "juniper", "kestrel", "lumen", "meadow", "nimbus", "onyx", "pixel", "quartz", "raven",
                                                "sierra", "tundra", "umber", "vortex", "willow", "xenon", "yarrow", "zephyr"};
        static constexpr const char* endings[] = {"js", "kit", "db", "ui", "net", "io", "cli", "core", "lab", "flow", "sync", "view"};
        for (std::size_t i = 0; i < racers; ++i) {
            std::string name = std::string(first[i % std::size(first)]) + "-" + endings[(i / std::size(first)) % std::size(endings)];
            if (i >= std::size(first) * std::size(endings))
                name += "-" + std::to_string(i / (std::size(first) * std::size(endings)));
            race->addItem(name);
            const double size = std::pow(uniform(0, 1), 3) * 900 + 10;
            downloads.push_back({size * uniform(0.2, 0.7), size * uniform(0.1, 0.6), size * uniform(0.05, 0.3)});
            momentum.push_back(uniform(0.0, 0.08));
        }
        nextMonth();
    }

    // A month on: each package grows by its pace (which wanders), now and then one takes off.
    void nextMonth() {
        static constexpr const char* names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (std::size_t i = 0; i < racers; ++i) {
            momentum[i] = std::clamp(momentum[i] + normal(0.004) + (uniform(0, 1) < 0.002 ? 0.12 : 0.0), -0.02, 0.25);
            for (double& d : downloads[i])
                d = std::max(0.0, d * (1 + momentum[i] * uniform(0.9, 1.1)));
            race->setValues(i, downloads[i]);
        }
        race->setPeriod(std::string(names[month % 12]) + " " + std::to_string(2015 + month / 12));
        ++month;
    }

    // The measures at `t`: random walks, memory's range wandering around it.
    void sample(double t) {
        cpuValue = std::clamp(cpuValue + normal(1.2) + (35 - cpuValue) * 0.002, 0.0, 100.0);
        memoryValue = std::clamp(memoryValue + normal(0.15), 20.0, 95.0);
        networkValue = std::max(0.0, networkValue * 0.995 + normal(0.4) + (uniform(0, 1) < 0.002 ? uniform(20, 60) : 0.0));
        spread = std::clamp(spread + normal(0.08), 1.5, 9.0);
        cpu->append(t, cpuValue);
        memory->append(t, memoryValue);
        network->append(t, networkValue);
        memoryHigh->append(t, memoryValue + spread);
        memoryLow->append(t, memoryValue - spread * 0.8);
    }

    void animate(bool on) {
        for (glw::Chart* chart : std::initializer_list<glw::Chart*>{line, pie, race, bars, tree})
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
            for (; fed + step <= now; fed += step)
                sample(fed + step);
        }, true);
        // the shares drift: every city's, every country's without cities
        drift.start(scheduler, std::chrono::milliseconds(800), [this] {
            const auto walk = [&](auto& self, std::vector<glw::PieSlice>& slices) -> void {
                for (glw::PieSlice& slice : slices) {
                    if (slice.children.empty())
                        slice.value = std::max(3.0, std::round(slice.value * (1 + normal(0.06))));
                    else
                        self(self, slice.children);
                }
            };
            walk(walk, regions);
            pie->setSlices(regions);
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
        // the race: a month every 0.6 seconds, ten years, then again
        months.start(scheduler, std::chrono::milliseconds(600), [this] {
            if (month >= 120) {
                month = 0;
                for (std::size_t i = 0; i < racers; ++i)
                    for (double& d : downloads[i])
                        d = std::max(1.0, d / 400);
            }
            nextMonth();
        }, true);
        count.start(scheduler, std::chrono::milliseconds(500), [this] {
            const std::size_t points = cpu->data().size() + memory->data().size() + network->data().size();
            status->setText(tr("{n} points, {rate} a second").arg("n", std::to_string(points)).arg("rate", std::to_string(static_cast<int>(rate * 3))));
        }, true);
        // what came before it opened: the last ten minutes, so that there is a history to pan into
        const double from = fed - 600;
        for (double t = from; t < fed; t += 1 / rate)
            sample(t);
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
    std::size_t racers = 500;
    std::optional<glw::ThemeStyle> style;
    std::optional<glw::ColorScheme> colors;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--rate" && i + 1 < argc)
            rate = std::clamp(std::atof(argv[++i]), 1.0, 100000.0);
        else if (arg == "--race" && i + 1 < argc)
            racers = static_cast<std::size_t>(std::clamp(std::atoi(argv[++i]), 1, 1'000'000));
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
        Demo demo{.app = app, .rate = rate};
        demo.racers = racers;
        app.openWindow({.title = tr("Charts"), .size = {1280, 820}}, demo.build());
        demo.start();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chart_demo: %s\n", e.what());
        return 1;
    }
}
