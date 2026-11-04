// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// A code editor over a log, with a menu bar (in the title bar, or under it with --menu-below) and a toolbar: their items
// are actions, with SVG icons (monochrome ones in the theme's color, a colored one), shortcuts that work in the window,
// states they share (Undo disabled when there is nothing to undo, Line Numbers checked). C++ is highlighted in a monospace
// font (the system's); the log is read only and is appended to. Persian in strings and comments is shaped and ordered as
// it is written, in lines that stay left to right.
//
//   code_editor [FILE] [--lines N] [--menu-below] [--frames N]
//
// FILE: what to edit (a sample otherwise); --lines N: N more lines, to see that a long text costs what shows.
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/MenuBar.hpp>
#include <GLWidgets/Widgets/Splitter.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using namespace glw;

constexpr const char* sample = R"(// A sample to edit: what the editor shows, and how.
#include <string>
#include <vector>

namespace demo {

/* A comment of
   several lines. */
struct Node {
    std::string name;           // سلام: Persian in a comment, shaped and in its order
    std::vector<int> pins;
    double weight = 0.5e-3;
};

int count(const std::vector<Node>& nodes) {
    int total = 0;
    for (const Node& node : nodes) {
        if (node.name == "خروجی")    // and in a string
            total += static_cast<int>(node.pins.size());
    }
    return total;
}

} // namespace demo
)";

// Monochrome icons, drawn in 24 x 24 with round strokes: shown in the theme's color.
Icon stroked(std::string_view shapes) {
    return Icon::fromSvg("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><g fill=\"none\" stroke=\"#000\" "
                         "stroke-width=\"1.7\" stroke-linecap=\"round\" stroke-linejoin=\"round\">" +
                             std::string(shapes) + "</g></svg>",
                         true);
}

struct Icons {
    Icon document = stroked(R"(<path d="M6 3h8l5 5v13H6z"/><path d="M14 3v5h5"/>)");
    Icon folder = stroked(R"(<path d="M3 6.5A1.5 1.5 0 0 1 4.5 5H9l2 2h8.5A1.5 1.5 0 0 1 21 8.5v9a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 17.5z"/>)");
    Icon save = stroked(R"(<path d="M5 4h11l3 3v13H5z"/><path d="M8 4v5h7V4"/><path d="M8 20v-6h8v6"/>)");
    Icon undo = stroked(R"(<path d="M9 5L4 10l5 5"/><path d="M4 10h10a5 5 0 0 1 0 10h-3"/>)");
    Icon redo = stroked(R"(<path d="M15 5l5 5-5 5"/><path d="M20 10H10a5 5 0 0 0 0 10h3"/>)");
    Icon cut = stroked(R"(<circle cx="6.5" cy="17.5" r="3"/><circle cx="17.5" cy="17.5" r="3"/><path d="M8.6 15.4L18 4"/><path d="M15.4 15.4L6 4"/>)");
    Icon copy = stroked(R"(<rect x="8" y="8" width="12" height="13" rx="2"/><path d="M16 8V5.5A2.5 2.5 0 0 0 13.5 3h-7A2.5 2.5 0 0 0 4 5.5v9A2.5 2.5 0 0 0 6.5 17H8"/>)");
    Icon paste = stroked(R"(<rect x="5" y="5" width="14" height="16" rx="2"/><rect x="9" y="3" width="6" height="4" rx="1"/>)");
    Icon numbers = stroked(R"(<path d="M10 6h10"/><path d="M10 12h10"/><path d="M10 18h10"/><path d="M4 5l2-1v5"/><path d="M4 15h3l-3 4h3"/>)");
    Icon lock = stroked(R"(<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>)");
    // in its own colors
    Icon run = Icon::fromSvg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M7 4.5v15a1 1 0 0 0 1.5.9l12-7.5a1 1 0 0 0 0-1.8l-12-7.5A1 1 0 0 0 7 4.5z" fill="#34d17c"/></svg>)");
};

// The first monospace font the system has.
const text::Font* monospace(text::FontLibrary& fonts) {
    for (const char* path : {"C:/Windows/Fonts/CascadiaMono.ttf", "C:/Windows/Fonts/consola.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
                             "/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Monaco.ttf"}) {
        if (!std::ifstream(path))
            continue;
        try {
            return &fonts.addFontFile(path);
        } catch (const std::exception&) {
            // not a font it can read: the next
        }
    }
    return nullptr;
}

std::shared_ptr<Action> action(const char* text, Icon icon, Shortcut shortcut, std::function<void()> handler) {
    auto a = std::make_shared<Action>(LocalizedText(text), std::move(handler));
    a->setIcon(std::move(icon));
    a->setShortcut(shortcut);
    return a;
}

} // namespace

