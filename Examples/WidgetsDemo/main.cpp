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
#include <GLWidgets/Widgets/Dialog.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Widgets/TableView.hpp>

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
    "Close it with its button, Alt+F4, or the one above.": "با دکمه‌اش، Alt+F4 یا دکمه‌ی بالا ببندیدش.",
    "A million rows of fifty columns. Drag a header's edge to resize its column; Shift and the wheel scroll across.":
      "یک میلیون سطر در پنجاه ستون. لبه‌ی سرستونی را بکشید تا ستونش اندازه شود؛ Shift و چرخ ماوس به پهلو می‌برند.",
    "Column {n}": "ستون {n}",
    "Menu": "منو",
    "New": "جدید",
    "Open…": "باز کردن…",
    "Recent": "اخیر",
    "Disabled": "غیرفعال",
    "Size": "اندازه",
    "Small": "کوچک",
    "Medium": "متوسط",
    "Large": "بزرگ",
    "{item} chosen.": "«{item}» انتخاب شد.",
    "Dialog…": "پنجره‌ی گفتگو…",
    "New script": "اسکریپت جدید",
    "Kind": "نوع",
    "Script": "اسکریپت",
    "Function": "تابع",
    "Macro": "ماکرو",
    "Create": "ساختن",
    "Cancel": "لغو",
    "Script name": "نام اسکریپت",
    "{name} made.": "«{name}» ساخته شد.",
    "Close the window?": "پنجره بسته شود؟",
    "What is not saved is lost.": "آنچه ذخیره نشده از دست می‌رود.",
    "Every widget, in the other theme's colors": "همه‌ی ویجت‌ها، با رنگ‌های پوسته‌ی دیگر",
    "Asks first": "اول می‌پرسد",
    "A modal dialog: the window waits for it": "یک گفتگوی مودال: پنجره منتظرش می‌ماند"
  }
})";

// A million items, made when they show: the model holds their number only.
class Million final : public glw::ListModel {
public:
    std::size_t count() const override { return 1'000'000; }
};

// A million rows of fifty columns, made when they show.
class Records final : public glw::TableModel {
public:
    std::size_t rowCount() const override { return 1'000'000; }
    std::size_t columnCount() const override { return 50; }
    glw::LocalizedText header(std::size_t column) const override { return tr("Column {n}").arg("n", std::to_string(column + 1)); }
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
    glw::Button* menuButton = nullptr;
    bool light = false;

    // The form on one side; on the other a list of a million items over a table of a million rows; handles between them.
    std::unique_ptr<glw::Widget> build() {
        std::unique_ptr<glw::Widget> items = list();
        items->setStretch(1);
        items->setMinimumSize({0, 120});
        std::unique_ptr<glw::Widget> rows = table();
        rows->setStretch(1);
        rows->setMinimumSize({0, 120});
        return ui::Splitter({}, ui::ScrollArea({}, form()).stretch(3).minimumSize({280, 0}),
                            ui::Splitter({.orientation = glw::Orientation::Vertical}, std::move(items), std::move(rows))
                                .stretch(2)
                                .minimumSize({160, 0}));
    }

