// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The chrome of a borderless window, as widgets: a frame holding a title bar over the content. The title bar drags the
// window where nothing else is under the mouse, and its window buttons minimize, maximize and close it; the frame's edges
// resize it. They look the same on every system; the buttons are at the side the system puts them (Theme), mirrored with
// the layout direction.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace glw {

class Label;

enum class WindowButtonKind : std::uint8_t { Minimize, Maximize, Close };

class GLW_EXPORT WindowButton : public Widget {
public:
    explicit WindowButton(WindowButtonKind kind);
    WindowButtonKind kind() const { return kind_; }

protected:
    Size measureContent(Size available) override;
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;

private:
    WindowButtonKind kind_;
    bool pressed_ = false;
};

class GLW_EXPORT TitleBar : public Widget {
public:
    TitleBar();

    void setTitle(std::string title);
    // What the title bar shows beside its buttons (its title by default; the tab strip of a window...). The title bar
    // drags the window where the center's widgets are not under the mouse.
    void setCenter(std::unique_ptr<Widget> center);
    Widget* center() const { return center_; }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override;

private:
    Widget* center_ = nullptr;
    Widget* buttons_ = nullptr;
    Label* title_ = nullptr;
};

class GLW_EXPORT WindowFrame : public Widget {
public:
    explicit WindowFrame(std::unique_ptr<Widget> content = nullptr);

    TitleBar& titleBar() { return *titleBar_; }
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() const { return content_; }

protected:
    void paint(PaintContext& context) override;
    // Its edges resize the window, when the window is neither maximized nor minimized.
    std::optional<platform::HitArea> frameHitArea(Point point) const override;

private:
    TitleBar* titleBar_ = nullptr;
    Widget* content_ = nullptr;
};

} // namespace glw
