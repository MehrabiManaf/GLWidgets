// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// A small test harness: TEST(name) { ... CHECK(condition); ... } in any test file, run by Main.cpp. A failed CHECK reports
// itself and the test goes on; the program fails when one did.
#include <cstdio>
#include <vector>

namespace glw::tests {

struct TestCase {
    const char* name;
    void (*run)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

inline bool check(bool ok, const char* expression, const char* file, int line) {
    if (!ok) {
        ++failures();
        std::printf("    failed: %s\n      at %s:%d\n", expression, file, line);
    }
    return ok;
}

} // namespace glw::tests

#define TEST(name)                                                              \
    static void test_##name();                                                  \
    static const ::glw::tests::Registrar registrar_##name(#name, &test_##name); \
    static void test_##name()

#define CHECK(condition) ::glw::tests::check(static_cast<bool>(condition), #condition, __FILE__, __LINE__)
