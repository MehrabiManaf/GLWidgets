// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Table views: a million rows of a hundred columns with widgets for the cells that show only, scrolled down and across;
// columns resized by their headers' edges, the last one taking the room left; right to left; cells of kinds; the model's
// changes followed.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace glw;

namespace {

bool near(double a, double b) {
    return std::abs(a - b) < 0.5;
}

// "r<row>c<column>", made when asked: no memory per cell.
class Grid final : public TableModel {
public:
    Grid(std::size_t rows, std::size_t columns) : rows_(rows), columns_(columns) {}
    std::size_t rowCount() const override { return rows_; }
    std::size_t columnCount() const override { return columns_; }
    LocalizedText header(std::size_t column) const override { return LocalizedText(prefix_ + "C" + std::to_string(column)); }
    std::string text(std::size_t row, std::size_t column) const {
        return "r" + std::to_string(row + shift_) + "c" + std::to_string(column);
    }

    void insertRows(std::size_t at, std::size_t count) {
        rows_ += count;
        notifyRowsInserted(at, count);
    }
    void removeRows(std::size_t at, std::size_t count) {
        rows_ -= count;
        notifyRowsRemoved(at, count);
    }
    void renumber(std::size_t shift) {
        shift_ = shift;
        notifyRowsChanged(0, rows_);
    }
    void setColumns(std::size_t columns, std::string prefix) {
        columns_ = columns;
        prefix_ = std::move(prefix);
        notifyColumnsChanged();
    }

private:
    std::size_t rows_, columns_;
    std::size_t shift_ = 0;
    std::string prefix_;
};

std::size_t widgetsIn(const Widget& widget) {
    std::size_t count = 1;
    for (const auto& child : widget.children())
        count += widgetsIn(*child);
    return count;
}

struct Tabled {
    UiRoot root{tests::testContext()};
    TableView* table = nullptr;
    std::shared_ptr<Grid> grid;

    Tabled(std::size_t rows, std::size_t columns, Size size = {600, 400}) : grid(std::make_shared<Grid>(rows, columns)) {
        auto made = std::make_unique<TableView>();
        table = made.get();
        table->setModel(grid);
        table->setDelegate(std::make_unique<TableTextDelegate>([g = grid.get()](std::size_t r, std::size_t c) { return LocalizedText(g->text(r, c)); }));
        root.setSize(size);
        root.setContent(std::move(made));
        root.layout();
    }

    // The text a cell (or a header: row none) shows.
    std::string shown(std::optional<std::size_t> row, std::size_t column) const {
        const Widget* cell = row ? table->cellWidget(*row, column) : table->headerWidget(column);
        return cell ? static_cast<const Label&>(*cell).shownText() : std::string("(not shown)");
    }
    float left(std::size_t row, std::size_t column) const {
        return table->cellWidget(row, column)->mapToRoot({0, 0}).x;
    }
    void press(Point at) { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at}); }
    void release(Point at) {
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
        root.layout();
    }
    void move(Point to) {
        root.dispatch(platform::MouseMoved{1, to});
        root.layout();
    }
    void key(platform::Key k) {
        root.dispatch(platform::KeyChanged{1, k, platform::KeyModifiers::None, true, false});
        root.layout();
    }
};

} // namespace

