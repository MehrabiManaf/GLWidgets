// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Widgets in an application: a window with chrome of its own (title bar, buttons, borders that resize it, redrawn while it
// is resized), labels in Persian and English, buttons that take the focus (Tab moves it), stacks that stretch, and the
// whole interface mirrored for right to left with one button. The interface is written with the builders of
// Widgets/Declarative.hpp.
//
//   widgets_demo [--frames N]
//
// --frames N draws N frames and exits: a smoke test.
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace {

namespace ui = glw::ui;

constexpr const char* persian =
    "این پنجره از ویجت‌ها ساخته شده است: نوار عنوان و دکمه‌هایش هم ویجت‌اند. لبه‌های پنجره اندازه‌اش را تغییر می‌دهند و "
    "Tab تمرکز را بین دکمه‌ها جابه‌جا می‌کند.";
constexpr const char* english =
    "This window is made of widgets, its title bar too. Its edges resize it, and Tab moves the focus between the buttons.";

// A card taking `stretch` shares of its row, saying so.
ui::Element<glw::Panel> card(const glw::Theme& theme, float stretch, std::string text) {
    return ui::Panel({.background = theme.surface, .border = theme.border, .cornerRadius = theme.cornerRadius},
                     ui::Label({.text = std::move(text), .color = theme.mutedText}).alignment(glw::Alignment::Center, glw::Alignment::Center))
        .stretch(stretch)
        .padding(glw::Insets::all(12))
        .minimumSize({0, 90});
}

// The window's interface, and what its handlers change: bound pointers live as long as the handlers that use them, so they
// are members, and the handlers capture `this`.
struct Page {
    glw::Window* window = nullptr;
    glw::Label* status = nullptr;

    std::unique_ptr<glw::Widget> build(const glw::Theme& theme) {
        return ui::VStack({.spacing = 16},
                   ui::Label({.text = "GLWidgets — ویجت‌ها", .fontSize = 26}),
                   // a paragraph in each language: each has its own direction, whatever the interface's
                   ui::Label({.text = persian, .color = theme.mutedText, .wrap = true}),
                   ui::Label({.text = english, .color = theme.mutedText, .wrap = true}),
                   ui::HStack({.spacing = 10},
                              ui::Button({.text = "Hello", .onClick = [this] { status->setText("Hello was clicked."); }}),
                              ui::Button({.text = "سلام", .onClick = [this] { status->setText("روی «سلام» کلیک شد."); }}),
                              ui::Button({.text = "Right to left / چپ به راست", .onClick = [this] { mirror(); }}),
                              ui::Button({.text = "Close", .onClick = [this] { window->close(); }})),
                   ui::Label({.text = "Nothing clicked yet."}).bind(status),
                   ui::HStack({.spacing = 12}, card(theme, 1, "stretch 1"), card(theme, 2, "stretch 2"), card(theme, 1, "stretch 1")),
                   ui::Spacer(),
                   ui::Label({.text = "Close it with its button, Alt+F4, or the one above.", .fontSize = 12, .color = theme.mutedText}))
            .padding(glw::Insets::all(28));
    }

    void mirror() {
        glw::UiRoot& root = window->ui();
        root.setLayoutDirection(root.rightToLeft() ? glw::LayoutDirection::LeftToRight : glw::LayoutDirection::RightToLeft);
    }
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
    }
    try {
        glw::Application app;
        Page page;
        page.window = &app.openWindow({.title = "GLWidgets — Widgets", .size = {980, 620}}, page.build(app.context().theme));
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "widgets_demo: %s\n", e.what());
        return 1;
    }
}
