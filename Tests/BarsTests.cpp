// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Actions, shortcuts, menu bars and toolbars: a shortcut triggers what the focused widget does not take; menus show their
// actions; a menu bar opens its menus by the mouse, the keys and its mnemonics, and goes across; a toolbar's buttons follow
// their actions and what does not fit is in its menu; SVG icons rasterized once at each size.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Core/Error.hpp>
#include <GLWidgets/Widgets/Action.hpp>
#include <GLWidgets/Widgets/Icon.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/MenuBar.hpp>
#include <GLWidgets/Widgets/StatusBar.hpp>
#include <GLWidgets/Widgets/TextEdit.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>
#include <GLWidgets/Widgets/WindowFrame.hpp>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>

using namespace glw;

namespace {

constexpr auto ctrl = platform::shortcutModifier;

struct Barred {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    int log = 0;

    void key(platform::Key k, platform::KeyModifiers m = platform::KeyModifiers::None, UiRoot* in = nullptr) {
        (in ? in : &root)->dispatch(platform::KeyChanged{1, k, m, true, false});
    }
    void click(Point at, UiRoot* in = nullptr) {
        UiRoot& r = in ? *in : root;
        r.dispatch(platform::MouseMoved{1, at});
        r.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
        r.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
    }
    std::vector<tests::FakePopup*> popups() const { return host.open(); }
};

Icon dot() {
    return Icon::fromPainter([](paint::Painter&, const Rect&, Color) {});
}

} // namespace

TEST(actionsSayTheirShortcutsAndMnemonics) {
    Action a("&Save");
    a.setShortcut({platform::Key::S, platform::KeyModifiers::Control | platform::KeyModifiers::Shift});
#ifndef __APPLE__
    CHECK(a.shortcut().text() == "Ctrl+Shift+S");
#endif
    char m = 0;
    CHECK(withoutMnemonic("&File", &m) == "File" && m == 'f');
    CHECK(withoutMnemonic("Save && Close", &m) == "Save & Close" && m == 0);
    // what shows it is told; checkable: checked when triggered; disabled: nothing
    int told = 0, done = 0;
    const std::size_t token = a.observe([&] { ++told; });
    a.setCheckable(true);
    a.setOnTriggered([&] { ++done; });
    a.trigger();
    CHECK(a.checked() && done == 1 && told == 2);
    a.setEnabled(false);
    a.trigger();
    CHECK(a.checked() && done == 1);
    a.unobserve(token);
    a.setEnabled(true);
    CHECK(told == 3);
}

TEST(aShortcutTriggersWhatTheFocusedWidgetDoesNotTake) {
    Barred b;
    auto edit = std::make_unique<TextEdit>();
    TextEdit& text = *edit;
    b.root.setContent(std::move(edit));
    b.root.setSize({400, 300});
    b.root.layout();
    auto save = std::make_shared<Action>("Save", [&] { ++b.log; });
    save->setShortcut({platform::Key::S, ctrl});
    auto undo = std::make_shared<Action>("Undo", [&] { b.log += 100; });
    undo->setShortcut({platform::Key::Z, ctrl});
    b.root.addShortcuts(&b, {save, undo});
    text.focus();
    b.key(platform::Key::S, ctrl);
    CHECK(b.log == 1);
    // the field takes Ctrl+Z itself
    b.key(platform::Key::Z, ctrl);
    CHECK(b.log == 1);
    // disabled: nothing; gone: nothing
    save->setEnabled(false);
    b.key(platform::Key::S, ctrl);
    CHECK(b.log == 1);
    save->setEnabled(true);
    b.root.removeShortcuts(&b);
    b.key(platform::Key::S, ctrl);
    CHECK(b.log == 1);
}

