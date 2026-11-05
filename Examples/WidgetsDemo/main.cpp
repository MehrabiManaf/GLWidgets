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
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>

#include <algorithm>
#include <compare>
#include <unordered_map>
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
    "A million items, and only those that show are widgets; Ctrl and Shift select several. Drag the line between the two sides.":
      "یک میلیون مورد، و فقط آن‌هایی که دیده می‌شوند ویجت‌اند؛ با Ctrl و Shift چند تا را انتخاب کنید. خط میان دو طرف را بکشید.",
    "Close it with its button, Alt+F4, or the one above.": "با دکمه‌اش، Alt+F4 یا دکمه‌ی بالا ببندیدش.",
    "A million rows of fifty columns. Click a header to sort by it; double click a cell (or F2, or type) to edit it; drag a header's edge to resize its column.":
      "یک میلیون سطر در پنجاه ستون. روی سرستونی کلیک کنید تا بر اساس آن مرتب شود؛ روی خانه‌ای دوبار کلیک کنید (یا F2، یا تایپ) تا ویرایش شود؛ لبه‌ی سرستون را بکشید تا ستونش اندازه شود.",
    "Rows whose number starts with…": "سطرهایی که شماره‌شان با این شروع می‌شود…",
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
    "Appearance": "ظاهر",
    "Variables": "متغیرها",
    "Advanced": "پیشرفته",
    "Show the grid": "شبکه دیده شود",
    "Snap to the grid": "چسبیدن به شبکه",
    "Adds a variable": "یک متغیر اضافه می‌کند",
    "Variable {n}": "متغیر {n}",
    "Its value": "مقدارش",
    "Undo steps": "گام‌های واگرد",
    "Click a header to open or close it; + adds a variable.": "روی سرتیتری کلیک کنید تا باز یا بسته شود؛ + یک متغیر اضافه می‌کند.",
    "A modal dialog: the window waits for it": "یک گفتگوی مودال: پنجره منتظرش می‌ماند",
    "Folder {n}": "پوشه‌ی {n}",
    "File {n}": "پرونده‌ی {n}",
    "A thousand folders of a thousand files, all open: a million rows. Open and close them; click a header to sort each folder's files by it.":
      "هزار پوشه با هزار پرونده در هر کدام، همه باز: یک میلیون سطر. بازشان کنید و ببندید؛ روی سرستونی کلیک کنید تا پرونده‌های هر پوشه بر اساس آن مرتب شوند.",
    "Size": "اندازه",
    "Modified": "تغییر",
    "Items": "موردها"
  }
})";

// A million items, made when they show: the model holds their number only.
class Million final : public glw::ListModel {
public:
    std::size_t count() const override { return 1'000'000; }
    std::partial_ordering compare(std::size_t a, std::size_t b) const override { return a <=> b; }
};

// A million rows of fifty columns of numbers, made when they show (only those edited are kept): sorted by any column,
// edited in place.
class Records final : public glw::TableModel {
public:
    std::size_t rowCount() const override { return 1'000'000; }
    std::size_t columnCount() const override { return 50; }
    glw::LocalizedText header(std::size_t column) const override { return tr("Column {n}").arg("n", std::to_string(column + 1)); }
    glw::ItemFlags flags(std::size_t, std::size_t) const override { return glw::ItemFlags::Editable; }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(std::size_t a, std::size_t b, std::size_t column) const override {
        return value(a, column) <=> value(b, column);
    }

    // The first column numbers the rows; the others, numbers that look random.
    double value(std::size_t row, std::size_t column) const {
        if (const auto it = edited_.find(key(row, column)); it != edited_.end())
            return it->second;
        if (column == 0)
            return static_cast<double>(row + 1);
        return static_cast<double>((row * 7919 + column * 104729) % 100000) / 100;
    }
    std::string text(std::size_t row, std::size_t column) const {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, column == 0 ? "%.0f" : "%.2f", value(row, column));
        return buffer;
    }
    void set(std::size_t row, std::size_t column, const std::string& text) {
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (end == text.c_str())
            return;             // not a number: as it was
        edited_[key(row, column)] = v;
        notifyRowsChanged(row, 1);
    }

private:
    static std::uint64_t key(std::size_t row, std::size_t column) { return static_cast<std::uint64_t>(row) * 64 + column; }
    std::unordered_map<std::uint64_t, double> edited_;
};

