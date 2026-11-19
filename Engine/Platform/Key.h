#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace apex::platform {

// Values are USB HID keyboard usage codes (page 0x07), which are also SDL scancodes.
// Names designate the physical position on a US QWERTY layout: the AZERTY "Z" key is Key::W.
enum class Key : std::uint16_t {
    Unknown = 0,
    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,
    Digit1 = 30,
    Digit2 = 31,
    Digit3 = 32,
    Digit4 = 33,
    Digit5 = 34,
    Digit6 = 35,
    Digit7 = 36,
    Digit8 = 37,
    Digit9 = 38,
    Digit0 = 39,
    Return = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,
    LeftCtrl = 224,
    LeftShift = 225,
    LeftAlt = 226,
    RightCtrl = 228,
    RightShift = 229,
    RightAlt = 230,
};

// Every named key, Key::Unknown excluded.
[[nodiscard]] std::span<const Key> allKeys() noexcept;

// Case-sensitive; the name is the enumerator name ("A", "Digit1", "LeftShift").
[[nodiscard]] std::optional<Key> keyFromName(std::string_view name) noexcept;

// "Unknown" for Key::Unknown and for values that have no enumerator.
[[nodiscard]] std::string_view keyName(Key key) noexcept;

} // namespace apex::platform