TEST(aTableOfAMillionRowsHasWidgetsForTheCellsThatShowOnly) {
    Tabled t(1'000'000, 100);
    // 600 wide, columns of 140: five show (the fifth in part); some 13 rows under the header
    const auto [first, last] = t.table->shownColumns();
    CHECK(first == 0 && last == 5);
    CHECK(t.shown(std::nullopt, 0) == "C0" && t.shown(0, 0) == "r0c0" && t.shown(2, 4) == "r2c4");
    CHECK(t.shown(0, 5) == "(not shown)");
    const std::size_t widgets = widgetsIn(*t.table);
    CHECK(widgets < 120);

    // down and across: the cells there, as many widgets
    t.table->list().scrollTo(500'000);
    t.table->scrollToColumn(60);
    t.root.layout();
    const auto [from, to] = t.table->shownColumns();
    CHECK(from <= 60 && to > 60 && to - from <= 6);
    CHECK(t.shown(500'000, 60) == "r500000c60" && t.shown(std::nullopt, 60) == "C60");
    CHECK(t.shown(500'000, 0) == "(not shown)" && t.shown(0, 0) == "(not shown)");
    CHECK(widgetsIn(*t.table) <= widgets + 8);

    // to the end
    t.table->setHorizontalOffset(1e9);
    t.root.layout();
    CHECK(near(t.table->horizontalOffset(), t.table->maximumHorizontalOffset()));
    CHECK(t.table->shownColumns().second == 100 && t.shown(500'000, 99) == "r500000c99");
}

TEST(columnsAreResizedByTheirHeadersEdgesTheLastTakingTheRoomLeft) {
    Tabled t(20, 3);
    // three columns in 600: the last one 600 - 280 wide (no bar: 20 rows fit... not quite: 20 x 28 > 372)
    const float view = t.table->list().viewport().width;
    CHECK(near(t.left(0, 1), 140) && near(t.left(0, 2), 280));
    CHECK(near(t.table->cellWidget(0, 2)->size().width, view - 280));
    CHECK(near(t.table->totalWidth(), view));

    // the edge between C0 and C1 dragged 60 to the end: C0 200 wide, C1 after it
    t.press({140, 14});
    t.move({170, 14});
    t.move({200, 14});
    t.release({200, 14});
    CHECK(near(t.table->columnWidth(0), 200) && near(t.left(0, 1), 200) && near(t.left(0, 2), 340));
    // not narrower than the least
    t.press({200, 14});
    t.move({0, 14});
    t.release({0, 14});
    CHECK(near(t.table->columnWidth(0), TableView::minimumColumnWidth));

    // a header clicked
    std::optional<std::size_t> clicked;
    t.table->setOnHeaderClicked([&](std::size_t column) { clicked = column; });
    const float c1 = t.left(0, 1);
    t.press({c1 + 60, 14});
    t.release({c1 + 60, 14});
    CHECK(clicked == 1u);
    CHECK(!t.table->current());

    // not stretched: the last one as set, the room after it empty
    t.table->setStretchLastColumn(false);
    t.root.layout();
    CHECK(near(t.table->cellWidget(0, 2)->size().width, 140));
}

TEST(aTableScrollsAcrossByTheWheelTheKeysAndItsBar) {
    Tabled t(10, 20);
    CHECK(t.table->horizontalOffset() == 0);
    // tilted to the right
    t.root.dispatch(platform::MouseWheelScrolled{1, {1, 0}, {300, 100}});
    t.root.layout();
    const double tilted = t.table->horizontalOffset();
    CHECK(tilted > 0);
    // Right and Left, while the rows have the focus
    t.table->list().focus();
    t.key(platform::Key::Right);
    CHECK(near(t.table->horizontalOffset(), tilted + 40));
    t.key(platform::Key::Left);
    t.key(platform::Key::Left);
    CHECK(near(t.table->horizontalOffset(), tilted - 40));
    // the wheel not tilted scrolls down (none here: the rows fit), not across
    t.root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {300, 100}});
    CHECK(near(t.table->horizontalOffset(), tilted - 40));

    // the bar across: its thumb dragged to the end
    t.table->setHorizontalOffset(0);
    t.root.layout();
    const float bar = 400 - 4;          // in the bar, at the bottom
    t.press({20, bar});
    t.move({300, bar});
    t.move({900, bar});
    t.release({900, bar});
    CHECK(near(t.table->horizontalOffset(), t.table->maximumHorizontalOffset()));
    // pressed before the thumb: a page back (nine tenths of what shows, as every scroll bar's), to a column's edge
    const double end = t.table->horizontalOffset();
    const double view = t.table->list().viewport().width;
    t.press({5, bar});
    t.release({5, bar});
    CHECK(t.table->horizontalOffset() < end - view * 0.5 && t.table->horizontalOffset() >= end - view - 140);
}

TEST(aTableRightToLeftHasItsFirstColumnAtTheRight) {
    Tabled t(5, 10);
    t.table->setLayoutDirection(LayoutDirection::RightToLeft);
    t.root.layout();
    const float view = t.table->list().viewport().width;
    CHECK(near(t.left(0, 0), view - 140) && near(t.left(0, 1), view - 280));
    CHECK(near(t.table->headerWidget(0)->mapToRoot({0, 0}).x, view - 140));
    // scrolled across: the columns move to the right
    t.table->setHorizontalOffset(100);
    t.root.layout();
    CHECK(near(t.left(0, 0), view - 40));
    // the edge of C0 is at its left now: dragged left, it is wider
    t.table->setHorizontalOffset(0);
    t.root.layout();
    const float edge = view - 140;
    t.press({edge, 14});
    t.move({edge - 30, 14});
    t.release({edge - 30, 14});
    CHECK(near(t.table->columnWidth(0), 170) && near(t.left(0, 0), view - 170));
}

TEST(aTablesCellsAreOfKinds) {
    // even columns' cells and odd ones', widgets of two kinds made apart
    struct Even final : Label {};
    struct Odd final : Label {};
    struct Kinds final : TableDelegate {
        int made[2] = {0, 0};
        std::size_t cellKind(std::size_t column) const override { return column % 2; }
        std::unique_ptr<Widget> makeCell(std::size_t kind) override {
            ++made[kind];
            if (kind == 0)
                return std::make_unique<Even>();
            return std::make_unique<Odd>();
        }
        void bindCell(Widget& cell, std::size_t row, std::size_t column) override {
            static_cast<Label&>(cell).setText(LocalizedText(std::to_string(row) + ":" + std::to_string(column)));
        }
    };
    Tabled t(100, 40);
    auto kinds = std::make_unique<Kinds>();
    Kinds* k = kinds.get();
    t.table->setDelegate(std::move(kinds));
    t.root.layout();
    const auto even = [&](std::size_t column) { return dynamic_cast<Even*>(t.table->cellWidget(3, column)) != nullptr; };
    const auto odd = [&](std::size_t column) { return dynamic_cast<Odd*>(t.table->cellWidget(3, column)) != nullptr; };
    CHECK(even(0) && odd(1));
    const int made = k->made[0] + k->made[1];
    // scrolled across, column by column: each cell of its kind, and few more made
    for (std::size_t column = 5; column < 40; ++column) {
        t.table->scrollToColumn(column);
        t.root.layout();
        CHECK(column % 2 == 0 ? even(column) : odd(column));
        CHECK(t.shown(3, column) == "3:" + std::to_string(column));
    }
    CHECK(k->made[0] + k->made[1] <= made + 2 * 16);
}

TEST(aTableFollowsItsModel) {
    Tabled t(100, 4);
    t.table->setCurrent(10);
    // rows inserted before the current one: it stays on its row
    t.grid->insertRows(0, 5);
    t.root.layout();
    CHECK(t.table->current() == 15u);
    t.grid->removeRows(0, 2);
    t.root.layout();
    CHECK(t.table->current() == 13u);
    // changed (the rows that show stayed in place as rows came before them)
    const std::size_t top = t.table->list().shownItems().first;
    const std::string topText = "r" + std::to_string(top + 1000) + "c1";
    t.grid->renumber(1000);
    t.root.layout();
    CHECK(t.shown(top, 1) == topText);
    // the columns: others, with other headers; the widths set kept
    t.table->setColumnWidth(0, 90);
    t.grid->setColumns(6, "New");
    t.root.layout();
    CHECK(t.shown(std::nullopt, 0) == "NewC0" && near(t.table->columnWidth(0), 90));
    CHECK(near(t.left(top, 1), 90) && t.shown(top, 1) == topText);
    t.grid->setColumns(2, "Few");
    t.root.layout();
    CHECK(t.shown(std::nullopt, 1) == "FewC1" && t.shown(std::nullopt, 2) == "(not shown)");
    CHECK(t.table->cellWidget(top, 2) == nullptr);
}

// Names and their types: the types' column as wide as the widest (measured again as the rows change), the names' all the
// rest, whatever the table's width; a drag of the fitting column's edge sets it as the user says from then on.
// Names as wide as they need, a long text beside them all the rest: narrower than both, the text gives way (cut) before
// the table scrolls across.
TEST(aColumnGivesWayBeforeTheTableScrolls) {
    std::vector<std::pair<std::string, std::string>> rows{
        {"Order", "{Id, Customer, Street, City, Postcode, Country, Phone, Email, Quantity, Price}"}};
    class Members final : public TableModel {
    public:
        explicit Members(std::vector<std::pair<std::string, std::string>>& rows) : rows_(rows) {}
        std::size_t rowCount() const override { return rows_.size(); }
        std::size_t columnCount() const override { return 2; }

    private:
        std::vector<std::pair<std::string, std::string>>& rows_;
    };
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<TableView>();
    TableView& table = *made;
    table.setModel(std::make_shared<Members>(rows));
    table.setDelegate(std::make_unique<TableTextDelegate>(
        [&rows](std::size_t r, std::size_t c) { return LocalizedText(c == 0 ? rows[r].first : rows[r].second); }));
    table.setColumnSizing(0, ColumnSizing::FitContent);
    table.setColumnSizing(1, ColumnSizing::FitContent);
    table.setColumnYields(1, true);
    table.setStretchLastColumn(false);
    root.setSize({240, 200});
    root.setContent(std::move(made));
    root.layout();
    const auto widthOf = [&](std::size_t column) {
        const Widget* header = table.headerWidget(column);
        return header ? header->size().width : 0.0f;
    };
    // narrow: the names as they need, the text the rest (cut), nothing to scroll
    const float view = table.list().viewport().width;
    CHECK(near(widthOf(0), table.contentWidth(0)) && near(widthOf(0) + widthOf(1), view));
    CHECK(near(table.totalWidth(), view));
    // wide: the text as it needs
    root.setSize({1200, 200});
    root.layout();
    CHECK(near(widthOf(1), table.contentWidth(1)));
    // not giving way: the table scrolls across when narrow
    table.setColumnYields(1, false);
    root.setSize({240, 200});
    root.layout();
    CHECK(table.totalWidth() > table.list().viewport().width + 1);
}

// A table of the theme's lists' style follows the theme as it changes, its own style left alone.
TEST(aTableTakesTheThemesListStyleAsTheThemeChanges) {
    UiContext& ui = tests::testContext();
    UiRoot root(ui);
    auto made = std::make_unique<TableView>();
    TableView& table = *made;
    root.setContent(std::move(made));
    CHECK(table.style().headerLine == ui.theme.table.headerLine && &table.style() == &ui.theme.table);
    table.setThemeStyle(&Theme::itemView);
    CHECK(&table.style() == &ui.theme.itemView);
    // another theme: its lists' style
    const Theme saved = ui.theme;
    ui.theme.itemView.selected = Color{0.25f, 0.5f, 0.75f, 1};
    root.styleChanged();
    CHECK(table.style().selected == (Color{0.25f, 0.5f, 0.75f, 1}));
    // its own wins
    ItemViewStyle own = ui.theme.table;
    own.selected = Color{1, 0, 0, 1};
    table.setStyle(own);
    CHECK(table.style().selected == (Color{1, 0, 0, 1}));
    ui.theme = saved;
}

TEST(columnsFitTheirContentOrStretch) {
    std::vector<std::pair<std::string, std::string>> rows{{"Speed", "float"}, {"Name", "string"}};
    class Members final : public TableModel {
    public:
        explicit Members(std::vector<std::pair<std::string, std::string>>& rows) : rows_(rows) {}
        std::size_t rowCount() const override { return rows_.size(); }
        std::size_t columnCount() const override { return 2; }
        LocalizedText header(std::size_t column) const override { return LocalizedText(column == 0 ? "Name" : "Type"); }
        void added() { notifyRowsInserted(rows_.size() - 1, 1); }

    private:
        std::vector<std::pair<std::string, std::string>>& rows_;
    };
    auto members = std::make_shared<Members>(rows);
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<TableView>();
    TableView& table = *made;
    table.setModel(members);
    table.setDelegate(std::make_unique<TableTextDelegate>(
        [&rows](std::size_t r, std::size_t c) { return LocalizedText(c == 0 ? rows[r].first : rows[r].second); }));
    table.setColumnSizing(0, ColumnSizing::Stretch);
    table.setColumnSizing(1, ColumnSizing::FitContent);
    CHECK(table.columnSizing(1) == ColumnSizing::FitContent && table.columnSizing(0) == ColumnSizing::Stretch);
    root.setSize({400, 300});
    root.setContent(std::move(made));
    root.layout();
    const auto widthOf = [&](std::size_t column) {
        const Widget* header = table.headerWidget(column);
        return header ? header->size().width : 0.0f;
    };
    // the types as wide as they need, the names the rest
    const float fitted = table.contentWidth(1);
    CHECK(near(table.columnWidth(1), fitted) && near(widthOf(1), fitted));
    CHECK(near(widthOf(0) + widthOf(1), table.list().viewport().width));
    CHECK(near(table.totalWidth(), table.list().viewport().width));
    // narrower: the names give room, the types keep theirs
    root.setSize({250, 300});
    root.layout();
    CHECK(near(widthOf(1), fitted) && near(widthOf(0) + widthOf(1), table.list().viewport().width));
    // narrower than the theme's width and the types together: the names, never set, give their room still (nothing across)
    root.setSize({160, 300});
    root.layout();
    CHECK(near(widthOf(1), fitted) && near(table.totalWidth(), table.list().viewport().width));
    root.setSize({250, 300});
    root.layout();
    // a row with a longer type: measured again
    rows.emplace_back("Points", "array<map<string, float>>");
    members->added();
    root.layout();
    CHECK(table.columnWidth(1) > fitted + 20 && near(table.columnWidth(1), table.contentWidth(1)));
    // a drag of its header's edge (at its end, in view): as the user says from then on (Fixed), not measured again
    root.setSize({700, 300});
    root.layout();
    const Widget& typeHeader = *table.headerWidget(1);
    const Point edge = typeHeader.mapToRoot({typeHeader.size().width - 1, typeHeader.size().height / 2});
    root.dispatch(platform::MouseMoved{1, edge});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, edge});
    root.dispatch(platform::MouseMoved{1, {edge.x - 30, edge.y}});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, {edge.x - 30, edge.y}});
    root.layout();
    CHECK(table.columnSizing(1) == ColumnSizing::Fixed);
    const float dragged = table.columnWidth(1);
    rows.emplace_back("Other", "a type with a much longer name than any of the others");
    members->added();
    root.layout();
    CHECK(near(table.columnWidth(1), dragged));
    // the last column stretched by default when none is said to
    table.setColumnSizing(0, ColumnSizing::Fixed);
    table.setColumnSizing(1, ColumnSizing::Fixed);
    root.layout();
    CHECK(widthOf(0) + widthOf(1) >= table.list().viewport().width - 0.5f);
}
