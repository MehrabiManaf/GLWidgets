// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A row of actions as buttons (their icons, their texts too if asked), separators and any widgets (a field, a combo box),
// at the top of a window (Window::setToolBar) or anywhere. A button follows its action: disabled with it, pressed while it
// is checked, its tooltip the action's (its text and shortcut by default). What does not fit goes into a menu at its end.
// The shortcuts of its actions work in its window.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Action.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace glw {

class Label;
class UiRoot;

class GLW_EXPORT ToolButton : public Widget {
public:
    // Its icon, and its text after it when `showText` (or when it has no icon).
    explicit ToolButton(std::shared_ptr<Action> action, bool showText = false);
    ~ToolButton() override;

    const std::shared_ptr<Action>& action() const { return action_; }
    // Its size (as high as it is wide, without a text) and its icon's, in dips, other than its toolbar's (ToolBarStyle): a
    // smaller button in a panel's header. None: the style's.
    void setSizes(std::optional<float> buttonSize, std::optional<float> iconSize);

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onRootChanged() override;
    void onLanguageChanged() override;

private:
    void update();                          // from its action
    const ToolBarStyle& style() const;

    std::shared_ptr<Action> action_;
    std::size_t observing_ = 0;
    bool showText_;
    Label* label_ = nullptr;
    bool pressed_ = false;
    std::optional<float> buttonSize_, iconSize_;
    float buttonSize() const { return buttonSize_.value_or(style().buttonSize); }
    float iconSize() const { return iconSize_.value_or(style().iconSize); }
    ColorTransition background_{[this] { invalidatePaint(); }};
    ColorTransition outline_{[this] { invalidatePaint(); }};
    Ripple ripple_{[this] { invalidatePaint(); }};
    Press press_{[this] { invalidatePaint(); }};
};

class GLW_EXPORT ToolBar : public Widget {
public:
    ToolBar();
    ~ToolBar() override;

    ToolButton& addAction(std::shared_ptr<Action> action, bool showText = false);
    void addSeparator();
    template <class W>
    W& addWidget(std::unique_ptr<W> widget) {
        W& added = *widget;
        addItem(std::move(widget), nullptr);
        return added;
    }
    // The actions that do not fit, now (in its menu at its end).
    std::vector<std::shared_ptr<Action>> overflow() const;
    // How big its buttons (square) and their icons are, those there and those added; none: the style's.
    void setButtonSizes(std::optional<float> buttonSize, std::optional<float> iconSize);
    float buttonSize() const { return buttonSize_.value_or(style().buttonSize); }

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void onRootChanged() override;

private:
    struct Item {
        Widget* widget = nullptr;           // none: a separator
        std::shared_ptr<Action> action;     // a button's
        bool shown = true;
        float x = 0;                        // a separator's
    };
    void addItem(std::unique_ptr<Widget> widget, std::shared_ptr<Action> action);
    void showOverflow();
    void registerShortcuts();
    const ToolBarStyle& style() const;

    std::vector<Item> items_;
    ToolButton* more_ = nullptr;            // the menu of what does not fit
    std::optional<float> buttonSize_, iconSize_;
    std::shared_ptr<Action> moreAction_;
    UiRoot* registered_ = nullptr;
};

} // namespace glw