TEST(aMenuBarOpensItsMenusByTheMouseTheKeysAndItsMnemonics) {
    Barred b;
    auto created = std::make_shared<Action>("&New", [&] { ++b.log; });
    created->setShortcut({platform::Key::N, ctrl});
    auto bar = std::make_unique<MenuBar>();
    MenuBar& menus = *bar;
    menus.setMenus({{.text = "&File", .items = {{.action = created}, {.text = "Save"}}}, {.text = "&Edit", .items = {{.text = "Undo"}}}});
    b.root.setContent(std::move(bar));
    b.root.setSize({400, 40});
    b.root.layout();
    // a click on File: its menu under its title, its action's shortcut shown
    b.click({20, 20});
    CHECK(menus.openMenu() == 0u && b.popups().size() == 1);
    CHECK(b.popups()[0]->anchor.y == 0 && b.popups()[0]->anchor.height == 40);
    // Right in it: the menu beside
    b.key(platform::Key::Right, platform::KeyModifiers::None, &b.popups()[0]->root);
    CHECK(menus.openMenu() == 1u && b.popups().size() == 1);
    // the mouse over File while Edit is open: File's
    b.root.dispatch(platform::MouseMoved{1, {15, 20}});
    CHECK(menus.openMenu() == 0u && b.popups().size() == 1);
    b.key(platform::Key::Escape, platform::KeyModifiers::None, &b.popups()[0]->root);
    CHECK(b.popups().empty() && !menus.openMenu());
    // Alt+E, F10; a shortcut of its menus, with none open
    b.key(platform::Key::E, platform::KeyModifiers::Alt);
    CHECK(menus.openMenu() == 1u);
    b.key(platform::Key::Escape, platform::KeyModifiers::None, &b.popups()[0]->root);
    b.key(platform::Key::F10);
    CHECK(menus.openMenu() == 0u);
    b.key(platform::Key::Escape, platform::KeyModifiers::None, &b.popups()[0]->root);
    b.key(platform::Key::N, ctrl);
    CHECK(b.log == 1);
    // the menu's first item: the action, triggered by Enter (chosen first when opened by the keys)
    b.key(platform::Key::F, platform::KeyModifiers::Alt);
    b.key(platform::Key::Enter, platform::KeyModifiers::None, &b.popups()[0]->root);
    CHECK(b.log == 2 && b.popups().empty());
}

TEST(aToolBarsButtonsFollowTheirActionsAndWhatDoesNotFitIsInItsMenu) {
    Barred b;
    auto bar = std::make_unique<ToolBar>();
    ToolBar& tools = *bar;
    std::vector<std::shared_ptr<Action>> actions;
    for (int i = 0; i < 6; ++i) {
        auto a = std::make_shared<Action>(LocalizedText("Tool " + std::to_string(i)), [&b, i] { b.log += 1 << i; });
        a->setIcon(dot());
        actions.push_back(a);
        ToolButton& button = tools.addAction(a);
        (void)button;
        if (i == 2)
            tools.addSeparator();
    }
    actions[0]->setShortcut({platform::Key::S, ctrl});
    actions[1]->setCheckable(true);
    b.root.setContent(std::move(bar));
    b.root.setSize({400, 40});
    b.root.layout();
    CHECK(tools.overflow().empty());
    // a click: triggered; its tooltip, its text and shortcut
    ToolButton& first = static_cast<ToolButton&>(*tools.children()[0]);
    CHECK(first.toolTip() && first.toolTip()->source == "Tool 0 (" + actions[0]->shortcut().text() + ")");
    const Point at = first.mapToRoot({first.size().width / 2, first.size().height / 2});
    b.click(at);
    CHECK(b.log == 1);
    // the toolbar's shortcuts work in its window
    b.key(platform::Key::S, ctrl);
    CHECK(b.log == 2);
    // checkable: checked by a click; disabled: its button too
    const ToolButton& second = static_cast<ToolButton&>(*tools.children()[1]);
    b.click(second.mapToRoot({4, 4}));
    CHECK(actions[1]->checked());
    actions[2]->setEnabled(false);
    CHECK(!tools.children()[2]->enabled());
    // narrow: the last ones in its menu
    b.root.setSize({140, 40});
    b.root.layout();
    const auto hidden = tools.overflow();
    CHECK(!hidden.empty() && hidden.back() == actions[5]);
}

