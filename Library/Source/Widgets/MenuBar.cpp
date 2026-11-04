// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/MenuBar.hpp>

#include <GLWidgets/Paint/Painter.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Layout.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <algorithm>

namespace glw {

namespace {

// The key of a mnemonic's letter or digit.
platform::Key keyOf(char c) {
    using platform::Key;
    if (c >= 'a' && c <= 'z')
        return static_cast<Key>(static_cast<int>(Key::A) + (c - 'a'));
    if (c >= '0' && c <= '9')
        return static_cast<Key>(static_cast<int>(Key::Digit0) + (c - '0'));
    return Key::Unknown;
}

// The actions with shortcuts in `items` and their submenus.
void shortcutsOf(const std::vector<MenuItem>& items, std::vector<std::shared_ptr<Action>>& out) {
    for (const MenuItem& item : items) {
        if (item.action && !item.action->shortcut().empty())
            out.push_back(item.action);
        shortcutsOf(item.items, out);
    }
}

} // namespace

MenuBar::MenuBar() {
    setAlignment(Alignment::Start, Alignment::Stretch);
}

MenuBar::~MenuBar() {
    if (registered_)
        registered_->removeShortcuts(this);
}

const MenuBarStyle& MenuBar::style() const {
    if (const UiContext* ui = context())
        return ui->theme.menuBar;
    static const MenuBarStyle unstyled = Theme::dark().menuBar;
    return unstyled;
}

void MenuBar::setMenus(std::vector<MenuItem> menus) {
    for (Label* title : titles_)
        removeChild(*title);
    titles_.clear();
    menus_ = std::move(menus);
    const float padding = style().padding;
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        auto& title = add(std::make_unique<Label>());
        title.setPadding({padding, 0, padding, 0});
        title.setAlignment(Alignment::Start, Alignment::Center);
        title.setHitTestVisible(false);
        titles_.push_back(&title);
    }
    showTitles();
    registerShortcuts();
    invalidateLayout();
}

void MenuBar::showTitles() {
    mnemonics_.assign(menus_.size(), 0);
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const std::string text = context() ? translate(menus_[i].text) : menus_[i].text.source;
        titles_[i]->setText(LocalizedText(withoutMnemonic(text, &mnemonics_[i])));
    }
}

void MenuBar::registerShortcuts() {
    UiRoot* now = root();
    if (registered_ && registered_ != now)
        registered_->removeShortcuts(this);
    registered_ = now;
    if (!now)
        return;
    std::vector<std::shared_ptr<Action>> actions;
    shortcutsOf(menus_, actions);
    // Alt and a mnemonic opens its menu; F10, the first
    for (std::size_t i = 0; i < menus_.size(); ++i) {
        const platform::Key key = keyOf(mnemonics_[i]);
        if (key == platform::Key::Unknown)
            continue;
        auto opens = std::make_shared<Action>(LocalizedText{}, [this, i] { open(i, true); });
        opens->setShortcut({key, platform::KeyModifiers::Alt});
        actions.push_back(std::move(opens));
    }
    if (!menus_.empty()) {
        auto first = std::make_shared<Action>(LocalizedText{}, [this] { open(0, true); });
        first->setShortcut({platform::Key::F10});
        actions.push_back(std::move(first));
    }
    now->addShortcuts(this, std::move(actions));
}

void MenuBar::open(std::size_t index, bool byKeys) {
    if (index >= menus_.size())
        return;
    if (UiRoot* old = std::exchange(menuPopup_, nullptr); old && old->host())
        old->host()->closeWindow();
    const std::uint64_t opening = ++opening_;
    open_ = index;
    menuPopup_ = showMenu(*this, titles_[index]->bounds(), menus_[index].items,
                          MenuOptions{.onClosed =
                                          [this, opening] {
                                              if (opening_ != opening)
                                                  return;       // another opened since
                                              open_.reset();
                                              menuPopup_ = nullptr;
                                              invalidatePaint();
                                          },
                                      .onAcross =
                                          [this](int by) {
                                              if (open_ && !menus_.empty()) {
                                                  const std::size_t n = menus_.size();
                                                  open((*open_ + n + static_cast<std::size_t>(by + static_cast<int>(n))) % n, true);
                                              }
                                          },
                                      .selectFirst = byKeys});
    if (!menuPopup_)
        open_.reset();
    invalidatePaint();
}

std::optional<std::size_t> MenuBar::titleAt(Point point) const {
    for (std::size_t i = 0; i < titles_.size(); ++i) {
        if (titles_[i]->bounds().contains(point))
            return i;
    }
    return std::nullopt;
}

Size MenuBar::measureContent(Size) {
    float width = 0, height = 0;
    for (Label* title : titles_) {
        const Size s = title->measure({unbounded, unbounded});
        width += s.width;
        height = std::max(height, s.height);
    }
    return {width, std::max(height, style().height)};
}

void MenuBar::arrangeContent(const Rect& content) {
    // from the start side
    const bool rtl = rightToLeft();
    float x = rtl ? content.right() : content.x;
    for (Label* title : titles_) {
        const float w = title->measure({unbounded, unbounded}).width;
        if (rtl)
            x -= w;
        title->arrange({x, content.y, w, content.height});
        if (!rtl)
            x += w;
    }
}

void MenuBar::paint(PaintContext& context) {
    const MenuBarStyle& s = style();
    paint::Painter& p = context.painter;
    p.fillRect({0, 0, size().width, size().height}, s.background);
    for (std::size_t i = 0; i < titles_.size(); ++i) {
        const VisualState state{.hovered = hovered_ == i, .pressed = open_ == i};
        const Rect r = titles_[i]->bounds();
        const float inset = std::min(4.0f, r.height / 6);
        p.fillRoundedRect({r.x, r.y + inset, r.width, r.height - 2 * inset}, s.cornerRadius, s.title.resolve(state));
        titles_[i]->setColor(s.text);
    }
}

void MenuBar::onMouse(MouseEvent& event) {
    const std::optional<std::size_t> at = titleAt(event.position);
    switch (event.type) {
    case MouseEventType::Move:
        if (at != hovered_) {
            hovered_ = at;
            invalidatePaint();
        }
        // a menu open (by the keys, maybe, while the mouse stayed): the one under the mouse instead
        if (open_ && at && *at != *open_)
            open(*at);
        break;
    case MouseEventType::Press:
        if (event.button == platform::MouseButton::Left && at) {
            event.handled = true;
            open(*at);
        }
        break;
    default: break;
    }
}

void MenuBar::onHoverChanged(bool hovered) {
    if (!hovered && hovered_) {
        hovered_.reset();
        invalidatePaint();
    }
}

void MenuBar::onRootChanged() {
    showTitles();
    registerShortcuts();
}

void MenuBar::onLanguageChanged() {
    showTitles();
    registerShortcuts();
    invalidateLayout();
}

} // namespace glw
