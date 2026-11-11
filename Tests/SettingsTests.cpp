// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Check.hpp"

#include <GLWidgets/Widgets/Settings.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace glw;

TEST(settingsKeepTheirValuesByTypeAndInOrder) {
    Settings s;
    CHECK(!s.isDirty() && s.keys().empty());
    s.setString("theme", "dark");
    s.setBoolean("snap", true);
    s.setNumber("zoom", 1.25);
    s.setStrings("recent", {"a.flow", "b.flow"});
    CHECK(s.isDirty());
    CHECK(s.string("theme") == "dark" && s.boolean("snap") && s.number("zoom") == 1.25);
    CHECK((s.strings("recent") == std::vector<std::string>{"a.flow", "b.flow"}));
    CHECK((s.keys() == std::vector<std::string>{"theme", "snap", "zoom", "recent"}));
    // another type than asked for, or none: the default
    CHECK(s.boolean("theme", true) && s.number("snap", 7) == 7 && s.string("zoom", "x") == "x" && s.strings("theme").empty());
    CHECK(s.integer("zoom") == 1 && s.integer("missing", 3) == 3);
    // set again: in its place
    s.setString("theme", "light");
    CHECK(s.keys().front() == "theme" && s.string("theme") == "light");
    s.remove("snap");
    CHECK(!s.contains("snap") && s.keys().size() == 3);
}

TEST(settingsGoToAFileAndComeBack) {
    const auto folder = std::filesystem::temp_directory_path() / "glw_settings_test";
    std::filesystem::remove_all(folder);
    const std::string file = (folder / "sub" / "settings.json").string();
    {
        Settings s(file);
        CHECK(s.load() && s.keys().empty());           // not there yet: no error
        s.setString("name", "\xD9\x81\xD9\x84\xD9\x88\xDA\xAF\xD8\xB1\xD8\xA7\xD9\x81 \"q\"\n");
        s.setString("workspace", R"({"windows":[{"x":1}]})");
        s.setNumber("width", 1280);
        std::string error;
        CHECK(s.save(&error) && !s.isDirty());          // its folder made
        CHECK(!std::filesystem::exists(file + ".new"));
    }
    {
        Settings s(file);
        CHECK(s.load() && !s.isDirty());
        CHECK(s.string("name") == "\xD9\x81\xD9\x84\xD9\x88\xDA\xAF\xD8\xB1\xD8\xA7\xD9\x81 \"q\"\n");
        CHECK(s.string("workspace") == R"({"windows":[{"x":1}]})");
        CHECK(s.integer("width") == 1280);
    }
    {
        // not JSON: false, and what it held stays
        std::ofstream(folder / "sub" / "settings.json") << "{ broken";
        Settings s(file);
        s.setBoolean("kept", true);
        std::string error;
        CHECK(!s.load(&error) && !error.empty() && s.boolean("kept"));
        CHECK(!s.fromJson("[1, 2]") && s.boolean("kept"));
    }
    std::filesystem::remove_all(folder);
}
