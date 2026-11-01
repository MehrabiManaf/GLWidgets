// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Core/Assert.hpp>

#include <cstdio>
#include <cstdlib>

namespace glw::detail {

void assertionFailed(const char* kind, const char* expression, const char* file, int line) {
    std::fprintf(stderr, "GLWidgets: %s failed: %s\n  at %s:%d\n", kind, expression, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace glw::detail
