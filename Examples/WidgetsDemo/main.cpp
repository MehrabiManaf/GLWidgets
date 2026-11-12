// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Widgets in an application: a window with chrome of its own (title bar, buttons, borders that resize it, redrawn while it
// is resized), a text field, buttons that take the focus (Tab moves it), cards that stretch. Its texts are translated: the
// language button shows them in Persian, which also mirrors the window; the style and the colors are chosen in combo boxes,
// and every widget changes, eased.
// The interface is written with the builders of Widgets/Declarative.hpp. Its other side is a list of a million items: only
// those that show are widgets.
//
// Its tabs show the newer controls: a form in a grid (spin boxes, a combo box searched by typing, fields that take only
// some texts), colors dragged onto boxes (into another window too), the system's file dialogs with what was chosen kept
// in the user's preferences (Settings). A status bar along the bottom says what happened; the window asks before it
// closes; its colors may follow the system's, and its layout stay left to right in Persian.
//
//   widgets_demo [--frames N] [--language fa] [--style studio|fluid|plain|plastic|metal|solid|glass]
//                [--colors dark|light|dusk|sand|turquoise] [--light]
//
// --light: the same as --colors light.
//
// --frames N draws N frames and exits: a smoke test.
#include <GLWidgets/Widgets/Application.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/Dialog.hpp>
#include <GLWidgets/Widgets/DragDrop.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/Menu.hpp>
#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Platform/Platform.hpp>
#include <GLWidgets/Widgets/Settings.hpp>
#include <GLWidgets/Widgets/StatusBar.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <compare>
#include <functional>
#include <memory>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    "Style": "سبک",
    "Colors": "رنگ‌ها",
    "Dark": "تیره",
    "Light": "روشن",
    "Dusk": "غروب",
    "Sand": "ماسه",
    "Turquoise": "فیروزه",
    "Every widget, in these colors": "همه‌ی ویجت‌ها، با این رنگ‌ها",
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
    "Items": "موردها",
    "Ready": "آماده",
    "Form": "فرم",
    "Drag and drop": "کشیدن و رها کردن",
    "Files": "پرونده‌ها",
    "A form in a grid: its labels in one column, its fields in the other.": "فرمی در یک جدول: برچسب‌ها در یک ستون، فیلدها در ستون دیگر.",
    "Count": "تعداد",
    "Zoom": "بزرگ‌نمایی",
    "Temperature": "دما",
    "Node": "گره",
    "Search the nodes…": "جستجوی گره‌ها…",
    "Identifier": "شناسه",
    "Letters, digits and _, not first a digit": "حرف، رقم و _، نه رقم در اول",
    "Year": "سال",
    "Four digits": "چهار رقم",
    "The arrows, Up and Down, Page Up and Page Down, the wheel; Persian digits are taken too. Type into the node box to search it.":
      "فلش‌ها، بالا و پایین، Page Up و Page Down، چرخ ماوس؛ رقم‌های فارسی هم پذیرفته می‌شوند. در جعبه‌ی گره تایپ کنید تا جستجو شود.",
    "Count: {n}": "تعداد: {n}",
    "{name} is {value}.": "{name}: {value}",
    "Red": "قرمز",
    "Green": "سبز",
    "Blue": "آبی",
    "Drag a color onto a box, here or in the other window; Escape cancels.": "رنگی را روی یک جعبه بکشید، اینجا یا در پنجره‌ی دیگر؛ Escape لغوش می‌کند.",
    "Drop a color here": "رنگی را اینجا رها کنید",
    "{color} dropped here": "{color} اینجا رها شد",
    "{color} dropped.": "{color} رها شد.",
    "Another window…": "پنجره‌ای دیگر…",
    "Drop here": "اینجا رها کنید",
    "Open files…": "باز کردن پرونده‌ها…",
    "Save as…": "ذخیره با نام…",
    "Choose a folder…": "انتخاب پوشه…",
    "The system's own dialogs. What was chosen last is kept in the preferences, for the next time.":
      "پنجره‌های خود سیستم. آخرین انتخاب در تنظیمات نگه داشته می‌شود، برای دفعه‌ی بعد.",
    "Nothing chosen yet.": "هنوز چیزی انتخاب نشده.",
    "Last time: {path}": "دفعه‌ی پیش: {path}",
    "Cancelled.": "لغو شد.",
    "The dialog failed: {error}": "پنجره ناموفق بود: {error}",
    "{n} chosen: {path}": "{n} انتخاب شد: {path}",
    "Saved in {path}": "در {path} ذخیره شد",
    "FlowGraph scripts": "اسکریپت‌های FlowGraph",
    "Text": "متن",
    "Colors as the system's": "رنگ‌ها مثل سیستم",
    "Layout left to right": "چیدمان چپ به راست",
    "Ask before closing the window": "پیش از بستن پنجره بپرسد",
    "Alt+F4 and the title bar's button ask too.": "Alt+F4 و دکمه‌ی نوار عنوان هم می‌پرسند.",
    "Ctrl+Tab shows the next tab; with the focus on the tabs, the arrows.": "Ctrl+Tab زبانه‌ی بعدی را نشان می‌دهد؛ وقتی تمرکز روی زبانه‌هاست، فلش‌ها.",
    "The form's fields": "فیلدهای فرم",
    "Colors to drag": "رنگ‌هایی برای کشیدن",
    "The system's dialogs": "پنجره‌های سیستم"
  }
})";

