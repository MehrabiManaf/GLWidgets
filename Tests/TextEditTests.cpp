// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Text edits: typing, Enter keeping the indent, Tab indenting, Backspace in the indent, lines joined; undo and redo (what
// is typed one character after the other undone as one); the caret moved by the keys and the mouse; the clipboard; a
// million lines laid out as far as they show; a log appended to, following its end; C++ highlighted, comments over lines.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Paint/DrawList.hpp>
#include <GLWidgets/Widgets/Highlighter.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace glw;

namespace {

struct Edited {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    TextEdit* edit = nullptr;

    explicit Edited(std::string_view text = "") {
        auto made = std::make_unique<TextEdit>();
        edit = made.get();
        edit->setText(text);
        root.setSize({600, 400});
        root.setContent(std::move(made));
        root.layout();
        edit->focus();
    }
    void key(platform::Key k, platform::KeyModifiers m = platform::KeyModifiers::None) {
        root.dispatch(platform::KeyChanged{1, k, m, true, false});
        root.layout();
    }
    void type(const std::string& text) {
        root.dispatch(platform::TextInput{1, text});
        root.layout();
    }
    void click(Point at, int clicks = 1) {
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, clicks, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, clicks, at});
    }
};

constexpr auto shortcut = platform::shortcutModifier;
constexpr auto word = platform::wordModifier;
constexpr auto shift = platform::KeyModifiers::Shift;

} // namespace

TEST(textIsTypedAndUndoneByWhatWasTypedTogether) {
    Edited e("ab\ncd");
    CHECK(e.edit->lineCount() == 2 && e.edit->line(1) == "cd");
    e.edit->select({0, 1}, {0, 1});
    e.type("X");
    e.type("Y");
    CHECK(e.edit->text() == "aXYb\ncd" && e.edit->caret() == (TextPosition{0, 3}));
    // the two characters undone as one; done again
    e.key(platform::Key::Z, shortcut);
    CHECK(e.edit->text() == "ab\ncd" && e.edit->caret() == (TextPosition{0, 1}));
    e.key(platform::Key::Y, shortcut);
    CHECK(e.edit->text() == "aXYb\ncd");
    // Delete at a line's end joins it with the next; Backspace at a line's start joins it with the one before
    e.key(platform::Key::End);
    e.key(platform::Key::Delete);
    CHECK(e.edit->text() == "aXYbcd" && e.edit->lineCount() == 1 && e.edit->caret() == (TextPosition{0, 4}));
    e.key(platform::Key::End);
    e.type("\r\nz");                                        // typed line ends: \n
    CHECK(e.edit->text() == "aXYbcd\nz");
    e.key(platform::Key::Home);
    e.key(platform::Key::Backspace);
    CHECK(e.edit->text() == "aXYbcdz");
    // over a selection: replaced
    e.edit->select({0, 0}, {0, 3});
    e.type("Q");
    CHECK(e.edit->text() == "Qbcdz");
    CHECK(e.edit->canUndo() && !e.edit->canRedo());
}

TEST(enterKeepsTheIndentAndTabIndents) {
    Edited e("    if (x)");
    e.key(platform::Key::End);
    e.key(platform::Key::Enter);
    CHECK(e.edit->text() == "    if (x)\n    " && e.edit->caret() == (TextPosition{1, 4}));
    // Tab: to the next stop; Backspace in the indent: back to the stop before
    e.key(platform::Key::Tab);
    CHECK(e.edit->line(1) == "        ");
    e.type("y");
    e.key(platform::Key::Home);
    e.key(platform::Key::Backspace);
    CHECK(e.edit->line(1) == "    y");
    // lines selected: indented and unindented together, undone as one
    e.edit->select({0, 2}, {1, 1});
    e.key(platform::Key::Tab);
    CHECK(e.edit->line(0) == "        if (x)" && e.edit->line(1) == "        y");
    e.key(platform::Key::Tab, shift);
    e.key(platform::Key::Tab, shift);
    CHECK(e.edit->line(0) == "if (x)" && e.edit->line(1) == "y");
    e.key(platform::Key::Z, shortcut);
    CHECK(e.edit->line(0) == "    if (x)");
}

