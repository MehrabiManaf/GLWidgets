// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Translations (catalogs, arguments, outdated texts, directions), and styles: state colors, a widget's own style, a change
// of theme.
#include "Check.hpp"
#include "TestUi.hpp"

#include <GLWidgets/Widgets/Button.hpp>
#include <GLWidgets/Widgets/Label.hpp>
#include <GLWidgets/Widgets/Localization.hpp>
#include <GLWidgets/Widgets/UiRoot.hpp>

#include <string>

using namespace glw;

namespace {

constexpr const char* catalog = R"({
  "language": "fa",
  "texts": {
    "Close": "بستن",
    "Saved {file}": ["Saved {file}", "{file} ذخیره شد"],
    "Old text": ["The English it once was", "ترجمه‌ی کهنه"],
    "file.open": "باز کردن",
    "Escapes": "سلام 😀 \"\\\""
  }
})";

} // namespace

TEST(translations) {
    Translator t;
    std::string error;
    CHECK(t.loadCatalog(catalog, &error));
    CHECK(error.empty());
    CHECK(t.resolve(tr("Close")) == "Close");           // English until the language changes
    t.setLanguage("fa");
    CHECK(t.rightToLeft());
    CHECK(t.resolve(tr("Close")) == "بستن");
    CHECK(t.resolve(LocalizedText("Close")) == "Close");                             // not to be translated
    CHECK(t.resolve(tr("Saved {file}").arg("file", "a.txt")) == "a.txt ذخیره شد");    // the argument after translation
    CHECK(t.resolve(tr("Old text")) == "Old text");                                  // outdated: the English shows
    CHECK(t.resolve(tr("Open", "file.open")) == "باز کردن");                         // by its key
    CHECK(t.resolve(tr("Not translated")) == "Not translated");
    CHECK(t.resolve(tr("Escapes")) == "سلام \xF0\x9F\x98\x80 \"\\\"");               // \u, a surrogate pair, quotes
    CHECK((t.languages() == std::vector<std::string>{"en", "fa"}));
}

TEST(badCatalogs) {
    Translator t;
    std::string error;
    CHECK(!t.loadCatalog("{ \"language\": \"fa\", \"texts\": { \"a\": 1 } }", &error));
    CHECK(!error.empty());
    CHECK(!t.loadCatalog("{ \"texts\": {} }", &error));
    CHECK(!t.loadCatalog("{ \"language\": \"fa\", \"texts\": { \"a\": \"b\" ", &error));      // cut short
    CHECK(error.find("expected") != std::string::npos);
}

TEST(rightToLeftLanguages) {
    CHECK(isRightToLeftLanguage("fa"));
    CHECK(isRightToLeftLanguage("fa-IR"));
    CHECK(isRightToLeftLanguage("ar_EG"));
    CHECK(isRightToLeftLanguage("he"));
    CHECK(!isRightToLeftLanguage("en"));
    CHECK(!isRightToLeftLanguage("fr-CA"));
    CHECK(!isRightToLeftLanguage(""));
}

TEST(labelsFollowTheLanguage) {
    Translator& translator = tests::testTranslator();
    CHECK(translator.loadCatalog(catalog));
    UiRoot root(tests::testContext());
    root.setSize({300, 100});
    auto label = std::make_unique<Label>(tr("Close"));
    Label* l = label.get();
    root.setContent(std::move(label));
    root.layout();
    const float english = l->desiredSize().width;
    CHECK(l->shownText() == "Close");

    translator.setLanguage("fa");
    root.languageChanged();
    CHECK(root.needsLayout());
    root.layout();
    CHECK(l->shownText() == "بستن");
    CHECK(l->desiredSize().width != english);       // measured again, in the new text
    translator.setLanguage("en");
}

TEST(stateColors) {
    const StateColor c{.normal = {0, 0, 0, 1}, .hovered = Color{1, 0, 0, 1}, .pressed = Color{0, 1, 0, 1}, .disabled = Color{0, 0, 1, 1}};
    CHECK(c.resolve({}) == Color{0, 0, 0, 1});
    CHECK(c.resolve({.hovered = true}) == Color{1, 0, 0, 1});
    CHECK(c.resolve({.hovered = true, .pressed = true}) == Color{0, 1, 0, 1});
    CHECK(c.resolve({.hovered = true, .pressed = true, .disabled = true}) == Color{0, 0, 1, 1});
    CHECK(c.resolve({.focused = true}) == Color{0, 0, 0, 1});    // no focused color: the normal one
}

TEST(stylesOfAWidgetAndOfTheTheme) {
    UiContext& ui = tests::testContext();
    UiRoot root(ui);
    root.setSize({300, 100});
    auto button = std::make_unique<Button>("OK");
    Button* b = button.get();
    root.setContent(std::move(button));
    CHECK(b->padding() == ui.theme.button.padding);

    // a theme with other paddings: the button takes them when told
    const Theme saved = ui.theme;
    ui.theme.button.padding = Insets::all(20);
    root.styleChanged();
    CHECK(b->padding() == Insets::all(20));

    // its own style wins over the theme's
    ButtonStyle own = ui.theme.button;
    own.padding = Insets::all(3);
    b->setStyle(own);
    CHECK(b->padding() == Insets::all(3));
    b->setStyle(std::nullopt);
    CHECK(b->padding() == Insets::all(20));
    ui.theme = saved;
}
