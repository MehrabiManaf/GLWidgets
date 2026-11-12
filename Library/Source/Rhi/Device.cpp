// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include <GLWidgets/Rhi/Device.hpp>

namespace glw::rhi {

std::size_t bytesPerPixel(TextureFormat format) {
    switch (format) {
    case TextureFormat::Unknown: return 0;
    case TextureFormat::RGBA8:
    case TextureFormat::BGRA8:
    case TextureFormat::RGBA8Srgb:
    case TextureFormat::BGRA8Srgb:
    case TextureFormat::RGB10A2:
    case TextureFormat::Depth32Float: return 4;
    case TextureFormat::R8: return 1;
    case TextureFormat::RGBA16Float: return 8;
    }
    return 0;
}

Texture::~Texture() = default;
Buffer::~Buffer() = default;
Sampler::~Sampler() = default;
Shader::~Shader() = default;
Pipeline::~Pipeline() = default;
Surface::~Surface() = default;
CommandList::~CommandList() = default;
Device::~Device() = default;

} // namespace glw::rhi
