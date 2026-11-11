// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Collapsible panels: their height eased from the header's to the header's and all of their content, and back; their
// content hidden when closed (the focus in it going to the header); opened and closed by a click and by the keys (mirrored
// in right to left); at once when asked.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/CollapsiblePanel.hpp>
#include <GLWidgets/Widgets/Icon.hpp>
#include <GLWidgets/Widgets/LineEdit.hpp>
#include <GLWidgets/Widgets/ToolBar.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace glw;
using namespace std::chrono_literals;

namespace {

bool near(float a, float b) {
    return std::abs(a - b) < 0.5f;
}

struct Animated {
    Animated() { tests::testScheduler().setAnimationsEnabled(true); }
    ~Animated() { tests::testScheduler().setAnimationsEnabled(false); }
};

// A panel of two boxes 40 high: open, 30 + 8 + 40 + 8 + 40 + 12 = 138.
struct Panelled {
    UiRoot root{tests::testContext()};
    CollapsiblePanel* panel = nullptr;
    LineEdit* field = nullptr;
    std::vector<bool> toggled;

    explicit Panelled(bool expanded = false) {
        auto made = std::make_unique<CollapsiblePanel>(CollapsiblePanel::Props{
            .title = "Details", .expanded = expanded, .onToggled = [this](bool open) { toggled.push_back(open); }});
        panel = made.get();
        auto& box = panel->addContent(std::make_unique<Widget>());
        box.setMinimumSize({100, 40});
        field = &panel->addContent(std::make_unique<LineEdit>());
        field->setMinimumSize({0, 40});
        field->setMaximumSize({unbounded, 40});
        made->setAlignment(Alignment::Stretch, Alignment::Start);
        root.setSize({300, 400});
        root.setContent(std::move(made));
        root.layout();
    }
    float height() {
        root.layout();
        return panel->size().height;
    }
    void click(Point at) {
        root.dispatch(platform::MouseMoved{1, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, true, 1, at});
        root.dispatch(platform::MouseButtonChanged{1, platform::MouseButton::Left, false, 1, at});
    }
    void key(platform::Key k) { root.dispatch(platform::KeyChanged{1, k, platform::KeyModifiers::None, true, false}); }
};

} // namespace

TEST(aCollapsiblePanelsHeightEasesOpenAndClosed) {
    Animated on;
    Panelled p;
    CHECK(near(p.height(), 30) && !p.panel->content().visible());
    // open: its content shows at once, cut to a height that eases (the theme's slow motion, 300 ms)
    p.panel->setExpanded(true);
    CHECK(p.panel->content().visible() && tests::testScheduler().animating());
    tests::advance(150ms);
    const float half = p.height();
    CHECK(half > 60 && half < 120);
    tests::advance(150ms);
    CHECK(near(p.height(), 138) && !tests::testScheduler().animating());
    // closed: back to its header, its content hidden at the end, not before
    p.panel->setExpanded(false);
    tests::advance(100ms);
    CHECK(p.panel->content().visible() && p.height() > 30);
    tests::advance(200ms);
    CHECK(near(p.height(), 30) && !p.panel->content().visible());
    // at once
    p.panel->setExpanded(true, false);
    CHECK(near(p.height(), 138) && !tests::testScheduler().animating());
}

TEST(aCollapsiblePanelOpensByItsHeaderAndTheKeys) {
    Panelled p;
    // a click on the header (not on its content): opened, and told
    p.click({100, 18});
    CHECK(p.panel->expanded() && p.toggled == std::vector<bool>{true} && p.panel->hasFocus());
    p.root.layout();
    p.click({100, 80});
    CHECK(p.panel->expanded() && p.toggled.size() == 1);
    // the keys: Left closes, Right opens, Enter and Space toggle
    p.panel->focus();
    p.key(platform::Key::Left);
    CHECK(!p.panel->expanded());
    p.key(platform::Key::Left);
    CHECK(!p.panel->expanded());
    p.key(platform::Key::Right);
    CHECK(p.panel->expanded());
    p.key(platform::Key::Space);
    CHECK(!p.panel->expanded() && p.toggled == (std::vector<bool>{true, false, true, false}));
    // right to left: mirrored
    p.root.setLayoutDirection(LayoutDirection::RightToLeft);
    p.key(platform::Key::Left);
    CHECK(p.panel->expanded());
    p.key(platform::Key::Right);
    CHECK(!p.panel->expanded());
}

TEST(aCollapsiblePanelClosingTakesTheFocusOutOfItsContent) {
    Panelled p(true);
    CHECK(near(p.height(), 138) && p.panel->content().visible());
    p.field->focus();
    CHECK(p.field->hasFocus());
    p.panel->setExpanded(false);
    CHECK(p.panel->hasFocus() && !p.field->hasFocus() && !p.panel->content().visible());
}

// The buttons at the header's end: as the style sizes them, its room from the header's end and between them, in the middle
// of its height.
TEST(aCollapsiblePanelsHeaderButtonsHaveTheStylesSizeAndRoom) {
    tests::FakeHost host;
    UiRoot root(tests::testContext(), &host);
    auto made = std::make_unique<CollapsiblePanel>(CollapsiblePanel::Props{.title = "Functions", .expanded = true});
    CollapsiblePanel& panel = *made;
    const auto withIcon = [](const char* text) {     // an icon: a square button (with a text alone it is as wide as its text)
        auto a = std::make_shared<Action>(LocalizedText(text));
        a->setIcon(Icon::fromSvg(R"(<svg viewBox="0 0 16 16"><path d="M2 8 H14"/></svg>)", true));
        return a;
    };
    ToolButton& add = panel.addAction(withIcon("Add"));
    ToolButton& more = panel.addAction(withIcon("More"));
    root.setContent(std::move(made));
    root.setSize({300, 200});
    root.layout();
    const CollapsiblePanelStyle& s = panel.style();
    const auto at = [&](const Widget& w) { return panel.mapFromRoot(w.mapToRoot({})); };
    CHECK(add.size().width == s.actionSize && add.size().height == s.actionSize && more.size().width == s.actionSize);
    CHECK(std::abs(at(more).x + s.actionSize - (300 - s.actionMargin)) < 0.5f);       // the last at the end, the room before it
    CHECK(std::abs(at(more).x - (at(add).x + s.actionSize) - s.actionMargin) < 0.5f); // the room between them
    CHECK(std::abs(at(add).y + s.actionSize / 2 - s.headerHeight / 2) < 0.5f);        // in the middle of the header
}
