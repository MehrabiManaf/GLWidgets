// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Docking, as a script editor would have it: the scripts are major tabs in the title bar (+ adds one, dragging a tab moves
// it along, its × closes it); each has its graph in its middle (its center: not a panel, its room kept) and its panels
// (palette, inspector, output, search) docked around it.
// Around the scripts are the application's panels, the same whichever script is shown: its files, and its log.
// Drag a panel's tab onto another stack's tabs, or onto the middle of the guides the stack shows, to join them; onto one of
// the guides around it to split the stack; drag the line between two stacks to share their room.
//
// Across windows: a script's tab dragged out of the title bar opens in a window of its own, and dropped on another
// window's title bar goes there. A panel's tab dragged out of its stack's tab bar floats at once, in a window of its own
// that follows the mouse; that window, dragged by its tab or by its title, docks where it is let go on a guide. The button
// at the end of a stack's tab bar floats its panel (docks it back, in a floating window); the Search panel has one more,
// which clears its field. Tabs that do not fit scroll: by the wheel, or by the thin bar along their bottom.
//
//   docking_demo [--frames N] [--language fa] [--light] [--workspace FILE]
//
// --frames N draws N frames and exits: a smoke test. --workspace FILE starts with the windows FILE has (if it is there),
// and saves them there when the demo ends: the windows, the scripts in each, where every panel is.
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/Docking.hpp>
#include <GLWidgets/Widgets/MajorTabs.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>
#include <GLWidgets/Widgets/Workspace.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace ui = glw::ui;
using glw::tr;

