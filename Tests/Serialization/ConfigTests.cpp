#include "Serialization/EngineConfig.h"
#include "Serialization/InputConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

using namespace apex::serialization;
using Catch::Matchers::ContainsSubstring;

namespace {

constexpr std::string_view kEngine = "Config/engine.json";
constexpr std::string_view kInput = "Config/input.json";

template <typename Config>
void requireError(const apex::core::Result<Config>& config, std::string_view source,
                  std::initializer_list<std::string_view> pieces) {
    REQUIRE_FALSE(config.has_value());
    CAPTURE(config.error().message);
    REQUIRE_THAT(config.error().message, ContainsSubstring(std::string(source)));
    for (const std::string_view piece : pieces) {
        REQUIRE_THAT(config.error().message, ContainsSubstring(std::string(piece)));
    }
}

} // namespace

TEST_CASE("The engine configuration reads the window and the free camera", "[serialization]") {
    const auto config = parseEngineConfig(R"({
        "version": 1,
        "window": { "width": 1280, "height": 720 },
        "free_camera": { "speed": 8.5, "sprint_multiplier": 2, "look_sensitivity": 0.001 },
        "physics": { "gravity": [0, -1.62, 0] },
        "character": { "height": 1.6, "radius": 0.25, "walk_speed": 3, "run_speed": 6, "jump_height": 1,
                       "max_slope_deg": 40, "step_height": 0, "push_force": 250 },
        "third_person_camera": { "distance": 6, "aim_height": 1.2 },
        "shadows": { "enabled": false, "map_size": 4096, "area": 60, "constant_bias": 0, "slope_bias": 1.5 },
        "rendering": { "exposure_ev": -1.5, "tonemapping": false, "ibl": false }
    })",
                                          kEngine);
    REQUIRE(config.has_value());
    REQUIRE(config->windowWidth == 1280);
    REQUIRE(config->windowHeight == 720);
    REQUIRE(config->freeCameraSpeed == 8.5f);
    REQUIRE(config->freeCameraSprintMultiplier == 2.0f);
    REQUIRE(config->freeCameraLookSensitivity == 0.001f);
    REQUIRE(config->gravity == glm::vec3(0.0f, -1.62f, 0.0f));
    REQUIRE(config->characterHeight == 1.6f);
    REQUIRE(config->characterRadius == 0.25f);
    REQUIRE(config->characterWalkSpeed == 3.0f);
    REQUIRE(config->characterRunSpeed == 6.0f);
    REQUIRE(config->characterJumpHeight == 1.0f);
    REQUIRE(config->characterMaxSlopeDegrees == 40.0f);
    REQUIRE(config->characterStepHeight == 0.0f);
    REQUIRE(config->characterPushForce == 250.0f);
    REQUIRE(config->thirdPersonCameraDistance == 6.0f);
    REQUIRE(config->thirdPersonCameraAimHeight == 1.2f);
    REQUIRE_FALSE(config->shadowsEnabled);
    REQUIRE(config->shadowMapSize == 4096);
    REQUIRE(config->shadowAreaSize == 60.0f);
    REQUIRE(config->shadowConstantBias == 0.0f);
    REQUIRE(config->shadowSlopeBias == 1.5f);
    REQUIRE(config->exposureStops == -1.5f);
    REQUIRE_FALSE(config->toneMapping);
    REQUIRE_FALSE(config->imageBasedLighting);
}

TEST_CASE("Fields left out of the engine configuration keep the specification's values", "[serialization]") {
    const auto config = parseEngineConfig(R"({"version": 1, "window": {"width": 800}})", kEngine);
    REQUIRE(config.has_value());
    REQUIRE(config->windowWidth == 800);
    REQUIRE(config->windowHeight == 1080);
    REQUIRE(config->freeCameraSpeed == 5.0f);
    REQUIRE(config->freeCameraSprintMultiplier == 4.0f);
    REQUIRE(config->gravity == glm::vec3(0.0f, -9.81f, 0.0f));
    REQUIRE(*parseEngineConfig(R"({"version": 1})", kEngine) == EngineConfig{});
}

