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

    auto& catalog = catalogs_[*language->string()];
    for (const auto& [key, value] : *texts->object()) {
        if (const std::string* translation = value.string()) {
            catalog.insert_or_assign(key, Entry{{}, *translation});
        } else if (const json::Array* pair = value.array(); pair && pair->size() == 2 && (*pair)[0].string() && (*pair)[1].string()) {
            catalog.insert_or_assign(key, Entry{*(*pair)[0].string(), *(*pair)[1].string()});
        } else {
            return fail("the text \"" + key + "\" is neither a translation nor [source, translation]");
        }
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
    std::string result = text.source;
    if (text.translated) {
        if (const auto catalog = catalogs_.find(language_); catalog != catalogs_.end()) {
            const auto entry = catalog->second.find(text.key.empty() ? text.source : text.key);
            // a translation of another English text is outdated: the source shows instead
            if (entry != catalog->second.end() && !entry->second.translation.empty() &&
                (entry->second.source.empty() || entry->second.source == text.source))
                result = entry->second.translation;
        }
    }
    for (const auto& [name, value] : text.arguments) {
        const std::string placeholder = "{" + name + "}";
        for (std::size_t at = result.find(placeholder); at != std::string::npos; at = result.find(placeholder, at + value.size()))
            result.replace(at, placeholder.size(), value);
    }
    return result;
}

} // namespace glw