TEST(svgIconsAreRasterizedOnceAtEachSize) {
    IconAtlas atlas(1, 256);
    const Icon square = Icon::fromSvg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="0" y="0" width="16" height="16" fill="#ff0000"/></svg>)");
    const Icon mono = Icon::fromSvg(R"(<svg viewBox="0 0 16 16"><rect width="16" height="16" fill="#ff0000"/></svg>)", true);
    const PixelRect a = atlas.image(square, 16, 16);
    CHECK(!a.empty() && a.width == 16 && a.height == 16);
    CHECK(atlas.image(square, 16, 16) == a);                // kept
    const PixelRect big = atlas.image(square, 32, 32);
    CHECK(!big.empty() && !(big == a));
    // its own colors, premultiplied; a monochrome one, coverage only (white)
    const auto pixel = [&](const PixelRect& r, int x, int y) {
        const std::byte* p = &atlas.pixels()[(static_cast<std::size_t>(r.y + y) * 256 + r.x + x) * 4];
        return std::to_integer<int>(p[0]) << 24 | std::to_integer<int>(p[1]) << 16 | std::to_integer<int>(p[2]) << 8 | std::to_integer<int>(p[3]);
    };
    CHECK(pixel(a, 8, 8) == static_cast<int>(0xff0000ffu));
    const PixelRect m = atlas.image(mono, 16, 16);
    CHECK(pixel(m, 8, 8) == static_cast<int>(0xffffffffu));
    // not SVG: refused
    bool refused = false;
    try {
        Icon::fromSvg("not an svg");
    } catch (const Error&) {
        refused = true;
    }
    CHECK(refused);
}

// The window's close button asks its host (which may ask the application whether to save first), rather than closing it.
TEST(theCloseButtonAsksTheHostToClose) {
    Barred b;
    b.root.setContent(std::make_unique<WindowFrame>(std::make_unique<Widget>()));
    b.root.setSize({640, 400});
    b.root.layout();
    const WindowButton* close = nullptr;
    std::function<void(const Widget&)> find = [&](const Widget& w) {
        if (auto* button = dynamic_cast<const WindowButton*>(&w); button && button->kind() == WindowButtonKind::Close)
            close = button;
        for (const auto& c : w.children())
            find(*c);
    };
    find(*b.root.content());
    CHECK(close != nullptr);
    if (!close)
        return;
    b.click(close->mapToRoot({close->size().width / 2, close->size().height / 2}));
    CHECK(b.host.closeRequests == 1);
}

// A status bar: a message for a while, its steady text otherwise, widgets at its end; along a frame's bottom.
TEST(aStatusBarShowsMessagesForAWhileAndKeepsItsWidgets) {
    tests::FakeHost host;
    UiRoot root{tests::testContext(), &host};
    auto frame = std::make_unique<WindowFrame>(std::make_unique<Widget>());
    WindowFrame& f = *frame;
    root.setContent(std::move(frame));
    StatusBar& bar = f.setStatusBar(std::make_unique<StatusBar>());
    auto state = std::make_unique<Widget>();
    state->setMinimumSize({60, 16});
    Widget& running = bar.addPermanent(std::move(state));
    root.setSize({640, 400});
    root.layout();
    // the last of the frame, along the bottom; the widget at its end
    CHECK(f.children().back().get() == &bar && bar.bounds().bottom() == 400);
    CHECK(running.bounds().right() == bar.size().width - 10);
    // a new content goes above it
    f.setContent(std::make_unique<Widget>());
    CHECK(f.children().back().get() == &bar);
    bar.setText("Ready");
    const auto shown = [&] {
        return tests::testContext().translator.resolve(static_cast<const Label&>(*bar.children()[0]).text());
    };
    CHECK(shown() == "Ready");
    bar.showMessage("Saved", std::chrono::milliseconds(3000));
    CHECK(shown() == "Saved" && bar.message());
    tests::advance(std::chrono::milliseconds(2999));
    CHECK(shown() == "Saved");
    tests::advance(std::chrono::milliseconds(1));
    CHECK(shown() == "Ready" && !bar.message());
    bar.showMessage("Stays");
    tests::advance(std::chrono::milliseconds(60000));
    CHECK(shown() == "Stays");
    bar.clearMessage();
    CHECK(shown() == "Ready");
}
