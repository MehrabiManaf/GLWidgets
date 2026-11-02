// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A box that is checked or not, with its text: toggled by a click on either, or by Space when it has the focus. The box is
// on the start side (the right in right to left).
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <optional>

namespace glw {

class GLW_EXPORT CheckBox : public Widget {
public:
    // What a check box is made with (ui::CheckBox): designated initializers, in this order.
    struct Props {
        LocalizedText text;
        bool checked = false;
        std::function<void(bool)> onToggled;            // by the user (not by setChecked)
        std::optional<CheckBoxStyle> style;             // none: the theme's
    };

    CheckBox();
    explicit CheckBox(Props props);

    bool checked() const { return checked_; }
    void setChecked(bool checked);
    // As the user does: checked or not the other way, and onToggled told.
    void toggle();
    void setText(LocalizedText text) { label_->setText(std::move(text)); }
    const LocalizedText& text() const { return label_->text(); }
    void setOnToggled(std::function<void(bool)> onToggled) { onToggled_ = std::move(onToggled); }
    void setStyle(std::optional<CheckBoxStyle> style);
    const CheckBoxStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onFocusChanged(bool focused) override;

private:
    VisualState state() const;

    Label* label_ = nullptr;
    bool checked_ = false;
    std::function<void(bool)> onToggled_;
    std::optional<CheckBoxStyle> style_;
    Rect box_;
    bool pressed_ = false;
};

} // namespace glw