TEST(theCaretMovesByTheKeysAndTheMouse) {
    Edited e("int count = 42;\n    return count;\nx");
    // by words
    e.key(platform::Key::Right, word);
    CHECK(e.edit->caret() == (TextPosition{0, 3}));
    e.key(platform::Key::Right, word);
    CHECK(e.edit->caret() == (TextPosition{0, 9}));
    e.key(platform::Key::Left, word);
    CHECK(e.edit->caret() == (TextPosition{0, 4}));
    // Home: the first character that is not a space, then the start
    e.key(platform::Key::Down);
    e.key(platform::Key::Home);
    CHECK(e.edit->caret() == (TextPosition{1, 4}));
    e.key(platform::Key::Home);
    CHECK(e.edit->caret() == (TextPosition{1, 0}));
    // Shift: selected; Ctrl+End, Ctrl+Home
    e.key(platform::Key::End, shift);
    CHECK(e.edit->hasSelection() && e.edit->text(e.edit->selection()) == "    return count;");
    e.key(platform::Key::End, shortcut);
    CHECK(e.edit->caret() == (TextPosition{2, 1}) && !e.edit->hasSelection());
    e.key(platform::Key::Home, shortcut);
    CHECK(e.edit->caret() == (TextPosition{}));
    // the mouse: where a position shows is where a click puts the caret; a double click, a word
    const Point at = e.edit->mapToRoot(e.edit->pointOf({1, 7}));
    e.click({at.x + 1, at.y + 4});
    CHECK(e.edit->caret() == (TextPosition{1, 7}));
    e.click({at.x + 1, at.y + 4}, 2);
    CHECK(e.edit->text(e.edit->selection()) == "return");
    // the clipboard
    e.key(platform::Key::C, shortcut);
    CHECK(e.host.clipboard == "return");
    e.key(platform::Key::End, shortcut);
    e.key(platform::Key::V, shortcut);
    CHECK(e.edit->line(2) == "xreturn");
}

TEST(aMillionLinesAreLaidOutAsFarAsTheyShow) {
    std::string big;
    for (int i = 0; i < 1'000'000; ++i)
        big += "line " + std::to_string(i) + "\n";
    Edited e(big);
    CHECK(e.edit->lineCount() == 1'000'001);
    paint::DrawList list;
    e.root.paint(list);
    const auto [first, last] = e.edit->shownLines();
    CHECK(first == 0 && last > 10 && last < 40);
    CHECK(e.edit->laidOutLines() <= last - first + 2);
    // the middle: shown there, not much more kept
    e.edit->select({500'000, 0}, {500'000, 0});
    e.root.paint(list);
    const auto [from, to] = e.edit->shownLines();
    CHECK(from <= 500'000 && to > 500'000);
    CHECK(e.edit->laidOutLines() < 120);
    // to the end, typing there
    e.key(platform::Key::End, shortcut);
    e.type("end");
    CHECK(e.edit->line(1'000'000) == "end");
}

TEST(aLogFollowsItsEndWhileItIsThere) {
    Edited e;
    e.edit->setReadOnly(true);
    for (int i = 0; i < 100; ++i)
        e.edit->append("message " + std::to_string(i));
    e.root.layout();
    CHECK(e.edit->lineCount() == 100 && e.edit->line(0) == "message 0");
    CHECK(e.edit->verticalOffset() > 0 && e.edit->shownLines().second == 100);
    // read only: typing does nothing
    e.type("x");
    CHECK(e.edit->line(99) == "message 99");
    // scrolled up: it stays there as lines come
    e.edit->setVerticalOffset(0);
    e.edit->append("more");
    e.root.layout();
    CHECK(e.edit->verticalOffset() == 0 && e.edit->lineCount() == 101);
}

TEST(cppIsHighlightedCommentsGoingOverLines) {
    const CppHighlighter cpp;
    std::vector<TokenSpan> spans;
    CHECK(cpp.highlight("int x = 42; // the answer", 0, spans) == 0);
    CHECK(spans.size() == 3);
    CHECK(spans[0].kind == TokenKind::Type && spans[0].begin == 0 && spans[0].end == 3);
    CHECK(spans[1].kind == TokenKind::Number && spans[1].begin == 8 && spans[1].end == 10);
    CHECK(spans[2].kind == TokenKind::Comment && spans[2].begin == 12);
    spans.clear();
    CHECK(cpp.highlight("return f(\"a \\\" b\"); /* goes", 0, spans) == 1);
    CHECK(spans[0].kind == TokenKind::Keyword && spans[1].kind == TokenKind::Function && spans[2].kind == TokenKind::String);
    CHECK(spans[2].end == 17 && spans[3].kind == TokenKind::Comment);
    spans.clear();
    CHECK(cpp.highlight("on */ if", 1, spans) == 0);
    CHECK(spans[0].kind == TokenKind::Comment && spans[0].end == 5 && spans[1].kind == TokenKind::Keyword);
    spans.clear();
    CHECK(cpp.highlight("#include <vector> // here", 0, spans) == 0);
    CHECK(spans[0].kind == TokenKind::Preprocessor && spans[1].kind == TokenKind::Comment);
}
