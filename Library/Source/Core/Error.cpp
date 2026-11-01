// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Core/Error.hpp>

namespace glw {

// out of line: the vtable and the type info are in the library, so an Error thrown inside it is caught by its type outside
Error::~Error() = default;

} // namespace glw