// A color's name, dragged ("demo/color"): onto a drop box, in this window or the other one.
class Chip final : public glw::Widget {
public:
    explicit Chip(std::string name) : name_(std::move(name)) {
        setPadding({14, 6, 14, 6});
        setCursor(glw::platform::Cursor::Move);
        auto label = std::make_unique<glw::Label>(glw::Label::Props{.text = tr(name_)});
        label->setHitTestVisible(false);
        add(std::move(label));
    }

protected:
    void paint(glw::PaintContext& context) override {
        const glw::ButtonStyle& s = context.ui.theme.button;
        const glw::Rect r{0, 0, size().width, size().height};
        context.painter.fillRoundedRect(r, 6, s.background.normal);
        context.painter.strokeRoundedRect(r.inflated(-0.5f), 6, 1, s.border.normal);
    }
    void onMouse(glw::MouseEvent& e) override {
        // pressed and moved a few dips: dragged
        e.handled = true;
        if (e.type == glw::MouseEventType::Press) {
            pressed_ = e.position;
        } else if (e.type == glw::MouseEventType::Release) {
            pressed_.reset();
        } else if (e.type == glw::MouseEventType::Move && pressed_ &&
                   std::hypot(e.position.x - pressed_->x, e.position.y - pressed_->y) > 4) {
            pressed_.reset();
            root()->startDrag({.data = {"demo/color", name_}});
        }
    }

private:
    std::string name_;
    std::optional<glw::Point> pressed_;
};

// Takes a color dragged onto it, lit while one is over it; says which it took.
class DropBox final : public glw::Widget {
public:
    DropBox(glw::LocalizedText text, std::function<void(const std::string&)> dropped) : dropped_(std::move(dropped)) {
        setPadding(glw::Insets::all(12));
        setMinimumSize({0, 80});
        auto label = std::make_unique<glw::Label>(glw::Label::Props{.text = std::move(text), .muted = true, .wrap = true});
        label->setAlignment(glw::Alignment::Center, glw::Alignment::Center);
        label->setHitTestVisible(false);
        label_ = &add(std::move(label));
    }

protected:
    void paint(glw::PaintContext& context) override {
        const glw::LineEditStyle& s = context.ui.theme.lineEdit;
        const glw::Rect r{0, 0, size().width, size().height};
        context.painter.fillRoundedRect(r, 6, s.background.normal);
        context.painter.strokeRoundedRect(r.inflated(-1), 6, over_ ? 2.0f : 1.0f, over_ ? s.caret : s.border.normal);
    }
    glw::DropAction onDragOver(const glw::DragEvent& e) override {
        if (e.data.format != "demo/color")
            return glw::DropAction::None;
        if (!over_) {
            over_ = true;
            invalidatePaint();
        }
        return glw::DropAction::Copy;
    }
    void onDragLeave() override {
        over_ = false;
        invalidatePaint();
    }
    glw::DropAction onDrop(const glw::DragEvent& e) override {
        const std::string color = context() ? context()->translator.resolve(tr(e.data.content)) : e.data.content;
        label_->setText(tr("{color} dropped here").arg("color", color));
        if (dropped_)
            dropped_(color);
        return glw::DropAction::Copy;
    }

private:
    std::function<void(const std::string&)> dropped_;
    glw::Label* label_ = nullptr;
    bool over_ = false;
};

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
    glw::LocalizedText header(std::size_t column) const override { return tr("Column {n}").arg("n", column + 1); }
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
            return tr("Folder {n}").arg("n", node.id + 1);
        return tr("File {n}").arg("n", std::to_string((node.id - count) / count + 1) + "." + std::to_string((node.id - count) % count + 1));
    }
};

