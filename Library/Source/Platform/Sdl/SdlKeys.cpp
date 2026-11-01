// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Platform/Sdl/SdlKeys.hpp"

#include <SDL3/SDL_mouse.h>

namespace glw::platform::sdl {

namespace {

Key offset(Key first, SDL_Keycode key, SDL_Keycode firstKey) {
    return static_cast<Key>(static_cast<int>(first) + static_cast<int>(key - firstKey));
}

} // namespace

Key toKey(SDL_Keycode key) {
    // the letters are the US layout's whatever the layout (SDL_HINT_KEYCODE_OPTIONS "latin_letters", SDL's default)
    if (key >= SDLK_A && key <= SDLK_Z)
        return offset(Key::A, key, SDLK_A);
    if (key >= SDLK_0 && key <= SDLK_9)
        return offset(Key::Digit0, key, SDLK_0);
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return offset(Key::F1, key, SDLK_F1);
    switch (key) {
    case SDLK_ESCAPE: return Key::Escape;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return Key::Enter;
    case SDLK_TAB: return Key::Tab;
    case SDLK_BACKSPACE: return Key::Backspace;
    case SDLK_SPACE: return Key::Space;
    case SDLK_INSERT: return Key::Insert;
    case SDLK_DELETE: return Key::Delete;
    case SDLK_LEFT: return Key::Left;
    case SDLK_RIGHT: return Key::Right;
    case SDLK_UP: return Key::Up;
    case SDLK_DOWN: return Key::Down;
    case SDLK_HOME: return Key::Home;
    case SDLK_END: return Key::End;
    case SDLK_PAGEUP: return Key::PageUp;
    case SDLK_PAGEDOWN: return Key::PageDown;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT: return Key::Shift;
    case SDLK_LCTRL:
    case SDLK_RCTRL: return Key::Control;
    case SDLK_LALT:
    case SDLK_RALT: return Key::Alt;
    case SDLK_LGUI:
    case SDLK_RGUI: return Key::Super;
    default: return Key::Unknown;
    }
}

KeyModifiers toModifiers(SDL_Keymod modifiers) {
    KeyModifiers result = KeyModifiers::None;
    if (modifiers & SDL_KMOD_SHIFT)
        result = result | KeyModifiers::Shift;
    if (modifiers & SDL_KMOD_CTRL)
        result = result | KeyModifiers::Control;
    if (modifiers & SDL_KMOD_ALT)
        result = result | KeyModifiers::Alt;
    if (modifiers & SDL_KMOD_GUI)
        result = result | KeyModifiers::Super;
    return result;
}

std::optional<MouseButton> toMouseButton(Uint8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT: return MouseButton::Left;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    case SDL_BUTTON_RIGHT: return MouseButton::Right;
    case SDL_BUTTON_X1: return MouseButton::Back;
    case SDL_BUTTON_X2: return MouseButton::Forward;
    default: return std::nullopt;
    }
}

} // namespace glw::platform::sdl
