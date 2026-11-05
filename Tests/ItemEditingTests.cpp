// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Item views sorted, filtered and edited: lists sorted by their model's comparison and filtered, the model's indices kept
// for what is asked of them; a table sorted by a click on a header (ascending, descending, not), cells edited in fields
// and in other widgets (a check box), Tab to the next editable cell; a tree's children sorted, its nodes filtered with
// their ancestors opened; editing started by F2, typing and a double click, ended by Enter, Escape and the focus leaving.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/CheckBox.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/TableView.hpp>
#include <GLWidgets/Widgets/TreeView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace glw;

namespace {

void key(UiRoot& root, platform::Key k, platform::KeyModifiers modifiers = platform::KeyModifiers::None) {
    root.dispatch(platform::KeyChanged{1, k, modifiers, true, false});
    root.layout();
}

void type(UiRoot& root, std::string text) {
    root.dispatch(platform::TextInput{1, std::move(text)});
    root.layout();
}

void click(UiRoot& root, Point at, int clicks = 1) {
    root.dispatch(platform::MouseMoved{1, at});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, clicks, at});
    root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, clicks, at});
    root.layout();
}

// A list of fruit, as a ListView shows a StringListModel by itself.
struct Fruit {
    UiRoot root{tests::testContext()};
    ListView* list = nullptr;
    std::shared_ptr<StringListModel> texts =
        std::make_shared<StringListModel>(std::vector<LocalizedText>{"pear", "Apple", "fig", "banana"});

    Fruit() {
        auto made = std::make_unique<ListView>();
        list = made.get();
        list->setModel(texts);
        root.setSize({300, 300});
        root.setContent(std::move(made));
        root.layout();
    }
    // the texts as they show, in order
    std::string shown() const {
        std::string out;
        for (std::size_t at = 0; at < list->shownCount(); ++at)
            out += (at ? " " : "") + texts->text(list->modelIndex(at)).source;
        return out;
    }
};

} // namespace

TEST(aListSortsAndFiltersItsItemsAsTheyAre) {
    Fruit f;
    CHECK(f.shown() == "pear Apple fig banana");
    f.list->setCurrent(3);                                      // banana
    f.list->setSortOrder(SortOrder::Ascending);
    CHECK(f.shown() == "Apple banana fig pear");                // case aside
    CHECK(f.list->current() == 3u && f.list->position(3) == 1u);   // the model's index kept, where it shows now
    f.list->setSortOrder(SortOrder::Descending);
    CHECK(f.shown() == "pear fig banana Apple");
    // filtered: those it accepts; the current one kept while it shows
    f.list->setFilter([&](std::size_t i) { return f.texts->text(i).source.find('a') != std::string::npos; });
    CHECK(f.shown() == "pear banana" && f.list->current() == 3u);
    f.list->setFilter([&](std::size_t i) { return f.texts->text(i).source.size() == 3; });
    CHECK(f.shown() == "fig" && !f.list->current() && !f.list->position(0));
    // the model's changes, sorted and filtered as they come
    f.list->setFilter({});
    f.texts->append("cherry");
    CHECK(f.shown() == "pear fig cherry banana Apple");
    // the delegate still binds the model's items: the row showing "fig" is the model's item 2's
    f.root.layout();
    CHECK(f.list->rowWidget(2) != nullptr && f.list->itemAt({50, 30}) == 2u);
    f.list->setSortOrder(std::nullopt);
    CHECK(f.shown() == "pear Apple fig banana cherry");
}

