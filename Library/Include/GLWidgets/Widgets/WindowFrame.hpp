// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The chrome of a borderless window, as widgets: a frame holding a title bar over the content. The title bar drags the
// window where nothing else is under the mouse, and its window buttons minimize, maximize and close it; the frame's edges
// resize it. They look the same on every system; the buttons are at the side the system puts them (Theme), mirrored with
// the layout direction.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include <GLWidgets/Widgets/MenuBar.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>

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

    void setTitle(LocalizedText title);
    // What the title bar shows beside its buttons (its title by default; the tab strip of a window...). The title bar
    // drags the window where the center's widgets are not under the mouse.
    void setCenter(std::unique_ptr<Widget> center);
    Widget* center() const { return center_; }
    // What it shows at its start, before its center (a menu bar); none: nothing.
    void setStart(std::unique_ptr<Widget> start);
    Widget* start() const { return start_; }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    Widget* center_ = nullptr;
    Widget* start_ = nullptr;
    Widget* buttons_ = nullptr;
    Label* title_ = nullptr;
};

class GLW_EXPORT WindowFrame : public Widget {
public:
    explicit WindowFrame(std::unique_ptr<Widget> content = nullptr);

    TitleBar& titleBar() { return *titleBar_; }
    void setContent(std::unique_ptr<Widget> content);
    Widget* content() const { return content_; }
    // Its menu bar, in its title bar or under it (none: none); its toolbar, under them.
    MenuBar& setMenuBar(std::unique_ptr<MenuBar> menuBar, MenuBarPlacement placement = MenuBarPlacement::TitleBar);
    MenuBar* menuBar() const { return menuBar_; }
    ToolBar& setToolBar(std::unique_ptr<ToolBar> toolBar);
    ToolBar* toolBar() const { return toolBar_; }

protected:
    void paint(PaintContext& context) override;
    void onRootChanged() override;
    void onStyleChanged() override;
    // Its edges resize the window, when the window is neither maximized nor minimized.
    std::optional<platform::HitArea> frameHitArea(Point point) const override;

private:
    TitleBar* titleBar_ = nullptr;
    Widget* content_ = nullptr;
    MenuBar* menuBar_ = nullptr;
    ToolBar* toolBar_ = nullptr;
};

} // namespace glw
