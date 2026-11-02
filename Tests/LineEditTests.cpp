// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The text field: typing, deleting, selecting, the clipboard, moving on the screen through Persian and English, an input
// method's composition, the mouse.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <string>

using namespace glw;
using platform::Key;
using platform::KeyModifiers;

namespace {

// A field filling a root of its own, with the focus.
struct Field {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    LineEdit* edit = nullptr;
    int changes = 0;
    std::string submitted;

    explicit Field(std::string text = {}) {
        auto e = std::make_unique<LineEdit>(LineEdit::Props{
            .text = std::move(text),
            .onChanged = [this](const std::string&) { ++changes; },
            .onSubmit = [this](const std::string& t) { submitted = t; },
        });
        edit = e.get();
        root.setSize({400, 40});
        root.setContent(std::move(e));
        root.layout();
        root.setFocus(edit);
    }

    void type(const std::string& text) { root.dispatch(platform::TextInput{1, text}); }
    void key(Key k, KeyModifiers m = KeyModifiers::None) { root.dispatch(platform::KeyChanged{1, k, m, true, false}); }
};

} // namespace

TEST(typingAndDeleting) {
    Field f;
    CHECK(f.host.textInput);            // the focus starts text input
    f.type("سلام");
    f.type(" hi");
    CHECK(f.edit->text() == "سلام hi");
    CHECK(f.edit->caret() == f.edit->text().size());
    CHECK(f.changes == 2);
    f.key(Key::Backspace);
    CHECK(f.edit->text() == "سلام h");
    f.key(Key::Home);
    f.key(Key::Delete);
    CHECK(f.edit->text() == "لام h");   // a whole letter: two bytes
    f.key(Key::Enter);
    CHECK(f.submitted == "لام h");
    f.root.setFocus(nullptr);
    CHECK(!f.host.textInput);
}

TEST(deletingGraphemes) {
    Field f;
    f.type("ae\xCC\x81");       // a, then e with a combining acute accent
    f.key(Key::Backspace);
    CHECK(f.edit->text() == "a");       // the accent goes with its letter
}

TEST(selectionIsReplaced) {
    Field f("hello world");
    f.edit->select(0, 5);
    f.type("bye");
    CHECK(f.edit->text() == "bye world");
    CHECK(f.edit->caret() == 3 && f.edit->anchor() == 3);
}

TEST(clipboard) {
    Field f("one two");
    f.key(Key::A, platform::shortcutModifier);
    CHECK(f.edit->selection() == std::pair<std::size_t, std::size_t>{0, 7});
    f.key(Key::C, platform::shortcutModifier);
    CHECK(f.host.clipboard == "one two");
    f.key(Key::X, platform::shortcutModifier);
    CHECK(f.edit->text().empty());
    f.host.clipboard = "a\r\nb";
    f.key(Key::V, platform::shortcutModifier);
    CHECK(f.edit->text() == "a b");     // one line
}

TEST(arrowsAndShift) {
    Field f("abc");
    f.key(Key::Home);
    CHECK(f.edit->caret() == 0);
    f.key(Key::Right);
    CHECK(f.edit->caret() == 1);
    f.key(Key::Right, KeyModifiers::Shift);
    CHECK(f.edit->caret() == 2 && f.edit->anchor() == 1);
    f.key(Key::Left);       // a selection collapses to its left side
    CHECK(f.edit->caret() == 1 && f.edit->anchor() == 1);
    f.key(Key::End);
    CHECK(f.edit->caret() == 3);
}

TEST(arrowsMoveOnTheScreenInPersian) {
    Field f("سلام");
    // the end of a right to left text is on the left: Right goes back into it, Left goes nowhere
    CHECK(f.edit->caret() == 8);
    f.key(Key::Left);
    CHECK(f.edit->caret() == 8);
    f.key(Key::Right);
    CHECK(f.edit->caret() == 6);
    f.key(Key::Home);
    f.key(Key::Left);
    CHECK(f.edit->caret() == 2);
}

TEST(words) {
    Field f("one two three");
    f.key(Key::Left, platform::wordModifier);
    CHECK(f.edit->caret() == 8);        // the start of "three"
    f.key(Key::End);
    f.key(Key::Backspace, platform::wordModifier);
    CHECK(f.edit->text() == "one two ");
}

TEST(inputMethodComposition) {
    Field f("x");
    f.root.dispatch(platform::TextComposition{1, "ab", 2, 0});
    CHECK(f.edit->text() == "x");       // composing: not typed yet
    f.key(Key::Backspace);              // the input method's
    CHECK(f.edit->text() == "x");
    f.type("AB");
    CHECK(f.edit->text() == "xAB");
}

TEST(clickPutsTheCaret) {
    Field f("hello world");
    const Point left = f.edit->mapToRoot({f.edit->padding().start + 1, f.edit->size().height / 2});
    f.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, left});
    f.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, left});
    CHECK(f.edit->caret() == 0);
    // a double click selects the word
    f.edit->setText("hello");
    f.root.layout();
    const Point inHello = f.edit->mapToRoot({f.edit->padding().start + 10, f.edit->size().height / 2});
    f.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 2, inHello});
    f.root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 2, inHello});
    CHECK(f.edit->selection() == std::pair<std::size_t, std::size_t>{0, 5});
}
