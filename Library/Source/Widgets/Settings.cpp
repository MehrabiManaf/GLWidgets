// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Widgets/Settings.hpp>

#include "Widgets/Json.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace glw {

namespace {

std::filesystem::path fsPath(std::string_view utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace

struct Settings::Impl {
    std::string path;
    json::Object values;
    bool dirty = false;

    const json::Value* find(std::string_view key) const {
        for (const auto& [k, v] : values) {
            if (k == key)
                return &v;
        }
        return nullptr;
    }
    void set(std::string_view key, json::Value value) {
        dirty = true;
        for (auto& [k, v] : values) {
            if (k == key) {
                v = std::move(value);
                return;
            }
        }
        values.emplace_back(std::string(key), std::move(value));
    }
};

Settings::Settings(std::string path) : impl_(std::make_unique<Impl>()) { impl_->path = std::move(path); }
Settings::~Settings() = default;
Settings::Settings(Settings&&) noexcept = default;
Settings& Settings::operator=(Settings&&) noexcept = default;

const std::string& Settings::path() const { return impl_->path; }
void Settings::setPath(std::string path) { impl_->path = std::move(path); }

bool Settings::load(std::string* error) {
    const auto file = fsPath(impl_->path);
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        impl_->values.clear();
        impl_->dirty = false;
        return true;
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error)
            *error = "cannot read " + impl_->path;
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!fromJson(text, error))
        return false;
    impl_->dirty = false;
    return true;
}

bool Settings::save(std::string* error) const {
    const auto file = fsPath(impl_->path);
    std::error_code ec;
    if (file.has_parent_path())
        std::filesystem::create_directories(file.parent_path(), ec);
    auto temporary = file;
    temporary += ".new";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << toJson() << '\n';
        out.close();
        if (!out) {
            if (error)
                *error = "cannot write " + impl_->path;
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        if (error)
            *error = "cannot write " + impl_->path + ": " + ec.message();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    impl_->dirty = false;
    return true;
}

bool Settings::isDirty() const { return impl_->dirty; }

bool Settings::contains(std::string_view key) const { return impl_->find(key) != nullptr; }

void Settings::remove(std::string_view key) {
    auto& values = impl_->values;
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it->first == key) {
            values.erase(it);
            impl_->dirty = true;
            return;
        }
    }
}

void Settings::clear() {
    if (!impl_->values.empty())
        impl_->dirty = true;
    impl_->values.clear();
}

std::vector<std::string> Settings::keys() const {
    std::vector<std::string> keys;
    for (const auto& [k, v] : impl_->values)
        keys.push_back(k);
    return keys;
}

std::string Settings::string(std::string_view key, std::string_view fallback) const {
    if (const json::Value* v = impl_->find(key); v && v->string())
        return *v->string();
    return std::string(fallback);
}

bool Settings::boolean(std::string_view key, bool fallback) const {
    if (const json::Value* v = impl_->find(key)) {
        if (const bool* b = std::get_if<bool>(&v->data))
            return *b;
    }
    return fallback;
}

double Settings::number(std::string_view key, double fallback) const {
    if (const json::Value* v = impl_->find(key)) {
        if (const double* d = std::get_if<double>(&v->data))
            return *d;
    }
    return fallback;
}

int Settings::integer(std::string_view key, int fallback) const {
    const double d = number(key, std::nan(""));
    if (std::isnan(d) || d < -2147483648.0 || d > 2147483647.0)
        return fallback;
    return static_cast<int>(std::lround(d));
}

std::vector<std::string> Settings::strings(std::string_view key) const {
    std::vector<std::string> result;
    if (const json::Value* v = impl_->find(key); v && v->array()) {
        for (const json::Value& item : *v->array()) {
            if (item.string())
                result.push_back(*item.string());
        }
    }
    return result;
}

void Settings::setString(std::string_view key, std::string_view value) { impl_->set(key, {std::string(value)}); }
void Settings::setBoolean(std::string_view key, bool value) { impl_->set(key, {value}); }
void Settings::setNumber(std::string_view key, double value) { impl_->set(key, {value}); }

void Settings::setStrings(std::string_view key, const std::vector<std::string>& values) {
    json::Array array;
    for (const auto& s : values)
        array.push_back({s});
    impl_->set(key, {std::move(array)});
}

std::string Settings::toJson() const { return json::write({impl_->values}); }

bool Settings::fromJson(std::string_view text, std::string* error) {
    std::optional<json::Value> parsed = json::parse(text, error);
    if (!parsed)
        return false;
    json::Object* object = std::get_if<json::Object>(&parsed->data);
    if (!object) {
        if (error)
            *error = "the settings are not an object";
        return false;
    }
    impl_->values = std::move(*object);
    impl_->dirty = true;
    return true;
}

} // namespace glw