// The window's interface, and what its handlers change: bound pointers live as long as the handlers that use them, so they
// are members, and the handlers capture `this`.
struct Page {
    glw::Application& app;
    glw::Window* window = nullptr;
    glw::Label* status = nullptr;
    glw::Widget* buttons = nullptr;
    glw::Widget* cards = nullptr;
    glw::Button* menuButton = nullptr;
    int variableCount = 0;
    glw::ComboBox* styleBox = nullptr;
    glw::ComboBox* colorsBox = nullptr;
    glw::StatusBar* statusBar = nullptr;
    glw::Label* countShown = nullptr;           // in the status bar
    glw::Label* fileShown = nullptr;
    glw::Settings settings{std::string()};      // what the file dialogs chose last, in the user's preferences
    bool askBeforeClosing = true;

    // The preferences kept from the last time (before build: the files' tab shows them).
    void loadSettings() {
        const std::string folder = app.platform().preferencesFolder("GLWidgets", "WidgetsDemo");
        if (folder.empty())
            return;
        settings.setPath(folder + "settings.json");
        std::string error;
        if (!settings.load(&error))
            std::fprintf(stderr, "widgets_demo: %s\n", error.c_str());
    }

    // Once the window is open: its status bar, and the question before it closes.
    void attach() {
        statusBar = &window->setStatusBar();
        statusBar->setText(tr("Ready"));
        countShown = &statusBar->addPermanent(std::make_unique<glw::Label>(glw::Label::Props{.text = tr("Count: {n}").arg("n", 3), .muted = true}));
        window->setOnCloseRequested([this] {
            if (!askBeforeClosing)
                return true;
            confirmClose();
            return false;
        });
        // the colors, followed: the combo box says which they are now
        app.setOnSystemThemeChanged([this](bool) { colorsBox->setCurrent(static_cast<std::size_t>(app.colorScheme())); });
    }

    void tell(glw::LocalizedText text) {
        if (statusBar)
            statusBar->showMessage(std::move(text), std::chrono::seconds(4));
    }

    // The newer controls, in tabs.
    std::unique_ptr<glw::Widget> newControls() {
        glw::TabView* tabs = nullptr;
        std::unique_ptr<glw::Widget> view = ui::TabView({.closable = false}).bind(tabs).minimumSize({0, 360}).build();
        tabs->addTab(tr("Form"), formTab());
        tabs->addTab(tr("Drag and drop"), dragTab());
        tabs->addTab(tr("Files"), filesTab());
        tabs->setTabToolTip(0, {.title = tr("Form"), .text = tr("The form's fields")});
        tabs->setTabToolTip(1, {.title = tr("Drag and drop"), .text = tr("Colors to drag")});
        tabs->setTabToolTip(2, {.title = tr("Files"), .text = tr("The system's dialogs")});
        tabs->setCurrent(0);
        return ui::VStack({.spacing = 8},
                          ui::Label({.text = tr("Ctrl+Tab shows the next tab; with the focus on the tabs, the arrows."), .muted = true, .wrap = true}),
                          std::move(view));
    }

