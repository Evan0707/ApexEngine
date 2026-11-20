#include "Resources/FileReader.h"
#include "Serialization/EngineConfig.h"
#include "Serialization/InputConfig.h"
#include "Serialization/Json.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <initializer_list>
#include <string>
#include <vector>

namespace apex::serialization {

namespace {

// Larger than any window a display can show; guards against a typo with too many digits.
constexpr float kMaxWindowSize = 16384.0f;
// Shadow map sizes: below, shadows are blocks; above, the memory of a 4K texture for one light.
constexpr float kMinShadowMapSize = 256.0f;
constexpr float kMaxShadowMapSize = 8192.0f;
// The range of the panel's slider (decision 0021).
constexpr float kMaxExposureStops = 4.0f;

// Parses the text, checks that it is an object of the supported version.
[[nodiscard]] core::Result<Json> parseConfigRoot(std::string_view text, const JsonContext& context) {
    core::Result<Json> root = parseJson(text, context);
    if (!root) {
        return root;
    }
    if (core::Result<void> valid = requireObject(*root, context, {}); !valid) {
        return std::unexpected(valid.error());
    }
    const auto version = root->find("version");
    if (version == root->end()) {
        return jsonError(context, "version",
                         std::format("missing; this build reads version {}", kConfigVersion));
    }
    if (!version->is_number_integer()) {
        return jsonError(context, "version", "expected an integer");
    }
    if (const auto number = version->get<long long>(); number != kConfigVersion) {
        return jsonError(
            context, "version",
            std::format("version {} is not supported; this build reads version {}", number, kConfigVersion));
    }
    return root;
}

// The text of a file, with NotFound kept as is for the caller to fall back on defaults.
[[nodiscard]] core::Result<std::string> readConfigFile(const std::filesystem::path& file,
                                                       std::string_view sourceName) {
    const core::Result<std::vector<std::byte>> bytes = resources::readFile(file);
    if (!bytes) {
        return core::makeError(bytes.error().code,
                               std::format("Configuration '{}': {}", sourceName, bytes.error().message));
    }
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

[[nodiscard]] core::Result<std::uint32_t> readWindowSize(const Json& value, const JsonContext& context,
                                                         std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (!number) {
        return std::unexpected(number.error());
    }
    if (!value.is_number_integer() || *number < 1.0f || *number > kMaxWindowSize) {
        return jsonError(context, field,
                         std::format("expected a whole number of pixels between 1 and {}", kMaxWindowSize));
    }
    return static_cast<std::uint32_t>(*number);
}

[[nodiscard]] core::Result<bool> readBool(const Json& value, const JsonContext& context,
                                          std::string_view field) {
    if (!value.is_boolean()) {
        return jsonError(context, field, "expected true or false");
    }
    return value.get<bool>();
}

[[nodiscard]] core::Result<std::uint32_t> readShadowMapSize(const Json& value, const JsonContext& context,
                                                            std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (!number) {
        return std::unexpected(number.error());
    }
    if (!value.is_number_integer() || *number < kMinShadowMapSize || *number > kMaxShadowMapSize) {
        return jsonError(context, field,
                         std::format("expected a whole number of pixels between {} and {}", kMinShadowMapSize,
                                     kMaxShadowMapSize));
    }
    return static_cast<std::uint32_t>(*number);
}

[[nodiscard]] core::Result<float> readExposure(const Json& value, const JsonContext& context,
                                               std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && std::abs(*number) > kMaxExposureStops) {
        return jsonError(
            context, field,
            std::format("expected stops between {} and {}", -kMaxExposureStops, kMaxExposureStops));
    }
    return number;
}

[[nodiscard]] core::Result<float> readNotNegative(const Json& value, const JsonContext& context,
                                                  std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && *number < 0.0f) {
        return jsonError(context, field, "expected a number that is not negative");
    }
    return number;
}

// A slope a character can stand on: flat ground and walls excluded.
[[nodiscard]] core::Result<float> readSlopeDegrees(const Json& value, const JsonContext& context,
                                                   std::string_view field) {
    const core::Result<float> number = readNumber(value, context, field);
    if (number && (*number <= 0.0f || *number >= 90.0f)) {
        return jsonError(context, field, "expected an angle between 0 and 90 degrees, both excluded");
    }
    return number;
}

using NumberReader = core::Result<float> (*)(const Json&, const JsonContext&, std::string_view);

struct NumberField {
    std::string_view key;
    float& target;
    NumberReader read = readPositive;
};

// Reads the optional object `name` of the root, made of numbers only. A key that is not in `fields` is an
// error; a field left out keeps its value.
[[nodiscard]] core::Result<void> readNumberSection(const Json& root, std::string_view name,
                                                   const JsonContext& context,
                                                   std::initializer_list<NumberField> fields) {
    const auto section = root.find(name);
    if (section == root.end()) {
        return {};
    }
    if (core::Result<void> valid = requireObject(*section, context, name); !valid) {
        return valid;
    }
    for (const auto& [key, value] : section->items()) {
        const std::string path = joinField(name, key);
        const auto* const field = std::ranges::find(fields, std::string_view(key), &NumberField::key);
        if (field == fields.end()) {
            return jsonError(context, path, "unknown field");
        }
        const core::Result<float> number = field->read(value, context, path);
        if (!number) {
            return std::unexpected(number.error());
        }
        field->target = *number;
    }
    return {};
}

// The required string `key` of one binding.
[[nodiscard]] core::Result<void> readRequiredString(const Json& object, std::string_view key,
                                                    const JsonContext& context, std::string_view parent,
                                                    std::string& target) {
    if (!object.contains(key)) {
        return jsonError(context, joinField(parent, key), "missing");
    }
    return readOptional(object, key, context, parent, target, readString);
}

// Reads the optional array `key` of the root, one element at a time with `readElement(json, path)`.
template <typename Element, typename ReadElement>
[[nodiscard]] core::Result<void> readArray(const Json& root, std::string_view key, const JsonContext& context,
                                           std::vector<Element>& target, ReadElement&& readElement) {
    const auto found = root.find(key);
    if (found == root.end()) {
        return {};
    }
    if (!found->is_array()) {
        return jsonError(context, key, "expected an array");
    }
    for (std::size_t index = 0; index < found->size(); ++index) {
        const std::string path = std::format("{}[{}]", key, index);
        const Json& element = (*found)[index];
        if (core::Result<void> valid = requireObject(element, context, path); !valid) {
            return valid;
        }
        core::Result<Element> value = readElement(element, path);
        if (!value) {
            return std::unexpected(value.error());
        }
        target.push_back(std::move(*value));
    }
    return {};
}

} // namespace