TEST_CASE("An invalid engine configuration names the file and the field", "[serialization]") {
    requireError(parseEngineConfig(R"({"window": {}})", kEngine), kEngine, {"field 'version'"});
    requireError(parseEngineConfig(R"({"version": 3})", kEngine), kEngine, {"version 3 is not supported"});
    requireError(parseEngineConfig(R"({"version": 1, "window": {"width": 0}})", kEngine), kEngine,
                 {"field 'window.width'"});
    requireError(parseEngineConfig(R"({"version": 1, "window": {"height": 720.5}})", kEngine), kEngine,
                 {"field 'window.height'", "whole number"});
    requireError(parseEngineConfig(R"({"version": 1, "window": {"widht": 1280}})", kEngine), kEngine,
                 {"field 'window.widht'", "unknown field"});
    requireError(parseEngineConfig(R"({"version": 1, "free_camera": {"speed": -1}})", kEngine), kEngine,
                 {"field 'free_camera.speed'", "greater than 0"});
    requireError(parseEngineConfig(R"({"version": 1, "physics": {"gravity": [0, -9.81]}})", kEngine), kEngine,
                 {"field 'physics.gravity'"});
    requireError(parseEngineConfig(R"({"version": 1, "physics": {"gravty": [0, -9.81, 0]}})", kEngine),
                 kEngine, {"field 'physics.gravty'", "unknown field"});
    requireError(parseEngineConfig(R"({"version": 1, "character": {"walk_speed": 0}})", kEngine), kEngine,
                 {"field 'character.walk_speed'", "greater than 0"});
    requireError(parseEngineConfig(R"({"version": 1, "character": {"max_slope_deg": 90}})", kEngine), kEngine,
                 {"field 'character.max_slope_deg'", "between 0 and 90"});
    requireError(parseEngineConfig(R"({"version": 1, "character": {"step_height": -0.1}})", kEngine), kEngine,
                 {"field 'character.step_height'", "not negative"});
    requireError(parseEngineConfig(R"({"version": 1, "character": {"radius": 1}})", kEngine), kEngine,
                 {"field 'character.height'", "twice the radius"});
    requireError(parseEngineConfig(R"({"version": 1, "character": {"jump": 2}})", kEngine), kEngine,
                 {"field 'character.jump'", "unknown field"});
    requireError(parseEngineConfig(R"({"version": 1, "character": 3})", kEngine), kEngine,
                 {"field 'character'"});
    requireError(parseEngineConfig(R"({"version": 1, "third_person_camera": {"distance": 0}})", kEngine),
                 kEngine, {"field 'third_person_camera.distance'", "greater than 0"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"enabled": 1}})", kEngine), kEngine,
                 {"field 'shadows.enabled'", "true or false"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"map_size": 2048.5}})", kEngine), kEngine,
                 {"field 'shadows.map_size'", "whole number"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"map_size": 128}})", kEngine), kEngine,
                 {"field 'shadows.map_size'", "between 256 and 8192"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"area": 0}})", kEngine), kEngine,
                 {"field 'shadows.area'", "greater than 0"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"slope_bias": -1}})", kEngine), kEngine,
                 {"field 'shadows.slope_bias'", "not negative"});
    requireError(parseEngineConfig(R"({"version": 1, "shadows": {"pcf": 3}})", kEngine), kEngine,
                 {"field 'shadows.pcf'", "unknown field"});
    requireError(parseEngineConfig(R"({"version": 1, "rendering": {"exposure_ev": 5}})", kEngine), kEngine,
                 {"field 'rendering.exposure_ev'", "between -4 and 4"});
    requireError(parseEngineConfig(R"({"version": 1, "rendering": {"tonemapping": "aces"}})", kEngine),
                 kEngine, {"field 'rendering.tonemapping'", "true or false"});
    requireError(parseEngineConfig(R"({"version": 1, "vsync": true})", kEngine), kEngine, {"field 'vsync'"});
    requireError(parseEngineConfig("{", kEngine), kEngine, {"invalid JSON"});
}