TEST(aListItemIsEditedInPlace) {
    Fruit f;
    f.list->focus();
    f.list->setCurrent(1);
    // not editable: F2 does nothing
    key(f.root, platform::Key::F2);
    CHECK(!f.list->editing());
    f.texts->setEditable(true);
    // F2: a field over it, its text in it chosen, the focus in it
    key(f.root, platform::Key::F2);
    CHECK(f.list->editing() && f.list->editedItem() == 1u);
    auto* field = dynamic_cast<LineEdit*>(f.list->editor());
    CHECK(field && field->text() == "Apple" && field->hasFocus());
    // typed over, Enter: put in, the focus back to the list
    type(f.root, "Apricot");
    key(f.root, platform::Key::Enter);
    CHECK(!f.list->editing() && f.texts->text(1).source == "Apricot" && f.list->hasFocus());
    // Escape: left as it was
    key(f.root, platform::Key::F2);
    type(f.root, "Plum");
    key(f.root, platform::Key::Escape);
    CHECK(!f.list->editing() && f.texts->text(1).source == "Apricot");
    // typing on it: edited, what was typed in the field
    type(f.root, "K");
    CHECK(f.list->editing() && static_cast<LineEdit*>(f.list->editor())->text() == "K");
    type(f.root, "iwi");
    // the focus leaving it (the list pressed): put in
    f.list->focus();
    f.root.layout();
    CHECK(!f.list->editing() && f.texts->text(1).source == "Kiwi");
    // a double click: edited, not activated
    bool activated = false;
    f.list->setOnActivated([&](std::size_t) { activated = true; });
    const Rect row = *f.list->itemRect(2);
    click(f.root, row.center(), 2);
    CHECK(f.list->editing() && f.list->editedItem() == 2u && !activated);
    f.list->cancelEdit();
    // only by the triggers it has
    f.list->setEditTriggers(EditTriggers::F2);
    type(f.root, "x");
    CHECK(!f.list->editing());
}

namespace {

// People: a name (a field), and whether they come (a check box); sortable by name.
class People final : public TableModel {
public:
    std::vector<std::string> names{"Sara", "Ali", "Mina", "Reza"};
    std::vector<bool> coming{true, false, true, false};
    std::size_t rowCount() const override { return names.size(); }
    std::size_t columnCount() const override { return 3; }
    LocalizedText header(std::size_t column) const override { return LocalizedText(column == 0 ? "Name" : column == 1 ? "Comes" : "Id"); }
    ItemFlags flags(std::size_t, std::size_t column) const override { return column < 2 ? ItemFlags::Editable : ItemFlags::None; }
    bool sortable(std::size_t column) const override { return column == 0; }
    std::partial_ordering compare(std::size_t a, std::size_t b, std::size_t) const override { return names[a] <=> names[b]; }
    void changed(std::size_t row) { notifyRowsChanged(row, 1); }
};

// Names in fields (the text delegate's), whether they come in a check box.
class PeopleDelegate final : public TableDelegate {
public:
    explicit PeopleDelegate(People& people) : people_(people), text_([&p = people](std::size_t r, std::size_t c) {
        return LocalizedText(c == 0 ? p.names[r] : c == 1 ? (p.coming[r] ? "yes" : "no") : std::to_string(r));
    }) {
        text_.setOnEdited([&p = people](std::size_t r, std::size_t, std::string name) {
            p.names[r] = std::move(name);
            p.changed(r);
        });
    }
    std::unique_ptr<Widget> makeCell(std::size_t kind) override { return text_.makeCell(kind); }
    void bindCell(Widget& cell, std::size_t row, std::size_t column) override { text_.bindCell(cell, row, column); }
    std::unique_ptr<Widget> makeEditor(std::size_t row, std::size_t column) override {
        if (column == 1)
            return std::make_unique<CheckBox>(CheckBox::Props{.text = "Comes", .checked = people_.coming[row]});
        return text_.makeEditor(row, column);
    }
    void commitEdit(Widget& editor, std::size_t row, std::size_t column) override {
        if (column == 1) {
            people_.coming[row] = static_cast<CheckBox&>(editor).checked();
            people_.changed(row);
        } else {
            text_.commitEdit(editor, row, column);
        }
    }

private:
    People& people_;
    TableTextDelegate text_;
};

struct Peopled {
    UiRoot root{tests::testContext()};
    TableView* table = nullptr;
    std::shared_ptr<People> people = std::make_shared<People>();

