// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Localization.hpp>

#include "Widgets/Json.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace glw {

bool isRightToLeftLanguage(std::string_view language) {
    // the primary subtag ("fa" of "fa-IR")
    const std::string_view primary = language.substr(0, language.find_first_of("-_"));
    for (std::string_view rtl : {"ar", "fa", "he", "ur", "ps", "sd", "ug", "yi", "dv", "ckb", "ku-Arab", "syr", "iw"}) {
        if (primary == rtl)
            return true;
    }
    return false;
}

bool Translator::loadCatalog(std::string_view text, std::string* error) {
    const auto fail = [&](const std::string& message) {
        if (error)
            *error = message;
        return false;
    };
    std::string parseError;
    const std::optional<json::Value> document = json::parse(text, &parseError);
    if (!document)
        return fail("not JSON: " + parseError);
    if (!document->object())
        return fail("a catalog is a JSON object");
    const json::Value* language = document->member("language");
    const json::Value* texts = document->member("texts");
    if (!language || !language->string())
        return fail("a catalog says its \"language\"");
    if (!texts || !texts->object())
        return fail("a catalog has its \"texts\" in an object");

    // a counted text's forms: {"one": ..., "other": ...}
    static constexpr const char* categories[] = {"zero", "one", "two", "few", "many", "other"};
    const auto formsOf = [&](const json::Value& value, Entry& entry) {
        const auto* object = value.object();
        if (!object)
            return false;
        entry.counted = true;
        for (const auto& [name, form] : *object) {
            const std::string* text = form.string();
            const auto at = std::find_if(std::begin(categories), std::end(categories), [&](const char* c) { return name == c; });
            if (!text || at == std::end(categories))
                return false;
            entry.forms[static_cast<std::size_t>(at - std::begin(categories))] = *text;
        }
        return true;
    };
    auto& catalog = catalogs_[*language->string()];
    for (const auto& [key, value] : *texts->object()) {
        Entry entry;
        const json::Array* pair = value.array();
        if (const std::string* translation = value.string()) {
            entry.translation = *translation;
        } else if (value.object()) {
            if (!formsOf(value, entry))
                return fail("the forms of \"" + key + "\" are not zero, one, two, few, many, other");
        } else if (pair && pair->size() == 2 && (*pair)[0].string() && ((*pair)[1].string() || (*pair)[1].object())) {
            entry.source = *(*pair)[0].string();
            if ((*pair)[1].string())
                entry.translation = *(*pair)[1].string();
            else if (!formsOf((*pair)[1], entry))
                return fail("the forms of \"" + key + "\" are not zero, one, two, few, many, other");
        } else {
            return fail("the text \"" + key + "\" is neither a translation, its forms, nor [source, either]");
        }
        catalog.insert_or_assign(key, std::move(entry));
    }
    return true;
}

bool Translator::loadCatalogFile(const std::string& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error)
            *error = "cannot open " + path;
        return false;
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return loadCatalog(text, error);
}

void Translator::setLanguage(std::string language) {
    language_ = std::move(language);
    if (!chosen_)
        locale_ = Locale::of(language_);
}

void Translator::setLocale(std::optional<Locale> locale) {
    chosen_ = std::move(locale);
    locale_ = chosen_ ? *chosen_ : Locale::of(language_);
}

std::vector<std::string> Translator::languages() const {
    std::vector<std::string> result{"en"};
    for (const auto& [language, texts] : catalogs_) {
        if (language != "en")
            result.push_back(language);
    }
    std::sort(result.begin() + 1, result.end());
    return result;
}

std::string Translator::resolve(const LocalizedText& text) const {
    // counted: English's form for the count, until a translation gives its language's
    const bool counted = !text.plural.empty();
    std::string result = counted && pluralCategory("en", text.count, text.countDecimals) != PluralCategory::One ? text.plural : text.source;
    if (text.translated) {
        // the language's catalog, or its primary language's ("fa" for "fa-IR")
        auto catalog = catalogs_.find(language_);
        if (catalog == catalogs_.end())
            catalog = catalogs_.find(language_.substr(0, language_.find_first_of("-_")));
        if (catalog != catalogs_.end()) {
            const auto found = catalog->second.find(text.key.empty() ? text.source : text.key);
            // a translation of another English text is outdated: the source shows instead
            if (found != catalog->second.end() && (found->second.source.empty() || found->second.source == text.source)) {
                const Entry& entry = found->second;
                if (entry.counted) {
                    const auto category = static_cast<std::size_t>(pluralCategory(language_, text.count, text.countDecimals));
                    const std::string& form = !entry.forms[category].empty() ? entry.forms[category] : entry.forms[5];
                    if (!form.empty())
                        result = form;
                } else if (!entry.translation.empty()) {
                    result = entry.translation;
                }
            }
        }
    }
    for (const LocalizedText::Argument& argument : text.arguments) {
        const std::string value = argument.number ? locale_.number(*argument.number, argument.decimals) : argument.text;
        const std::string placeholder = "{" + argument.name + "}";
        for (std::size_t at = result.find(placeholder); at != std::string::npos; at = result.find(placeholder, at + value.size()))
            result.replace(at, placeholder.size(), value);
    }
    return result;
}

} // namespace glw
