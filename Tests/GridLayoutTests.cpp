// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <memory>
#include <vector>

using namespace glw;

namespace {

// A form of two columns (the labels as wide as the widest, the fields the rest), 300 by 200, its cells added by `cell`.
struct Form {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    Widget* panel;
    GridLayout* grid;
    explicit Form(std::vector<GridColumn> columns = {{}, {.stretch = 1}}) {
        auto p = std::make_unique<Widget>();
        panel = p.get();
        auto g = std::make_unique<GridLayout>(std::move(columns));
        grid = g.get();
        panel->setLayout(std::move(g));
        root.setContent(std::move(p));
        root.setSize({300, 200});
    }
    Widget& cell(Size size) {
        auto w = std::make_unique<Widget>();
        w->setMinimumSize(size);
        return panel->add(std::move(w));
    }
    void layout() {
        panel->invalidateLayout();
        root.layout();
    }
};

} // namespace

TEST(aGridPutsLabelsAndFieldsInColumns) {
    Form f;
    Widget& name = f.cell({60, 20});
    Widget& nameField = f.cell({40, 24});
    Widget& type = f.cell({90, 20});
    Widget& typeField = f.cell({40, 24});
    f.layout();
    // the labels' column as wide as the widest label; the fields' the rest, after the spacing (8)
    CHECK(name.bounds() == Rect{0, 0, 90, 24});
    CHECK(nameField.bounds() == Rect{98, 0, 202, 24});
    // the next row after the tallest of the first and the spacing (6)
    CHECK(type.bounds() == Rect{0, 30, 90, 24});
    CHECK(typeField.bounds() == Rect{98, 30, 202, 24});
    // what it wants with no bounds: every column as wide as its widest
    CHECK(f.panel->measure({unbounded, unbounded}) == Size{138, 54});
}

TEST(aGridChildSpansColumnsAndStartsARowWhenItDoesNotFit) {
    Form f({{}, {.stretch = 1}, {}});
    Widget& a = f.cell({50, 20});
    Widget& wide = f.cell({30, 20});
    f.grid->setSpan(wide, 3);           // not in what is left of the first row: on a row of its own
    Widget& b = f.cell({50, 20});
    Widget& c = f.cell({20, 20});
    f.grid->setSpan(c, 2);
    f.layout();
    CHECK(a.bounds() == Rect{0, 0, 50, 20});
    CHECK(wide.bounds() == Rect{0, 26, 300, 20});
    CHECK(b.bounds() == Rect{0, 52, 50, 20});
    // the stretching column and the last (empty of children of their own: as wide as nothing) together
    CHECK(c.bounds() == Rect{58, 52, 242, 20});
    CHECK(f.grid->span(wide) == 3 && f.grid->span(a) == 1);
    f.grid->setSpan(wide, 9);
    CHECK(f.grid->span(wide) == 3);     // at most all of them
}

TEST(aGridGoesFromTheRightInRightToLeft) {
    Form f;
    f.root.setLayoutDirection(LayoutDirection::RightToLeft);
    Widget& label = f.cell({90, 20});
    Widget& field = f.cell({40, 24});
    f.layout();
    CHECK(label.bounds() == Rect{210, 0, 90, 24});
    CHECK(field.bounds() == Rect{0, 0, 202, 24});
}

TEST(aGridGivesWhatIsLeftToRowsThatStretchAndSkipsHiddenChildren) {
    Form f;
    f.cell({60, 20});
    Widget& hidden = f.cell({40, 24});
    hidden.setVisible(false);
    Widget& field = f.cell({40, 24});       // in the hidden one's place
    Widget& note = f.cell({60, 20});
    Widget& text = f.cell({40, 30});
    text.setStretch(1);
    f.layout();
    CHECK(field.bounds() == Rect{68, 0, 232, 24});
    CHECK(note.bounds().y == 30);
    // the second row takes the rest of the height: 200 - 24 - 6
    CHECK(text.bounds() == Rect{68, 30, 232, 170});
}

TEST(aGridIsBuiltDeclaratively) {
    Widget* note = nullptr;
    std::unique_ptr<Widget> grid =
        ui::Grid({.columns = {{.minimum = 100}, {.stretch = 1}}},
                 ui::make<Widget>().minimumSize({60, 20}), ui::make<Widget>().minimumSize({40, 20}),
                 ui::make<Widget>().minimumSize({40, 20}).bind(note))
            .with([&](Widget& g) { static_cast<GridLayout*>(g.layout())->setSpan(*note, 2); });
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    Widget* g = grid.get();
    root.setContent(std::move(grid));
    root.setSize({300, 200});
    root.layout();
    CHECK(g->children()[1]->bounds() == Rect{108, 0, 192, 20});
    CHECK(note->bounds() == Rect{0, 26, 300, 20});
}