    Peopled() {
        auto made = std::make_unique<TableView>();
        table = made.get();
        table->setModel(people);
        table->setDelegate(std::make_unique<PeopleDelegate>(*people));
        root.setSize({600, 300});
        root.setContent(std::move(made));
        root.layout();
    }
    std::string shown() const {
        std::string out;
        for (std::size_t at = 0; at < table->shownRowCount(); ++at)
            out += (at ? " " : "") + people->names[table->list().modelIndex(at)];
        return out;
    }
    Point header(std::size_t column) const {
        const Widget* h = table->headerWidget(column);
        return h->mapToRoot({h->size().width / 2, h->size().height / 2});
    }
    Point cell(std::size_t row, std::size_t column) const {
        const Widget* c = table->cellWidget(row, column);
        return c->mapToRoot({c->size().width / 2, c->size().height / 2});
    }
};

} // namespace

TEST(aTableSortsByAHeaderClickAndFilters) {
    Peopled p;
    CHECK(p.shown() == "Sara Ali Mina Reza");
    // a sortable column's header: ascending, descending, not
    click(p.root, p.header(0));
    CHECK(p.shown() == "Ali Mina Reza Sara" && p.table->sortColumn() == 0u && p.table->sortOrder() == SortOrder::Ascending);
    click(p.root, p.header(0));
    CHECK(p.shown() == "Sara Reza Mina Ali" && p.table->sortOrder() == SortOrder::Descending);
    click(p.root, p.header(0));
    CHECK(p.shown() == "Sara Ali Mina Reza" && !p.table->sortColumn());
    // not sortable: nothing
    click(p.root, p.header(2));
    CHECK(!p.table->sortColumn());
    // filtered, and sorted: the delegate binds the model's rows still
    p.table->sortBy(0);
    p.table->setFilter([&](std::size_t r) { return p.people->coming[r]; });
    p.root.layout();
    CHECK(p.shown() == "Mina Sara");
    CHECK(static_cast<Label*>(p.table->cellWidget(2, 0))->text().source == "Mina");
}

TEST(aTableCellIsEditedInWhateverWidgetItsDelegateMakes) {
    Peopled p;
    p.table->list().focus();
    // a double click on a name: a field
    click(p.root, p.cell(1, 0), 2);
    CHECK(p.table->editing() && p.table->editedCell() == std::pair<std::size_t, std::size_t>{1, 0});
    CHECK(dynamic_cast<LineEdit*>(p.table->editor()) != nullptr);
    type(p.root, "Arash");
    // Tab: put in, the next editable cell (a check box)
    key(p.root, platform::Key::Tab);
    CHECK(p.people->names[1] == "Arash");
    auto* box = dynamic_cast<CheckBox*>(p.table->editor());
    CHECK(box && !box->checked() && p.table->editedCell() == std::pair<std::size_t, std::size_t>{1, 1});
    box->setChecked(true);
    key(p.root, platform::Key::Enter);
    CHECK(!p.table->editing() && p.people->coming[1]);
    // not editable (the id): nothing
    CHECK(!p.table->edit(1, 2));
    // F2 on the current row: its current cell
    p.table->list().focus();
    key(p.root, platform::Key::F2);
    CHECK(p.table->editing() && p.table->editedCell() == std::pair<std::size_t, std::size_t>{1, 1});
    key(p.root, platform::Key::Escape);
    CHECK(!p.table->editing() && p.people->coming[1]);
    // typing on the current row, the name's cell current: edited, what was typed in its field
    p.table->list().focus();
    p.table->edit(1, 0);
    key(p.root, platform::Key::Escape);
    type(p.root, "N");
    CHECK(p.table->editing() && static_cast<LineEdit*>(p.table->editor())->text() == "N");
    key(p.root, platform::Key::Escape);
    // sorted while edited: the edit goes, nothing put in
    p.table->edit(0, 0);
    type(p.root, "Zed");
    p.table->sortBy(0);
    CHECK(!p.table->editing() && p.people->names[0] == "Sara");
}

