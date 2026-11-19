#pragma once

#include "Core/Error.h"
#include "Platform/Key.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace apex::input {

enum class MouseAxis { X, Y };

// Key names are platform::Key enumerator names ("W", "Space", "LeftShift").
struct ButtonBinding {
    std::string action;
    std::string key;
};

struct KeyAxisBinding {
    std::string action;
    std::string negativeKey;
    std::string positiveKey;
};

struct MouseAxisBinding {
    std::string action;
    MouseAxis axis;
    float scale = 1.0f;
};

struct InputBindings {
    std::vector<ButtonBinding> buttons;
    std::vector<KeyAxisBinding> keyAxes;
    std::vector<MouseAxisBinding> mouseAxes;
};

struct ActionId {
    std::uint16_t index;

    friend constexpr bool operator==(ActionId, ActionId) = default;
};

enum class ActionKind { Button, Axis };

struct ResolvedButton {
    ActionId action;
    platform::Key key;
};

struct ResolvedKeyAxis {
    ActionId action;
    platform::Key negative;
    platform::Key positive;
};

struct ResolvedMouseAxis {
    ActionId action;
    MouseAxis axis;
    float scale;
};

// Ids are dense indices in order of first appearance: buttons, then key axes, then mouse axes.
class ActionMap {
public:
    [[nodiscard]] static core::Result<ActionMap> create(const InputBindings& bindings);

    [[nodiscard]] std::optional<ActionId> find(std::string_view name) const noexcept;
    [[nodiscard]] std::string_view name(ActionId id) const noexcept;
    [[nodiscard]] ActionKind kind(ActionId id) const noexcept;
    [[nodiscard]] std::size_t actionCount() const noexcept { return m_names.size(); }

    [[nodiscard]] std::span<const ResolvedButton> buttons() const noexcept { return m_buttons; }
    [[nodiscard]] std::span<const ResolvedKeyAxis> keyAxes() const noexcept { return m_keyAxes; }
    [[nodiscard]] std::span<const ResolvedMouseAxis> mouseAxes() const noexcept { return m_mouseAxes; }

private:
    ActionMap() = default;

    std::vector<std::string> m_names;
    std::vector<ActionKind> m_kinds;
    std::vector<ResolvedButton> m_buttons;
    std::vector<ResolvedKeyAxis> m_keyAxes;
    std::vector<ResolvedMouseAxis> m_mouseAxes;
};

} // namespace apex::input
