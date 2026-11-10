// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Text shown to users, in their language.
//
// A LocalizedText is either shown as it is (a name, a number, what the user typed: made from a string) or translated
// (made by tr): its English source text is looked up in the catalog of the language in effect, by a key (the source text
// itself unless one is given). Arguments ({name} in the text) are put in after translation, so translators can move them:
//
//   label.setText(glw::tr("Saved {file}").arg("file", name));
//
// Widgets keep LocalizedTexts, not strings: when the language changes (Application::setLanguage), every widget shows its
// texts in the new one, and an interface written right to left (Persian, Arabic, Hebrew...) is mirrored.
//
// A catalog is a JSON file, one per language:
//   { "language": "fa", "texts": { "Close": "بستن", "Saved {file}": ["Saved {file}", "{file} ذخیره شد"] } }
// A text is its translation, or [the source text it translates, its translation]: when the source text is not the one of
// the LocalizedText any more (the English changed), the translation is outdated and not used.
//
// A number put in (arg with a number) is written in the language's way (its digits and marks: Locale), and a count
// chooses its text's form (trPlural: the English one and other forms; a catalog's translation, its language's forms):
//
//   glw::trPlural("{n} file", "{n} files", count);      // 1 file, 3 files; in Persian: ۳ فایل
//   { "texts": { "{n} file": { "one": "{n} فایل", "other": "{n} فایل" } } }
// The forms are CLDR's: zero, one, two, few, many, other (pluralCategory); one missing, "other" is taken.
#include <GLWidgets/Export.hpp>
#include <GLWidgets/Widgets/Locale.hpp>

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace glw {

struct LocalizedText {
    // What is put in place of {name}: a text, or a number (written in the language's way, `decimals` after its point).
    struct Argument {
        std::string name;
        std::string text;
        std::optional<double> number;
        int decimals = 0;
        friend bool operator==(const Argument&, const Argument&) = default;
    };

    std::string source;                     // English when translated; what is shown otherwise; counted: its one form
    std::string key;                        // what the catalogs know it by; empty: the source text
    bool translated = false;                // false: shown as it is
    std::vector<Argument> arguments;
    // Counted (trPlural): the English of other counts than one, and the count (with its decimals) that chooses
    std::string plural;
    double count = 0;
    int countDecimals = 0;

    LocalizedText() = default;
    // Shown as it is.
    LocalizedText(std::string text) : source(std::move(text)) {}
    LocalizedText(const char* text) : source(text) {}

    // `value` in place of {name}.
    LocalizedText& arg(std::string name, std::string value) & {
        arguments.push_back({std::move(name), std::move(value), std::nullopt, 0});
        return *this;
    }
    LocalizedText arg(std::string name, std::string value) && {
        arg(std::move(name), std::move(value));
        return std::move(*this);
    }
    // A number in place of {name}, `decimals` after its point, in the language's digits and marks.
    template <class T>
        requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
    LocalizedText& arg(std::string name, T value, int decimals = 0) & {
        arguments.push_back({std::move(name), {}, static_cast<double>(value), decimals});
        return *this;
    }
    template <class T>
        requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
    LocalizedText arg(std::string name, T value, int decimals = 0) && {
        arg(std::move(name), value, decimals);
        return std::move(*this);
    }
    bool empty() const { return source.empty(); }

    friend bool operator==(const LocalizedText&, const LocalizedText&) = default;
};

// A text to translate: `source` in English, known by `key` in the catalogs (by `source` itself when there is none).
inline LocalizedText tr(std::string source, std::string key = {}) {
    LocalizedText text(std::move(source));
    text.key = std::move(key);
    text.translated = true;
    return text;
}

// A text whose form a count chooses: `one` (the English of one: "{n} file"), `other` (of other counts: "{n} files"),
// known by `key` in the catalogs (by `one` when there is none). The count is put in for {n}, `decimals` after its point.
template <class T>
    requires(std::integral<T> || std::floating_point<T>) && (!std::same_as<T, bool>)
LocalizedText trPlural(std::string one, std::string other, T count, int decimals = 0, std::string key = {}) {
    LocalizedText text = tr(std::move(one), std::move(key));
    text.plural = std::move(other);
    text.count = static_cast<double>(count);
    text.countDecimals = decimals;
    text.arg("n", count, decimals);
    return text;
}

// Whether `language` (a BCP 47 tag: "fa", "ar-EG"...) is written right to left.
GLW_EXPORT bool isRightToLeftLanguage(std::string_view language);

class GLW_EXPORT Translator {
public:
    // Adds the texts of a catalog (later ones win for the same key). False, and `error`, when it is not a catalog.
    bool loadCatalog(std::string_view json, std::string* error = nullptr);
    bool loadCatalogFile(const std::string& path, std::string* error = nullptr);

    // The language texts are shown in (its catalog, or its primary language's: "fa" for "fa-IR"). "en" (the default)
    // shows the source texts. Its locale goes with it (Locale::of), unless one is set.
    void setLanguage(std::string language);
    const std::string& language() const { return language_; }
    // How numbers, dates and counts are written: the language's (Locale::of), or one of the application's (none: the
    // language's again).
    void setLocale(std::optional<Locale> locale);
    const Locale& locale() const { return locale_; }
    bool rightToLeft() const { return isRightToLeftLanguage(language_); }
    // The languages with a catalog, and "en".
    std::vector<std::string> languages() const;

    // What `text` shows in the language in effect: translated when it is to be and its translation is up to date, its
    // arguments put in.
    std::string resolve(const LocalizedText& text) const;

private:
    struct Entry {
        std::string source;                 // what the translation is of; empty: whatever the key's source text is
        std::string translation;
        bool counted = false;               // its forms, by PluralCategory, instead
        std::array<std::string, 6> forms;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, Entry>> catalogs_;    // language -> key -> entry
    std::string language_ = "en";
    Locale locale_;
    std::optional<Locale> chosen_;          // the application's
};

} // namespace glw