namespace {

// a: a1 a2 (a2x), b — editable, sorted by name.
class Nodes final : public TreeModel {
public:
    struct Node {
        std::string name;
        std::vector<std::size_t> children;
    };
    std::vector<Node> nodes{{"a", {1, 2}}, {"a1", {}}, {"a2", {3}}, {"a2x", {}}, {"b", {}}};
    std::vector<std::size_t> top{0, 4};
    std::size_t childCount(std::optional<TreeNode> parent) const override { return parent ? nodes[parent->id].children.size() : top.size(); }
    TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
        return {parent ? nodes[parent->id].children[index] : top[index]};
    }
    ItemFlags flags(TreeNode) const override { return ItemFlags::Editable; }
    std::partial_ordering compare(TreeNode a, TreeNode b) const override { return nodes[a.id].name <=> nodes[b.id].name; }
    void rename(TreeNode node) {
        // its place among its siblings
        for (std::size_t p = 0; p < nodes.size(); ++p) {
            const auto& c = nodes[p].children;
            if (const auto it = std::find(c.begin(), c.end(), node.id); it != c.end()) {
                notifyChanged(TreeNode{p}, static_cast<std::size_t>(it - c.begin()), 1);
                return;
            }
        }
        const auto it = std::find(top.begin(), top.end(), node.id);
        notifyChanged(std::nullopt, static_cast<std::size_t>(it - top.begin()), 1);
    }
};

struct Treed {
    UiRoot root{tests::testContext()};
    TreeView* tree = nullptr;
    std::shared_ptr<Nodes> nodes = std::make_shared<Nodes>();

    Treed() {
        auto made = std::make_unique<TreeView>();
        tree = made.get();
        tree->setModel(nodes);
        auto delegate = std::make_unique<TreeTextDelegate>([n = nodes.get()](TreeNode node) { return LocalizedText(n->nodes[node.id].name); });
        delegate->setOnEdited([n = nodes.get()](TreeNode node, std::string name) {
            n->nodes[node.id].name = std::move(name);
            n->rename(node);
        });
        tree->setDelegate(std::move(delegate));
        root.setSize({300, 400});
        root.setContent(std::move(made));
        root.layout();
    }
    std::string shown() const {
        std::string out;
        for (std::size_t i = 0; i < tree->shownCount(); ++i)
            out += (i ? " " : "") + std::string(tree->depthOf(i), '.') + nodes->nodes[tree->shownNode(i).id].name;
        return out;
    }
};

} // namespace

TEST(aTreeSortsItsChildrenAndFiltersItsNodes) {
    Treed t;
    t.tree->setExpanded({0}, true);
    t.tree->setExpanded({2}, true);
    CHECK(t.shown() == "a .a1 .a2 ..a2x b");
    t.tree->setSortOrder(SortOrder::Descending);
    CHECK(t.shown() == "b a .a2 ..a2x .a1");
    t.tree->setSortOrder(std::nullopt);
    // filtered: what it accepts, with its ancestors, opened to show it
    t.tree->collapseAll();
    t.tree->setFilter([&](TreeNode n) { return t.nodes->nodes[n.id].name == "a2x"; });
    CHECK(t.shown() == "a .a2 ..a2x");
    // an ancestor closed by the user: closed
    t.tree->setExpanded({2}, false);
    CHECK(t.shown() == "a .a2");
    t.tree->setFilter({});
    CHECK(t.shown() == "a b");
    // in a batch: sorted again once, at its end
    t.tree->setSortOrder(SortOrder::Ascending);
    t.nodes->beginChanges();
    t.nodes->nodes[0].name = "z";
    t.nodes->rename({0});
    t.nodes->nodes[4].name = "c";
    t.nodes->rename({4});
    CHECK(t.shown() == "z c");                  // in the order it was, their new names, until the batch ends
    t.nodes->endChanges();
    CHECK(t.shown() == "c z");
}

TEST(aTreesNodeIsEditedAfterItsArrow) {
    Treed t;
    t.tree->list().focus();
    t.tree->setCurrent({4});
    key(t.root, platform::Key::F2);
    CHECK(t.tree->editing());
    // the field after the indent and the arrow, not over them
    const Widget* row = t.tree->list().rowWidget(1);
    const Point fieldAt = t.tree->editor()->mapToRoot({});
    CHECK(fieldAt.x > row->mapToRoot({}).x + 10);
    type(t.root, "bee");
    key(t.root, platform::Key::Enter);
    CHECK(!t.tree->editing() && t.nodes->nodes[4].name == "bee" && t.shown() == "a bee");
}