constexpr const char* persianCatalog = R"({
  "language": "fa",
  "texts": {
    "Docking": "داک",
    "Float": "شناور کن",
    "Dock": "داک کن",
    "Hide the tab bar": "پنهان کردن نوار تب‌ها",
    "Close": "بستن",
    "Script {n}": "اسکریپت {n}",
    "Palette": "پالت",
    "Inspector": "بازرس",
    "Output": "خروجی",
    "Search": "جستجو",
    "The graph of script {n}": "گراف اسکریپت {n}",
    "Drag a tab onto another stack's tabs, or onto the middle guide it shows, to join them; onto a guide around it to split the stack. Drag the line between two stacks to share their room. The + in the title bar adds a script.":
      "یک تب را روی تب‌های پشته‌ای دیگر، یا روی راهنمای وسطی که آن پشته نشان می‌دهد، بکشید تا به آن بپیوندد؛ روی یکی از راهنماهای اطراف آن بکشید تا پشته را تقسیم کند. خط میان دو پشته را بکشید تا جایشان را جابه‌جا کنید. + در نوار عنوان یک اسکریپت می‌افزاید.",
    "Drag a tab out of its tab bar to float it, and a floating window by its tab or its title onto a guide to dock it. Drag a script's tab out of the title bar to open it in a window of its own.":
      "یک تب را از نوار تب‌هایش بیرون بکشید تا شناور شود، و یک پنجره‌ی شناور را با تب یا عنوانش روی یک راهنما بکشید تا داک شود. تب یک اسکریپت را از نوار عنوان بیرون بکشید تا در پنجره‌ای جدا باز شود.",
    "Files": "فایل‌ها",
    "Log": "گزارش",
    "Opened {file}.": "{file} باز شد.",
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

// The project's files, as folders: a node's id is its index.
class ProjectFiles final : public glw::TreeModel {
public:
    ProjectFiles() {
        const std::size_t scripts = add(std::nullopt, "scripts");
        for (const char* name : {"main.flow", "player.flow", "enemy.flow", "inventory.flow", "dialogue.flow", "save.flow"})
            add(scripts, name);
        const std::size_t ai = add(scripts, "ai");
        for (const char* name : {"patrol.flow", "chase.flow", "flee.flow"})
            add(ai, name);
        const std::size_t levels = add(std::nullopt, "levels");
        for (int i = 1; i <= 12; ++i)
            add(levels, "level" + std::to_string(i) + ".map");
        const std::size_t ui = add(std::nullopt, "ui");
        for (const char* name : {"menu.ui", "hud.ui", "settings.ui"})
            add(ui, name);
        add(std::nullopt, "project.json");
    }

    const std::string& name(glw::TreeNode node) const { return nodes_[node.id].name; }

    std::size_t childCount(std::optional<glw::TreeNode> parent) const override {
        return parent ? nodes_[parent->id].children.size() : top_.size();
    }
    glw::TreeNode child(std::optional<glw::TreeNode> parent, std::size_t index) const override {
        return {parent ? nodes_[parent->id].children[index] : top_[index]};
    }

private:
    struct Node {
        std::string name;
        std::vector<std::size_t> children;
    };
    std::size_t add(std::optional<std::size_t> parent, std::string name) {
        nodes_.push_back({std::move(name), {}});
        (parent ? nodes_[*parent].children : top_).push_back(nodes_.size() - 1);
        return nodes_.size() - 1;
    }
    std::vector<Node> nodes_;
    std::vector<std::size_t> top_;
};

struct Studio {
    glw::Application& app;
    int scripts = 0;

    // A script: its graph in the middle, the other panels docked around it.
    std::unique_ptr<glw::Widget> script(int n) {
        const std::string number = std::to_string(n);
        auto area = std::make_unique<glw::DockArea>();

        auto graph = ui::VStack({.spacing = 12},
                                ui::Label({.text = tr("The graph of script {n}").arg("n", number), .fontSize = 20}),
                                ui::Label({.text = tr("Drag a tab onto another stack's tabs, or onto the middle guide it shows, to join them; "
                                                      "onto a guide around it to split the stack. Drag the line between two stacks to share "
                                                      "their room. The + in the title bar adds a script."),
                                           .muted = true,
                                           .wrap = true}),
                                ui::Label({.text = tr("Drag a tab out of its tab bar to float it, and a floating window by its tab or its title "
                                                      "onto a guide to dock it. Drag a script's tab out of the title bar to open it in a window "
                                                      "of its own."),
                                           .muted = true,
                                           .wrap = true}))
                         .padding(glw::Insets::all(24));
        // the graph: the script's center, as FlowGraph's script view; the panels dock around it
        area->setCenter(ui::Card(std::move(graph)));
        glw::Widget* middle = area->center();

        ui::Children names;
        for (const char* name : nodes)
            names.push_back(ui::Card(ui::Label({.text = name})).padding(glw::Insets::symmetric(10, 6)));
        area->addPanel({"palette", tr("Palette"), ui::ScrollArea({}, ui::VStack({.spacing = 4}, std::move(names)).padding(glw::Insets::all(10)))},
                       middle, glw::DockSide::Start, 0.22f);

        area->addPanel({"inspector", tr("Inspector"),
                        padded(ui::VStack({.spacing = 10}, ui::Label({.text = tr("Name")}), ui::LineEdit({.text = "Main"}),
                                          ui::CheckBox({.text = tr("Pure")}), ui::Label({.text = tr("Comment")}), ui::LineEdit({})))},
                       middle, glw::DockSide::End, 0.28f);

        ui::Children lines;
        for (int i = 1; i <= 12; ++i)
            lines.push_back(ui::Label({.text = tr("Script {n} compiled.").arg("n", number), .muted = true}));
        glw::DockTabs& bottom = area->addPanel({"output", tr("Output"), ui::ScrollArea({}, ui::VStack({.spacing = 4}, std::move(lines)).padding(glw::Insets::all(10)))},
                                               middle, glw::DockSide::Bottom, 0.3f);
        // a button of its own on the tab bar while it is shown: it clears the field
        glw::LineEdit* field = nullptr;
        glw::DockPanel search{"search", tr("Search"), padded(ui::VStack({}, ui::LineEdit({.placeholder = tr("Search the nodes")}).bind(field)))};
        search.buttons.push_back({[](glw::PaintContext& context, const glw::Rect& r, glw::Color color) {
                                      context.painter.drawLine({r.x + 2, r.y + 2}, {r.right() - 2, r.bottom() - 2}, 1.2f, color);
                                      context.painter.drawLine({r.x + 2, r.bottom() - 2}, {r.right() - 2, r.y + 2}, 1.2f, color);
                                  },
                                  [field] { field->setText(""); }});
        area->addPanel(std::move(search), &bottom);
        return area;
    }

    // The application's panels, around the scripts: whichever is shown, in any window. By their ids: a saved workspace asks
    // for them so.
    static std::optional<glw::DockPanel> globalPanel(const std::string& id) {
        if (id == "files") {
            // a tree of the project's folders, its scripts' open
            auto files = std::make_shared<ProjectFiles>();
            auto tree = std::make_unique<glw::TreeView>();
            tree->setModel(files);
            tree->setDelegate(std::make_unique<glw::TreeTextDelegate>([files](glw::TreeNode n) { return glw::LocalizedText(files->name(n)); }));
            tree->setExpanded(files->child(std::nullopt, 0), true);
            tree->setPadding(glw::Insets::all(4));
            return glw::DockPanel{.id = "files", .title = tr("Files"), .content = std::move(tree), .scope = glw::DockScope::Global};
        }
        if (id == "log") {
            ui::Children lines;
            for (const char* name : {"main.flow", "player.flow", "enemy.flow"})
                lines.push_back(ui::Label({.text = tr("Opened {file}.").arg("file", name), .muted = true}));
            return glw::DockPanel{.id = "log",
                                  .title = tr("Log"),
                                  .content = ui::ScrollArea({}, ui::VStack({.spacing = 4}, std::move(lines)).padding(glw::Insets::all(10))),
                                  .scope = glw::DockScope::Global};
        }
        return std::nullopt;
    }

    static glw::LocalizedText title(int n) { return tr("Script {n}").arg("n", std::to_string(n)); }

    // A script by its id ("script-N"), for a saved workspace.
    std::optional<glw::WorkspaceDocument> document(const std::string& id) {
        const int n = id.rfind("script-", 0) == 0 ? std::atoi(id.c_str() + 7) : 0;
        if (n <= 0)
            return std::nullopt;
        scripts = std::max(scripts, n);
        return glw::WorkspaceDocument{title(n), script(n)};
    }

    // A window's scripts: + adds one to them.
    std::unique_ptr<glw::MajorTabs> makeTabs() {
        auto tabs = std::make_unique<glw::MajorTabs>();
        tabs->setOnNewTab([this](glw::MajorTabs& into) { addScript(into); });
        return tabs;
    }

    // In `into`: the first window's tabs, or those of a window a script was dragged out to.
    void addScript(glw::MajorTabs& into) {
        ++scripts;
        const std::size_t index = into.addTab(title(scripts), script(scripts));
        into.setTabId(index, "script-" + std::to_string(scripts));
    }

    // The windows of a first start: two scripts, the application's panels around them.
    void openFirstWindow() {
        std::unique_ptr<glw::MajorTabs> tabs = makeTabs();
        addScript(*tabs);
        addScript(*tabs);
        tabs->setActive(0);
        std::unique_ptr<glw::MajorTabStrip> strip = tabs->makeStrip();
        auto window = std::make_unique<glw::DockArea>(glw::DockScope::Global);
        window->setCenter(std::move(tabs));
        window->addPanel(*globalPanel("files"), nullptr, glw::DockSide::Start, 0.16f);
        window->addPanel(*globalPanel("log"), nullptr, glw::DockSide::Bottom, 0.2f);
        glw::Window& shown = app.openWindow({.title = tr("Docking"), .size = {1280, 800}}, std::move(window));
        shown.frame()->titleBar().setCenter(std::move(strip));
    }
};

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), {});
}

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    std::string language = "en";
    std::string workspace;
    bool light = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--language" && i + 1 < argc)
            language = argv[++i];
        else if (arg == "--workspace" && i + 1 < argc)
            workspace = argv[++i];
        else if (arg == "--light")
            light = true;
    }
    try {
        glw::Application app;
        app.translator().loadCatalog(persianCatalog);
        app.setLanguage(language);
        if (light)
            app.setTheme(glw::Theme::light());

        Studio studio{app};
        bool restored = false;
        if (!workspace.empty()) {
            // the windows as they were, made again from the ids they were saved with; saved again at the end
            if (const std::optional<std::string> saved = readFile(workspace)) {
                const glw::WorkspaceFactory factory{.title = tr("Docking"),
                                                    .tabs = [&studio] { return studio.makeTabs(); },
                                                    .document = [&studio](const std::string& id) { return studio.document(id); },
                                                    .panel = &Studio::globalPanel};
                std::string error;
                restored = glw::restoreWorkspace(app.docking(), *saved, factory, &error);
                if (!restored)
                    std::fprintf(stderr, "docking_demo: %s: %s\n", workspace.c_str(), error.c_str());
            }
            app.setOnQuitting([&app, &workspace] {
                std::ofstream(workspace, std::ios::binary) << glw::saveWorkspace(app.docking());
            });
        }
        if (!restored)
            studio.openFirstWindow();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "docking_demo: %s\n", e.what());
        return 1;
    }
}
