// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A push button with a label: clicked by the mouse (pressed and released on it), or by Enter or Space when it has the
// focus.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <string>

namespace glw {

class GLW_EXPORT Button : public Widget {
public:
    explicit Button(std::string text = {}, std::function<void()> onClick = {});

    void setText(std::string text);
    const std::string& text() const;
    // Called when it is clicked. It may destroy the button only through something that waits for the event to end
    // (Window::close does).
    void setOnClick(std::function<void()> onClick);
    Label& label() { return *label_; }
    void click();

protected:
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    Label* label_ = nullptr;
    std::function<void()> onClick_;
    bool pressed_ = false;
};

} // namespace glw
