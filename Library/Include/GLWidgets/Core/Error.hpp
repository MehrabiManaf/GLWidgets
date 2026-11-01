// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// What GLWidgets throws when something it cannot do without fails at run time: the platform does not start, no GPU device can
// be created, a window cannot be made. A bug of the caller is a failed contract check (Assert.hpp), not an Error.
#include <GLWidgets/Export.hpp>

#include <stdexcept>

namespace glw {

class GLW_EXPORT Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    ~Error() override;
};

} // namespace glw