// A thousand folders of a thousand files, made when they show: a node's id says which it is.
class Folders final : public glw::TreeModel {
public:
    static constexpr std::uint64_t count = 1000;
    std::size_t childCount(std::optional<glw::TreeNode> parent) const override {
        return !parent ? count : parent->id < count ? count : 0;
    }
    glw::TreeNode child(std::optional<glw::TreeNode> parent, std::size_t index) const override {
        return {!parent ? index : count + parent->id * count + index};
    }
    bool hasChildren(glw::TreeNode node) const override { return node.id < count; }
    // its columns: the name, the size, when it was changed; sorted by any of them
    std::size_t columnCount() const override { return 3; }
    glw::LocalizedText header(std::size_t column) const override {
        return column == 0 ? tr("Name") : column == 1 ? tr("Size") : tr("Modified");
    }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(glw::TreeNode a, glw::TreeNode b, std::size_t column) const override {
        if (column == 1)
            return size(a) <=> size(b);
        if (column == 2)
            return day(a) <=> day(b);
        return a.id <=> b.id;
    }
    // what a file holds, and the day it was changed: numbers that look random (a folder: its files')
    static std::uint64_t size(glw::TreeNode node) { return node.id < count ? 1000 * 500 : (node.id * 7919) % 1000 + 1; }
    static std::uint64_t day(glw::TreeNode node) { return (node.id * 104729) % 3650; }
    static glw::LocalizedText text(glw::TreeNode node, std::size_t column) {
        if (column == 1)
            return glw::LocalizedText(std::to_string(size(node)) + " KB");
        if (column == 2) {
            const std::uint64_t d = day(node);
            return glw::LocalizedText(std::to_string(2016 + d / 365) + "-" + std::to_string(d % 365 / 31 + 1) + "-" + std::to_string(d % 31 + 1));
        }
        if (node.id < count)
            return tr("Folder {n}").arg("n", std::to_string(node.id + 1));
        return tr("File {n}").arg("n", std::to_string((node.id - count) / count + 1) + "." + std::to_string((node.id - count) % count + 1));
    }
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
    int variableCount = 0;
    bool light = false;

    // The form on one side; on the other a list of a million items, a tree of a million rows and a table of a million rows;
    // handles between them.
    std::unique_ptr<glw::Widget> build() {
        std::unique_ptr<glw::Widget> items = list();
        items->setStretch(1);
        items->setMinimumSize({0, 120});
        std::unique_ptr<glw::Widget> nodes = tree();
        nodes->setStretch(1);
        nodes->setMinimumSize({0, 120});
        std::unique_ptr<glw::Widget> rows = table();
        rows->setStretch(1);
        rows->setMinimumSize({0, 120});
        return ui::Splitter({}, ui::ScrollArea({}, form()).stretch(3).minimumSize({280, 0}),
                            ui::Splitter({.orientation = glw::Orientation::Vertical}, std::move(items), std::move(nodes), std::move(rows))
                                .stretch(2)
                                .minimumSize({160, 0}));
    }