core::Result<EngineConfig> parseEngineConfig(std::string_view text, std::string_view sourceName) {
    const JsonContext context{.kind = "Configuration", .source = sourceName, .entity = {}};
    const core::Result<Json> root = parseConfigRoot(text, context);
    if (!root) {
        return std::unexpected(root.error());
    }
    if (core::Result<void> known =
            rejectUnknownFields(*root,
                                {"version", "window", "free_camera", "physics", "character",
                                 "third_person_camera", "shadows", "rendering"},
                                context, {});
        !known) {
        return std::unexpected(known.error());
    }

    EngineConfig config;
    if (const auto window = root->find("window"); window != root->end()) {
        core::Result<void> read = requireObject(*window, context, "window");
        if (read) {
            read = rejectUnknownFields(*window, {"width", "height"}, context, "window");
        }
        if (read) {
            read = readOptional(*window, "width", context, "window", config.windowWidth, readWindowSize);
        }
        if (read) {
            read = readOptional(*window, "height", context, "window", config.windowHeight, readWindowSize);
        }
        if (!read) {
            return std::unexpected(read.error());
        }
    }
    if (core::Result<void> read = readNumberSection(*root, "free_camera", context,
                                                    {{"speed", config.freeCameraSpeed},
                                                     {"sprint_multiplier", config.freeCameraSprintMultiplier},
                                                     {"look_sensitivity", config.freeCameraLookSensitivity}});
        !read) {
        return std::unexpected(read.error());
    }
    if (core::Result<void> read =
            readNumberSection(*root, "character", context,
                              {{"height", config.characterHeight},
                               {"radius", config.characterRadius},
                               {"walk_speed", config.characterWalkSpeed},
                               {"run_speed", config.characterRunSpeed},
                               {"jump_height", config.characterJumpHeight},
                               {"max_slope_deg", config.characterMaxSlopeDegrees, readSlopeDegrees},
                               {"step_height", config.characterStepHeight, readNotNegative},
                               {"push_force", config.characterPushForce, readNotNegative}});
        !read) {
        return std::unexpected(read.error());
    }
    if (core::Result<void> read =
            readNumberSection(*root, "third_person_camera", context,
                              {{"distance", config.thirdPersonCameraDistance},
                               {"aim_height", config.thirdPersonCameraAimHeight, readNotNegative}});
        !read) {
        return std::unexpected(read.error());
    }
    if (config.characterHeight <= 2.0f * config.characterRadius) {
        return jsonError(context, "character.height", "the height must be more than twice the radius");
    }
    if (const auto physics = root->find("physics"); physics != root->end()) {
        core::Result<void> read = requireObject(*physics, context, "physics");
        if (read) {
            read = rejectUnknownFields(*physics, {"gravity"}, context, "physics");
        }
        if (read) {
            read = readOptional(*physics, "gravity", context, "physics", config.gravity, readVec3);
        }
        if (!read) {
            return std::unexpected(read.error());
        }
    }
    if (const auto shadows = root->find("shadows"); shadows != root->end()) {
        core::Result<void> read = requireObject(*shadows, context, "shadows");
        if (read) {
            read = rejectUnknownFields(
                *shadows, {"enabled", "map_size", "area", "constant_bias", "slope_bias"}, context, "shadows");
        }
        if (read) {
            read = readOptional(*shadows, "enabled", context, "shadows", config.shadowsEnabled, readBool);
        }
        if (read) {
            read = readOptional(*shadows, "map_size", context, "shadows", config.shadowMapSize,
                                readShadowMapSize);
        }
        if (read) {
            read = readOptional(*shadows, "area", context, "shadows", config.shadowAreaSize, readPositive);
        }
        if (read) {
            read = readOptional(*shadows, "constant_bias", context, "shadows", config.shadowConstantBias,
                                readNotNegative);
        }
        if (read) {
            read = readOptional(*shadows, "slope_bias", context, "shadows", config.shadowSlopeBias,
                                readNotNegative);
        }
        if (!read) {
            return std::unexpected(read.error());
        }
    }
    if (const auto rendering = root->find("rendering"); rendering != root->end()) {
        core::Result<void> read = requireObject(*rendering, context, "rendering");
        if (read) {
            read =
                rejectUnknownFields(*rendering, {"exposure_ev", "tonemapping", "ibl"}, context, "rendering");
        }
        if (read) {
            read = readOptional(*rendering, "exposure_ev", context, "rendering", config.exposureStops,
                                readExposure);
        }
        if (read) {
            read =
                readOptional(*rendering, "tonemapping", context, "rendering", config.toneMapping, readBool);
        }
        if (read) {
            read = readOptional(*rendering, "ibl", context, "rendering", config.imageBasedLighting, readBool);
        }
        if (!read) {
            return std::unexpected(read.error());
        }
    }
    return config;
}

