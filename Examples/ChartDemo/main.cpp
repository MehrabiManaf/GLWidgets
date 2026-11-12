// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Charts fed live: a time series of three measures (an area under the first, a band of the range around the second)
// appended `--rate` points a second each, its last minute shown and following; a donut of shares that drift, in rings
// (regions, their countries, a few cities: a click goes into a slice, a click in the hole out); a race of `--race`
// packages' downloads (stacked by system), a month every 0.6 seconds; stacked bars of each second's counts, appended as
// they come; a treemap of a project's folders and files, their sizes changing now and then; a dashboard of the measures
// now (gauges, CPU's and memory's colored by bands, the network's a half circle with a needle) and of their last half
// minute (sparklines, the errors' in columns); heat maps of 200 endpoints' latency over the last minutes (a column a
// second, incidents and spikes in them) and of requests by hour and weekday; a network's traffic by protocol, stacked
// (in shares, if asked), ten points a second over the last two minutes; a price as candles made of trades twenty times
// a second (of a second, five, fifteen, a minute or five: its buttons), its volume under it and two moving averages over
// it (its legend hides each); how requests' latencies fall, live: a histogram of reads' and writes' (their median, 90th
// and 99th percentiles, the share under each value), and boxes of eight services'; requests' sizes against their
// latencies, API calls' and static files', 40000 of each (their density; dots when zoomed into fewer), a line through
// the calls'; a program profiled: a trace of its threads' work, frame after frame (the last quarter of a second,
// following), and a flame graph of where its time goes (stacks sampled as it runs; a search finds a function); where requests flow, from
// their sources through the edge and the services to the stores (a Sankey diagram, its values drifting). Above them, the style and the
// colors of everything, whether the charts ease, how the bars are laid out, and whether the donut and the race show
// only their top ones (the rest as one of others). Each chart is a docked panel: moved, split, floated, closed by its
// tab; the View menu hides and shows each (one closed by its tab too), back beside a neighbour of where it was.
//
//   chart_demo [--rate N] [--race N] [--style studio|fluid|plain|plastic|metal|solid|glass] [--colors dark|light|dusk|sand|turquoise]
//              [--language fa] [--frames N]
//
// In another language its numbers and times are written as it writes them (--language fa: Persian digits, the Solar
// Hijri calendar's days and months); its texts stay English (it has no catalog).
//
// On a chart: the wheel zooms (with Ctrl, the y axis of the line chart), a drag pans (back to the newest, it follows
// again), the right button drags a rectangle to zoom into, a double click shows all again; a legend's item hides its
// series; a click on a treemap's tile goes into it, a right click out.
#include <GLWidgets/Charts/BarChart.hpp>
#include <GLWidgets/Charts/BarRace.hpp>
#include <GLWidgets/Charts/BoxChart.hpp>
#include <GLWidgets/Charts/FlameGraph.hpp>
#include <GLWidgets/Charts/CandleChart.hpp>
#include <GLWidgets/Charts/Gauge.hpp>
#include <GLWidgets/Charts/HeatMap.hpp>
#include <GLWidgets/Charts/HistogramChart.hpp>
#include <GLWidgets/Charts/LineChart.hpp>
#include <GLWidgets/Charts/PieChart.hpp>
#include <GLWidgets/Charts/SankeyChart.hpp>
#include <GLWidgets/Charts/ScatterChart.hpp>
#include <GLWidgets/Charts/Sparkline.hpp>
#include <GLWidgets/Charts/TimelineChart.hpp>
#include <GLWidgets/Charts/TreeMap.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Action.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/MenuBar.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <map>
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
    // the dashboard: the measures now, and their last half minute (with the last value by each)
    std::array<glw::Gauge*, 3> gauges{};
    std::array<glw::Sparkline*, 4> sparks{};
    std::array<glw::Label*, 4> lasts{};
    // the heat maps: each endpoint's latency (its own usual one), now and then an incident on some of them
    glw::HeatMap* latency = nullptr;
    glw::HeatMap* weekly = nullptr;
    std::vector<double> usual;
    std::int64_t latencyColumn = 0;
    std::size_t incidentFrom = 0, incidentTo = 0;
    std::int64_t incidentUntil = -1;
    glw::Label* status = nullptr;
    // the charts' panels: those hidden kept to come back; the View menu's items
    glw::DockArea* area = nullptr;
    std::map<std::string, glw::DockPanel> hidden;
    std::map<std::string, std::shared_ptr<glw::Action>> toggles;
    glw::BarRace* race = nullptr;
    std::size_t racers = 500;
    std::vector<std::array<double, 3>> downloads;   // each package's, by system
    std::vector<double> momentum;                   // how fast each grows
    int month = 0;
    glw::Timer feed, drift, second, disk, count, months, now, flows, trades, requests, frames, flowing;
    // latencies: reads' and writes' in a histogram, each service's in a box
    glw::HistogramChart* histogram = nullptr;
    glw::HistogramSeries* reads = nullptr;
    glw::HistogramSeries* writes = nullptr;
    glw::BoxChart* boxes = nullptr;
    std::vector<double> serviceLatency;
    // requests' sizes and latencies
    glw::ScatterChart* scatter = nullptr;
    glw::ScatterSeries* calls = nullptr;
    glw::ScatterSeries* statics = nullptr;
    // a program profiled: its threads' work, frame after frame; its stacks, sampled
    glw::TimelineChart* trace = nullptr;
    glw::FlameGraph* flame = nullptr;
    double traced = 0;                      // when its last frame began
    std::array<double, 4> workerFree{};     // when each worker is done with its job
    // where requests flow: each link's usual value
    glw::SankeyChart* flow = nullptr;
    std::vector<glw::SankeyLink> flowValues;
    // the price: a walk, its trades made into candles
    glw::CandleChart* candles = nullptr;
    double price = 250, trend = 0;
    // the traffic by protocol, stacked: each one's rate drifting, now and then a burst
    glw::LineChart* traffic = nullptr;
    std::array<glw::LineSeries*, 4> protocols{};
    std::array<double, 4> rates{40, 18, 9, 4};
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
        network = &line->addSeries({.name = tr("Network MB/s"), .secondAxis = true, .capacity = capacity});      // (another unit)
        memoryHigh = &line->addSeries({.line = false, .capacity = capacity});
        memoryLow = &line->addSeries({.line = false, .capacity = capacity});
        // an area under CPU, fading down; a band of memory's range around it
        line->addArea({.upper = cpu, .opacity = 0.32f, .gradient = true});
        line->addArea({.name = tr("Memory range"), .upper = memoryHigh, .lower = memoryLow, .color = memory->color(), .opacity = 0.22f});
        // marks: a limit across it, when it started down it (scrolling away with the minute)
        line->addAnnotation({.kind = glw::ChartAnnotation::Kind::YLine, .from = 80, .label = tr("CPU limit"), .dashed = true});
        line->addAnnotation({.kind = glw::ChartAnnotation::Kind::XLine, .from = glw::chartNow(), .label = tr("Started")});

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
        bars->addAnnotation({.kind = glw::ChartAnnotation::Kind::YLine, .from = 150, .label = tr("Capacity"), .dashed = true});

        auto racing = std::make_unique<glw::BarRace>(glw::BarRace::Props{
            .title = tr("Downloads of {n} packages, by system").arg("n", racers), .shown = 10,
            .parts = {tr("Windows"), tr("Linux"), tr("macOS")}});
        race = racing.get();
        makeRace();

        auto map = std::make_unique<glw::TreeMap>(glw::TreeMap::Props{.title = tr("A project's files, kB")});
        tree = map.get();
        project = makeProject();
        tree->setRoot(project);

        // each chart a panel: the dashboard at the left edge, the line chart and the race by it, the bars by the race, the
        // donut and the treemap on the right
        auto docked = std::make_unique<glw::DockArea>();
        docked->setStretch(1);          // all of the window under the bar above it
        area = docked.get();
        const auto panel = [](std::string id, glw::LocalizedText title, std::unique_ptr<glw::Widget> chart) {
            chart->setPadding(glw::Insets::all(14));
            return glw::DockPanel{.id = std::move(id), .title = std::move(title), .content = std::move(chart)};
        };
        glw::DockTabs& lineStack = area->addPanel(panel("line", tr("Measures"), std::move(made)));
        area->addPanel(panel("price", tr("Price"), makePrice()), &lineStack, glw::DockSide::Center, 0.5f);
        area->addPanel(panel("profile", tr("Profile"), makeProfile()), &lineStack, glw::DockSide::Center, 0.5f);
        area->addPanel(panel("flow", tr("Flow"), makeFlow()), &lineStack, glw::DockSide::Center, 0.5f);
        glw::DockTabs& donutStack = area->addPanel(panel("donut", tr("Requests"), std::move(donut)), &lineStack, glw::DockSide::End, 0.4f);
        area->addPanel(panel("spread", tr("Distributions"), makeDistributions()), &donutStack, glw::DockSide::Center, 0.5f);
        area->addPanel(panel("scatter", tr("Scatter"), makeScatter()), &donutStack, glw::DockSide::Center, 0.5f);
        glw::DockTabs& raceStack = area->addPanel(panel("race", tr("Race"), std::move(racing)), &lineStack, glw::DockSide::Bottom, 0.5f);
        glw::DockTabs& barsStack = area->addPanel(panel("bars", tr("Events"), std::move(stacked)), &raceStack, glw::DockSide::End, 0.5f);
        area->addPanel(panel("heat", tr("Latency"), heatMaps()), &barsStack, glw::DockSide::Center, 0.5f);
        glw::DockTabs& treeStack = area->addPanel(panel("tree", tr("Files"), std::move(map)), &donutStack, glw::DockSide::Bottom, 0.5f);
        area->addPanel(panel("traffic", tr("Traffic"), makeTraffic()), &treeStack, glw::DockSide::Center, 0.5f);
        area->addPanel(glw::DockPanel{.id = "dash", .title = tr("Dashboard"), .content = dashboard()}, nullptr, glw::DockSide::Start, 0.22f);
        // one closed by its tab: kept, its item in the View menu unchecked
        area->setOnPanelClosed([this](glw::DockPanel closed) {
            const std::string id = closed.id;
            hidden.insert_or_assign(id, std::move(closed));
            if (const auto toggle = toggles.find(id); toggle != toggles.end())
                toggle->second->setChecked(false);
        });
        return ui::VStack(
                   {.spacing = 10},
                   ui::HStack({.spacing = 10},
                              ui::Label({.text = tr("Style")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {"Studio", "Fluid", "Plain", "Plastic", "Metal", "Solid", "Glass"},
                                            .current = static_cast<std::size_t>(app.style()),
                                            .onChanged = [this](std::size_t i) { app.setStyle(static_cast<glw::ThemeStyle>(i)); }}),
                              ui::Label({.text = tr("Colors")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Dark"), tr("Light"), tr("Dusk"), tr("Sand"), tr("Turquoise")},
                                            .current = static_cast<std::size_t>(app.colorScheme()),
                                            .onChanged = [this](std::size_t i) { app.setColorScheme(static_cast<glw::ColorScheme>(i)); }}),
                              ui::CheckBox({.text = tr("Eased"), .checked = true, .onToggled = [this](bool on) { animate(on); }})
                                  .alignment(glw::Alignment::Start, glw::Alignment::Center),
                              // the bars, the tiles and the slices coming in again, as when they first showed
                              ui::Button({.text = tr("Again"), .onClick = [this] { replay(); }})
                                  .toolTip(tr("The charts' pieces coming in again")),
                              ui::Label({.text = tr("Bars")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Stacked"), tr("Grouped"), tr("Percent")},
                                            .onChanged = [this](std::size_t i) { bars->setLayout(static_cast<glw::BarLayout>(i)); }}),
                              ui::CheckBox({.text = tr("Shares"), .onToggled = [this](bool on) { traffic->setPercent(on); }})
                                  .alignment(glw::Alignment::Start, glw::Alignment::Center),
                              // the scatter's sizes and latencies, which span decades, on logarithmic axes
                              ui::CheckBox({.text = tr("Log"),
                                            .onToggled = [this](bool on) {
                                                scatter->setLogX(on);
                                                scatter->setLogY(on);
                                            }})
                                  .alignment(glw::Alignment::Start, glw::Alignment::Center)
                                  .toolTip(tr("The scatter chart's axes logarithmic")),
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
                   std::unique_ptr<glw::Widget>(std::move(docked)))
            .padding(glw::Insets::all(12));
    }

    // The measures now: CPU and memory side by side, colored by how near a limit they are, the network under them; a
    // sparkline of each (and of the errors) with its last value.
    std::unique_ptr<glw::Widget> dashboard() {
        const glw::Color fine{0.30f, 0.69f, 0.47f, 1}, near{0.96f, 0.65f, 0.20f, 1}, over{0.91f, 0.30f, 0.27f, 1};
        // fine to a limit, near it, over it
        const auto bands = [&](double limit, double close) {
            return std::vector<glw::Gauge::Band>{{.to = close, .color = fine}, {.to = limit, .color = near}, {.to = 100, .color = over}};
        };
        const auto gauge = [](glw::Gauge*& out, glw::LocalizedText title, glw::Gauge::Props props) {
            return glw::ui::Element<glw::Gauge>(std::make_unique<glw::Gauge>(std::move(props)))
                .bind(out)
                .with([&](glw::Gauge& g) { g.setTitle(std::move(title)); })
                .stretch(1);
        };
        const auto row = [&](glw::LocalizedText name, std::size_t i, glw::Sparkline::Props props) {
            return glw::ui::HStack({.spacing = 8},
                                   glw::ui::Label({.text = std::move(name)}).minimumSize({64, 0}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                   glw::ui::Element<glw::Sparkline>(std::make_unique<glw::Sparkline>(std::move(props))).bind(sparks[i]).stretch(1),
                                   glw::ui::Label({.text = std::string("-")})
                                       .bind(lasts[i])
                                       .minimumSize({48, 0})
                                       .alignment(glw::Alignment::End, glw::Alignment::Center));
        };
        auto made = glw::ui::VStack(
                        {.spacing = 12},
                        glw::ui::HStack({.spacing = 8},
                                        gauge(gauges[0], tr("CPU"), {.bands = bands(90, 70), .unit = "%"}),
                                        gauge(gauges[1], tr("Memory"), {.bands = bands(90, 75), .unit = "%"})),
                        gauge(gauges[2], tr("Network"), {.range = {0, 60}, .sweep = 180, .series = 2, .unit = "MB/s", .needle = true}),
                        row(tr("CPU"), 0, {.capacity = 60, .series = 0, .range = glw::ChartRange{0, 100}}),
                        row(tr("Memory"), 1, {.capacity = 60, .series = 1, .extremes = true}),
                        row(tr("Network"), 2, {.capacity = 60, .series = 2, .level = 20}),
                        row(tr("Errors"), 3, {.kind = glw::Sparkline::Kind::Columns, .capacity = 30, .color = over}))
                        .build();
        return made;
    }

    // Requests' latencies: reads' and writes' (their last 20000 each) in a histogram, with their quantiles and the share
    // under each value; each service's (its last 600) in a box.
    std::unique_ptr<glw::Widget> makeDistributions() {
        auto made = std::make_unique<glw::HistogramChart>(glw::HistogramChart::Props{.title = tr("Latency of requests, ms"), .scale = glw::HistogramChart::Scale::Percent, .cumulative = true});
        histogram = made.get();
        reads = &histogram->addSeries({.name = tr("Reads"), .capacity = 20000});
        writes = &histogram->addSeries({.name = tr("Writes"), .capacity = 20000});
        histogram->addAnnotation({.kind = glw::ChartAnnotation::Kind::XLine, .from = 100, .label = tr("Objective"), .dashed = true});
        auto services = std::make_unique<glw::BoxChart>(glw::BoxChart::Props{
            .title = tr("Latency by service, ms"),
            .categories = {tr("Auth"), tr("Users"), tr("Orders"), tr("Payments"), tr("Search"), tr("Catalog"), tr("Cart"), tr("Stock")},
            .capacity = 600});
        boxes = services.get();
        serviceLatency = {8, 14, 22, 45, 30, 12, 18, 26};
        return glw::ui::VStack({.spacing = 12}, std::unique_ptr<glw::Widget>(std::move(made)), std::unique_ptr<glw::Widget>(std::move(services)))
            .build();
    }

    // Requests' sizes against their latencies: API calls' (slower as they are larger, a line through them) and static
    // files' (quick whatever their size), the last 40000 of each.
    std::unique_ptr<glw::Widget> makeScatter() {
        auto made = std::make_unique<glw::ScatterChart>(glw::ScatterChart::Props{.title = tr("Size against latency of requests (kB, ms)")});
        scatter = made.get();
        calls = &scatter->addSeries({.name = tr("API calls"), .capacity = 40000, .trend = true});
        statics = &scatter->addSeries({.name = tr("Static files"), .capacity = 40000});
        // the latency promised: what is above it is late
        scatter->addAnnotation({.kind = glw::ChartAnnotation::Kind::YLine, .from = 60, .label = tr("Objective, 60 ms"), .dashed = true});
        return made;
    }

    // Where requests flow: from their sources through the edge and the services to the stores.
    std::unique_ptr<glw::Widget> makeFlow() {
        auto made = std::make_unique<glw::SankeyChart>(
            glw::SankeyChart::Props{.title = tr("Requests through the system, thousands a minute"), .unit = "k"});
        flow = made.get();
        const std::size_t web = flow->addNode(tr("Web")), mobile = flow->addNode(tr("Mobile")), partners = flow->addNode(tr("Partners"));
        const std::size_t cdn = flow->addNode(tr("CDN")), balancer = flow->addNode(tr("Load balancer"));
        const std::size_t auth = flow->addNode(tr("Auth")), catalog = flow->addNode(tr("Catalog")), search = flow->addNode(tr("Search"));
        const std::size_t orders = flow->addNode(tr("Orders"));
        const std::size_t cached = flow->addNode(tr("Served from the edge")), cache = flow->addNode(tr("Cache"));
        const std::size_t database = flow->addNode(tr("Database")), queue = flow->addNode(tr("Queue"));
        flowValues = {{web, cdn, 60},      {web, balancer, 40},     {mobile, cdn, 25},     {mobile, balancer, 45}, {partners, balancer, 30},
                      {cdn, cached, 70},   {cdn, balancer, 15},     {balancer, auth, 30},  {balancer, catalog, 45}, {balancer, search, 25},
                      {balancer, orders, 30}, {auth, cache, 20},    {auth, database, 10},  {catalog, cache, 30},   {catalog, database, 15},
                      {search, cache, 10}, {search, database, 15},  {orders, database, 20}, {orders, queue, 10}};
        flow->setLinks(flowValues);
        return made;
    }

    // A program profiled: its threads' spans of work, the last quarter of a second of them, following; where its time
    // goes, a search above it.
    std::unique_ptr<glw::Widget> makeProfile() {
        auto timeline = std::make_unique<glw::TimelineChart>(glw::TimelineChart::Props{
            .title = tr("Threads, frame by frame"), .xAxis = glw::ChartAxisKind::Time,
            .lanes = {tr("Main"), tr("Render"), tr("Worker 1"), tr("Worker 2"), tr("Worker 3"), tr("Worker 4")}, .capacity = 20000,
            .window = 0.25});
        trace = timeline.get();
        auto graph = std::make_unique<glw::FlameGraph>(glw::FlameGraph::Props{.title = tr("Where the time goes")});
        flame = graph.get();
        return glw::ui::VStack({.spacing = 10},
                               glw::ui::Element<glw::TimelineChart>(std::move(timeline)).stretch(1),
                               glw::ui::HStack({.spacing = 8},
                                               glw::ui::Label({.text = tr("Find")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                               glw::ui::LineEdit({.placeholder = tr("a function's name"),
                                                                  .onChanged = [this](const std::string& text) { flame->setHighlight(text); }})
                                                   .stretch(1)),
                               glw::ui::Element<glw::FlameGraph>(std::move(graph)).stretch(1))
            .build();
    }

    // A frame of the program from `t0` (seconds): the main thread's frame and what it does in it, the render thread's
    // after it, jobs on the workers that are free; its stacks sampled, a sample a tenth of a millisecond.
    void nextFrame(double t0) {
        const double ms = 0.001;
        const auto span = [&](std::size_t lane, double from, double length, const char* name, std::uint32_t depth) {
            trace->addSpan(lane, {.start = from, .end = from + length, .name = name, .depth = depth});
            return from + length;
        };
        const auto sample = [&](std::initializer_list<const char*> stack, double length) {
            std::vector<std::string> names(stack.begin(), stack.end());
            flame->addStack(names, std::round(length / ms * 10));
        };
        // the main thread: input, update (physics, scripts, animation), layout, paint
        const double input = uniform(0.2, 0.6) * ms, physics = uniform(0.8, 2.4) * ms, scripts = uniform(0.5, 2.0) * ms;
        const double animation = uniform(0.3, 0.9) * ms, layout = uniform(0.6, 1.6) * ms, paint = uniform(1.5, 3.5) * ms;
        const double hitch = uniform(0, 1) < 0.03 ? uniform(4, 9) * ms : 0;        // now and then, a slow script
        double t = span(0, t0, input, "Input", 1);
        const double update = t;
        t = span(0, t, physics, "Physics", 2);
        t = span(0, t, scripts + hitch, "Scripts", 2);
        t = span(0, t, animation, "Animation", 2);
        span(0, update, t - update, "Update", 1);
        t = span(0, t, layout, "Layout", 1);
        t = span(0, t, paint, "Paint", 1);
        span(0, t0, t - t0, "Frame", 0);
        sample({"main", "Frame", "Input"}, input);
        sample({"main", "Frame", "Update", "Physics", "Broadphase"}, physics * 0.4);
        sample({"main", "Frame", "Update", "Physics", "Solver"}, physics * 0.6);
        sample({"main", "Frame", "Update", "Scripts", "Interpreter"}, scripts * 0.7 + hitch);
        sample({"main", "Frame", "Update", "Scripts", "Garbage collector"}, scripts * 0.3);
        sample({"main", "Frame", "Update", "Animation"}, animation);
        sample({"main", "Frame", "Layout", "Measure text"}, layout * 0.55);
        sample({"main", "Frame", "Layout", "Arrange"}, layout * 0.45);
        sample({"main", "Frame", "Paint", "Draw text"}, paint * 0.5);
        sample({"main", "Frame", "Paint", "Fill paths"}, paint * 0.5);
        // the render thread: the frame's commands after it was painted
        const double submit = uniform(1.0, 2.5) * ms, present = uniform(0.4, 1.2) * ms;
        double r = span(1, t, submit, "Submit", 0);
        span(1, r, present, "Present", 0);
        sample({"render", "Submit", "Upload buffers"}, submit * 0.35);
        sample({"render", "Submit", "Draw calls"}, submit * 0.65);
        sample({"render", "Present"}, present);
        // the workers: a job on each that is free, sometimes
        static constexpr const char* jobs[] = {"Decode image", "Load texture", "Compress", "Find path", "Bake light"};
        for (std::size_t w = 0; w < workerFree.size(); ++w) {
            if (workerFree[w] > t0 || uniform(0, 1) > 0.45)
                continue;
            const char* job = jobs[std::uniform_int_distribution<std::size_t>(0, std::size(jobs) - 1)(random)];
            const double from = t0 + uniform(0, 4) * ms, length = uniform(2, 22) * ms;
            workerFree[w] = span(2 + w, from, length, job, 0);
            sample({"workers", job}, length);
        }
    }

    // A tenth of a second of requests: their sizes and latencies.
    void nextSizes() {
        std::lognormal_distribution<double> size(std::log(40.0), 0.45);
        for (int i = 0; i < 60; ++i) {
            const double kb = size(random);
            calls->append(kb, std::max(0.5, 6 + 0.35 * kb + normal(4 + 0.05 * kb)));
            const double file = size(random) * 2;
            statics->append(file, std::max(0.2, 1.5 + 0.01 * file + std::abs(normal(1.2))));
        }
    }

    // A tenth of a second of requests: reads quick, writes slower, both with a long tail; each service's of its own.
    void nextRequests() {
        std::lognormal_distribution<double> read(std::log(12.0), 0.45), write(std::log(32.0), 0.55);
        double some[20];
        for (double& v : some)
            v = read(random) + (uniform(0, 1) < 0.01 ? uniform(50, 200) : 0);
        reads->add(some);
        for (int i = 0; i < 8; ++i)
            some[i] = write(random) + (uniform(0, 1) < 0.02 ? uniform(80, 300) : 0);
        writes->add(std::span<const double>(some, 8));
        for (std::size_t k = 0; k < serviceLatency.size(); ++k) {
            std::lognormal_distribution<double> service(std::log(serviceLatency[k]), 0.4);
            double values[3];
            for (double& v : values)
                v = service(random) + (uniform(0, 1) < 0.005 ? uniform(1, 2.5) * serviceLatency[k] : 0);
            boxes->addValues(k, values);
        }
    }

    // A price, its trades in candles of a second (an hour of them kept), shown in candles of five seconds (or of the
    // period chosen), the last 80; the averages of its last 20 and 50 closes.
    std::unique_ptr<glw::Widget> makePrice() {
        auto made = std::make_unique<glw::CandleChart>(glw::CandleChart::Props{
            .title = tr("A price"), .period = 1, .periods = {1, 5, 15, 60, 300}, .capacity = 3600, .window = 80});
        candles = made.get();
        candles->setPeriod(5);
        candles->addAverage(20);
        candles->addAverage(50);
        candles->addAnnotation({.kind = glw::ChartAnnotation::Kind::XLine, .from = glw::chartNow(), .label = tr("Opened")});
        return made;
    }

    // A trade: the price a step on (drifting now up, now down), a volume.
    void nextTrade(double t) {
        trend = std::clamp(trend + normal(0.002), -0.01, 0.01);
        price = std::max(1.0, price * (1 + trend * 0.01 + normal(0.0009)));
        candles->trade(t, price, std::round(std::exp(uniform(0, 5))));
    }

    // A network's traffic by protocol, stacked, its last two minutes (ten minutes kept).
    std::unique_ptr<glw::Widget> makeTraffic() {
        auto made = std::make_unique<glw::LineChart>(
            glw::LineChart::Props{.title = tr("Traffic by protocol, MB/s"), .xAxis = glw::ChartAxisKind::Time, .window = 120});
        traffic = made.get();
        const glw::LocalizedText names[] = {tr("HTTP"), tr("gRPC"), tr("WebSocket"), tr("Other")};
        for (std::size_t k = 0; k < protocols.size(); ++k)
            protocols[k] = &traffic->addSeries({.name = names[k], .stacked = true, .capacity = 6000});
        // one crosshair with the measures' chart and the price's: all of them are over the same time
        auto cursor = std::make_shared<glw::ChartCursor>();
        line->setCursor(cursor);
        traffic->setCursor(cursor);
        candles->setCursor(cursor);
        return made;
    }

    // A tenth of a second of traffic: each protocol's rate drifting back to its usual one, now and then a burst.
    void nextTraffic(double t) {
        static constexpr std::array<double, 4> typical{40, 18, 9, 4};
        for (std::size_t k = 0; k < rates.size(); ++k) {
            rates[k] = std::max(0.0, rates[k] + (typical[k] - rates[k]) * 0.02 + normal(typical[k] * 0.03));
            if (uniform(0, 1) < 0.002)
                rates[k] += typical[k] * uniform(0.8, 2.0);
            protocols[k]->append(t, rates[k]);
        }
    }

    // Each endpoint's latency, a column a second over the last five minutes (an hour kept), by powers of ten; requests by
    // hour and weekday, the value in each cell.
    std::unique_ptr<glw::Widget> heatMaps() {
        std::vector<glw::LocalizedText> endpoints;
        const char* resources[] = {"users", "orders", "payments", "search", "catalog", "cart", "auth", "reviews", "shipping", "stock"};
        const std::pair<const char*, const char*> operations[] = {{"GET", ""}, {"POST", ""}, {"GET", "/{id}"}, {"PUT", "/{id}"}};
        for (int version = 1; version <= 5; ++version) {
            for (const char* resource : resources) {
                for (const auto& [verb, item] : operations) {
                    endpoints.emplace_back(std::string(verb) + " /v" + std::to_string(version) + "/" + resource + item);
                    usual.push_back(std::exp(uniform(std::log(8.0), std::log(180.0))));
                }
            }
        }
        auto live = std::make_unique<glw::HeatMap>(glw::HeatMap::Props{
            .title = tr("Latency of {n} endpoints, ms").arg("n", endpoints.size()), .rows = std::move(endpoints),
            .xAxis = glw::ChartAxisKind::Time, .start = std::floor(glw::chartNow()) - 600, .step = 1, .capacity = 3600, .window = 300,
            .scale = {.logarithmic = true, .clip = 0.99f}});
        latency = live.get();
        auto matrix = std::make_unique<glw::HeatMap>(glw::HeatMap::Props{
            .title = tr("Requests by hour and weekday, thousands"),
            .rows = {tr("Saturday"), tr("Sunday"), tr("Monday"), tr("Tuesday"), tr("Wednesday"), tr("Thursday"), tr("Friday")}});
        weekly = matrix.get();
        std::vector<glw::LocalizedText> hours;
        std::vector<double> counts;
        for (int hour = 0; hour < 24; ++hour) {
            hours.emplace_back((hour < 10 ? "0" : "") + std::to_string(hour));
            for (int day = 0; day < 7; ++day) {
                // busy in working hours, on working days (Friday the weekend here)
                const double working = day >= 1 && day <= 5 ? 1.0 : 0.45;
                const double busy = std::exp(-std::pow((hour - 14) / 4.5, 2)) * 0.85 + 0.15;
                counts.push_back(std::round(working * busy * 90 + uniform(0, 8)));
            }
        }
        weekly->setColumnNames(std::move(hours));
        weekly->setValues(24, counts);
        return glw::ui::VStack({.spacing = 12}, std::unique_ptr<glw::Widget>(std::move(live)), std::unique_ptr<glw::Widget>(std::move(matrix)))
            .build();
    }

    // A second of each endpoint's latency: its usual one, varying; a spike now and then; an incident on a run of them.
    void nextLatency() {
        if (latencyColumn > incidentUntil && uniform(0, 1) < 0.012) {
            incidentFrom = std::uniform_int_distribution<std::size_t>(0, usual.size() - 30)(random);
            incidentTo = incidentFrom + std::uniform_int_distribution<std::size_t>(5, 30)(random);
            incidentUntil = latencyColumn + std::uniform_int_distribution<std::int64_t>(10, 40)(random);
        }
        std::vector<double> column(usual.size());
        for (std::size_t e = 0; e < usual.size(); ++e) {
            double v = usual[e] * std::exp(normal(0.25));
            if (latencyColumn <= incidentUntil && e >= incidentFrom && e < incidentTo)
                v *= uniform(6, 14);
            if (uniform(0, 1) < 0.002)
                v *= uniform(5, 30);
            column[e] = v;
        }
        latency->appendColumn(column);
        ++latencyColumn;
    }

    // The View menu: an item for each chart, checked while it shows.
    std::unique_ptr<glw::MenuBar> menus() {
        std::vector<glw::MenuItem> items;
        for (const auto& [id, title] : std::initializer_list<std::pair<const char*, const char*>>{
                 {"dash", "&Dashboard"}, {"line", "&Measures"}, {"price", "&Price"}, {"donut", "Re&quests"}, {"race", "R&ace"}, {"bars", "&Events"}, {"heat", "&Latency"}, {"spread", "&Distributions"}, {"scatter", "&Scatter"}, {"profile", "Pr&ofile"}, {"flow", "Flo&w"}, {"traffic", "&Traffic"},
                 {"tree", "&Files"}}) {
            auto toggle = std::make_shared<glw::Action>(tr(title));
            toggle->setCheckable(true);
            toggle->setChecked(true);
            toggle->setOnTriggered([this, id = std::string(id), toggle = toggle.get()] {
                if (toggle->checked())
                    show(id);
                else
                    area->closePanel(id);
            });
            toggles[id] = toggle;
            items.push_back({.action = toggle});
        }
        auto bar = std::make_unique<glw::MenuBar>();
        bar->setMenus({{.text = tr("&View"), .items = std::move(items)}});
        return bar;
    }

    // A chart hidden shown again: beside the first of its neighbours that shows (where it was), or at the end.
    void show(const std::string& id) {
        const auto it = hidden.find(id);
        if (it == hidden.end())
            return;
        glw::DockPanel panel = std::move(it->second);
        hidden.erase(it);
        static const std::map<std::string, std::vector<std::pair<std::string, glw::DockSide>>> beside{
            {"line", {{"race", glw::DockSide::Top}, {"donut", glw::DockSide::Start}, {"bars", glw::DockSide::Top}}},
            {"donut", {{"tree", glw::DockSide::Top}, {"line", glw::DockSide::End}}},
            {"race", {{"bars", glw::DockSide::Start}, {"line", glw::DockSide::Bottom}}},
            {"bars", {{"race", glw::DockSide::End}, {"tree", glw::DockSide::Start}, {"line", glw::DockSide::Bottom}}},
            {"tree", {{"donut", glw::DockSide::Bottom}, {"bars", glw::DockSide::End}}},
            {"dash", {}},
            {"flow", {{"line", glw::DockSide::Center}, {"price", glw::DockSide::Center}, {"race", glw::DockSide::Top}}},
            {"profile", {{"line", glw::DockSide::Center}, {"price", glw::DockSide::Center}, {"race", glw::DockSide::Top}}},
            {"spread", {{"donut", glw::DockSide::Center}, {"tree", glw::DockSide::Top}}},
            {"scatter", {{"donut", glw::DockSide::Center}, {"tree", glw::DockSide::Top}}},
            {"price", {{"line", glw::DockSide::Center}, {"race", glw::DockSide::Top}}},
            {"traffic", {{"tree", glw::DockSide::Center}, {"donut", glw::DockSide::Bottom}}},
            {"heat", {{"bars", glw::DockSide::Center}, {"race", glw::DockSide::Center}, {"line", glw::DockSide::Bottom}}},
        };
        if (id == "dash") {             // at the left edge again
            area->addPanel(std::move(panel), nullptr, glw::DockSide::Start, 0.22f);
            return;
        }
        for (const auto& [next, side] : beside.at(id)) {
            if (glw::DockTabs* tabs = area->tabsOf(next)) {
                area->addPanel(std::move(panel), tabs, side, 0.5f);
                return;
            }
        }
        area->addPanel(std::move(panel), nullptr, glw::DockSide::End, 0.4f);
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

    // A month on: each package grows by its pace (which wanders), now and then one takes off. Its name in the language's
    // calendar (the month of the language's calendar the first of this one is in).
    void nextMonth() {
        const glw::Locale& locale = app.translator().locale();
        for (std::size_t i = 0; i < racers; ++i) {
            momentum[i] = std::clamp(momentum[i] + normal(0.004) + (uniform(0, 1) < 0.002 ? 0.12 : 0.0), -0.02, 0.25);
            for (double& d : downloads[i])
                d = std::max(0.0, d * (1 + momentum[i] * uniform(0.9, 1.1)));
            race->setValues(i, downloads[i]);
        }
        const glw::CivilDate day = locale.date({2015 + month / 12, month % 12 + 1, 1});
        race->setPeriod(locale.shortMonths[static_cast<std::size_t>(day.month - 1)] + " " + locale.digitsOf(day.year));
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

    void replay() {
        for (glw::Chart* chart : std::initializer_list<glw::Chart*>{pie, bars, tree, race, histogram, gauges[0], gauges[1], gauges[2]})
            chart->playEntrance();
    }

    void animate(bool on) {
        for (glw::Chart* chart : std::initializer_list<glw::Chart*>{line, pie, race, bars, tree, gauges[0], gauges[1], gauges[2], latency, weekly,
                                                                    traffic, candles, histogram, boxes, scatter, trace, flame, flow})
            chart->setAnimated(on);
        for (glw::Sparkline* spark : sparks)
            spark->setAnimated(on);
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
            sparks[3]->append(values[2]);
            lasts[3]->setText(app.translator().locale().number(values[2]));
            nextLatency();
        }, true);
        // the requests: their latencies, ten times a second
        requests.start(scheduler, std::chrono::milliseconds(100), [this] {
            nextRequests();
            nextSizes();
        }, true);
        // the program profiled: its frames, sixty a second, as they end
        frames.start(scheduler, std::chrono::milliseconds(50), [this] {
            for (const double until = glw::chartNow(); traced + 1.0 / 60 <= until; traced += 1.0 / 60)
                nextFrame(traced);
        }, true);
        // the flows: each a little more or less than usual, now and then
        flowing.start(scheduler, std::chrono::milliseconds(1500), [this] {
            for (const glw::SankeyLink& link : flowValues)
                flow->setLink(link.source, link.target, std::max(1.0, std::round(link.value * (1 + normal(0.15)))));
        }, true);
        // the trades: twenty a second
        trades.start(scheduler, std::chrono::milliseconds(50), [this] { nextTrade(glw::chartNow()); }, true);
        // the traffic: ten points a second
        flows.start(scheduler, std::chrono::milliseconds(100), [this] { nextTraffic(glw::chartNow()); }, true);
        // the dashboard: the measures now, twice a second
        now.start(scheduler, std::chrono::milliseconds(500), [this] {
            const double values[] = {cpuValue, memoryValue, networkValue};
            for (std::size_t i = 0; i < 3; ++i) {
                gauges[i]->setValue(values[i]);
                sparks[i]->append(values[i]);
                lasts[i]->setText(glw::formatNumber(values[i], 1, 0, app.translator().locale()));
            }
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
            status->setText(tr("{n} points, {rate} a second").arg("n", points).arg("rate", static_cast<int>(rate * 3)));
        }, true);
        // what came before it opened: the last ten minutes, so that there is a history to pan into
        const double from = fed - 600;
        for (double t = from; t < fed; t += 1 / rate)
            sample(t);
        for (int i = 0; i < 600; ++i)
            nextLatency();
        for (double t = fed - 600; t < fed; t += 0.1)
            nextTraffic(t);
        for (double t = fed - 3600; t < fed; t += 0.25)
            nextTrade(t);
        for (int i = 0; i < 300; ++i)
            nextRequests();
        for (int i = 0; i < 700; ++i)
            nextSizes();
        for (traced = fed - 5; traced + 1.0 / 60 <= fed; traced += 1.0 / 60)
            nextFrame(traced);
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
    std::string language = "en";
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
        else if (arg == "--language" && i + 1 < argc)
            language = argv[++i];
    }
    try {
        glw::Application app;
        app.setLanguage(language);
        if (style)
            app.setStyle(*style, false);
        if (colors)
            app.setColorScheme(*colors, false);
        Demo demo{.app = app, .rate = rate};
        demo.racers = racers;
        glw::Window& window = app.openWindow({.title = tr("Charts"), .size = {1280, 820}}, demo.build());
        window.setMenuBar(demo.menus(), glw::MenuBarPlacement::TitleBar);
        demo.start();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "chart_demo: %s\n", e.what());
        return 1;
    }
}
