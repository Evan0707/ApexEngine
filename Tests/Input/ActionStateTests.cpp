#include "Input/ActionState.h"

#include "Input/ActionMap.h"
#include "Platform/Event.h"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

using apex::input::ActionId;
using apex::input::ActionMap;
using apex::input::ActionState;
using apex::input::InputBindings;
using apex::input::MouseAxis;
using apex::platform::Event;
using apex::platform::Key;

namespace {

ActionMap makeMap(const InputBindings& bindings) {
    auto map = ActionMap::create(bindings);
    REQUIRE(map.has_value());
    return std::move(*map);
}

ActionId requireAction(const ActionMap& map, std::string_view name) {
    const std::optional<ActionId> id = map.find(name);
    REQUIRE(id.has_value());
    return *id;
}

Event press(Key key) {
    return apex::platform::KeyChanged{key, true, false};
}

Event release(Key key) {
    return apex::platform::KeyChanged{key, false, false};
}

Event repeat(Key key) {
    return apex::platform::KeyChanged{key, true, true};
}

Event move(float deltaX, float deltaY) {
    return apex::platform::MouseMoved{deltaX, deltaY};
}

void feed(ActionState& state, const ActionMap& map, std::initializer_list<Event> events) {
    state.apply(map, std::span<const Event>(events.begin(), events.size()));
}

} // namespace

TEST_CASE("A new state has every button released and every axis at zero", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}, .keyAxes = {{"Move", "S", "W"}}});
    const ActionState state(map);
    const ActionId jump = requireAction(map, "Jump");
    CHECK_FALSE(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));
    CHECK_FALSE(state.wasReleased(jump));
    CHECK(state.axis(requireAction(map, "Move")) == 0.0f);
}

TEST_CASE("Pressing, holding and releasing a button", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState state(map);

    feed(state, map, {press(Key::Space)});
    CHECK(state.isHeld(jump));
    CHECK(state.wasPressed(jump));
    CHECK_FALSE(state.wasReleased(jump));

    state.clearTransient();
    CHECK(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));

    feed(state, map, {press(Key::Space), repeat(Key::Space)});
    CHECK(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));

    feed(state, map, {release(Key::Space)});
    CHECK_FALSE(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));
    CHECK(state.wasReleased(jump));

    state.clearTransient();
    feed(state, map, {release(Key::Space)});
    CHECK_FALSE(state.wasReleased(jump));
}

TEST_CASE("Key repeats are ignored", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState state(map);

    feed(state, map, {repeat(Key::Space)});
    CHECK_FALSE(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));
}

TEST_CASE("A tap within one frame sets both edges and leaves the button released", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState state(map);

    feed(state, map, {press(Key::Space), release(Key::Space)});
    CHECK_FALSE(state.isHeld(jump));
    CHECK(state.wasPressed(jump));
    CHECK(state.wasReleased(jump));
}

TEST_CASE("A button stays held while any of its keys is held", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}, {"Jump", "Return"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState state(map);

    feed(state, map, {press(Key::Space)});
    state.clearTransient();
    feed(state, map, {press(Key::Return)});
    CHECK(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));

    feed(state, map, {release(Key::Space)});
    CHECK(state.isHeld(jump));
    CHECK_FALSE(state.wasReleased(jump));

    feed(state, map, {release(Key::Return)});
    CHECK_FALSE(state.isHeld(jump));
    CHECK(state.wasReleased(jump));
}

TEST_CASE("A key axis gives +1, -1, or 0 when both keys are held", "[input]") {
    const ActionMap map = makeMap({.keyAxes = {{"Move", "S", "W"}}});
    const ActionId moveAxis = requireAction(map, "Move");
    ActionState state(map);

    feed(state, map, {press(Key::W)});
    CHECK(state.axis(moveAxis) == 1.0f);
    feed(state, map, {press(Key::S)});
    CHECK(state.axis(moveAxis) == 0.0f);
    feed(state, map, {release(Key::W)});
    CHECK(state.axis(moveAxis) == -1.0f);
    state.clearTransient();
    CHECK(state.axis(moveAxis) == -1.0f);
    feed(state, map, {release(Key::S)});
    CHECK(state.axis(moveAxis) == 0.0f);
}

TEST_CASE("Key axis bindings of one action are summed then clamped", "[input]") {
    const ActionMap map = makeMap({.keyAxes = {{"Move", "S", "W"}, {"Move", "Down", "Up"}}});
    const ActionId moveAxis = requireAction(map, "Move");
    ActionState state(map);

    feed(state, map, {press(Key::W), press(Key::Up)});
    CHECK(state.axis(moveAxis) == 1.0f);
    feed(state, map, {release(Key::Up), press(Key::Down)});
    CHECK(state.axis(moveAxis) == 0.0f);
    feed(state, map, {release(Key::W), press(Key::S)});
    CHECK(state.axis(moveAxis) == -1.0f);
}

TEST_CASE("Mouse motion is scaled, summed, and cleared by clearTransient", "[input]") {
    const ActionMap map =
        makeMap({.mouseAxes = {{"LookX", MouseAxis::X, 0.5f}, {"LookY", MouseAxis::Y, -1.0f}}});
    const ActionId lookX = requireAction(map, "LookX");
    const ActionId lookY = requireAction(map, "LookY");
    ActionState state(map);

    feed(state, map, {move(4.0f, 2.0f), move(6.0f, -5.0f)});
    CHECK(state.axis(lookX) == 5.0f);
    CHECK(state.axis(lookY) == 3.0f);

    state.clearTransient();
    CHECK(state.axis(lookX) == 0.0f);
    CHECK(state.axis(lookY) == 0.0f);
}

