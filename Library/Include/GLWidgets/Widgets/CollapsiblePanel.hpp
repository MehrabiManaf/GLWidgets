// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A header over a column of content that opens and closes: its height eases between the header's alone and the header's
// with all of its content (the content cut to it meanwhile), its arrow turning with it. Closed, its content is hidden: not
// painted, not under the mouse, not in the order of the focus. The header opens and closes it by a click, Enter or Space,
// Right and Left (mirrored in right to left); buttons at its end trigger actions (a + that adds an item).
//
//   auto variables = std::make_unique<glw::CollapsiblePanel>(glw::CollapsiblePanel::Props{.title = "Variables", .expanded = true});
//   variables->addContent(std::make_unique<glw::Label>("Speed"));
//   variables->addAction(add);              // its icon at the header's end
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Action.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <memory>

namespace glw {

class Label;
class ToolButton;

class GLW_EXPORT CollapsiblePanel : public Widget {
public:
    // What a panel is made with (ui::CollapsiblePanel): designated initializers, in this order.
    struct Props {
        LocalizedText title;
        bool expanded = false;              // at first, at once
        std::function<void(bool)> onToggled;    // opened or closed by the user (not by setExpanded)
    };

    CollapsiblePanel();
    explicit CollapsiblePanel(Props props);

    void setTitle(LocalizedText title);
    // Into its content, a column, at its end.
    template <class W>
    W& addContent(std::unique_ptr<W> widget) {
        W& added = *widget;
        content_->addChild(std::move(widget));
        return added;
    }
    Widget& content() const { return *content_; }
    // A button at the header's end, for `action` (its icon).
    ToolButton& addAction(std::shared_ptr<Action> action);

    // Opened or closed: eased (the theme's motion), or at once.
    void setExpanded(bool expanded, bool animated = true);
    bool expanded() const { return expanded_; }
    // How open it shows now, from 0 (closed) to 1 (open).
    float openness() const { return progress_.value(); }
    void setOnToggled(std::function<void(bool)> handler) { onToggled_ = std::move(handler); }

    const CollapsiblePanelStyle& style() const;

protected:
    Size measureContent(Size available) override;
    void arrangeContent(const Rect& content) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool hovered) override;
    void onFocusChanged(bool focused) override;
    void onRootChanged() override;
    void onStyleChanged() override;

private:
    void toggle();                          // by the user
    void stepped();                         // the animation moved
    Rect header() const;
    float headerHeight() const;

    Label* title_ = nullptr;
    Widget* actions_ = nullptr;             // at the header's end
    Widget* content_ = nullptr;
    bool expanded_ = false;
    bool headerHovered_ = false;
    bool pressed_ = false;
    Animation progress_{[this] { stepped(); }};
    ColorTransition headerColor_{[this] { invalidatePaint(); }};
    std::function<void(bool)> onToggled_;
};

} // namespace glw
