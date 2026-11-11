// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// An application's preferences, kept in a JSON file: strings, booleans, numbers and lists of strings, by key. A saved
// workspace (saveWorkspace) or any other text is kept as a string.
//
//   glw::Settings settings(app.platform().preferencesFolder("Org", "App") + "settings.json");
//   settings.load();                                    // a file not there yet: nothing, and no error
//   auto recent = settings.strings("recentFiles");
//   settings.setStrings("recentFiles", recent);
//   settings.save();
//
// A value of another type than asked for gives the default. Keys are kept in the order they were first set.
#include <GLWidgets/Export.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace glw {

class GLW_EXPORT Settings {
public:
    // Kept in the file `path` (UTF-8).
    explicit Settings(std::string path = {});
    ~Settings();
    Settings(Settings&&) noexcept;
    Settings& operator=(Settings&&) noexcept;

    const std::string& path() const;
    void setPath(std::string path);

    // Reads the file, in place of what it held. A file not there is no error: nothing is set. False (and `error`), with
    // nothing changed, when it cannot be read or is not an object of JSON.
    bool load(std::string* error = nullptr);
    // Writes the file (a new file first, then put in place of the old one: a crash does not leave half a file), making
    // its folder if needed. False (and `error`) when it cannot be written.
    bool save(std::string* error = nullptr) const;
    // Something was set or removed since it was loaded or saved.
    bool isDirty() const;

    bool contains(std::string_view key) const;
    void remove(std::string_view key);
    void clear();
    std::vector<std::string> keys() const;

    std::string string(std::string_view key, std::string_view fallback = {}) const;
    bool boolean(std::string_view key, bool fallback = false) const;
    double number(std::string_view key, double fallback = 0) const;
    int integer(std::string_view key, int fallback = 0) const;
    std::vector<std::string> strings(std::string_view key) const;

    void setString(std::string_view key, std::string_view value);
    void setBoolean(std::string_view key, bool value);
    void setNumber(std::string_view key, double value);
    void setStrings(std::string_view key, const std::vector<std::string>& values);

    // All of it as JSON text, and back (false, with nothing changed, when `text` is not an object of JSON).
    std::string toJson() const;
    bool fromJson(std::string_view text, std::string* error = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace glw
