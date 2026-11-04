// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Menu.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Bevel.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>

namespace glw {

namespace {

constexpr float textInset = 12;         // from the start side to an item's text
constexpr float markRoom = 22;          // a check mark's, before the texts, when an item has one
constexpr float shortcutGap = 40;       // between a text and its shortcut
constexpr float arrowRoom = 20;         // a submenu's arrow, at the end side
constexpr float separatorHeight = 9;

const MenuStyle& menuStyle(const Widget& widget) {
    if (const UiRoot* r = widget.root())
        return r->context().theme.menu;
    static const MenuStyle unstyled = Theme::dark().menu;
    return unstyled;
}

// The content of a menu's popup: its items, in rows.
class MenuView final : public Widget {
public:
    MenuView(std::vector<MenuItem> items, MenuView* parent, float minimumWidth, std::function<void()> onClosed,
             std::function<void(int)> onAcross = {})
        : items_(std::move(items)), parent_(parent), minimumWidth_(minimumWidth), onClosed_(std::move(onClosed)),
          onAcross_(std::move(onAcross)) {
        setFocusable(true);
        for (const MenuItem& item : items_) {
            Row row;
            if (!item.separator) {
                row.text = &add(std::make_unique<Label>(item.text));
                row.text->setHitTestVisible(false);
                row.text->setAlignment(Alignment::Start, Alignment::Center);
                if (!item.shortcut.source.empty()) {
                    row.shortcut = &add(std::make_unique<Label>(item.shortcut));
                    row.shortcut->setHitTestVisible(false);
                    row.shortcut->setAlignment(Alignment::Start, Alignment::Center);
                }
            }
            marks_ = marks_ || item.checked.has_value() || !item.icon.empty();
            arrows_ = arrows_ || !item.items.empty();
            rows_.push_back(row);
        }
    }

    // The first item that can be chosen, chosen (a menu opened by the keys).
    void selectFirst() { current_ = step(std::nullopt, 1); invalidatePaint(); }

protected:
    Size measureContent(Size) override {
        const MenuStyle& s = menuStyle(*this);
        float text = 0, shortcut = 0, height = 2 * s.padding;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const Row& row = rows_[i];
            if (row.text)
                text = std::max(text, row.text->measure({unbounded, unbounded}).width);
            if (row.shortcut)
                shortcut = std::max(shortcut, row.shortcut->measure({unbounded, unbounded}).width);
            height += items_[i].separator ? separatorHeight : s.itemHeight;
        }
        const float width = start() + text + (shortcut > 0 ? shortcutGap + shortcut : 0) + (arrows_ ? arrowRoom : 0) + textInset;
        return {std::max({width, s.minimumWidth, minimumWidth_}), height};
    }

    void arrangeContent(const Rect& content) override {
        const MenuStyle& s = menuStyle(*this);
        const bool rtl = rightToLeft();
        const float width = content.width;
        const float end = width - textInset - (arrows_ ? arrowRoom : 0);
        float y = s.padding;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            Row& row = rows_[i];
            row.y = y;
            row.height = items_[i].separator ? separatorHeight : s.itemHeight;
            if (row.text) {
                const float from = start();
                arrangeInCell(*row.text, mirrored({from, y, std::max(0.0f, end - from), row.height}, width, rtl), rtl);
            }
            if (row.shortcut) {
                const float w = row.shortcut->measure({unbounded, unbounded}).width;
                arrangeInCell(*row.shortcut, mirrored({end - w, y, w, row.height}, width, rtl), rtl);
            }
            y += row.height;
        }
    }

