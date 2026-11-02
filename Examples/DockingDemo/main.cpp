// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Docking, as a script editor would have it: the scripts are major tabs in the title bar (+ adds one, dragging a tab moves
// it along, its × closes it); each has its panels (graph, palette, inspector, output, search) docked around its graph.
// Drag a panel's tab onto another stack's tabs to join them, or to an edge of a stack to split it; drag the line between
// two stacks to share their room.
//
// Across windows: a script's tab dragged out of the title bar opens in a window of its own, and dropped on another
// window's title bar goes there. A panel's tab let go out of its script floats in a window of its own; its tab dragged
// back onto a stack docks it again.
//
//   docking_demo [--frames N] [--language fa]
//
// --frames N draws N frames and exits: a smoke test.
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace {

namespace ui = glw::ui;
using glw::tr;

constexpr const char* persianCatalog = R"({
  "language": "fa",
  "texts": {
    "Docking": "داک",
    "Script {n}": "اسکریپت {n}",
    "Graph": "گراف",
    "Palette": "پالت",
    "Inspector": "بازرس",
    "Output": "خروجی",
    "Search": "جستجو",
    "The graph of script {n}": "گراف اسکریپت {n}",
    "Drag a tab onto another stack's tabs to join them, or to an edge of a stack to split it. Drag the line between two stacks to share their room. The + in the title bar adds a script.":
      "یک تب را روی تب‌های پشته‌ای دیگر بکشید تا به آن بپیوندد، یا به لبه‌ی یک پشته تا آن را تقسیم کند. خط میان دو پشته را بکشید تا جایشان را جابه‌جا کنید. + در نوار عنوان یک اسکریپت می‌افزاید.",
    "Drag a tab out of its window to float it, a script's tab out of the title bar to open it in a window of its own.":
      "یک تب را از پنجره‌اش بیرون بکشید تا شناور شود، و تب یک اسکریپت را از نوار عنوان بیرون بکشید تا در پنجره‌ای جدا باز شود.",
    "Name": "نام",
    "Pure": "خالص",
    "Comment": "توضیح",
    "Search the nodes": "جستجوی گره‌ها",
    "Script {n} compiled.": "اسکریپت {n} کامپایل شد."
  }
})";

constexpr const char* nodes[] = {"Branch", "Sequence", "For Each", "While", "Delay", "Print", "Add", "Subtract", "Multiply",
                                 "Divide", "Compare", "And", "Or", "Not", "Get Variable", "Set Variable", "Make Struct",
                                 "Break Struct", "Switch on Enum", "Call Function"};

std::unique_ptr<glw::Widget> padded(ui::Element<glw::Widget> content) {
    return std::move(content).padding(glw::Insets::all(12));
}

struct Studio {
    glw::Application& app;
    glw::MajorTabs* documents = nullptr;
    int scripts = 0;

    // A script: its graph in the middle, the other panels docked around it.
    std::unique_ptr<glw::Widget> script(int n) {
        const std::string number = std::to_string(n);
        auto area = std::make_unique<glw::DockArea>();

        auto graph = ui::VStack({.spacing = 12},
                                ui::Label({.text = tr("The graph of script {n}").arg("n", number), .fontSize = 20}),
                                ui::Label({.text = tr("Drag a tab onto another stack's tabs to join them, or to an edge of a stack to split "
                                                      "it. Drag the line between two stacks to share their room. The + in the title bar adds "
                                                      "a script."),
                                           .muted = true,
                                           .wrap = true}),
                                ui::Label({.text = tr("Drag a tab out of its window to float it, a script's tab out of the title bar to open it "
                                                      "in a window of its own."),
                                           .muted = true,
                                           .wrap = true}))
                         .padding(glw::Insets::all(24));
        glw::DockTabs& middle = area->addPanel({"graph", tr("Graph"), std::move(graph), false});

        ui::Children names;
        for (const char* name : nodes)
            names.push_back(ui::Card(ui::Label({.text = name})).padding(glw::Insets::symmetric(10, 6)));
        area->addPanel({"palette", tr("Palette"), ui::ScrollArea({}, ui::VStack({.spacing = 4}, std::move(names)).padding(glw::Insets::all(10)))},
                       &middle, glw::DockSide::Start, 0.22f);

        area->addPanel({"inspector", tr("Inspector"),
                        padded(ui::VStack({.spacing = 10}, ui::Label({.text = tr("Name")}), ui::LineEdit({.text = "Main"}),
                                          ui::CheckBox({.text = tr("Pure")}), ui::Label({.text = tr("Comment")}), ui::LineEdit({})))},
                       &middle, glw::DockSide::End, 0.28f);

        ui::Children lines;
        for (int i = 1; i <= 12; ++i)
            lines.push_back(ui::Label({.text = tr("Script {n} compiled.").arg("n", number), .muted = true}));
        glw::DockTabs& bottom = area->addPanel({"output", tr("Output"), ui::ScrollArea({}, ui::VStack({.spacing = 4}, std::move(lines)).padding(glw::Insets::all(10)))},
                                               &middle, glw::DockSide::Bottom, 0.3f);
        area->addPanel({"search", tr("Search"), padded(ui::VStack({}, ui::LineEdit({.placeholder = tr("Search the nodes")})))}, &bottom);
        return area;
    }

    // In `into`: the first window's tabs, or those of a window a script was dragged out to.
    void addScript(glw::MajorTabs& into) {
        ++scripts;
        into.addTab(tr("Script {n}").arg("n", std::to_string(scripts)), script(scripts));
    }
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    std::string language = "en";
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--language" && i + 1 < argc)
            language = argv[++i];
    }
    try {
        glw::Application app;
        app.translator().loadCatalog(persianCatalog);
        app.setLanguage(language);

        Studio studio{app};
        auto tabs = std::make_unique<glw::MajorTabs>();
        studio.documents = tabs.get();
        studio.addScript(*tabs);
        studio.addScript(*tabs);
        tabs->setActive(0);
        tabs->setOnNewTab([&studio](glw::MajorTabs& into) { studio.addScript(into); });

        glw::Window& window = app.openWindow({.title = tr("Docking"), .size = {1280, 800}}, std::move(tabs));
        window.frame()->titleBar().setCenter(studio.documents->makeStrip());
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "docking_demo: %s\n", e.what());
        return 1;
    }
}
