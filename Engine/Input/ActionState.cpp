#include "Input/ActionState.h"

#include "Core/Assert.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace apex::input {

ActionState::ActionState(const ActionMap& map) {
    m_slots.reserve(map.actionCount());
    for (std::size_t i = 0; i < map.actionCount(); ++i) {
        m_slots.push_back({.kind = map.kind(ActionId{static_cast<std::uint16_t>(i)})});
    }
}

void ActionState::apply(const ActionMap& map, std::span<const platform::Event> events) noexcept {
    APEX_ASSERT(map.actionCount() == m_slots.size(), "ActionState was built from another ActionMap");
    for (const platform::Event& event : events) {
        if (const auto* key = std::get_if<platform::KeyChanged>(&event)) {
            if (!key->repeat) {
                changeKey(map, key->key, key->pressed);
            }
        } else if (const auto* motion = std::get_if<platform::MouseMoved>(&event)) {
            for (const ResolvedMouseAxis& binding : map.mouseAxes()) {
                const float delta = binding.axis == MouseAxis::X ? motion->deltaX : motion->deltaY;
                m_slots[binding.action.index].mouseDelta += delta * binding.scale;
            }
        } else if (std::holds_alternative<platform::FocusLost>(event)) {
            for (std::size_t i = 0; i < kKeyCount; ++i) {
                if (m_heldKeys[i]) {
                    changeKey(map, static_cast<platform::Key>(i), false);
                }
            }
        }
    }
}

void ActionState::changeKey(const ActionMap& map, platform::Key key, bool pressed) noexcept {
    const std::size_t index = std::to_underlying(key);
    APEX_ASSERT(index < kKeyCount, "Key value does not fit the held key table");
    if (m_heldKeys[index] == pressed) {
        return;
    }
    m_heldKeys[index] = pressed;
    const std::int32_t step = pressed ? 1 : -1;

    for (const ResolvedButton& binding : map.buttons()) {
        if (binding.key != key) {
            continue;
        }
        Slot& button = m_slots[binding.action.index];
        button.heldBindings += step;
        const bool held = button.heldBindings > 0;
        button.pressed = button.pressed || (held && !button.held);
        button.released = button.released || (!held && button.held);
        button.held = held;
    }
    for (const ResolvedKeyAxis& binding : map.keyAxes()) {
        Slot& keyAxis = m_slots[binding.action.index];
        if (binding.positive == key) {
            keyAxis.keyAxisSum += step;
        }
        if (binding.negative == key) {
            keyAxis.keyAxisSum -= step;
        }
    }
}

void ActionState::accumulate(const ActionState& frame) noexcept {
    APEX_ASSERT(frame.m_slots.size() == m_slots.size(), "ActionStates were built from different ActionMaps");
    m_heldKeys = frame.m_heldKeys;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        Slot& target = m_slots[i];
        const Slot& source = frame.m_slots[i];
        target.held = source.held;
        target.heldBindings = source.heldBindings;
        target.keyAxisSum = source.keyAxisSum;
        target.pressed = target.pressed || source.pressed;
        target.released = target.released || source.released;
        target.mouseDelta += source.mouseDelta;
    }
}

void ActionState::clearTransient() noexcept {
    for (Slot& entry : m_slots) {
        entry.pressed = false;
        entry.released = false;
        entry.mouseDelta = 0.0f;
    }
}

bool ActionState::isHeld(ActionId id) const noexcept {
    return slot(id, ActionKind::Button).held;
}

bool ActionState::wasPressed(ActionId id) const noexcept {
    return slot(id, ActionKind::Button).pressed;
}

bool ActionState::wasReleased(ActionId id) const noexcept {
    return slot(id, ActionKind::Button).released;
}

float ActionState::axis(ActionId id) const noexcept {
    const Slot& found = slot(id, ActionKind::Axis);
    return static_cast<float>(std::clamp(found.keyAxisSum, -1, 1)) + found.mouseDelta;
}

const ActionState::Slot& ActionState::slot(ActionId id, ActionKind expected) const noexcept {
    APEX_ASSERT(id.index < m_slots.size(), "ActionId does not belong to this ActionState");
    const Slot& found = m_slots[id.index];
    APEX_ASSERT(found.kind == expected, "Button query on an axis action, or axis query on a button action");
    return found;
}

} // namespace apex::input
