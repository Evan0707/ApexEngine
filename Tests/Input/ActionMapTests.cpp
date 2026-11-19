#include "Input/ActionMap.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

using apex::core::ErrorCode;
using apex::input::ActionId;
using apex::input::ActionKind;
using apex::input::ActionMap;
using apex::input::InputBindings;
using apex::input::MouseAxis;
using apex::platform::Key;

namespace {

// Spec 5.2 defaults for an AZERTY keyboard, written as physical (US QWERTY) key names.
InputBindings defaultBindings() {
    return {
        .buttons = {{"Jump", "Space"},
                    {"Sprint", "LeftShift"},
                    {"ToggleCursor", "Escape"},
                    {"ToggleDebug", "F1"},
                    {"SwitchCamera", "F3"}},
        .keyAxes = {{"MoveForward", "S", "W"}, {"MoveRight", "A", "D"}, {"MoveUp", "Q", "E"}},
        .mouseAxes = {{"LookX", MouseAxis::X, 1.0f}, {"LookY", MouseAxis::Y, -1.0f}},
    };
}

ActionId requireAction(const ActionMap& map, std::string_view name) {
    const std::optional<ActionId> id = map.find(name);
    REQUIRE(id.has_value());
    return *id;
}

void requireInvalid(const InputBindings& bindings, std::string_view expectedInMessage) {
    const auto map = ActionMap::create(bindings);
    REQUIRE_FALSE(map.has_value());
    CHECK(map.error().code == ErrorCode::InvalidArgument);
    CAPTURE(map.error().message);
    CHECK(map.error().message.contains(expectedInMessage));
}

} // namespace

TEST_CASE("Default bindings resolve to actions and physical keys", "[input]") {
    const auto created = ActionMap::create(defaultBindings());
    REQUIRE(created.has_value());
    const ActionMap& map = *created;

    REQUIRE(map.actionCount() == 10);
    REQUIRE(map.buttons().size() == 5);
    REQUIRE(map.keyAxes().size() == 3);
    REQUIRE(map.mouseAxes().size() == 2);

    const ActionId jump = requireAction(map, "Jump");
    const ActionId forward = requireAction(map, "MoveForward");
    const ActionId lookY = requireAction(map, "LookY");
    CHECK(jump.index == 0);
    CHECK(requireAction(map, "SwitchCamera").index == 4);
    CHECK(forward.index == 5);
    CHECK(requireAction(map, "LookX").index == 8);
    CHECK(lookY.index == 9);

    CHECK(map.kind(jump) == ActionKind::Button);
    CHECK(map.kind(forward) == ActionKind::Axis);
    CHECK(map.kind(lookY) == ActionKind::Axis);
    CHECK(map.name(forward) == "MoveForward");

    CHECK(map.buttons()[0].action == jump);
    CHECK(map.buttons()[0].key == Key::Space);
    CHECK(map.buttons()[1].key == Key::LeftShift);
    CHECK(map.buttons()[3].key == Key::F1);
    CHECK(map.keyAxes()[0].action == forward);
    CHECK(map.keyAxes()[0].negative == Key::S);
    CHECK(map.keyAxes()[0].positive == Key::W);
    CHECK(map.keyAxes()[2].negative == Key::Q);
    CHECK(map.keyAxes()[2].positive == Key::E);
    CHECK(map.mouseAxes()[1].action == lookY);
    CHECK(map.mouseAxes()[1].axis == MouseAxis::Y);
    CHECK(map.mouseAxes()[1].scale == -1.0f);
}

TEST_CASE("Finding an unknown action gives no id", "[input]") {
    const auto map = ActionMap::create(defaultBindings());
    REQUIRE(map.has_value());
    CHECK_FALSE(map->find("Fly").has_value());
    CHECK_FALSE(map->find("jump").has_value());
    CHECK_FALSE(map->find("").has_value());
}

TEST_CASE("An unknown key name is rejected with the action and the key", "[input]") {
    requireInvalid({.buttons = {{"Jump", "Spacebar"}}}, "'Spacebar'");
    requireInvalid({.buttons = {{"Jump", "Spacebar"}}}, "'Jump'");
    requireInvalid({.buttons = {{"Jump", "space"}}}, "'space'");
    requireInvalid({.keyAxes = {{"MoveForward", "S", "Z "}}}, "'Z '");
    requireInvalid({.keyAxes = {{"MoveForward", "", "W"}}}, "'MoveForward'");
}

TEST_CASE("An empty action name is rejected", "[input]") {
    requireInvalid({.buttons = {{"", "Space"}}}, "empty action name");
    requireInvalid({.mouseAxes = {{"", MouseAxis::X}}}, "empty action name");
}

TEST_CASE("An action cannot be both a button and an axis", "[input]") {
    requireInvalid({.buttons = {{"Jump", "Space"}}, .keyAxes = {{"Jump", "S", "W"}}}, "'Jump'");
    requireInvalid({.buttons = {{"Look", "F5"}}, .mouseAxes = {{"Look", MouseAxis::X}}}, "'Look'");
}

TEST_CASE("Several keys can trigger one button", "[input]") {
    const auto map = ActionMap::create({.buttons = {{"Jump", "Space"}, {"Jump", "Return"}}});
    REQUIRE(map.has_value());
    REQUIRE(map->actionCount() == 1);
    REQUIRE(map->buttons().size() == 2);
    CHECK(map->buttons()[0].action == map->buttons()[1].action);
    CHECK(map->buttons()[1].key == Key::Return);
}

TEST_CASE("A key axis and a mouse axis can drive one action", "[input]") {
    const auto map = ActionMap::create(
        {.keyAxes = {{"Turn", "Left", "Right"}}, .mouseAxes = {{"Turn", MouseAxis::X, 0.5f}}});
    REQUIRE(map.has_value());
    REQUIRE(map->actionCount() == 1);
    CHECK(map->kind(map->keyAxes()[0].action) == ActionKind::Axis);
    CHECK(map->keyAxes()[0].action == map->mouseAxes()[0].action);
}

TEST_CASE("One key can trigger several actions", "[input]") {
    const auto map = ActionMap::create({.buttons = {{"Jump", "Space"}, {"Confirm", "Space"}}});
    REQUIRE(map.has_value());
    REQUIRE(map->actionCount() == 2);
    CHECK_FALSE(map->buttons()[0].action == map->buttons()[1].action);
}

TEST_CASE("Empty bindings give an empty map", "[input]") {
    const auto map = ActionMap::create({});
    REQUIRE(map.has_value());
    CHECK(map->actionCount() == 0);
    CHECK(map->buttons().empty());
    CHECK(map->keyAxes().empty());
    CHECK(map->mouseAxes().empty());
    CHECK_FALSE(map->find("Jump").has_value());
}

TEST_CASE("Action ids are limited to 16 bits", "[input]") {
    constexpr std::size_t kLimit = 65536;
    InputBindings bindings;
    for (std::size_t i = 0; i < kLimit; ++i) {
        bindings.mouseAxes.push_back({std::format("Action{}", i), MouseAxis::X});
    }
    const auto full = ActionMap::create(bindings);
    REQUIRE(full.has_value());
    CHECK(full->actionCount() == kLimit);
    CHECK(full->name(ActionId{65535}) == "Action65535");

    bindings.mouseAxes.push_back({"OneTooMany", MouseAxis::X});
    requireInvalid(bindings, "'OneTooMany'");
}
