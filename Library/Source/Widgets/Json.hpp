// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A small JSON reader, for the catalogs of translations: objects, arrays, strings (with every escape, \u surrogate pairs
// included), numbers (as doubles), true, false, null.
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace glw::json {

struct Value;
// vectors, which may hold a type not complete yet (a map may not): an object's members in order, a key at most once
using Object = std::vector<std::pair<std::string, Value>>;
using Array = std::vector<Value>;

struct Value {
    // null, a boolean, a number, a string, an array, an object
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    const std::string* string() const { return std::get_if<std::string>(&data); }
    const Array* array() const { return std::get_if<Array>(&data); }
    const Object* object() const { return std::get_if<Object>(&data); }
    // The member `key` of an object, if it is one and has it.
    const Value* member(std::string_view key) const {
        if (const Object* o = object()) {
            for (const auto& [k, v] : *o) {
                if (k == key)
                    return &v;
            }
        }
        return nullptr;
    }
};

// The value `text` holds, or none (and `error`) when it is not JSON.
std::optional<Value> parse(std::string_view text, std::string* error = nullptr);

} // namespace glw::json
