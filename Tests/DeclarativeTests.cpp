// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// The builders of Widgets/Declarative.hpp: the trees they make, the props and settings they give, bind.
#include "Check.hpp"

#include <GLWidgets/Text/Font.hpp>
#include <GLWidgets/Text/GlyphAtlas.hpp>
#include <GLWidgets/Widgets/Declarative.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <cmath>
#include <string>

using namespace glw;
namespace ui = glw::ui;

namespace {

UiContext& context() {
    static text::FontLibrary fonts;
    static text::GlyphAtlas atlas(1, 256);
    static UiContext ui{fonts, atlas, Theme{}};
    return ui;
}

bool near(float a, float b) {
    return std::abs(a - b) < 0.01f;
}

// A widget of the application's own, made by ui::make.
struct Swatch : Widget {
    Color color;
    explicit Swatch(Color c) : color(c) { setMinimumSize({20, 20}); }
};

} // namespace

TEST(builtTreeHasTheShapeWritten) {
    Label* title = nullptr;
    Button* ok = nullptr;
    std::unique_ptr<Widget> tree = ui::VStack({.spacing = 8},
                                              ui::Label({.text = "Title", .fontSize = 20}).bind(title),
                                              ui::HStack({.spacing = 4}, ui::Button({.text = "OK"}).bind(ok), ui::Button({.text = "Cancel"})),
                                              ui::Spacer());
    CHECK(tree->children().size() == 3);
    CHECK(tree->children()[1]->children().size() == 2);
    CHECK(title == tree->children()[0].get());
    CHECK(title && title->text() == "Title");
    CHECK(ok && ok->text() == "OK" && ok->parent() == tree->children()[1].get());
    CHECK(tree->children()[2]->stretch() == 1);
    CHECK(dynamic_cast<StackLayout*>(tree->layout()) && static_cast<StackLayout*>(tree->layout())->spacing() == 8);
}

TEST(chainedSettings) {
    Widget* w = nullptr;
    std::unique_ptr<Widget> tree = ui::VStack({},
                                              ui::Label({.text = "x"})
                                                  .stretch(2)
                                                  .margin(Insets::all(3))
                                                  .padding(Insets::symmetric(5, 6))
                                                  .alignment(Alignment::End, Alignment::Center)
                                                  .minimumSize({10, 11})
                                                  .enabled(false)
                                                  .direction(LayoutDirection::RightToLeft)
                                                  .bind(w));
    CHECK(w != nullptr);
    CHECK(w->stretch() == 2);
    CHECK(w->margin() == Insets::all(3));
    CHECK(w->padding() == Insets::symmetric(5, 6));
    CHECK(w->horizontalAlignment() == Alignment::End && w->verticalAlignment() == Alignment::Center);
    CHECK(w->minimumSize() == Size{10, 11});
    CHECK(!w->enabled());
    CHECK(w->layoutDirection() == LayoutDirection::RightToLeft);
}

TEST(handlersAndWith) {
    int clicks = 0;
    Button* button = nullptr;
    std::unique_ptr<Widget> tree = ui::Button({.text = "Go", .onClick = [&] { ++clicks; }})
                                       .with([](Button& b) { b.setText("Go!"); })
                                       .bind(button);
    button->click();
    CHECK(clicks == 1);
    CHECK(button->text() == "Go!");
}

TEST(dynamicChildrenAndOwnWidgets) {
    ui::Children rows;
    for (int i = 0; i < 5; ++i)
        rows.push_back(ui::Label({.text = std::to_string(i)}));
    Swatch* swatch = nullptr;
    std::unique_ptr<Widget> tree = ui::VStack({}, ui::VStack({.spacing = 2}, std::move(rows)), ui::make<Swatch>(Color{1, 0, 0, 1}).bind(swatch));
    CHECK(tree->children()[0]->children().size() == 5);
    CHECK(swatch && swatch->color == Color{1, 0, 0, 1});

    // a widget made by hand is a child too
    auto made = std::make_unique<Label>("by hand");
    std::unique_ptr<Widget> mixed = ui::HStack({}, std::move(made), ui::Label({.text = "built"}));
    CHECK(mixed->children().size() == 2);
}

TEST(builtTreeLaysOut) {
    UiRoot root(context());
    root.setSize({300, 200});
    Widget* left = nullptr;
    Widget* right = nullptr;
    root.setContent(ui::HStack({.spacing = 10},
                               ui::Panel({.background = Color{1, 1, 1, 1}}).stretch(1).bind(left),
                               ui::Panel({.background = Color{0, 0, 0, 1}}).stretch(2).bind(right))
                        .padding(Insets::all(10)));
    root.layout();
    // 300 - 2 x 10 padding - 10 spacing = 270: a third and two thirds
    CHECK(near(left->bounds().x, 10) && near(left->bounds().width, 90));
    CHECK(near(right->bounds().x, 110) && near(right->bounds().width, 180));
    CHECK(near(left->bounds().height, 180));
}
