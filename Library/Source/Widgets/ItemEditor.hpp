// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The editor of an item of a view (a list's row, a tree's node, a table's cell): a widget its delegate made (a field, a
// check box, a combo box...), in a frame over the item, with the focus. Enter, or the focus leaving it, commits it (its
// value put in the model by the delegate); Escape cancels it. The view places it as it scrolls, and ends it when its item
// goes.
#include <GLWidgets/Widgets/Widget.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace glw {

class ItemEditor {
public:
    explicit ItemEditor(Widget& view) : view_(view) {}
    ~ItemEditor();
    ItemEditor(const ItemEditor&) = delete;
    ItemEditor& operator=(const ItemEditor&) = delete;

    bool active() const { return frame_ != nullptr; }
    // The delegate's widget, while it is edited.
    Widget* editor() const { return editor_; }

    // Shows `editor` (the view's coordinates: `place`), focused; `commit` puts its value in the model.
    void begin(std::unique_ptr<Widget> editor, const Rect& place, std::function<void()> commit);
    // Over its item where it shows now; none: its item does not show (scrolled away), it waits out of sight.
    void place(const std::optional<Rect>& rect);
    // Its value put in, and it goes / it goes. Either may be called from the editor's own handlers: it goes after the
    // event.
    void commit();
    void cancel();
    // Enter commits, Escape cancels (true: the key was taken).
    bool key(const KeyEvent& event);

private:
    class Frame;
    void end();

    Widget& view_;
    Frame* frame_ = nullptr;
    Widget* editor_ = nullptr;
    std::function<void()> commit_;
};

} // namespace glw