core::Result<EngineConfig> loadEngineConfigFile(const std::filesystem::path& file,
                                                std::string_view sourceName) {
    const core::Result<std::string> text = readConfigFile(file, sourceName);
    if (!text) {
        return std::unexpected(text.error());
    }
    return parseEngineConfig(*text, sourceName);
}

InputConfig defaultInputConfig() {
    return InputConfig{
        .buttons =
            {
                {"Jump", "Space"},
                {"Sprint", "LeftShift"},
                {"ToggleCursor", "Escape"},
                {"ToggleDebug", "F1"},
                {"ToggleColliders", "F2"},
                {"SwitchCamera", "F3"},
                {"ToggleShadows", "F4"},
            },
        .keyAxes =
            {
                {"MoveForward", "S", "W"},
                {"MoveRight", "A", "D"},
                {"MoveUp", "Q", "E"},
            },
        .mouseAxes =
            {
                {"LookX", MouseAxisName::X, 1.0f},
                {"LookY", MouseAxisName::Y, 1.0f},
            },
    };
}

core::Result<InputConfig> parseInputConfig(std::string_view text, std::string_view sourceName) {
    const JsonContext context{.kind = "Configuration", .source = sourceName, .entity = {}};
    const core::Result<Json> root = parseConfigRoot(text, context);
    if (!root) {
        return std::unexpected(root.error());
    }
    if (core::Result<void> known =
            rejectUnknownFields(*root, {"version", "buttons", "key_axes", "mouse_axes"}, context, {});
        !known) {
        return std::unexpected(known.error());
    }

    InputConfig config;
    core::Result<void> read =
        readArray(*root, "buttons", context, config.buttons,
                  [&](const Json& element, std::string_view path) -> core::Result<ButtonConfig> {
                      ButtonConfig button;
                      core::Result<void> field =
                          rejectUnknownFields(element, {"action", "key"}, context, path);
                      if (field) {
                          field = readRequiredString(element, "action", context, path, button.action);
                      }
                      if (field) {
                          field = readRequiredString(element, "key", context, path, button.key);
                      }
                      if (!field) {
                          return std::unexpected(field.error());
                      }
                      return button;
                  });
    if (read) {
        read = readArray(
            *root, "key_axes", context, config.keyAxes,
            [&](const Json& element, std::string_view path) -> core::Result<KeyAxisConfig> {
                KeyAxisConfig axis;
                core::Result<void> field =
                    rejectUnknownFields(element, {"action", "negative", "positive"}, context, path);
                if (field) {
                    field = readRequiredString(element, "action", context, path, axis.action);
                }
                if (field) {
                    field = readRequiredString(element, "negative", context, path, axis.negativeKey);
                }
                if (field) {
                    field = readRequiredString(element, "positive", context, path, axis.positiveKey);
                }
                if (!field) {
                    return std::unexpected(field.error());
                }
                return axis;
            });
    }
    if (read) {
        read = readArray(
            *root, "mouse_axes", context, config.mouseAxes,
            [&](const Json& element, std::string_view path) -> core::Result<MouseAxisConfig> {
                MouseAxisConfig axis;
                std::string name;
                core::Result<void> field =
                    rejectUnknownFields(element, {"action", "axis", "scale"}, context, path);
                if (field) {
                    field = readRequiredString(element, "action", context, path, axis.action);
                }
                if (field) {
                    field = readRequiredString(element, "axis", context, path, name);
                }
                if (field) {
                    field = readOptional(element, "scale", context, path, axis.scale, readNumber);
                }
                if (!field) {
                    return std::unexpected(field.error());
                }
                if (name != "x" && name != "y") {
                    return jsonError(context, joinField(path, "axis"),
                                     std::format("unknown axis \"{}\"; expected \"x\" or \"y\"", name));
                }
                axis.axis = name == "x" ? MouseAxisName::X : MouseAxisName::Y;
                return axis;
            });
    }
    if (!read) {
        return std::unexpected(read.error());
    }
    return config;
}

core::Result<InputConfig> loadInputConfigFile(const std::filesystem::path& file,
                                              std::string_view sourceName) {
    const core::Result<std::string> text = readConfigFile(file, sourceName);
    if (!text) {
        return std::unexpected(text.error());
    }
    return parseInputConfig(*text, sourceName);
}

} // namespace apex::serialization