    // Spin boxes, a combo box searched by typing, fields that take only some texts: in a grid's two columns.
    std::unique_ptr<glw::Widget> formTab() {
        std::vector<glw::LocalizedText> nodes;
        for (const char* name : {"Add",       "Append",   "Branch",   "Break",    "Call",      "Clamp",   "Compare", "Concat",
                                 "Contains",  "Cos",      "Delay",    "Divide",   "Do Once",   "Equal",   "Floor",   "For Each",
                                 "For Loop",  "Format",   "Gate",     "Get",      "Greater",   "Length",  "Lerp",    "Less",
                                 "Make Array", "Max",     "Min",      "Modulo",   "Multiply",  "Not",     "Or",      "Print",
                                 "Random",    "Remove",   "Round",    "Select",   "Sequence",  "Set",     "Sin",     "Split",
                                 "Sqrt",      "Subtract", "Switch",   "To Int",   "To String", "Trim",    "While",   "Xor"})
            nodes.emplace_back(name);
        const auto label = [](glw::LocalizedText text) {
            return ui::Label({.text = std::move(text)}).alignment(glw::Alignment::Start, glw::Alignment::Center);
        };
        // a name of code: letters, digits and _, not a digit first; a year: four digits at most (Persian ones too)
        const auto identifier = [](std::string_view text) {
            for (std::size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
                if (!letter && !(i > 0 && c >= '0' && c <= '9'))
                    return false;
            }
            return text.size() <= 24;
        };
        const auto year = [](std::string_view text) {
            const std::string ascii = glw::SpinBox::toAscii(text);
            return ascii.size() <= 4 && std::all_of(ascii.begin(), ascii.end(), [](char c) { return c >= '0' && c <= '9'; });
        };
        return ui::VStack({.spacing = 12},
                          ui::Label({.text = tr("A form in a grid: its labels in one column, its fields in the other."), .muted = true, .wrap = true}),
                          ui::Grid({},
                                   label(tr("Count")),
                                   ui::SpinBox({.value = 3, .minimum = 0, .maximum = 99, .onChanged = [this](double v) {
                                                    countShown->setText(tr("Count: {n}").arg("n", static_cast<int>(v)));
                                                }}),
                                   label(tr("Zoom")),
                                   ui::SpinBox({.value = 1.25, .minimum = 0.25, .maximum = 8, .step = 0.25, .decimals = 2,
                                                .onChanged = [this](double v) { said(tr("Zoom"), v, 2); }}),
                                   label(tr("Temperature")),
                                   ui::SpinBox({.value = -4.5, .minimum = -50, .maximum = 50, .step = 0.5, .decimals = 1,
                                                .onChanged = [this](double v) { said(tr("Temperature"), v, 1); }}),
                                   label(tr("Node")),
                                   ui::ComboBox({.items = nodes,
                                                 .current = 0,
                                                 .onChanged = [this, nodes](std::size_t i) { chosen(nodes[i]); },
                                                 .searchable = true,
                                                 .placeholder = tr("Search the nodes…")}),
                                   label(tr("Identifier")),
                                   ui::LineEdit({.placeholder = tr("Letters, digits and _, not first a digit"), .accepts = identifier}),
                                   label(tr("Year")),
                                   ui::LineEdit({.placeholder = tr("Four digits"), .accepts = year})),
                          ui::Label({.text = tr("The arrows, Up and Down, Page Up and Page Down, the wheel; Persian digits are taken too. "
                                                "Type into the node box to search it."),
                                     .fontSize = 12,
                                     .muted = true,
                                     .wrap = true}))
            .padding(glw::Insets::all(16));
    }

    void said(const glw::LocalizedText& name, double value, int decimals) {
        tell(tr("{name} is {value}.").arg("name", app.translator().resolve(name)).arg("value", value, decimals));
    }

    // Colors dragged onto boxes, in this window or another.
    std::unique_ptr<glw::Widget> dragTab() {
        const auto dropped = [this](const std::string& color) { tell(tr("{color} dropped.").arg("color", color)); };
        auto first = std::make_unique<DropBox>(tr("Drop a color here"), dropped);
        first->setStretch(1);
        auto second = std::make_unique<DropBox>(tr("Drop a color here"), dropped);
        second->setStretch(1);
        return ui::VStack({.spacing = 12},
                          ui::Label({.text = tr("Drag a color onto a box, here or in the other window; Escape cancels."), .muted = true, .wrap = true}),
                          ui::HStack({.spacing = 10}, std::make_unique<Chip>("Red"), std::make_unique<Chip>("Green"), std::make_unique<Chip>("Blue")),
                          ui::HStack({.spacing = 10}, std::move(first), std::move(second)),
                          ui::Button({.text = tr("Another window…"), .onClick = [this] { openOther(); }})
                              .alignment(glw::Alignment::Start, glw::Alignment::Start))
            .padding(glw::Insets::all(16));
    }

