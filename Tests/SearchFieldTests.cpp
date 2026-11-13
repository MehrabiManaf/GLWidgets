// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/SearchField.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace glw;
using platform::Key;
using platform::KeyModifiers;

namespace {

// A search field filling a root of its own (200 by 30), its field with the focus.
struct Searched {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    SearchField* search = nullptr;
    std::vector<std::string> changes;

    Searched() {
        std::unique_ptr<Widget> s =
            ui::SearchField({.onChanged = [this](const std::string& text) { changes.push_back(text); }}).bind(search);
        root.setSize({200, 30});
        root.setContent(std::move(s));
        root.layout();
        root.setFocus(&search->field());
    }
    void type(const std::string& text) { root.dispatch(platform::TextInput{1, text}); }
    void key(Key k) { root.dispatch(platform::KeyChanged{1, k, KeyModifiers::None, true, false}); }
    void click(Point p) {
        root.dispatch(platform::MouseMoved{1, p});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, p});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, p});
    }
};

} // namespace

TEST(aSearchFieldIsClearedByItsButtonOrByEscape) {
    Searched s;
    // empty: the magnifier, which clears nothing
    s.click({190, 15});
    CHECK(s.changes.empty());
    s.type("node");
    CHECK(s.search->text() == "node" && (s.changes == std::vector<std::string>{"node"}));
    // the button at its end: empty, told, the focus kept in it
    s.click({190, 15});
    CHECK(s.search->text().empty() && (s.changes == std::vector<std::string>{"node", ""}));
    CHECK(s.root.focusWidget() == &s.search->field());
    // a click in the text clears nothing
    s.type("x");
    s.click({20, 15});
    CHECK(s.search->text() == "x");
    // Escape
    s.key(Key::Escape);
    CHECK(s.search->text().empty() && s.changes.back().empty() && s.changes.size() == 4);
    // set: not told
    s.search->setText("set");
    CHECK(s.search->text() == "set" && s.changes.size() == 4);
    s.search->clear();
    CHECK(s.search->text().empty() && s.changes.size() == 5);
}
