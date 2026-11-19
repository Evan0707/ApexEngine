#pragma once

#include "Input/ActionMap.h"
#include "Platform/Event.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace apex::input {

// One instance is fed by apply() each frame, another by accumulate() for the fixed steps (README).
class ActionState {
public:
    explicit ActionState(const ActionMap& map);

    // `map` must be the map this state was built from.
    void apply(const ActionMap& map, std::span<const platform::Event> events) noexcept;
    void accumulate(const ActionState& frame) noexcept;
    // Held keys and key axes are kept.
    void clearTransient() noexcept;

    [[nodiscard]] bool isHeld(ActionId id) const noexcept;
    [[nodiscard]] bool wasPressed(ActionId id) const noexcept;
    [[nodiscard]] bool wasReleased(ActionId id) const noexcept;
    // Key axis clamped to [-1, 1], plus the scaled mouse motion since the last clearTransient().
    [[nodiscard]] float axis(ActionId id) const noexcept;

private:
    struct Slot {
        ActionKind kind;
        bool held = false;
        bool pressed = false;
        bool released = false;
        std::int32_t heldBindings = 0; // Button: bindings whose key is held.
        std::int32_t keyAxisSum = 0;   // Axis: +1 per held positive key, -1 per held negative key.
        float mouseDelta = 0.0f;
    };

    static constexpr std::size_t kKeyCount = 256;

    void changeKey(const ActionMap& map, platform::Key key, bool pressed) noexcept;
    [[nodiscard]] const Slot& slot(ActionId id, ActionKind expected) const noexcept;

    std::vector<Slot> m_slots;
    std::array<bool, kKeyCount> m_heldKeys{};
};

} // namespace apex::input