namespace {

// Numbers, changed as a live feed changes: inserted, removed and changed anywhere, one or many at a time.
class Values final : public ListModel {
public:
    std::vector<int> values;
    std::size_t count() const override { return values.size(); }
    std::partial_ordering compare(std::size_t a, std::size_t b) const override { return values[a] <=> values[b]; }
    void insert(std::size_t at, std::vector<int> added) {
        values.insert(values.begin() + static_cast<std::ptrdiff_t>(at), added.begin(), added.end());
        notifyInserted(at, added.size());
    }
    void remove(std::size_t at, std::size_t n) {
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(at), values.begin() + static_cast<std::ptrdiff_t>(at + n));
        notifyRemoved(at, n);
    }
    void change(std::size_t at, std::vector<int> now) {
        std::copy(now.begin(), now.end(), values.begin() + static_cast<std::ptrdiff_t>(at));
        notifyChanged(at, now.size());
    }
};

// A thousand random changes of a list sorted (`order`) and filtered or not: after each, the order must be what a sort and
// a filter from nothing give.
bool followsItemByItem(unsigned seed, std::optional<SortOrder> order, bool filtered, bool batched = false) {
    UiRoot root(tests::testContext());
    auto values = std::make_shared<Values>();
    for (int i = 0; i < 300; ++i)
        values->values.push_back((i * 37) % 101);
    auto made = std::make_unique<ListView>();
    ListView& list = *made;
    list.setModel(values);
    root.setSize({200, 200});
    root.setContent(std::move(made));
    list.setSortOrder(order);
    const auto accepts = [&](std::size_t i) { return values->values[i] % 3 != 0; };
    if (filtered)
        list.setFilter(accepts);
    root.layout();
    const auto expected = [&] {
        std::vector<std::size_t> want;
        for (std::size_t i = 0; i < values->values.size(); ++i) {
            if (!filtered || accepts(i))
                want.push_back(i);
        }
        if (order) {
            std::stable_sort(want.begin(), want.end(), [&](std::size_t a, std::size_t b) {
                return *order == SortOrder::Ascending ? values->values[a] < values->values[b] : values->values[a] > values->values[b];
            });
        }
        return want;
    };
    const auto matches = [&] {
        const std::vector<std::size_t> want = expected();
        if (list.shownCount() != want.size())
            return false;
        for (std::size_t at = 0; at < want.size(); ++at) {
            if (list.modelIndex(at) != want[at] || list.position(want[at]) != at)
                return false;
        }
        return true;
    };
    if (!matches())
        return false;
    std::mt19937 random(seed);
    const auto any = [&](std::size_t n) { return static_cast<std::size_t>(random() % std::max<std::size_t>(1, n)); };
    const auto numbers = [&](std::size_t n) {
        std::vector<int> made(n);
        for (int& v : made)
            v = static_cast<int>(random() % 101);
        return made;
    };
    bool all = true;
    int left = 0;                   // batched: the changes left in the batch
    for (int step = 0; step < 1000 && all; ++step) {
        if (batched && left == 0) {
            // a few changes (each placed where it goes), or many (sorted among themselves, merged in one pass)
            left = random() % 4 == 0 ? 60 + static_cast<int>(random() % 80) : 1 + static_cast<int>(random() % 25);
            values->beginChanges();
        }
        const std::size_t n = values->values.size();
        switch (random() % 5) {
        case 0: values->insert(any(n + 1), numbers(1 + any(3))); break;
        case 1: values->insert(any(n + 1), numbers(40 + any(30))); break;        // many at once: merged
        case 2:
            if (n > 10) {
                const std::size_t at = any(n - 5);
                values->remove(at, 1 + any(4));
            }
            break;
        default:
            if (n > 0) {
                const std::size_t at = any(n);
                values->change(at, numbers(std::min<std::size_t>(n - at, 1 + any(3))));
            }
            break;
        }
        if (batched && --left > 0)
            continue;       // in the batch: what shows meanwhile is not asked
        if (batched)
            values->endChanges();
        all = matches();
    }
    if (left > 0)
        values->endChanges();
    if (!all || !matches())
        return false;
    // the current item followed, by the model's index: through an insert before it, and moved by a change
    list.setCurrent(list.modelIndex(5));
    const int value = values->values[*list.current()];
    values->insert(0, {1000});
    if (!list.current() || values->values[*list.current()] != value)
        return false;
    if (!order)
        return true;            // not sorted: a change does not move it
    values->change(*list.current(), {order == SortOrder::Ascending ? -1 : 1001});     // to the top, still current
    return list.current() && list.position(*list.current()) == 0u;
}

} // namespace