    void paint(PaintContext& context) override {
        const MenuStyle& s = menuStyle(*this);
        paint::Painter& p = context.painter;
        const bool rtl = rightToLeft();
        const float width = size().width;
        const Rect all{0, 0, width, size().height};
        p.fillRect(all, s.background);
        p.strokeRoundedRect(all, 0, 1, s.border);
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            const Row& row = rows_[i];
            const MenuItem& item = items_[i];
            if (item.separator) {
                p.fillRect({textInset, row.y + separatorHeight / 2, width - 2 * textInset, 1}, s.separator);
                continue;
            }
            const bool chosen = current_ == i && item.enabled;
            if (chosen)
                p.fillRoundedRect({s.padding, row.y, width - 2 * s.padding, row.height}, s.cornerRadius, lit(s.highlight, 0.06f),
                                  lit(s.highlight, -0.04f));
            // the texts paint after this, in the colors of the state
            if (row.text)
                row.text->setColor(!item.enabled ? s.disabledText : chosen ? s.highlightText : s.text);
            if (row.shortcut)
                row.shortcut->setColor(!item.enabled ? s.disabledText : chosen ? withAlpha(s.highlightText, 0.8f) : s.shortcut);
            const Color mark = !item.enabled ? s.disabledText : chosen ? s.highlightText : s.mark;
            const float middle = row.y + row.height / 2;
            if (!item.icon.empty()) {
                // its icon where the mark goes; checked, on a soft square
                const float x = rtl ? width - textInset - markRoom / 2 : textInset + markRoom / 2 - 2;
                if (item.checked.value_or(false))
                    p.fillRoundedRect({x - 10, middle - 10, 20, 20}, s.cornerRadius, chosen ? withAlpha(s.highlightText, 0.22f) : withAlpha(s.highlight, 0.30f));
                item.icon.draw(context, {x - 8, middle - 8, 16, 16}, mark);
            } else if (item.checked.value_or(false)) {
                const float x = rtl ? width - textInset - markRoom / 2 + 2 : textInset + markRoom / 2 - 2;
                const Point points[] = {{x - 5, middle}, {x - 1.5f, middle + 3.5f}, {x + 5, middle - 4}};
                p.drawPolyline(points, 1.5f, mark);
            }
            if (!item.items.empty()) {
                // a chevron toward the end side
                const float x = rtl ? textInset : width - textInset;
                const float d = rtl ? 1.0f : -1.0f;
                const Point points[] = {{x + d * 6, middle - 4}, {x + d * 2, middle}, {x + d * 6, middle + 4}};
                p.drawPolyline(points, 1.4f, mark);
            }
        }
    }

    void onMouse(MouseEvent& event) override {
        const std::optional<std::size_t> at = rowAt(event.position);
        switch (event.type) {
        case MouseEventType::Move:
            if (at != current_ && (!at || selectable(*at))) {
                current_ = at;
                invalidatePaint();
                if (at && !items_[*at].items.empty())
                    openSubmenu(*at, false);
                else
                    closeSubmenu();
            }
            event.handled = true;
            break;
        case MouseEventType::Press:
            pressed_ = true;
            if (at && selectable(*at) && !items_[*at].items.empty())
                openSubmenu(*at, false);
            event.handled = true;
            break;
        case MouseEventType::Release:
            if (std::exchange(pressed_, false) && at && selectable(*at) && items_[*at].items.empty())
                trigger(*at);
            event.handled = true;
            break;
        default: break;
        }
    }

    void onKey(KeyEvent& event) override {
        if (!event.pressed)
            return;
        const bool rtl = rightToLeft();
        const bool in = event.key == (rtl ? platform::Key::Left : platform::Key::Right);
        const bool out = event.key == (rtl ? platform::Key::Right : platform::Key::Left);
        switch (event.key) {
        case platform::Key::Down: select(step(current_, 1)); break;
        case platform::Key::Up: select(step(current_, -1)); break;
        case platform::Key::Home: select(step(std::nullopt, 1)); break;
        case platform::Key::End: select(step(std::nullopt, -1)); break;
        case platform::Key::Enter:
        case platform::Key::Space:
            if (current_ && selectable(*current_)) {
                if (items_[*current_].items.empty())
                    trigger(*current_);
                else
                    openSubmenu(*current_, true);
            }
            break;
        case platform::Key::Escape: close(); break;
        default:
            if (in && current_ && selectable(*current_) && !items_[*current_].items.empty())
                openSubmenu(*current_, true);
            else if (out && parent_)
                close();
            else if ((in || out) && !parent_ && onAcross_)
                onAcross_(in ? 1 : -1);
            else
                return;
        }
        event.handled = true;
    }

    void onHoverChanged(bool hovered) override {
        // the mouse gone: nothing chosen, unless a submenu shows for it
        if (!hovered && !submenu_ && current_) {
            current_.reset();
            invalidatePaint();
        }
    }

    void onRootChanged() override { onStyleChanged(); }

    void onStyleChanged() override {
        const MenuStyle& s = menuStyle(*this);
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (rows_[i].text)
                rows_[i].text->setColor(items_[i].enabled ? s.text : s.disabledText);
            if (rows_[i].shortcut)
                rows_[i].shortcut->setColor(items_[i].enabled ? s.shortcut : s.disabledText);
        }
        invalidateLayout();
    }

    void onWindowClosing() override {
        if (parent_ && parent_->submenu_ == this)
            parent_->submenu_ = nullptr;
        submenu_ = nullptr;
        if (onClosed_)
            std::exchange(onClosed_, nullptr)();
    }

