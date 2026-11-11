// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/SpinBox.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace glw;
using platform::Key;
using platform::KeyModifiers;

namespace {

// A spin box filling a root of its own (200 by 30), its field with the focus.
struct Spin {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    SpinBox* spin = nullptr;
    std::vector<double> changes;

    explicit Spin(SpinBox::Props props = {}) {
        props.onChanged = [this](double v) { changes.push_back(v); };
        std::unique_ptr<Widget> s = ui::SpinBox(std::move(props)).bind(spin);
        root.setSize({200, 30});
        root.setContent(std::move(s));
        root.layout();
        root.setFocus(&spin->field());
    }
    const std::string& text() const { return spin->field().text(); }
    void retype(const std::string& text) {
        spin->field().selectAll();
        root.dispatch(platform::TextInput{1, text});
    }
    void key(Key k, KeyModifiers m = KeyModifiers::None) { root.dispatch(platform::KeyChanged{1, k, m, true, false}); }
    void press(Point p) { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, p}); }
    void release(Point p) { root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, p}); }
};

} // namespace

TEST(aSpinBoxShowsAndTakesNumbersAsItsDecimalsSay) {
    CHECK(SpinBox::format(12, 0) == "12");
    CHECK(SpinBox::format(1.5, 2) == "1.50");
    CHECK(SpinBox::format(2.5, -1) == "2.5");
    CHECK(SpinBox::format(-0.001, 2) == "0.00");        // no minus zero
    // what may be typed on the way to a number
    CHECK(SpinBox::acceptsTyping("", 0, true) && SpinBox::acceptsTyping("-", 0, true) && SpinBox::acceptsTyping("-12", 0, true));
    CHECK(!SpinBox::acceptsTyping("-1", 0, false));     // nothing below zero: no minus
    CHECK(!SpinBox::acceptsTyping("1.5", 0, true) && SpinBox::acceptsTyping("1.5", 2, true));
    CHECK(!SpinBox::acceptsTyping("1.555", 2, true));   // more decimals than it shows
    CHECK(SpinBox::acceptsTyping("1e-3", -1, true) && !SpinBox::acceptsTyping("1e3", 2, true));
    CHECK(!SpinBox::acceptsTyping("12a", 0, true) && !SpinBox::acceptsTyping("1-2", 0, true));
}

TEST(whatIsTypedIsTakenOnEnterOrWhenTheFocusLeaves) {
    Spin s({.value = 5, .minimum = 0, .maximum = 100});
    CHECK(s.text() == "5");
    s.retype("12");
    CHECK(s.spin->value() == 5 && s.changes.empty());   // not yet
    s.key(Key::Enter);
    CHECK(s.spin->value() == 12 && (s.changes == std::vector<double>{12}));
    s.retype("x");                                      // refused
    CHECK(s.text() == "12");
    s.retype("500");
    s.key(Key::Enter);
    CHECK(s.spin->value() == 100 && s.text() == "100");     // kept in its range
    s.retype("");
    s.key(Key::Enter);
    CHECK(s.spin->value() == 100 && s.text() == "100");     // no number: the value back
    CHECK(s.changes.size() == 2);
    s.retype("7");
    s.root.setFocus(nullptr);
    CHECK(s.spin->value() == 7 && s.changes.back() == 7);
    s.spin->setValue(9);                                // by the program: not told
    CHECK(s.text() == "9" && s.changes.size() == 3);
}

TEST(aSpinBoxStepsByKeysAndRoundsToItsDecimals) {
    Spin s({.value = 1, .minimum = -1, .maximum = 3, .step = 0.25, .decimals = 2});
    CHECK(s.text() == "1.00");
    s.key(Key::Up);
    CHECK(s.spin->value() == 1.25 && s.text() == "1.25");
    s.key(Key::Down);
    s.key(Key::Down);
    CHECK(s.spin->value() == 0.75);
    s.key(Key::PageUp);                                 // ten steps, to its maximum
    CHECK(s.spin->value() == 3);
    s.key(Key::PageDown);
    s.key(Key::PageDown);
    CHECK(s.spin->value() == -1);
    s.spin->setValue(1.234);
    CHECK(s.spin->value() == 1.23);
}

TEST(aSpinBoxsArrowsStepAndRepeatWhileHeld) {
    Spin s({.value = 0});
    s.press({195, 5});                                  // the up arrow, at the end
    CHECK(s.spin->value() == 1);
    tests::advance(std::chrono::milliseconds(400));     // held: again
    tests::advance(std::chrono::milliseconds(50));      // and again, faster
    tests::advance(std::chrono::milliseconds(50));
    CHECK(s.spin->value() == 3);
    s.release({195, 5});
    tests::advance(std::chrono::milliseconds(500));
    CHECK(s.spin->value() == 3);
    s.press({195, 25});                                 // the down arrow
    s.release({195, 25});
    CHECK(s.spin->value() == 2);
    s.press({20, 15});                                  // the field: no step
    s.release({20, 15});
    CHECK(s.spin->value() == 2 && (s.changes == std::vector<double>{1, 2, 3, 2}));
}

TEST(aSpinBoxShowsAndTakesTheDigitsOfItsLanguage) {
    // Persian's digits and marks taken as Latin ones; other text as it is
    CHECK(SpinBox::toAscii("\u06f1\u06f2\u066b\u06f5") == "12.5");
    CHECK(SpinBox::toAscii("\u200e\u2212\u06f3") == "-3");
    CHECK(SpinBox::toAscii("\u0664\u0662") == "42");
    CHECK(SpinBox::toAscii("7a") == "7a");
    CHECK(SpinBox::acceptsTyping("\u06f1\u066b\u06f5", 2, false) && !SpinBox::acceptsTyping("\u06f1\u066b\u06f5", 0, false));
    // a very large number with fixed decimals: written whole, not lost
    CHECK(SpinBox::format(1e300, 2).size() > 300);

    tests::testTranslator().setLocale(Locale::of("fa"));
    {
        Spin s({.value = 12.5, .minimum = -100, .maximum = 100, .decimals = 1});
        CHECK(s.text() == "\u06f1\u06f2\u066b\u06f5");
        s.retype("\u06f4\u06f2");
        s.key(Key::Enter);
        CHECK(s.spin->value() == 42);
        CHECK(s.text() == "\u06f4\u06f2\u066b\u06f0");
        s.retype("7");          // Latin digits too
        s.key(Key::Enter);
        CHECK(s.spin->value() == 7);
    }
    tests::testTranslator().setLocale(std::nullopt);
}