int main(int argc, char** argv) {
    int frames = -1, extra = 0;
    bool below = false;
    std::string file;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--lines" && i + 1 < argc)
            extra = std::max(0, std::atoi(argv[++i]));
        else if (arg == "--menu-below")
            below = true;
        else
            file = std::string(arg);
    }
    try {
        Application app;
        const text::Font* mono = monospace(app.fonts());
        const Icons icons;

        std::string code = sample;
        if (!file.empty()) {
            std::ifstream in(file, std::ios::binary);
            std::stringstream read;
            read << in.rdbuf();
            code = read.str();
        }
        for (int i = 0; i < extra; ++i)
            code += "int line" + std::to_string(i) + " = " + std::to_string(i) + "; // more\n";

        auto editor = std::make_unique<TextEdit>();
        TextEdit& edit = *editor;
        auto logged = std::make_unique<TextEdit>();
        TextEdit& log = *logged;
        auto info = std::make_unique<Label>(Label::Props{.fontSize = 12, .muted = true});
        Label& status = *info;
        if (mono) {
            edit.setFonts({mono});
            log.setFonts({mono});
        }
        edit.setHighlighter(std::make_unique<CppHighlighter>());
        edit.setText(code);
        log.setReadOnly(true);
        log.setLineNumbers(false);
        log.append(std::string("Opened ") + (file.empty() ? "the sample" : file) + ": " + std::to_string(edit.lineCount()) +
                   " lines" + (mono ? ", in " + std::string(mono->family()) : "") + ".");

        // ---- what can be done: once, for the menus, the toolbar and the shortcuts ----
        using platform::Key;
        constexpr auto ctrl = platform::shortcutModifier;
        Window* window = nullptr;
        auto newFile = action("&New", icons.document, {Key::N, ctrl}, [&] {
            edit.setText("");
            log.append("New file.");
        });
        auto openFile = action("&Open…", icons.folder, {Key::O, ctrl}, [&] { log.append("Open: not in this sample."); });
        auto saveFile = action("&Save", icons.save, {Key::S, ctrl}, [&] {
            log.append("Saved " + std::to_string(edit.lineCount()) + " lines, " + std::to_string(edit.text().size()) + " bytes.");
        });
        auto quit = action("E&xit", {}, {}, [&] { window->close(); });
        auto undo = action("&Undo", icons.undo, {Key::Z, ctrl}, [&] { edit.undo(); });
        auto redo = action("&Redo", icons.redo, {Key::Y, ctrl}, [&] { edit.redo(); });
        auto cut = action("Cu&t", icons.cut, {Key::X, ctrl}, [&] { edit.cut(); });
        auto copy = action("&Copy", icons.copy, {Key::C, ctrl}, [&] { edit.copy(); });
        auto paste = action("&Paste", icons.paste, {Key::V, ctrl}, [&] { edit.paste(); });
        auto selectAll = action("Select &All", {}, {Key::A, ctrl}, [&] { edit.selectAll(); });
        auto numbers = action("&Line Numbers", icons.numbers, {}, [&] {});
        numbers->setCheckable(true);
        numbers->setChecked(true);
        numbers->setOnTriggered([&] { edit.setLineNumbers(numbers->checked()); });
        auto readOnly = action("&Read Only", icons.lock, {}, [&] {});
        readOnly->setCheckable(true);
        readOnly->setOnTriggered([&] { edit.setReadOnly(readOnly->checked()); });
        auto run = action("&Run", icons.run, {Key::F5}, [&] { log.append("Ran it: nothing to run in a sample."); });
        run->setToolTip("Runs the script (F5)");

        const auto showWhere = [&] {
            const TextPosition at = edit.caret();
            status.setText(LocalizedText("Line " + std::to_string(at.line + 1) + ", column " + std::to_string(at.column + 1) + " · " +
                                         std::to_string(edit.lineCount()) + " lines"));
            undo->setEnabled(edit.canUndo());
            redo->setEnabled(edit.canRedo());
        };
        edit.setOnChanged(showWhere);
        showWhere();

        auto menus = std::make_unique<MenuBar>();
        menus->setMenus({
            {.text = "&File", .items = {{.action = newFile}, {.action = openFile}, {.action = saveFile}, MenuItem::separatorItem(), {.action = quit}}},
            {.text = "&Edit",
             .items = {{.action = undo}, {.action = redo}, MenuItem::separatorItem(), {.action = cut}, {.action = copy}, {.action = paste},
                       MenuItem::separatorItem(), {.action = selectAll}}},
            {.text = "&View", .items = {{.action = numbers}, {.action = readOnly}}},
            {.text = "&Run", .items = {{.action = run}}},
        });
        auto tools = std::make_unique<ToolBar>();
        for (const auto& a : {newFile, openFile, saveFile})
            tools->addAction(a);
        tools->addSeparator();
        tools->addAction(undo);
        tools->addAction(redo);
        tools->addSeparator();
        for (const auto& a : {cut, copy, paste})
            tools->addAction(a);
        tools->addSeparator();
        tools->addAction(numbers);
        tools->addAction(readOnly);
        tools->addSeparator();
        tools->addAction(run, true);

        // where the caret is, after the mouse or a key moved it
        struct Watching final : Widget {
            std::function<void()> where;
            void onPreviewMouse(MouseEvent&) override { where(); }
            void onKey(KeyEvent&) override { where(); }
        };
        auto content = std::make_unique<Watching>();
        content->where = showWhere;
        content->setLayout(std::make_unique<StackLayout>(Orientation::Vertical, 0.0f));
        auto split = std::make_unique<Splitter>(Splitter::Props{.orientation = Orientation::Vertical});
        editor->setStretch(3);
        logged->setStretch(1);
        split->add(std::move(editor));
        split->add(std::move(logged));
        split->setStretch(1);
        content->add(std::move(split));
        info->setPadding({12, 6, 12, 6});
        content->add(std::move(info));

        window = &app.openWindow({.title = "Code Editor", .size = {1100, 760}}, std::move(content));
        window->setMenuBar(std::move(menus), below ? MenuBarPlacement::BelowTitleBar : MenuBarPlacement::TitleBar);
        window->setToolBar(std::move(tools));
        edit.focus();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "code_editor: %s\n", e.what());
        return 1;
    }
}
