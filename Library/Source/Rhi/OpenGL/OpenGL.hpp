// SPDX-License-Identifier: MIT
#pragma once
#include <GLWidgets/Rhi/Device.hpp>

namespace glw::rhi::legacy {
std::unique_ptr<Device> create(platform::Platform& platform);
}
