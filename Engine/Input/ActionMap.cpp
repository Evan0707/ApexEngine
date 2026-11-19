#include "Input/ActionMap.h"

#include "Core/Assert.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <unordered_map>

namespace apex::input {

namespace {

constexpr std::size_t kMaxActions = std::size_t{std::numeric_limits<std::uint16_t>::max()} + 1;

[[nodiscard]] core::Result<platform::Key> resolveKey(std::string_view action, std::string_view key) {
    if (const std::optional<platform::Key> resolved = platform::keyFromName(key)) {
        return *resolved;
    }
    return core::makeError(core::ErrorCode::InvalidArgument,
                           std::format("Input binding: unknown key name '{}' for action '{}'", key, action));
}

} // namespace

core::Result<ActionMap> ActionMap::create(const InputBindings& bindings) {
    ActionMap map;
    // Views into `bindings`, which outlives this function body.
    std::unordered_map<std::string_view, std::uint16_t> indices;

    const auto resolveAction = [&](std::string_view action, ActionKind kind, std::string_view bindingType,
                                   std::size_t bindingIndex) -> core::Result<ActionId> {
        if (action.empty()) {
            return core::makeError(core::ErrorCode::InvalidArgument,
                                   std::format("Input binding: {} binding {} has an empty action name",
                                               bindingType, bindingIndex));
        }
        if (const auto found = indices.find(action); found != indices.end()) {
            if (map.m_kinds[found->second] != kind) {
                return core::makeError(
                    core::ErrorCode::InvalidArgument,
                    std::format("Input binding: action '{}' is bound both as a button and as an axis",
                                action));
            }
            return ActionId{found->second};
        }
        if (map.m_names.size() >= kMaxActions) {
            return core::makeError(core::ErrorCode::InvalidArgument,
                                   std::format("Input binding: action '{}' exceeds the limit of {} actions",
                                               action, kMaxActions));
        }
        const auto index = static_cast<std::uint16_t>(map.m_names.size());
        indices.emplace(action, index);
        map.m_names.emplace_back(action);
        map.m_kinds.push_back(kind);
        return ActionId{index};
    };

    for (std::size_t i = 0; i < bindings.buttons.size(); ++i) {
        const ButtonBinding& binding = bindings.buttons[i];
        const core::Result<ActionId> action = resolveAction(binding.action, ActionKind::Button, "button", i);
        if (!action) {
            return std::unexpected(action.error());
        }
        const core::Result<platform::Key> key = resolveKey(binding.action, binding.key);
        if (!key) {
            return std::unexpected(key.error());
        }
        map.m_buttons.push_back({*action, *key});
    }

    for (std::size_t i = 0; i < bindings.keyAxes.size(); ++i) {
        const KeyAxisBinding& binding = bindings.keyAxes[i];
        const core::Result<ActionId> action = resolveAction(binding.action, ActionKind::Axis, "key axis", i);
        if (!action) {
            return std::unexpected(action.error());
        }
        const core::Result<platform::Key> negative = resolveKey(binding.action, binding.negativeKey);
        if (!negative) {
            return std::unexpected(negative.error());
        }
        const core::Result<platform::Key> positive = resolveKey(binding.action, binding.positiveKey);
        if (!positive) {
            return std::unexpected(positive.error());
        }
        map.m_keyAxes.push_back({*action, *negative, *positive});
    }

    for (std::size_t i = 0; i < bindings.mouseAxes.size(); ++i) {
        const MouseAxisBinding& binding = bindings.mouseAxes[i];
        const core::Result<ActionId> action =
            resolveAction(binding.action, ActionKind::Axis, "mouse axis", i);
        if (!action) {
            return std::unexpected(action.error());
        }
        if (!std::isfinite(binding.scale)) {
            return core::makeError(
                core::ErrorCode::InvalidArgument,
                std::format("Input binding: mouse axis scale {} for action '{}' is not finite", binding.scale,
                            binding.action));
        }
        map.m_mouseAxes.push_back({*action, binding.axis, binding.scale});
    }

    return map;
}

std::optional<ActionId> ActionMap::find(std::string_view name) const noexcept {
    const auto found = std::ranges::find(m_names, name);
    if (found == m_names.end()) {
        return std::nullopt;
    }
    return ActionId{static_cast<std::uint16_t>(found - m_names.begin())};
}

std::string_view ActionMap::name(ActionId id) const noexcept {
    APEX_ASSERT(id.index < m_names.size(), "ActionId does not belong to this ActionMap");
    return m_names[id.index];
}

ActionKind ActionMap::kind(ActionId id) const noexcept {
    APEX_ASSERT(id.index < m_kinds.size(), "ActionId does not belong to this ActionMap");
    return m_kinds[id.index];
}

} // namespace apex::input