TEST(aSortedFilteredListFollowsItsModelItemByItem) {
    for (unsigned seed : {7u, 11u, 23u}) {
        CHECK(followsItemByItem(seed, SortOrder::Descending, true));
        CHECK(followsItemByItem(seed, SortOrder::Ascending, false));
        CHECK(followsItemByItem(seed, std::nullopt, true));
        CHECK(followsItemByItem(seed, SortOrder::Ascending, true, true));
        CHECK(followsItemByItem(seed, SortOrder::Descending, false, true));
    }
}

TEST(itemViewsShowTheirCellsBordersAsTheirStylesSay) {
    // by default: a table its cells' borders, a list and a tree the lines between their rows
    Fruit f;
    CHECK(f.list->style().rowLines && !f.list->style().columnLines);
    Peopled p;
    CHECK(p.table->style().rowLines && p.table->style().columnLines);
    CHECK(p.table->list().style().rowLines);                // its rows drawn with the table's style
    // their own: a list without lines; a table without borders
    ItemViewStyle bare = tests::testContext().theme.itemView;
    bare.rowLines = false;
    f.list->setStyle(bare);
    CHECK(!f.list->style().rowLines);
    ItemViewStyle plain = tests::testContext().theme.table;
    plain.rowLines = plain.columnLines = false;
    p.table->setStyle(plain);
    CHECK(!p.table->style().columnLines && !p.table->list().style().rowLines);
    // back to the theme's
    p.table->setStyle(std::nullopt);
    CHECK(p.table->list().style().rowLines);
}

TEST(aListsHeaderSortsItsItemsAndHidesAtWill) {
    Fruit f;
    CHECK(!f.list->headerShown());                          // no title: no header
    const float h = f.list->style().rowHeight;
    CHECK(f.list->itemAt({50, h / 2}) == 0u);
    f.list->setHeader("Fruit");
    f.root.layout();
    CHECK(f.list->headerShown() && !f.list->itemAt({50, h / 2}));   // the rows under it
    CHECK(f.list->itemAt({50, h * 1.5f}) == 0u);
    // a click: ascending, then descending, then as the model has them
    click(f.root, {100, h / 2});
    CHECK(f.shown() == "Apple banana fig pear" && f.list->sortOrder() == SortOrder::Ascending);
    click(f.root, {100, h / 2});
    CHECK(f.shown() == "pear fig banana Apple");
    click(f.root, {100, h / 2});
    CHECK(f.shown() == "pear Apple fig banana" && !f.list->sortOrder());
    // sorting off: a click does nothing
    f.list->setSortingEnabled(false);
    click(f.root, {100, h / 2});
    CHECK(!f.list->sortOrder());
    // hidden: the rows where it was
    f.list->setHeaderVisible(false);
    f.root.layout();
    CHECK(!f.list->headerShown() && f.list->itemAt({50, h / 2}) == 0u);
}