private:
    struct Row {
        Label* text = nullptr;
        Label* shortcut = nullptr;
        float y = 0;
        float height = 0;
    };

    float start() const { return textInset + (marks_ ? markRoom : 0); }

    static Rect mirrored(const Rect& r, float width, bool rtl) {
        return rtl ? Rect{width - r.x - r.width, r.y, r.width, r.height} : r;
    }

    bool selectable(std::size_t index) const { return !items_[index].separator && items_[index].enabled; }

    std::optional<std::size_t> rowAt(Point point) const {
        if (point.x < 0 || point.x >= size().width)
            return std::nullopt;
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            if (point.y >= rows_[i].y && point.y < rows_[i].y + rows_[i].height)
                return i;
        }
        return std::nullopt;
    }

    // The next item that can be chosen from `from` by `by` (1 or -1), round; from the first or the last without one.
    std::optional<std::size_t> step(std::optional<std::size_t> from, int by) const {
        const std::size_t count = items_.size();
        if (count == 0)
            return std::nullopt;
        std::size_t at = from ? *from : (by > 0 ? count - 1 : 0);
        for (std::size_t k = 0; k < count; ++k) {
            at = by > 0 ? (at + 1) % count : (at + count - 1) % count;
            if (selectable(at))
                return at;
        }
        return std::nullopt;
    }

    void select(std::optional<std::size_t> index) {
        if (index == current_)
            return;
        current_ = index;
        closeSubmenu();
        invalidatePaint();
    }

    void openSubmenu(std::size_t index, bool byKeys) {
        if (submenu_ && submenuOf_ == index) {
            if (byKeys)
                submenu_->selectFirst();
            return;
        }
        closeSubmenu();
        UiHost* host = root() ? root()->host() : nullptr;
        if (!host)
            return;
        // beside the item, its first item level with it
        const MenuStyle& s = menuStyle(*this);
        const Point corner = mapToRoot({0, rows_[index].y - s.padding});
        UiRoot* opened = host->openPopup({.content = std::make_unique<MenuView>(items_[index].items, this, 0.0f, nullptr),
                                          .anchor = {corner.x, corner.y, size().width, rows_[index].height + 2 * s.padding},
                                          .placement = PopupPlacement::After});
        if (!opened)
            return;
        submenu_ = static_cast<MenuView*>(opened->content());
        submenuOf_ = index;
        submenu_->focus();
        if (byKeys)
            submenu_->selectFirst();
    }

    void closeSubmenu() {
        if (MenuView* submenu = std::exchange(submenu_, nullptr))
            submenu->close();
    }

    // This menu (and its submenus) closes; the menu it came from stays.
    void close() {
        if (UiHost* host = root() ? root()->host() : nullptr)
            host->closeWindow();
    }

    void trigger(std::size_t index) {
        std::function<void()> handler = items_[index].onTriggered;
        MenuView* top = this;
        while (top->parent_)
            top = top->parent_;
        top->close();
        if (handler)
            handler();
    }

    std::vector<MenuItem> items_;
    std::vector<Row> rows_;
    MenuView* parent_;
    float minimumWidth_;
    std::function<void()> onClosed_;
    std::function<void(int)> onAcross_;
    bool marks_ = false;
    bool arrows_ = false;
    std::optional<std::size_t> current_;
    MenuView* submenu_ = nullptr;
    std::size_t submenuOf_ = 0;
    bool pressed_ = false;
};

// What an item shows from its action where it says nothing of its own, its text without its mnemonic's &, and so on in its
// submenu: the texts translated here, once (a menu is open a moment).
void resolve(std::vector<MenuItem>& items, const Translator& translator) {
    for (MenuItem& item : items) {
        if (const std::shared_ptr<Action> action = item.action) {
            if (item.text.source.empty())
                item.text = action->text();
            if (item.icon.empty())
                item.icon = action->icon();
            if (item.shortcut.source.empty() && !action->shortcut().empty())
                item.shortcut = LocalizedText(action->shortcut().text());
            item.enabled = item.enabled && action->enabled();
            if (action->checkable() && !item.checked)
                item.checked = action->checked();
            if (!item.onTriggered)
                item.onTriggered = [action] { action->trigger(); };
        }
        if (!item.text.source.empty())
            item.text = LocalizedText(withoutMnemonic(translator.resolve(item.text)));
        resolve(item.items, translator);
    }
}

UiRoot* open(Widget& from, const Rect& anchor, std::vector<MenuItem> items, float minimumWidth, MenuOptions options) {
    UiRoot* root = from.root();
    UiHost* host = root ? root->host() : nullptr;
    if (!host)
        return nullptr;
    resolve(items, root->context().translator);
    UiRoot* opened = host->openPopup({.content = std::make_unique<MenuView>(std::move(items), nullptr, minimumWidth, std::move(options.onClosed),
                                                                          std::move(options.onAcross)),
                                      .anchor = from.transformToRoot().map(anchor)});
    if (!opened)
        return nullptr;
    opened->content()->focus();
    if (options.selectFirst)
        static_cast<MenuView*>(opened->content())->selectFirst();
    return opened;
}

} // namespace

UiRoot* showContextMenu(Widget& from, Point at, std::vector<MenuItem> items, std::function<void()> onClosed) {
    return open(from, {at.x, at.y, 0, 0}, std::move(items), 0, {.onClosed = std::move(onClosed)});
}

UiRoot* showMenu(Widget& from, const Rect& anchor, std::vector<MenuItem> items, std::function<void()> onClosed) {
    return open(from, anchor, std::move(items), from.transformToRoot().map(anchor).width, {.onClosed = std::move(onClosed)});
}

UiRoot* showMenu(Widget& from, const Rect& anchor, std::vector<MenuItem> items, MenuOptions options) {
    return open(from, anchor, std::move(items), from.transformToRoot().map(anchor).width, std::move(options));
}

} // namespace glw
