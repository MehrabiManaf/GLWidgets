// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// The keys of key events, for shortcuts and navigation: what text a key types comes with TextInput, not from these.
// The letters are those of the US layout whatever the active layout is (Ctrl+C is Key::C on a Persian keyboard too).
#include <cstdint>

namespace glw::platform {

enum class Key : std::uint16_t {
    Unknown,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Escape, Enter, Tab, Backspace, Space, Insert, Delete,
    Left, Right, Up, Down, Home, End, PageUp, PageDown,
    Shift, Control, Alt, Super,
};

// Modifier keys held down, as flags. Super is the Windows key, Command on macOS.
enum class KeyModifiers : std::uint8_t {
    None    = 0,
    Shift   = 1 << 0,
    Control = 1 << 1,
    Alt     = 1 << 2,
    Super   = 1 << 3,
};

constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b) {
    return static_cast<KeyModifiers>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr KeyModifiers operator&(KeyModifiers a, KeyModifiers b) {
    return static_cast<KeyModifiers>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr bool hasAny(KeyModifiers set, KeyModifiers flags) { return (set & flags) != KeyModifiers::None; }

enum class MouseButton : std::uint8_t { Left, Middle, Right, Back, Forward };

} // namespace glw::platform
