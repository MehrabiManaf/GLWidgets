// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Widgets in an application: a window with chrome of its own (title bar, buttons, borders that resize it, redrawn while it
// is resized), labels in Persian and English, buttons that take the focus (Tab moves it), stacks that stretch, and the
// whole interface mirrored for right to left with one button.
//
//   widgets_demo [--frames N]
//
// --frames N draws N frames and exits: a smoke test.
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/Panel.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <string_view>

namespace {

std::unique_ptr<glw::Widget> stack(glw::Orientation orientation, float spacing) {
    auto w = std::make_unique<glw::Widget>();
    w->setLayout(std::make_unique<glw::StackLayout>(orientation, spacing));
    return w;
}

// A card of a share of a row: its stretch, written on it.
std::unique_ptr<glw::Widget> card(const glw::Theme& theme, float stretch, const std::string& text) {
    auto panel = std::make_unique<glw::Panel>(theme.surface, theme.cornerRadius);
    panel->setBorder(theme.border);
    panel->setStretch(stretch);
    panel->setPadding(glw::Insets::all(12));
    panel->setMinimumSize({0, 90});
    auto& label = panel->add(std::make_unique<glw::Label>(text));
    label.setColor(theme.mutedText);
    label.setAlignment(glw::Alignment::Center, glw::Alignment::Center);
    return panel;
}

std::unique_ptr<glw::Widget> content(glw::Application& app, glw::Window*& window) {
    const glw::Theme& theme = app.context().theme;
    auto page = stack(glw::Orientation::Vertical, 16);
    page->setPadding(glw::Insets::all(28));

    auto& heading = page->add(std::make_unique<glw::Label>("GLWidgets — ویجت‌ها"));
    heading.setFontSize(26);

    // a paragraph in each language: each has its own direction, whatever the interface's
    for (const char* paragraph : {"این پنجره از ویجت‌ها ساخته شده است: نوار عنوان و دکمه‌هایش هم ویجت‌اند. لبه‌های پنجره "
                                  "اندازه‌اش را تغییر می‌دهند و Tab تمرکز را بین دکمه‌ها جابه‌جا می‌کند.",
                                  "This window is made of widgets, its title bar too. Its edges resize it, and Tab moves "
                                  "the focus between the buttons."}) {
        auto& text = page->add(std::make_unique<glw::Label>(paragraph));
        text.setWrap(true);
        text.setColor(theme.mutedText);
    }

    auto buttons = stack(glw::Orientation::Horizontal, 10);
    auto& status = page->add(std::make_unique<glw::Label>("Nothing clicked yet."));
    glw::Label* statusLabel = &status;
    buttons->add(std::make_unique<glw::Button>("Hello", [statusLabel] { statusLabel->setText("Hello was clicked."); }));
    buttons->add(std::make_unique<glw::Button>("سلام", [statusLabel] { statusLabel->setText("روی «سلام» کلیک شد."); }));
    buttons->add(std::make_unique<glw::Button>("Right to left / چپ به راست", [&window] {
        glw::UiRoot& ui = window->ui();
        ui.setLayoutDirection(ui.rightToLeft() ? glw::LayoutDirection::LeftToRight : glw::LayoutDirection::RightToLeft);
    }));
    buttons->add(std::make_unique<glw::Button>("Close", [&window] { window->close(); }));
    // the status line after the buttons
    page->addChild(std::move(buttons), page->children().size() - 1);

    auto row = stack(glw::Orientation::Horizontal, 12);
    row->add(card(theme, 1, "stretch 1"));
    row->add(card(theme, 2, "stretch 2"));
    row->add(card(theme, 1, "stretch 1"));
    page->add(std::move(row));

    // what is left of the height, at the bottom
    auto& spacer = page->add(std::make_unique<glw::Widget>());
    spacer.setStretch(1);
    auto& footer = page->add(std::make_unique<glw::Label>("Escape-free: close it with its button, Alt+F4, or the one above."));
    footer.setColor(theme.mutedText);
    footer.setFontSize(12);
    return page;
}

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
    }
    try {
        glw::Application app;
        glw::Window* window = nullptr;
        window = &app.openWindow({.title = "GLWidgets — Widgets", .size = {980, 620}}, content(app, window));
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "widgets_demo: %s\n", e.what());
        return 1;
    }
}
