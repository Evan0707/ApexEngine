#include "Platform/Key.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace apex::platform {

namespace {

struct KeyEntry {
    Key key;
    std::string_view name;
};

constexpr std::array kKeyEntries{
    KeyEntry{Key::A, "A"},
    KeyEntry{Key::B, "B"},
    KeyEntry{Key::C, "C"},
    KeyEntry{Key::D, "D"},
    KeyEntry{Key::E, "E"},
    KeyEntry{Key::F, "F"},
    KeyEntry{Key::G, "G"},
    KeyEntry{Key::H, "H"},
    KeyEntry{Key::I, "I"},
    KeyEntry{Key::J, "J"},
    KeyEntry{Key::K, "K"},
    KeyEntry{Key::L, "L"},
    KeyEntry{Key::M, "M"},
    KeyEntry{Key::N, "N"},
    KeyEntry{Key::O, "O"},
    KeyEntry{Key::P, "P"},
    KeyEntry{Key::Q, "Q"},
    KeyEntry{Key::R, "R"},
    KeyEntry{Key::S, "S"},
    KeyEntry{Key::T, "T"},
    KeyEntry{Key::U, "U"},
    KeyEntry{Key::V, "V"},
    KeyEntry{Key::W, "W"},
    KeyEntry{Key::X, "X"},
    KeyEntry{Key::Y, "Y"},
    KeyEntry{Key::Z, "Z"},
    KeyEntry{Key::Digit1, "Digit1"},
    KeyEntry{Key::Digit2, "Digit2"},
    KeyEntry{Key::Digit3, "Digit3"},
    KeyEntry{Key::Digit4, "Digit4"},
    KeyEntry{Key::Digit5, "Digit5"},
    KeyEntry{Key::Digit6, "Digit6"},
    KeyEntry{Key::Digit7, "Digit7"},
    KeyEntry{Key::Digit8, "Digit8"},
    KeyEntry{Key::Digit9, "Digit9"},
    KeyEntry{Key::Digit0, "Digit0"},
    KeyEntry{Key::Return, "Return"},
    KeyEntry{Key::Escape, "Escape"},
    KeyEntry{Key::Backspace, "Backspace"},
    KeyEntry{Key::Tab, "Tab"},
    KeyEntry{Key::Space, "Space"},
    KeyEntry{Key::F1, "F1"},
    KeyEntry{Key::F2, "F2"},
    KeyEntry{Key::F3, "F3"},
    KeyEntry{Key::F4, "F4"},
    KeyEntry{Key::F5, "F5"},
    KeyEntry{Key::F6, "F6"},
    KeyEntry{Key::F7, "F7"},
    KeyEntry{Key::F8, "F8"},
    KeyEntry{Key::F9, "F9"},
    KeyEntry{Key::F10, "F10"},
    KeyEntry{Key::F11, "F11"},
    KeyEntry{Key::F12, "F12"},
    KeyEntry{Key::Right, "Right"},
    KeyEntry{Key::Left, "Left"},
    KeyEntry{Key::Down, "Down"},
    KeyEntry{Key::Up, "Up"},
    KeyEntry{Key::LeftCtrl, "LeftCtrl"},
    KeyEntry{Key::LeftShift, "LeftShift"},
    KeyEntry{Key::LeftAlt, "LeftAlt"},
    KeyEntry{Key::RightCtrl, "RightCtrl"},
    KeyEntry{Key::RightShift, "RightShift"},
    KeyEntry{Key::RightAlt, "RightAlt"},
};

constexpr std::array<Key, kKeyEntries.size()> kKeys = [] {
    std::array<Key, kKeyEntries.size()> keys{};
    for (std::size_t index = 0; index < kKeyEntries.size(); ++index) {
        keys[index] = kKeyEntries[index].key;
    }
    return keys;
}();

} // namespace

std::span<const Key> allKeys() noexcept {
    return kKeys;
}

std::optional<Key> keyFromName(std::string_view name) noexcept {
    const auto entry = std::ranges::find(kKeyEntries, name, &KeyEntry::name);
    if (entry == kKeyEntries.end()) {
        return std::nullopt;
    }
    return entry->key;
}

std::string_view keyName(Key key) noexcept {
    const auto entry = std::ranges::find(kKeyEntries, key, &KeyEntry::key);
    if (entry == kKeyEntries.end()) {
        return "Unknown";
    }
    return entry->name;
}

} // namespace apex::platform