namespace {

// docs: b a c, zip — a name and a size, sorted by either; the size editable.
class Files final : public TreeModel {
public:
    struct File {
        std::string name;
        int size = 0;
        std::vector<std::size_t> children;
    };
    std::vector<File> files{{"docs", 13, {1, 2, 3}}, {"b", 3, {}}, {"a", 9, {}}, {"c", 1, {}}, {"zip", 5, {}}};
    std::vector<std::size_t> top{0, 4};
    std::size_t childCount(std::optional<TreeNode> parent) const override { return parent ? files[parent->id].children.size() : top.size(); }
    TreeNode child(std::optional<TreeNode> parent, std::size_t index) const override {
        return {parent ? files[parent->id].children[index] : top[index]};
    }
    std::size_t columnCount() const override { return 2; }
    LocalizedText header(std::size_t column) const override { return column == 0 ? "Name" : "Size"; }
    ItemFlags flags(TreeNode, std::size_t column) const override { return column == 1 ? ItemFlags::Editable : ItemFlags::None; }
    bool sortable(std::size_t) const override { return true; }
    std::partial_ordering compare(TreeNode a, TreeNode b, std::size_t column) const override {
        if (column == 1)
            return files[a.id].size <=> files[b.id].size;
        return files[a.id].name <=> files[b.id].name;
    }
    void resized(TreeNode node) {
        const auto& c = files[0].children;
        if (const auto it = std::find(c.begin(), c.end(), node.id); it != c.end())
            notifyChanged(TreeNode{0}, static_cast<std::size_t>(it - c.begin()), 1);
        else
            notifyChanged(std::nullopt, node.id == 0 ? 0 : 1, 1);
    }
};

struct Filed {
    UiRoot root{tests::testContext()};
    TreeView* tree = nullptr;
    std::shared_ptr<Files> files = std::make_shared<Files>();

    Filed() {
        auto made = std::make_unique<TreeView>();
        tree = made.get();
        tree->setModel(files);
        auto delegate = std::make_unique<TreeTextDelegate>([f = files.get()](TreeNode node, std::size_t column) {
            return LocalizedText(column == 0 ? f->files[node.id].name : std::to_string(f->files[node.id].size));
        });
        delegate->setOnEdited([f = files.get()](TreeNode node, std::size_t column, std::string text) {
            if (column == 1) {
                f->files[node.id].size = std::stoi(text);
                f->resized(node);
            }
        });
        tree->setDelegate(std::move(delegate));
        tree->setColumnWidth(0, 150);
        tree->setColumnWidth(1, 100);
        tree->expandAll();
        root.setSize({300, 400});
        root.setContent(std::move(made));
        root.layout();
    }
    std::string shown() const {
        std::string out;
        for (std::size_t i = 0; i < tree->shownCount(); ++i)
            out += (i ? " " : "") + std::string(tree->depthOf(i), '.') + files->files[tree->shownNode(i).id].name;
        return out;
    }
    void clickHeader(std::size_t column) {
        const Widget* header = tree->table().headerWidget(column);
        const Point at = header->mapToRoot({});
        click(root, {at.x + header->size().width / 2, at.y + header->size().height / 2});
    }
};

} // namespace

TEST(aTreeOfColumnsShowsThemUnderAHeaderAndSortsByThem) {
    // a tree of one untitled column: no header
    Treed one;
    CHECK(!one.tree->headerShown());
    Filed t;
    CHECK(t.tree->headerShown() && t.tree->table().headerWidget(0) && t.tree->table().headerWidget(1));
    CHECK(t.shown() == "docs .b .a .c zip");
    // each cell bound to its column
    const Widget* cell = t.tree->table().cellWidget(1, 1);
    const auto* size = cell && !cell->children().empty() ? dynamic_cast<const Label*>(cell->children()[0].get()) : nullptr;
    CHECK(size && size->text().source == "3");
    // a header clicked: each node's children sorted by its column, ascending, descending, then not
    t.clickHeader(1);
    CHECK(t.shown() == "zip docs .c .b .a" && t.tree->sortColumn() == 1u);
    t.clickHeader(1);
    CHECK(t.shown() == "docs .a .b .c zip" && t.tree->sortOrder() == SortOrder::Descending);
    t.clickHeader(1);
    CHECK(t.shown() == "docs .b .a .c zip" && !t.tree->sortColumn());
    t.tree->sortBy(0);
    CHECK(t.shown() == "docs .a .b .c zip");
    t.tree->sortBy(std::nullopt);
    // a column of its own edited: the size, the name not
    t.tree->list().focus();
    CHECK(!t.tree->edit({1}, 0));
    CHECK(t.tree->edit({1}, 1) && t.tree->editor());
    type(t.root, "42");
    key(t.root, platform::Key::Enter);
    CHECK(!t.tree->editing() && t.files->files[1].size == 42);
    // hidden at will: the rows where it was
    const auto top = [&] { return t.tree->list().rowWidget(0)->mapToRoot({}).y - t.tree->mapToRoot({}).y; };
    const float under = top();
    CHECK(under > 10);
    t.tree->setHeaderVisible(false);
    t.root.layout();
    CHECK(!t.tree->headerShown() && top() < under - 10);
    t.tree->setHeaderVisible(true);
    t.root.layout();
    CHECK(t.tree->headerShown() && top() == under);
}

