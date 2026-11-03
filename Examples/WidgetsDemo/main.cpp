// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Widgets in an application: a window with chrome of its own (title bar, buttons, borders that resize it, redrawn while it
// is resized), a text field, buttons that take the focus (Tab moves it), cards that stretch. Its texts are translated: the
// language button shows them in Persian, which also mirrors the window; the theme button changes the colors of everything.
// The interface is written with the builders of Widgets/Declarative.hpp. Its other side is a list of a million items: only
// those that show are widgets.
//
//   widgets_demo [--frames N] [--language fa] [--light]
//
// --frames N draws N frames and exits: a smoke test.
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/ListView.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace {

namespace ui = glw::ui;
using glw::tr;

// The Persian catalog of this example. An application loads its catalogs from files (Translator::loadCatalogFile).
constexpr const char* persianCatalog = R"({
  "language": "fa",
  "texts": {
    "Widgets": "ویجت‌ها",
    "GLWidgets — Widgets": "GLWidgets — ویجت‌ها",
    "This window is made of widgets, its title bar too. Its edges resize it, and Tab moves the focus between the buttons and the field.":
      "این پنجره از ویجت‌ها ساخته شده است، نوار عنوانش هم. لبه‌هایش اندازه‌اش را تغییر می‌دهند و Tab تمرکز را بین دکمه‌ها و فیلد جابه‌جا می‌کند.",
    "Name": "نام",
    "Type your name": "نامتان را بنویسید",
    "Hello, {name}!": "سلام، {name}!",
    "Nothing typed yet.": "هنوز چیزی نوشته نشده.",
    "فارسی": "English",
    "Light theme": "پوسته‌ی روشن",
    "Dark theme": "پوسته‌ی تیره",
    "Close": "بستن",
    "a share": "یک سهم",
    "two shares": "دو سهم",
    "Show the cards": "کارت‌ها دیده شوند",
    "Enable the buttons": "دکمه‌ها فعال باشند",
    "Item {n}": "مورد {n}",
    "Drag the line between the two sides.": "خط میان دو طرف را بکشید.",
    "A million items, and only those that show are widgets. Drag the line between the two sides.":
      "یک میلیون مورد، و فقط آن‌هایی که دیده می‌شوند ویجت‌اند. خط میان دو طرف را بکشید.",
    "Close it with its button, Alt+F4, or the one above.": "با دکمه‌اش، Alt+F4 یا دکمه‌ی بالا ببندیدش."
  }
})";

// A million items, made when they show: the model holds their number only.
class Million final : public glw::ListModel {
public:
    std::size_t count() const override { return 1'000'000; }
};

// The window's interface, and what its handlers change: bound pointers live as long as the handlers that use them, so they
// are members, and the handlers capture `this`.
struct Page {
    glw::Application& app;
    glw::Window* window = nullptr;
    glw::Label* status = nullptr;
    glw::Button* themeButton = nullptr;
    glw::Widget* buttons = nullptr;
    glw::Widget* cards = nullptr;
    bool light = false;

    // The form on one side, a list of a million items on the other, a handle between them; each side scrolls.
    std::unique_ptr<glw::Widget> build() {
        std::unique_ptr<glw::Widget> items = list();
        items->setStretch(2);
        items->setMinimumSize({160, 0});
        return ui::Splitter({}, ui::ScrollArea({}, form()).stretch(3).minimumSize({280, 0}), std::move(items));
    }

    std::unique_ptr<glw::Widget> list() {
        auto view = std::make_unique<glw::ListView>();
        view->setModel(std::make_shared<Million>());
        view->setDelegate(std::make_unique<glw::TextDelegate>([](std::size_t i) { return tr("Item {n}").arg("n", std::to_string(i + 1)); }));
        view->setStretch(1);
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("A million items, and only those that show are widgets. Drag the line between the two sides."),
                                     .muted = true,
                                     .wrap = true}),
                          std::move(view))
            .padding(glw::Insets::all(16));
    }

    std::unique_ptr<glw::Widget> form() {
        return ui::VStack({.spacing = 16},
                   ui::Label({.text = tr("GLWidgets — Widgets"), .fontSize = 26}),
                   ui::Label({.text = tr("This window is made of widgets, its title bar too. Its edges resize it, and Tab moves the "
                                         "focus between the buttons and the field."),
                              .muted = true,
                              .wrap = true}),
                   ui::HStack({.spacing = 10},
                              ui::Label({.text = tr("Name")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::LineEdit({.placeholder = tr("Type your name"), .onChanged = [this](const std::string& name) { greet(name); }})
                                  .stretch(1)),
                   ui::Label({.text = tr("Nothing typed yet.")}).bind(status),
                   ui::CheckBox({.text = tr("Show the cards"), .checked = true, .onToggled = [this](bool on) { cards->setVisible(on); }}),
                   ui::CheckBox({.text = tr("Enable the buttons"), .checked = true, .onToggled = [this](bool on) { buttons->setEnabled(on); }}),
                   ui::HStack({.spacing = 10},
                              // in the other language: the button says which one it goes to
                              ui::Button({.text = tr("فارسی"), .onClick = [this] { switchLanguage(); }}),
                              ui::Button({.text = tr("Light theme"), .onClick = [this] { switchTheme(); }}).bind(themeButton),
                              ui::Button({.text = tr("Close"), .onClick = [this] { window->close(); }}))
                       .bind(buttons),
                   ui::HStack({.spacing = 12}, card(1, tr("a share")), card(2, tr("two shares")), card(1, tr("a share"))).bind(cards),
                   ui::Label({.text = tr("Close it with its button, Alt+F4, or the one above."), .fontSize = 12, .muted = true}))
            .padding(glw::Insets::all(28));
    }

    static ui::Element<glw::Card> card(float stretch, glw::LocalizedText text) {
        return ui::Card(ui::Label({.text = std::move(text), .muted = true}).alignment(glw::Alignment::Center, glw::Alignment::Center))
            .stretch(stretch)
            .padding(glw::Insets::all(12))
            .minimumSize({0, 90});
    }

    void greet(const std::string& name) {
        status->setText(name.empty() ? tr("Nothing typed yet.") : tr("Hello, {name}!").arg("name", name));
    }

    void switchLanguage() {
        app.setLanguage(app.translator().language() == "fa" ? "en" : "fa");
    }

    void switchTheme() {
        light = !light;
        app.setTheme(light ? glw::Theme::light() : glw::Theme::dark());
        themeButton->setText(light ? tr("Dark theme") : tr("Light theme"));
    }
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    std::string language = "en";
    bool light = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--language" && i + 1 < argc)
            language = argv[++i];
        else if (arg == "--light")
            light = true;
    }
    try {
        glw::Application app;
        std::string error;
        if (!app.translator().loadCatalog(persianCatalog, &error))
            std::fprintf(stderr, "widgets_demo: the Persian catalog: %s\n", error.c_str());
        app.setLanguage(language);
        Page page{app};
        page.window = &app.openWindow({.title = tr("Widgets"), .size = {980, 640}}, page.build());
        if (light)
            page.switchTheme();
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "widgets_demo: %s\n", e.what());
        return 1;
    }
}
