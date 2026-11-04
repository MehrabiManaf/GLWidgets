// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Widgets/ItemEditor.hpp"

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <utility>

namespace glw {

// Holds the editor: filling it, the view's background under it (what the item showed does not show through), told when the
// focus leaves it.
class ItemEditor::Frame final : public Widget {
public:
    explicit Frame(ItemEditor& owner) : owner_(owner) {}

protected:
    void paint(PaintContext& context) override {
        context.painter.fillRect({0, 0, size().width, size().height}, context.ui.theme.palette.base);
    }
    void onFocusWithinChanged(bool within) override {
        // the focus went elsewhere (another item pressed, another widget): what was typed is kept
        if (!within)
            owner_.commit();
    }

private:
    ItemEditor& owner_;
};

ItemEditor::~ItemEditor() = default;

void ItemEditor::begin(std::unique_ptr<Widget> editor, const Rect& place, std::function<void()> commit) {
    if (active())
        end();
    auto frame = std::make_unique<Frame>(*this);
    editor_ = &frame->addChild(std::move(editor));
    frame_ = static_cast<Frame*>(&view_.addChild(std::move(frame)));
    commit_ = std::move(commit);
    this->place(place);
    // the focus to it, or to the first of it that takes the focus
    if (UiRoot* root = view_.root()) {
        Widget* target = editor_;
        const auto first = [&](auto& self, Widget& w) -> Widget* {
            if (w.focusable() && w.enabledInTree())
                return &w;
            for (const auto& child : w.children()) {
                if (Widget* found = self(self, *child))
                    return found;
            }
            return nullptr;
        };
        if (Widget* focusable = first(first, *editor_))
            target = focusable;
        root->setFocus(target->focusable() ? target : nullptr);
    }
}

void ItemEditor::place(const std::optional<Rect>& rect) {
    if (!frame_)
        return;
    frame_->setVisible(rect.has_value());
    if (rect) {
        frame_->measure({rect->width, rect->height});
        frame_->arrange(*rect);
    }
}

void ItemEditor::commit() {
    if (!frame_ || !commit_)
        return;
    // once: what it calls may end it again (the focus leaving it as it goes)
    std::function<void()> apply = std::exchange(commit_, nullptr);
    apply();
    end();
}

void ItemEditor::cancel() {
    if (!frame_)
        return;
    commit_ = nullptr;
    end();
}

bool ItemEditor::key(const KeyEvent& event) {
    if (!frame_ || !event.pressed)
        return false;
    if (event.key == platform::Key::Enter) {
        commit();
        return true;
    }
    if (event.key == platform::Key::Escape) {
        cancel();
        return true;
    }
    return false;
}

void ItemEditor::end() {
    Frame* frame = std::exchange(frame_, nullptr);
    editor_ = nullptr;
    commit_ = nullptr;
    if (!frame)
        return;
    const bool hadFocus = view_.root() && [&] {
        for (const Widget* w = view_.root()->focusWidget(); w; w = w->parent()) {
            if (w == frame)
                return true;
        }
        return false;
    }();
    frame->setVisible(false);
    // the focus back to the view; the frame gone after the event (it may be in one of its handlers)
    if (hadFocus)
        view_.focus();
    Widget* view = &view_;
    const auto remove = [view, frame] { view->removeChild(*frame); };
    if (UiRoot* root = view_.root())
        root->defer(remove);
    else
        remove();
}

} // namespace glw