TEST(aColumnFitsWhatItShowsByADoubleClickOnItsHeadersEdge) {
    Peopled p;
    p.table->setColumnWidth(0, 300);
    p.root.layout();
    // the widest of its cells, and its header's
    float widest = 0;
    for (std::size_t row = 0; row < 4; ++row)
        widest = std::max(widest, p.table->cellWidget(row, 0)->measure({unbounded, 100}).width);
    widest = std::max(widest, p.table->headerWidget(0)->measure({unbounded, 100}).width);
    const float fitted = p.table->contentWidth(0);
    CHECK(fitted >= widest && fitted < 300);
    // a double click on its edge
    const Widget* h = p.table->headerWidget(0);
    const Point edge = h->mapToRoot({h->size().width, h->size().height / 2});
    click(p.root, edge);
    click(p.root, edge, 2);
    CHECK(p.table->columnWidth(0) == fitted && !p.table->sortColumn());
    // a longer name: wider
    p.people->names[2] = "Mina Mohammadzadeh Tabrizi";
    p.people->changed(2);
    p.root.layout();
    CHECK(p.table->contentWidth(0) > fitted);
    // of more rows than it measures: those that show, and some across them
    p.table->setFitRows(2);
    CHECK(p.table->contentWidth(0) > fitted);
}

TEST(aTreesColumnFitsItsNodesWithTheirIndentsAndArrows) {
    Filed t;
    float widest = 0;
    for (std::size_t row = 0; row < t.tree->shownCount(); ++row)
        widest = std::max(widest, t.tree->table().cellWidget(row, 0)->measure({unbounded, 100}).width);
    t.tree->fitColumn(0);
    t.root.layout();
    CHECK(t.tree->columnWidth(0) >= widest && t.tree->columnWidth(0) < 150);
}

TEST(aTouchpadScrollsAListAsTheFingersGoAWheelByWholeRows) {
    UiRoot root{tests::testContext()};
    auto made = std::make_unique<ListView>();
    ListView* list = made.get();
    list->setModel(std::make_shared<StringListModel>(std::vector<LocalizedText>(200, LocalizedText("x"))));
    root.setSize({300, 300});
    root.setContent(std::move(made));
    root.layout();
    const double h = list->style().rowHeight;
    // by fractions of a step: as they come, not snapped to the rows
    root.dispatch(platform::MouseWheelScrolled{1, {0, -0.1f}, {100, 100}, true});
    root.layout();
    const double one = list->offset();
    CHECK(one > 0 && std::fmod(one, h) > 0.5);
    root.dispatch(platform::MouseWheelScrolled{1, {0, -0.1f}, {100, 100}, true});
    root.layout();
    CHECK(std::abs(list->offset() - 2 * one) < 0.01);
    // a wheel's step: to a row's top
    root.dispatch(platform::MouseWheelScrolled{1, {0, -1}, {100, 100}});
    root.layout();
    CHECK(std::abs(std::fmod(list->offset(), h)) < 0.01 && list->offset() > 2 * one);
}