TEST_CASE("The input configuration reads buttons, key axes and mouse axes", "[serialization]") {
    const auto config = parseInputConfig(R"({
        "version": 1,
        "buttons": [ {"action": "Jump", "key": "Space"} ],
        "key_axes": [ {"action": "MoveForward", "negative": "S", "positive": "W"} ],
        "mouse_axes": [ {"action": "LookY", "axis": "y", "scale": -0.5}, {"action": "LookX", "axis": "x"} ]
    })",
                                         kInput);
    REQUIRE(config.has_value());
    REQUIRE(config->buttons == std::vector<ButtonConfig>{{"Jump", "Space"}});
    REQUIRE(config->keyAxes == std::vector<KeyAxisConfig>{{"MoveForward", "S", "W"}});
    REQUIRE(config->mouseAxes.size() == 2);
    REQUIRE(config->mouseAxes[0].axis == MouseAxisName::Y);
    REQUIRE(config->mouseAxes[0].scale == -0.5f);
    REQUIRE(config->mouseAxes[1].scale == 1.0f);
    REQUIRE(*parseInputConfig(R"({"version": 1})", kInput) == InputConfig{});
}

TEST_CASE("An invalid input configuration names the file and the binding", "[serialization]") {
    requireError(parseInputConfig(R"({"version": 1, "buttons": [{"action": "Jump"}]})", kInput), kInput,
                 {"field 'buttons[0].key'", "missing"});
    requireError(
        parseInputConfig(
            R"({"version": 1, "buttons": [{"action": "A", "key": "B"}, {"action": "", "key": "C"}]})",
            kInput),
        kInput, {"field 'buttons[1].action'", "empty"});
    requireError(parseInputConfig(R"({"version": 1, "buttons": {"Jump": "Space"}})", kInput), kInput,
                 {"field 'buttons'", "expected an array"});
    requireError(
        parseInputConfig(R"({"version": 1, "key_axes": [{"action": "M", "negative": "S", "postive": "W"}]})",
                         kInput),
        kInput, {"field 'key_axes[0].postive'", "unknown field"});
    requireError(
        parseInputConfig(R"({"version": 1, "mouse_axes": [{"action": "Look", "axis": "z"}]})", kInput),
        kInput, {"field 'mouse_axes[0].axis'", "\"z\""});
    requireError(parseInputConfig(R"({"version": 1, "mouse_axes": [5]})", kInput), kInput,
                 {"field 'mouse_axes[0]'", "expected an object"});
}

TEST_CASE("The default input configuration is the table of spec 5.2 by physical key", "[serialization]") {
    const InputConfig config = defaultInputConfig();
    REQUIRE(config.buttons.size() == 7);
    REQUIRE(config.buttons[0] == ButtonConfig{"Jump", "Space"});
    // AZERTY Z, Q, A sit where QWERTY has W, A, Q.
    REQUIRE(config.keyAxes == std::vector<KeyAxisConfig>{
                                  {"MoveForward", "S", "W"}, {"MoveRight", "A", "D"}, {"MoveUp", "Q", "E"}});
    REQUIRE(config.mouseAxes.size() == 2);
}

TEST_CASE("The configuration files of the repository are valid and equal to the defaults",
          "[serialization]") {
    const std::filesystem::path folder(APEX_CONFIG_DIR);
    const auto engine = loadEngineConfigFile(folder / "engine.json", kEngine);
    if (!engine) {
        FAIL(engine.error().message);
    }
    REQUIRE(*engine == EngineConfig{});
    const auto input = loadInputConfigFile(folder / "input.json", kInput);
    if (!input) {
        FAIL(input.error().message);
    }
    REQUIRE(*input == defaultInputConfig());
}

TEST_CASE("A missing configuration file is reported as not found", "[serialization]") {
    const std::filesystem::path folder(APEX_CONFIG_DIR);
    const auto engine = loadEngineConfigFile(folder / "absent.json", "Config/absent.json");
    REQUIRE_FALSE(engine.has_value());
    REQUIRE(engine.error().code == apex::core::ErrorCode::NotFound);
    REQUIRE(loadInputConfigFile(folder / "absent.json", "Config/absent.json").error().code ==
            apex::core::ErrorCode::NotFound);
}
