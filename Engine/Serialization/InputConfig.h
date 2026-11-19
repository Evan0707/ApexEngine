#pragma once

#include "Core/Error.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace apex::serialization {

// Config/input.json (spec 5.2): the actions and what triggers them, as written in the file. Key names
// are checked by whoever builds the action map from this: Serialization knows neither Input nor Platform
// (decision 0017).

struct ButtonConfig {
    std::string action;
    // Name of a physical key ("Space", "LeftShift").
    std::string key;

    [[nodiscard]] bool operator==(const ButtonConfig&) const noexcept = default;
};

struct KeyAxisConfig {
    std::string action;
    std::string negativeKey;
    std::string positiveKey;

    [[nodiscard]] bool operator==(const KeyAxisConfig&) const noexcept = default;
};

enum class MouseAxisName { X, Y };

struct MouseAxisConfig {
    std::string action;
    MouseAxisName axis = MouseAxisName::X;
    // Multiplies the motion in pixels.
    float scale = 1.0f;

    [[nodiscard]] bool operator==(const MouseAxisConfig&) const noexcept = default;
};

struct InputConfig {
    std::vector<ButtonConfig> buttons;
    std::vector<KeyAxisConfig> keyAxes;
    std::vector<MouseAxisConfig> mouseAxes;

    [[nodiscard]] bool operator==(const InputConfig&) const noexcept = default;
};

// The table of spec 5.2, by physical key: the AZERTY keys Z, Q, A sit where QWERTY has W, A, Q. Used
// when Config/input.json is absent.
[[nodiscard]] InputConfig defaultInputConfig();

// An error names the file (`sourceName`) and the field ("buttons[2].key"). Unknown fields are errors.
[[nodiscard]] core::Result<InputConfig> parseInputConfig(std::string_view text, std::string_view sourceName);

// NotFound when the file does not exist: the caller then decides to use defaultInputConfig().
[[nodiscard]] core::Result<InputConfig> loadInputConfigFile(const std::filesystem::path& file,
                                                            std::string_view sourceName);

} // namespace apex::serialization
