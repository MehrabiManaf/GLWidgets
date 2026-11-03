// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Item views: a list of a million items with widgets for what shows only, scrolled, chosen by the mouse and the keys; rows
// of their own heights; the model's changes followed.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/ListView.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace glw;

namespace {

bool near(double a, double b) {
    return std::abs(a - b) < 0.5;
}

// Numbers 0 to count - 1, made when asked: no memory per item.
class Numbers final : public ListModel {
public:
    explicit Numbers(std::size_t count) : count_(count) {}
    std::size_t count() const override { return count_; }
    std::string text(std::size_t index) const { return "Item " + std::to_string(index + shift_); }
    void insert(std::size_t at, std::size_t count) {
        count_ += count;
        notifyInserted(at, count);
    }
    void remove(std::size_t at, std::size_t count) {
        count_ -= count;
        notifyRemoved(at, count);
    }
    void renumber(std::size_t shift) {
        shift_ = shift;
        notifyChanged(0, count_);
    }

private:
    std::size_t count_;
    std::size_t shift_ = 0;
};

// The text a row shows.
std::string shown(const ListView& list, std::size_t index) {
    const Widget* row = list.rowWidget(index);
    return row ? static_cast<const Label&>(*row->children()[0]).shownText() : std::string("(not shown)");
}

struct Listed {
    UiRoot root{tests::testContext()};
    ListView* list = nullptr;
    std::shared_ptr<Numbers> numbers;

    explicit Listed(std::size_t count) : numbers(std::make_shared<Numbers>(count)) {
        auto made = std::make_unique<ListView>();
        list = made.get();
        list->setModel(numbers);
        list->setDelegate(std::make_unique<TextDelegate>([n = numbers.get()](std::size_t i) { return LocalizedText(n->text(i)); }));
        root.setSize({300, 400});
        root.setContent(std::move(made));
        root.layout();
    }

    void click(Point at, int clicks = 1) {
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, clicks, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, clicks, at});
        root.layout();
    }
    void key(platform::Key k) {
        root.dispatch(platform::KeyChanged{1, k, platform::KeyModifiers::None, true, false});
        root.layout();
    }
};

} // namespace