TEST_CASE("A key axis and a mouse axis on one action add up", "[input]") {
    const ActionMap map =
        makeMap({.keyAxes = {{"Turn", "Left", "Right"}}, .mouseAxes = {{"Turn", MouseAxis::X, 0.25f}}});
    const ActionId turn = requireAction(map, "Turn");
    ActionState state(map);

    feed(state, map, {press(Key::Right), move(8.0f, 0.0f)});
    CHECK(state.axis(turn) == 3.0f);
    state.clearTransient();
    CHECK(state.axis(turn) == 1.0f);
}

TEST_CASE("Losing the focus releases every held key", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}, {"Sprint", "LeftShift"}, {"Crouch", "C"}},
                                   .keyAxes = {{"Move", "S", "W"}}});
    const ActionId jump = requireAction(map, "Jump");
    const ActionId sprint = requireAction(map, "Sprint");
    const ActionId crouch = requireAction(map, "Crouch");
    const ActionId moveAxis = requireAction(map, "Move");
    ActionState state(map);

    feed(state, map, {press(Key::Space), press(Key::LeftShift), press(Key::W)});
    state.clearTransient();
    feed(state, map, {apex::platform::FocusLost{}});
    CHECK_FALSE(state.isHeld(jump));
    CHECK(state.wasReleased(jump));
    CHECK_FALSE(state.isHeld(sprint));
    CHECK(state.wasReleased(sprint));
    CHECK_FALSE(state.wasReleased(crouch));
    CHECK(state.axis(moveAxis) == 0.0f);

    feed(state, map, {press(Key::Space)});
    CHECK(state.isHeld(jump));
}

TEST_CASE("Unbound keys and window events change no action", "[input]") {
    const ActionMap map =
        makeMap({.buttons = {{"Jump", "Space"}}, .mouseAxes = {{"LookX", MouseAxis::X, 1.0f}}});
    const ActionId jump = requireAction(map, "Jump");
    const ActionId lookX = requireAction(map, "LookX");
    ActionState state(map);

    feed(state, map,
         {press(Key::F12), release(Key::F12), press(Key::Unknown), apex::platform::CloseRequested{},
          apex::platform::WindowResized{800, 600}, apex::platform::WindowMinimized{},
          apex::platform::WindowRestored{}});
    CHECK_FALSE(state.isHeld(jump));
    CHECK_FALSE(state.wasPressed(jump));
    CHECK_FALSE(state.wasReleased(jump));
    CHECK(state.axis(lookX) == 0.0f);
}

TEST_CASE("A press during a frame without steps reaches the next frame's first step", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState frame(map);
    ActionState simulation(map);

    // Frame 1: the key goes down, then zero steps.
    feed(frame, map, {press(Key::Space)});
    simulation.accumulate(frame);
    frame.clearTransient();

    // Frame 2: no event, one step.
    feed(frame, map, {});
    simulation.accumulate(frame);
    frame.clearTransient();
    CHECK(simulation.isHeld(jump));
    CHECK(simulation.wasPressed(jump));
    simulation.clearTransient();

    // Frame 3: one step, the key is still held.
    simulation.accumulate(frame);
    CHECK(simulation.isHeld(jump));
    CHECK_FALSE(simulation.wasPressed(jump));
}

TEST_CASE("A tap during a frame without steps is not lost", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState frame(map);
    ActionState simulation(map);

    feed(frame, map, {press(Key::Space)});
    simulation.accumulate(frame);
    frame.clearTransient();
    feed(frame, map, {release(Key::Space)});
    simulation.accumulate(frame);
    frame.clearTransient();

    CHECK_FALSE(simulation.isHeld(jump));
    CHECK(simulation.wasPressed(jump));
    CHECK(simulation.wasReleased(jump));
}

TEST_CASE("Only the first of several steps in a frame sees an edge", "[input]") {
    const ActionMap map = makeMap({.buttons = {{"Jump", "Space"}}});
    const ActionId jump = requireAction(map, "Jump");
    ActionState frame(map);
    ActionState simulation(map);

    feed(frame, map, {press(Key::Space)});
    simulation.accumulate(frame);
    frame.clearTransient();

    CHECK(simulation.wasPressed(jump));
    simulation.clearTransient();
    CHECK(simulation.isHeld(jump));
    CHECK_FALSE(simulation.wasPressed(jump));
}

TEST_CASE("Mouse motion over frames without steps reaches the next step once", "[input]") {
    const ActionMap map = makeMap({.mouseAxes = {{"LookX", MouseAxis::X, 2.0f}}});
    const ActionId lookX = requireAction(map, "LookX");
    ActionState frame(map);
    ActionState simulation(map);

    feed(frame, map, {move(1.0f, 0.0f)});
    simulation.accumulate(frame);
    frame.clearTransient();
    feed(frame, map, {move(3.0f, 0.0f), move(1.0f, 0.0f)});
    simulation.accumulate(frame);
    frame.clearTransient();

    CHECK(simulation.axis(lookX) == 10.0f);
    simulation.clearTransient();
    CHECK(simulation.axis(lookX) == 0.0f);

    // Next frame without motion: nothing is counted twice.
    feed(frame, map, {});
    simulation.accumulate(frame);
    CHECK(simulation.axis(lookX) == 0.0f);
}
