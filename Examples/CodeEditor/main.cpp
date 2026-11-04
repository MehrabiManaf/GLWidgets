// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// A code editor over a log: C++ highlighted in a monospace font (the system's), line numbers, the caret's line and column
// below; the log is read only and is appended to (Ctrl+S writes there what would be saved). Persian in strings and comments
// is shaped and ordered as it is written, in lines that stay left to right.
//
//   code_editor [FILE] [--lines N] [--frames N]
//
// FILE: what to edit (a sample otherwise); --lines N: N more lines, to see that a long text costs what shows.
#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Splitter.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>

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

} // namespace

int main(int argc, char** argv) {
    int frames = -1, extra = 0;
    std::string file;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--lines" && i + 1 < argc)
            extra = std::max(0, std::atoi(argv[++i]));
        else
            file = std::string(arg);
    }
    try {
        Application app;
        const text::Font* mono = monospace(app.fonts());

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

        const auto showWhere = [&edit, &status] {
            const TextPosition at = edit.caret();
            status.setText(LocalizedText("Line " + std::to_string(at.line + 1) + ", column " + std::to_string(at.column + 1) +
                                         " · " + std::to_string(edit.lineCount()) + " lines · Ctrl+S writes to the log"));
        };
        edit.setOnChanged(showWhere);
        showWhere();

        // what Ctrl+S would save, said in the log
        struct Saving final : Widget {
            TextEdit* edit = nullptr;
            TextEdit* log = nullptr;
            std::function<void()> where;
            void onPreviewKey(KeyEvent& event) override {
                if (event.pressed && event.key == platform::Key::S && platform::hasAny(event.modifiers, platform::shortcutModifier)) {
                    log->append("Saved " + std::to_string(edit->lineCount()) + " lines, " + std::to_string(edit->text().size()) + " bytes.");
                    event.handled = true;
                }
            }
            void onPreviewMouse(MouseEvent&) override { where(); }
            void onKey(KeyEvent&) override { where(); }
        };
        auto content = std::make_unique<Saving>();
        content->edit = &edit;
        content->log = &log;
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

        app.openWindow({.title = "Code Editor", .size = {1100, 760}}, std::move(content));
        edit.focus();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "code_editor: %s\n", e.what());
        return 1;
    }
}
