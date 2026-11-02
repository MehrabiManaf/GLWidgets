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
#include <GLWidgets/Export.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace glw {

struct LocalizedText {
    std::string source;                     // English when translated; what is shown otherwise
    std::string key;                        // what the catalogs know it by; empty: the source text
    bool translated = false;                // false: shown as it is
    std::vector<std::pair<std::string, std::string>> arguments;   // {name} -> value

    LocalizedText() = default;
    // Shown as it is.
    LocalizedText(std::string text) : source(std::move(text)) {}
    LocalizedText(const char* text) : source(text) {}

    // `value` in place of {name}.
    LocalizedText& arg(std::string name, std::string value) & {
        arguments.emplace_back(std::move(name), std::move(value));
        return *this;
    }
    LocalizedText arg(std::string name, std::string value) && {
        arguments.emplace_back(std::move(name), std::move(value));
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

// Whether `language` (a BCP 47 tag: "fa", "ar-EG"...) is written right to left.
GLW_EXPORT bool isRightToLeftLanguage(std::string_view language);

class GLW_EXPORT Translator {
public:
    // Adds the texts of a catalog (later ones win for the same key). False, and `error`, when it is not a catalog.
    bool loadCatalog(std::string_view json, std::string* error = nullptr);
    bool loadCatalogFile(const std::string& path, std::string* error = nullptr);

    // The language texts are shown in. "en" (the default) shows the source texts.
    void setLanguage(std::string language) { language_ = std::move(language); }
    const std::string& language() const { return language_; }
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
    };
    std::unordered_map<std::string, std::unordered_map<std::string, Entry>> catalogs_;    // language -> key -> entry
    std::string language_ = "en";
};

} // namespace glw
