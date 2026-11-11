// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
// Runs every TEST, or those whose name contains the first argument.
#include "Check.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    // each line as it is printed: a test that crashes leaves the ones before it said (piped, the output would be held)
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const auto& test : glw::tests::registry()) {
        if (filter && !std::strstr(test.name, filter))
            continue;
        const int before = glw::tests::failures();
        test.run();
        std::printf("%s %s\n", glw::tests::failures() == before ? "ok    " : "FAILED", test.name);
        ++run;
    }
    std::printf("%d tests, %d failed checks\n", run, glw::tests::failures());
    return glw::tests::failures() == 0 ? 0 : 1;
}