    std::unique_ptr<glw::Widget> table() {
        auto records = std::make_shared<Records>();
        auto view = std::make_unique<glw::TableView>();
        glw::TableView* shown = view.get();
        view->setModel(records);
        auto delegate = std::make_unique<glw::TableTextDelegate>([r = records.get()](std::size_t row, std::size_t column) {
            return glw::LocalizedText(r->text(row, column));
        });
        delegate->setOnEdited([r = records.get()](std::size_t row, std::size_t column, std::string text) { r->set(row, column, text); });
        view->setDelegate(std::move(delegate));
        view->setColumnWidth(0, 100);
        view->setStretch(1);
        // the rows whose number starts with what is typed
        auto filter = ui::LineEdit({.placeholder = tr("Rows whose number starts with…"),
                                    .onChanged = [shown, r = records.get()](const std::string& text) {
                                        if (text.empty())
                                            shown->setFilter({});
                                        else
                                            shown->setFilter([r, text](std::size_t row) { return r->text(row, 0).starts_with(text); });
                                    }});
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("A million rows of fifty columns. Click a header to sort by it; double click a cell (or F2, "
                                                "or type) to edit it; drag a header's edge to resize its column."),
                                     .muted = true,
                                     .wrap = true}),
                          std::move(filter), std::move(view))
            .padding(glw::Insets::all(16));
    }

    // A million rows, eased as its folders open and close (each pushing the hundreds of thousands after it).
    std::unique_ptr<glw::Widget> tree() {
        auto view = std::make_unique<glw::TreeView>();
        view->setModel(std::make_shared<Folders>());
        view->setDelegate(std::make_unique<glw::TreeTextDelegate>(&Folders::text));
        view->setColumnWidth(0, 220);
        view->setColumnWidth(1, 110);
        view->expandAll();
        view->setStretch(1);
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("A thousand folders of a thousand files, all open: a million rows. Open and close them; click a "
                                                "header to sort each folder's files by it."),
                                     .muted = true,
                                     .wrap = true}),
                          std::move(view))
            .padding(glw::Insets::all(16));
    }

    std::unique_ptr<glw::Widget> list() {
        auto view = std::make_unique<glw::ListView>();
        view->setModel(std::make_shared<Million>());
        view->setDelegate(std::make_unique<glw::TextDelegate>([](std::size_t i) { return tr("Item {n}").arg("n", std::to_string(i + 1)); }));
        view->setHeader(tr("Items"));           // a click sorts them
        view->setSelectionMode(glw::SelectionMode::Multiple);
        view->setStretch(1);
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("A million items, and only those that show are widgets; Ctrl and Shift select several. "
                                                "Drag the line between the two sides."),
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
                   ui::Label({.text = tr("Click a header to open or close it; + adds a variable."), .muted = true, .wrap = true}),
                   panels(),
                   ui::Label({.text = tr("Close it with its button, Alt+F4, or the one above."), .fontSize = 12, .muted = true}))
            .padding(glw::Insets::all(28));
    }

    // Panels that open and close, as a details panel has: the second adds a row by the + in its header.
    std::unique_ptr<glw::Widget> panels() {
        auto variables = std::make_unique<glw::CollapsiblePanel>(glw::CollapsiblePanel::Props{.title = tr("Variables"), .expanded = true});
        glw::CollapsiblePanel* list = variables.get();
        auto add = std::make_shared<glw::Action>(tr("Adds a variable"), [this, list] {
            ++variableCount;
            list->addContent(ui::HStack({.spacing = 10},
                                        ui::Label({.text = tr("Variable {n}").arg("n", std::to_string(variableCount))})
                                            .alignment(glw::Alignment::Start, glw::Alignment::Center)
                                            .minimumSize({110, 0}),
                                        ui::LineEdit({.placeholder = tr("Its value")}).stretch(1))
                                 .build());
            list->setExpanded(true);
        });
        add->setIcon(glw::Icon::fromPainter([](glw::paint::Painter& p, const glw::Rect& r, glw::Color c) {
            const glw::Point m = r.center();
            p.drawLine({m.x - r.width * 0.32f, m.y}, {m.x + r.width * 0.32f, m.y}, 1.6f, c);
            p.drawLine({m.x, m.y - r.height * 0.32f}, {m.x, m.y + r.height * 0.32f}, 1.6f, c);
        }));
        variables->addAction(add);
        for (int i = 0; i < 2; ++i)
            add->trigger();
        return ui::VStack({.spacing = 8},
                          ui::CollapsiblePanel({.title = tr("Appearance"), .expanded = true},
                                               ui::CheckBox({.text = tr("Show the grid"), .checked = true}),
                                               ui::CheckBox({.text = tr("Snap to the grid")}),
                                               ui::HStack({.spacing = 10},
                                                          ui::Label({.text = tr("Size")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                                          ui::ComboBox({.items = {tr("Small"), tr("Medium"), tr("Large")}, .current = 1}))),
                          std::move(variables),
                          ui::CollapsiblePanel({.title = tr("Advanced")},
                                               ui::HStack({.spacing = 10},
                                                          ui::Label({.text = tr("Undo steps")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                                                          ui::LineEdit({.text = "100"}).stretch(1))));
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

    // Eased, by its button; at once at the start (--light).
    void switchTheme(bool animated = true) {
        light = !light;
        app.setTheme(light ? glw::Theme::light() : glw::Theme::dark(), animated);
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
            page.switchTheme(false);
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "widgets_demo: %s\n", e.what());
        return 1;
    }
}
