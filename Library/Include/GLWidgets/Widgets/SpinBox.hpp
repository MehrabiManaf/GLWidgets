// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A number the user types, or steps up and down: its arrows at the end side (held, they repeat), Up and Down, Page Up and
// Page Down (ten steps), the wheel while it has the focus. A whole number, or one with a fixed or a free count of decimals.
//
// What is typed is taken on Enter, or when the focus leaves it: kept between the minimum and the maximum, and rounded to
// its decimals; text that is no number gives the value back. Only what a number may be on its way can be typed.
//
// It shows its number in the digits and marks of the language (Locale), and takes what is typed in them or in Latin ones.
//
// onChanged is called when the user changes the value (not by setValue). A double holds whole numbers exactly up to 2^53.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace glw {

class LineEdit;
struct Locale;

class GLW_EXPORT SpinBox : public Widget {
public:
    // What a spin box is made with (ui::SpinBox): designated initializers, in this order.
    struct Props {
        double value = 0;
        double minimum = std::numeric_limits<double>::lowest();
        double maximum = std::numeric_limits<double>::max();
        double step = 1;
        // 0: a whole number; above 0, that many decimals, always shown; below 0, as many as the number needs.
        int decimals = 0;
        std::function<void(double)> onChanged;
    };

    SpinBox();
    explicit SpinBox(Props props);

    double value() const { return value_; }
    void setValue(double value);
    void setRange(double minimum, double maximum);
    double minimum() const { return minimum_; }
    double maximum() const { return maximum_; }
    void setStep(double step) { step_ = step; }
    double step() const { return step_; }
    void setDecimals(int decimals);
    int decimals() const { return decimals_; }
    void setOnChanged(std::function<void(double)> handler) { onChanged_ = std::move(handler); }
    // The field it is typed in.
    LineEdit& field() const { return *field_; }

    // `value` as it shows it, and what of a text it may take on the way to a number.
    static std::string format(double value, int decimals);
    // (A language's digits and marks are taken as Latin ones: toAscii.)
    static bool acceptsTyping(std::string_view text, int decimals, bool negative);
    // A number written in a language's digits and marks (۱۲٫۵, −۳) in ASCII (12.5, -3); other text as it is.
    static std::string toAscii(std::string_view text);

protected:
    void paintOver(PaintContext& context) override;
    void onPreviewMouse(MouseEvent& event) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onFocusWithinChanged(bool within) override;
    void onRootChanged() override;
    void onLanguageChanged() override;

private:
    enum class Arrow : std::uint8_t { None, Up, Down };
    Arrow arrowAt(Point point) const;
    // Kept in the range, rounded to its decimals.
    double fit(double value) const;
    // By the user: `value` fitted, shown, and told when it changed.
    void change(double value);
    void stepBy(double steps);
    // What was typed, taken (or the value shown again when it is no number).
    void commit();
    void show();                            // the value, in the field, in the language's digits
    const Locale& locale() const;

    double value_ = 0;
    double minimum_ = std::numeric_limits<double>::lowest();
    double maximum_ = std::numeric_limits<double>::max();
    double step_ = 1;
    int decimals_ = 0;
    std::function<void(double)> onChanged_;
    LineEdit* field_ = nullptr;
    Arrow pressed_ = Arrow::None;
    Arrow hovered_ = Arrow::None;
    Timer repeat_;
};

} // namespace glw