    // A window of its own with a box to drop onto.
    void openOther() {
        auto box = std::make_unique<DropBox>(tr("Drop here"), [this](const std::string& color) { tell(tr("{color} dropped.").arg("color", color)); });
        box->setMargin(glw::Insets::all(16));
        app.openWindow({.title = tr("Drop here"), .size = {340, 220}}, std::move(box));
    }

    // The system's dialogs; what was chosen last, kept for the next time.
    std::unique_ptr<glw::Widget> filesTab() {
        using Kind = glw::platform::FileDialogKind;
        const std::string last = settings.string("lastFile");
        return ui::VStack({.spacing = 12},
                          ui::Label({.text = tr("The system's own dialogs. What was chosen last is kept in the preferences, for the next time."),
                                     .muted = true,
                                     .wrap = true}),
                          ui::HStack({.spacing = 10},
                                     ui::Button({.text = tr("Open files…"), .onClick = [this] { choose(Kind::OpenMany); }}),
                                     ui::Button({.text = tr("Save as…"), .onClick = [this] { choose(Kind::Save); }}),
                                     ui::Button({.text = tr("Choose a folder…"), .onClick = [this] { choose(Kind::Folder); }})),
                          ui::Label({.text = last.empty() ? tr("Nothing chosen yet.") : tr("Last time: {path}").arg("path", last), .wrap = true})
                              .bind(fileShown))
            .padding(glw::Insets::all(16));
    }