TEST(aListOfAMillionItemsHasWidgetsForWhatShowsOnly) {
    Listed l(1'000'000);
    const float row = l.list->style().rowHeight;
    const auto [first, last] = l.list->shownItems();
    CHECK(first == 0 && last == static_cast<std::size_t>(std::ceil(400 / row)));
    CHECK(l.list->children().size() == last - first);       // a widget for each that shows, no more
    CHECK(shown(*l.list, 0) == "Item 0");
    CHECK(near(l.list->maximumOffset(), 1'000'000.0 * row - 400));

    // to the middle: those there show, in the same widgets
    l.list->setOffset(500'000.0 * row);
    l.root.layout();
    CHECK(l.list->shownItems().first == 500'000 && shown(*l.list, 500'000) == "Item 500000");
    CHECK(l.list->children().size() <= static_cast<std::size_t>(std::ceil(400 / row)) + 1);
    const std::optional<Rect> at = l.list->itemRect(500'001);
    CHECK(at && near(at->y, row));
    paint::DrawList drawn;
    l.root.paint(drawn);
    CHECK(!drawn.vertices.empty());
}

TEST(aListIsChosenFromByTheMouseAndTheKeys) {
    Listed l(1000);
    std::vector<std::size_t> activated;
    l.list->setOnActivated([&](std::size_t i) { activated.push_back(i); });
    const float row = l.list->style().rowHeight;
    // a click: current and selected; twice: activated
    l.click({50, row * 2.5f});
    CHECK(l.list->current() == 2u && l.list->isSelected(2) && l.list->hasFocus());
    l.click({50, row * 3.5f}, 2);
    CHECK(l.list->current() == 3u && activated == std::vector<std::size_t>{3});
    // the keys: one down, a page down, to the end (scrolled there), Enter
    l.key(platform::Key::Down);
    CHECK(l.list->current() == 4u);
    l.key(platform::Key::PageDown);
    CHECK(*l.list->current() > 10);
    l.key(platform::Key::End);
    CHECK(l.list->current() == 999u && l.list->itemRect(999) && near(l.list->offset(), l.list->maximumOffset()));
    l.key(platform::Key::Enter);
    CHECK(activated.back() == 999);
    l.key(platform::Key::Home);
    CHECK(l.list->current() == 0u && near(l.list->offset(), 0));
}

TEST(rowsOfTheirOwnHeights) {
    UiRoot root(tests::testContext());
    root.setSize({300, 400});
    // a row of 20, 40 or 60 dips, by its item
    class Sized final : public ListDelegate {
    public:
        std::unique_ptr<Widget> makeRow() override { return std::make_unique<Widget>(); }
        void bindRow(Widget& row, std::size_t index) override { row.setMinimumSize({10, 20.0f + static_cast<float>(index % 3) * 20}); }
    };
    auto numbers = std::make_shared<Numbers>(10'000);
    auto made = std::make_unique<ListView>();
    ListView& list = *made;
    list.setModel(numbers);
    list.setDelegate(std::make_unique<Sized>());
    list.setRowHeight(0.0f);
    root.setContent(std::move(made));
    root.layout();
    // those that show: measured, one after the other
    const auto [first, last] = list.shownItems();
    CHECK(first == 0 && last > 5);
    for (std::size_t i = 1; i < last; ++i) {
        const std::optional<Rect> a = list.itemRect(i - 1), b = list.itemRect(i);
        CHECK(a && b && near(a->bottom(), b->y) && near(a->height, 20.0f + static_cast<float>((i - 1) % 3) * 20));
    }
    // far down: estimated before, measured as they show; the row at the offset stays where it is
    list.setOffset(100'000);
    root.layout();
    const std::size_t top = list.shownItems().first;
    const Rect before = *list.itemRect(top);
    list.setOffset(list.offset() + 1);
    list.setOffset(list.offset() - 1);
    root.layout();
    CHECK(list.shownItems().first == top && near(list.itemRect(top)->y, before.y));
    // to the end: the last item shows at the bottom, however its rows measure
    list.setOffset(1e12);
    root.layout();
    const std::optional<Rect> end = list.itemRect(9'999);
    CHECK(end && near(end->bottom(), 400));
}

TEST(aListFollowsItsModel) {
    Listed l(100);
    const float row = l.list->style().rowHeight;
    l.list->setOffset(20 * row);
    l.root.layout();
    l.list->setCurrent(25);
    l.root.layout();
    CHECK(l.list->shownItems().first == 20 && shown(*l.list, 20) == "Item 20");

    // ten inserted before what shows: the same items show where they were (now numbered on), the current one too
    l.numbers->insert(5, 10);
    l.root.layout();
    CHECK(l.list->shownItems().first == 30 && l.list->current() == 35u);
    CHECK(near(l.list->itemRect(30)->y, 0));
    // removed with the current one: the one in its place is current
    l.numbers->remove(33, 5);
    l.root.layout();
    CHECK(l.list->current() == 33u);
    // changed: shown again
    l.numbers->renumber(1000);
    l.root.layout();
    CHECK(shown(*l.list, 30) == "Item 1030");
    // all removed: none current, nothing shows
    l.numbers->remove(0, l.numbers->count());
    l.root.layout();
    CHECK(!l.list->current() && l.list->shownItems().first == l.list->shownItems().second);
}

TEST(aStringListShowsItsTexts) {
    UiRoot root(tests::testContext());
    root.setSize({300, 200});
    auto texts = std::make_shared<StringListModel>(std::vector<LocalizedText>{"One", "Two", "Three"});
    auto made = std::make_unique<ListView>();
    ListView& list = *made;
    list.setModel(texts);
    root.setContent(std::move(made));
    root.layout();
    CHECK(shown(list, 1) == "Two");
    texts->set(1, "Second");
    texts->append("Four");
    root.layout();
    CHECK(shown(list, 1) == "Second" && shown(list, 3) == "Four");
}
