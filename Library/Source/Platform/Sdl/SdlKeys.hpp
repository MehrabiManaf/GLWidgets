// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// SDL's keys, modifiers and mouse buttons as GLWidgets' (Keys.hpp).
#include <GLWidgets/Platform/Keys.hpp>

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>

#include <optional>

namespace glw::platform::sdl {

Key toKey(SDL_Keycode key);
KeyModifiers toModifiers(SDL_Keymod modifiers);
std::optional<MouseButton> toMouseButton(Uint8 button);     // none for the buttons GLWidgets does not know

} // namespace glw::platform::sdl
