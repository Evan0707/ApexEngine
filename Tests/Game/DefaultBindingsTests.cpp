#include "DefaultBindings.h"

#include "Input/ActionMap.h"
#include "Platform/Key.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

using apex::input::ActionKind;
using apex::input::ActionMap;
using apex::input::ResolvedButton;
using apex::platform::Key;

namespace {

Key buttonKey(const ActionMap& map, std::string_view action) {
    const auto id = map.find(action);
    REQUIRE(id.has_value());
    const auto buttons = map.buttons();
    const auto binding = std::ranges::find(buttons, *id, &ResolvedButton::action);
    REQUIRE(binding != buttons.end());
    return binding->key;
}

} // namespace

TEST_CASE("The default bindings build a valid action map", "[game]") {
    const auto map = ActionMap::create(apex::game::defaultBindings());
    REQUIRE(map.has_value());
    REQUIRE(map->actionCount() == 12);
}

TEST_CASE("The default bindings follow the AZERTY table of spec 5.2", "[game]") {
    const auto map = ActionMap::create(apex::game::defaultBindings());
    REQUIRE(map.has_value());

    // AZERTY Z, Q, A sit where QWERTY has W, A, Q.
    const auto forward = map->find("MoveForward");
    REQUIRE(forward.has_value());
    REQUIRE(map->kind(*forward) == ActionKind::Axis);
    REQUIRE(map->keyAxes()[0].action == *forward);
    REQUIRE(map->keyAxes()[0].negative == Key::S);
    REQUIRE(map->keyAxes()[0].positive == Key::W);
    REQUIRE(map->keyAxes()[1].negative == Key::A);
    REQUIRE(map->keyAxes()[1].positive == Key::D);
    REQUIRE(map->keyAxes()[2].negative == Key::Q);
    REQUIRE(map->keyAxes()[2].positive == Key::E);

    REQUIRE(buttonKey(*map, "Jump") == Key::Space);
    REQUIRE(buttonKey(*map, "Sprint") == Key::LeftShift);
    REQUIRE(buttonKey(*map, "ToggleCursor") == Key::Escape);
    REQUIRE(buttonKey(*map, "ToggleDebug") == Key::F1);
    REQUIRE(buttonKey(*map, "ToggleColliders") == Key::F2);
    REQUIRE(buttonKey(*map, "SwitchCamera") == Key::F3);
    REQUIRE(buttonKey(*map, "ToggleShadows") == Key::F4);

    REQUIRE(map->mouseAxes().size() == 2);
}