    void choose(glw::platform::FileDialogKind kind) {
        using Kind = glw::platform::FileDialogKind;
        glw::platform::FileDialogRequest request{.kind = kind, .location = settings.string("lastFolder")};
        if (kind != Kind::Folder) {
            const glw::Translator& t = app.translator();
            request.filters = {{t.resolve(tr("FlowGraph scripts")), "flow"}, {t.resolve(tr("Text")), "txt;md"}};
        }
        app.showFileDialog(
            std::move(request),
            [this, kind](const glw::platform::FileDialogResult& result) {
                if (result.failed()) {
                    fileShown->setText(tr("The dialog failed: {error}").arg("error", result.error));
                    return;
                }
                if (result.files.empty()) {
                    tell(tr("Cancelled."));
                    return;
                }
                const std::string& first = result.files.front();
                fileShown->setText(tr("{n} chosen: {path}").arg("n", result.files.size()).arg("path", first));
                // the folder to start in next time: the one chosen, or the one the file is in
                std::string folder = first;
                if (kind != Kind::Folder) {
                    const std::size_t slash = first.find_last_of("/\\");
                    folder = slash == std::string::npos ? std::string() : first.substr(0, slash + 1);
                }
                settings.setString("lastFile", first);
                settings.setString("lastFolder", folder);
                if (settings.path().empty())
                    return;
                std::string error;
                if (settings.save(&error))
                    tell(tr("Saved in {path}").arg("path", settings.path()));
                else
                    tell(glw::LocalizedText(error));
            },
            window);
    }

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
        view->setDelegate(std::make_unique<glw::TextDelegate>([](std::size_t i) { return tr("Item {n}").arg("n", i + 1); }));
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
                   ui::CheckBox({.text = tr("Colors as the system's"), .onToggled = [this](bool on) {
                                     app.setFollowsSystemColors(on);
                                     colorsBox->setCurrent(static_cast<std::size_t>(app.colorScheme()));
                                 }}),
                   ui::CheckBox({.text = tr("Layout left to right"), .onToggled = [this](bool on) {
                                     app.setLayoutDirection(on ? std::optional(glw::LayoutDirection::LeftToRight) : std::nullopt);
                                 }}),
                   ui::CheckBox({.text = tr("Ask before closing the window"), .checked = true, .onToggled = [this](bool on) { askBeforeClosing = on; }}),
                   ui::HStack({.spacing = 10},
                              // in the other language: the button says which one it goes to
                              ui::Button({.text = tr("فارسی"), .onClick = [this] { switchLanguage(); }}),
                              // the colors: the style's dark and light ones, two more of each, one of glazed tiles' turquoise
                              ui::Label({.text = tr("Colors")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Dark"), tr("Light"), tr("Dusk"), tr("Sand"), tr("Turquoise")},
                                            .current = 0,
                                            .onChanged = [this](std::size_t i) {
                                                app.setColorScheme(static_cast<glw::ColorScheme>(i));
                                            }})
                                  .bind(colorsBox)
                                  .toolTip(tr("Every widget, in these colors")),
                              ui::Button({.text = tr("Close"), .onClick = [this] { confirmClose(); }})
                                  .toolTip(glw::ToolTip{.title = tr("Close"), .text = tr("Asks first"), .note = tr("Alt+F4 and the title bar's button ask too.")}))
                       .bind(buttons),
                   ui::HStack({.spacing = 10},
                              ui::Button({.text = tr("Menu"), .onClick = [this] { showDemoMenu(); }}).bind(menuButton),
                              ui::Button({.text = tr("Dialog…"), .onClick = [this] { showDemoDialog(); }})
                                  .toolTip(tr("A modal dialog: the window waits for it")),
                              ui::Label({.text = tr("Size")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {tr("Small"), tr("Medium"), tr("Large")},
                                            .current = 1,
                                            .onChanged = [this](std::size_t i) { chosen(i == 0 ? tr("Small") : i == 1 ? tr("Medium") : tr("Large")); }}),
                              // the looks of the library: as a tool's, modern and moving, plain and still
                              ui::Label({.text = tr("Style")}).alignment(glw::Alignment::Start, glw::Alignment::Center),
                              ui::ComboBox({.items = {glw::LocalizedText("Studio"), glw::LocalizedText("Fluid"), glw::LocalizedText("Plain"),
                                                      glw::LocalizedText("Plastic"), glw::LocalizedText("Metal"), glw::LocalizedText("Solid"),
                                                      glw::LocalizedText("Glass")},
                                            .current = 0,
                                            .onChanged = [this](std::size_t i) {
                                                app.setStyle(static_cast<glw::ThemeStyle>(i));
                                            }})
                                  .bind(styleBox)),
                   ui::HStack({.spacing = 12}, card(1, tr("a share")), card(2, tr("two shares")), card(1, tr("a share"))).bind(cards),
                   ui::Label({.text = tr("Click a header to open or close it; + adds a variable."), .muted = true, .wrap = true}),
                   panels(),
                   newControls(),
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
                                        ui::Label({.text = tr("Variable {n}").arg("n", variableCount)})
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
};

} // namespace

int main(int argc, char** argv) {
    int frames = -1;
    std::string language = "en";
    std::optional<glw::ThemeStyle> style;       // none: the default (Studio)
    std::optional<glw::ColorScheme> colors;     // none: the default (Dark)
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--frames" && i + 1 < argc)
            frames = std::max(1, std::atoi(argv[++i]));
        else if (arg == "--language" && i + 1 < argc)
            language = argv[++i];
        else if (arg == "--light")
            colors = glw::ColorScheme::Light;
        else if (arg == "--style" && i + 1 < argc)
            style = glw::themeStyleNamed(argv[++i]);
        else if (arg == "--colors" && i + 1 < argc)
            colors = glw::colorSchemeNamed(argv[++i]);
    }
    try {
        glw::Application app;
        std::string error;
        if (!app.translator().loadCatalog(persianCatalog, &error))
            std::fprintf(stderr, "widgets_demo: the Persian catalog: %s\n", error.c_str());
        app.setLanguage(language);
        Page page{app};
        page.loadSettings();
        page.window = &app.openWindow({.title = tr("Widgets"), .size = {980, 640}}, page.build());
        page.attach();
        if (style)
            app.setStyle(*style, false);
        if (colors)
            app.setColorScheme(*colors, false);
        page.styleBox->setCurrent(static_cast<std::size_t>(app.style()));
        page.colorsBox->setCurrent(static_cast<std::size_t>(app.colorScheme()));
        return app.run(frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "widgets_demo: %s\n", e.what());
        return 1;
    }
}
