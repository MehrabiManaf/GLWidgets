// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// One of a few texts, chosen from a menu under it (Menu.hpp): a click, Enter, Space or Alt+Down opens it, the current
// text checked; Up and Down choose the one before or after without opening it.
//
// One of many (a type of a language among dozens): searchable, it opens a search popup instead (SearchPopup.hpp), a field
// over the list filtered as it is typed in, the current text selected; typing while it has the focus opens it with what
// was typed. Only one of its texts can be chosen.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Animation.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/Theme.hpp>
#include <GLWidgets/Widgets/Widget.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace glw {

class Label;

class GLW_EXPORT ComboBox : public Widget {
public:
    // What a combo box is made with (ui::ComboBox): designated initializers, in this order.
    struct Props {
        std::vector<LocalizedText> items;
        std::size_t current = 0;
        std::function<void(std::size_t)> onChanged;     // by the user (not by setCurrent)
        bool searchable = false;
        LocalizedText placeholder;                      // searchable: in the search field while it is empty
    };

    ComboBox();
    explicit ComboBox(Props props);

    void setItems(std::vector<LocalizedText> items);
    const std::vector<LocalizedText>& items() const { return items_; }
    std::size_t current() const { return current_; }
    void setCurrent(std::size_t index);
    void setOnChanged(std::function<void(std::size_t)> handler) { onChanged_ = std::move(handler); }
    void setSearchable(bool searchable, LocalizedText placeholder = {});
    bool searchable() const { return searchable_; }
    // Its menu (searchable: its search popup, with `typed` in the field), as by a click.
    void open(std::string typed = {});
    bool isOpen() const { return open_; }

protected:
    Size measureContent(Size available) override;
    void paint(PaintContext& context) override;
    void paintOver(PaintContext& context) override;
    void onMouse(MouseEvent& event) override;
    void onKey(KeyEvent& event) override;
    void onHoverChanged(bool) override { invalidatePaint(); }
    void onText(TextEvent& event) override;
    void onFocusChanged(bool focused) override;

private:
    const ButtonStyle& style() const;
    void choose(std::size_t index);         // by the user

    std::vector<LocalizedText> items_;
    std::size_t current_ = 0;
    std::function<void(std::size_t)> onChanged_;
    bool searchable_ = false;
    LocalizedText placeholder_;
    Label* label_ = nullptr;
    bool open_ = false;
    bool pressed_ = false;
    ColorTransition background_{[this] { invalidatePaint(); }};
    ColorTransition glow_{[this] { invalidatePaint(); }};
};

} // namespace glw