    std::unique_ptr<glw::Widget> table() {
        auto view = std::make_unique<glw::TableView>();
        view->setModel(std::make_shared<Records>());
        view->setDelegate(std::make_unique<glw::TableTextDelegate>([](std::size_t row, std::size_t column) {
            return glw::LocalizedText(std::to_string(row + 1) + " · " + std::to_string(column + 1));
        }));
        view->setColumnWidth(0, 100);
        view->setStretch(1);
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("A million rows of fifty columns. Drag a header's edge to resize its column; Shift and the "
                                                "wheel scroll across."),
                                     .muted = true,
                                     .wrap = true}),
                          std::move(view))
            .padding(glw::Insets::all(16));
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
                              ui::Button({.text = tr("Light theme"), .onClick = [this] { switchTheme(); }})
                                  .bind(themeButton)
                                  .toolTip(tr("Every widget, in the other theme's colors")),
                              ui::Button({.text = tr("Close"), .onClick = [this] { confirmClose(); }}).toolTip(tr("Asks first")))
                       .bind(buttons),
                   ui::HStack({.spacing = 10},
                              ui::Button({.text = tr("Menu"), .onClick = [this] { showDemoMenu(); }}).bind(menuButton),
                              ui::Button({.text = tr("Dialog…"), .onClick = [this] { showDemoDialog(); }})
                                  .toolTip(tr("A modal dialog: the window waits for it")),
                              ui::Label({.text = tr("Size")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Small"), tr("Medium"), tr("Large")},
                                            .current = 1,
                                            .onChanged = [this](std::size_t i) { chosen(i == 0 ? tr("Small") : i == 1 ? tr("Medium") : tr("Large")); }})),
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

    // A menu under its button: shortcuts, a submenu, a check mark, a disabled item.
    void showDemoMenu() {
        const glw::Rect anchor{0, 0, menuButton->size().width, menuButton->size().height};
        glw::showMenu(*menuButton, anchor,
                      {{.text = tr("New"), .onTriggered = [this] { chosen(tr("New")); }, .shortcut = "Ctrl+N"},
                       {.text = tr("Open…"), .onTriggered = [this] { chosen(tr("Open…")); }, .shortcut = "Ctrl+O"},
                       {.text = tr("Recent"),
                        .items = {{.text = "fizzbuzz.flow", .onTriggered = [this] { chosen("fizzbuzz.flow"); }},
                                  {.text = "inventory.flow", .onTriggered = [this] { chosen("inventory.flow"); }}}},
                       glw::MenuItem::separatorItem(),
                       {.text = tr("Show the cards"), .onTriggered = [this] { cards->setVisible(!cards->visible()); }, .checked = cards->visible()},
                       {.text = tr("Disabled"), .enabled = false},
                       glw::MenuItem::separatorItem(),
                       {.text = tr("Close"), .onTriggered = [this] { window->close(); }, .shortcut = "Alt+F4"}});
    }

    // A dialog asking before the window closes.
    void confirmClose() {
        glw::showMessage(*buttons, tr("Close the window?"), tr("What is not saved is lost."), {tr("Close"), tr("Cancel")},
                         [this](std::optional<std::size_t> button) {
                             if (button == 0u)
                                 window->close();
                         });
    }

    // A dialog with a form: a field (the focus starts there; Enter makes it) and a combo box (its menu over the dialog).
    void showDemoDialog() {
        auto name = std::make_shared<std::string>();
        auto kind = std::make_shared<std::size_t>(0);
        glw::LineEdit* field = nullptr;
        auto form = ui::VStack({.spacing = 10},
                               ui::Label({.text = tr("Script name"), .muted = true}),
                               ui::LineEdit({.placeholder = tr("New script"), .onChanged = [name](const std::string& text) { *name = text; }})
                                   .bind(field)
                                   .minimumSize({320, 0}),
                               ui::HStack({.spacing = 10},
                                          ui::Label({.text = tr("Kind")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                          ui::ComboBox({.items = {tr("Script"), tr("Function"), tr("Macro")},
                                                        .onChanged = [kind](std::size_t i) { *kind = i; }})));
        glw::showDialog(*buttons, {.title = tr("New script"),
                                   .content = std::move(form),
                                   .buttons = {tr("Create"), tr("Cancel")},
                                   .onClosed = [this, name](std::optional<std::size_t> button) {
                                       if (button == 0u)
                                           status->setText(tr("{name} made.").arg("name", name->empty() ? std::string("…") : *name));
                                   }});
    }

    void chosen(const glw::LocalizedText& item) {
        status->setText(tr("{item} chosen.").arg("item", app.translator().resolve(item)));
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
