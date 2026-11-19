#include "Debug/InputCapture.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <variant>
#include <vector>

using apex::debug::filterCapturedEvents;
using apex::platform::CloseRequested;
using apex::platform::Event;
using apex::platform::Key;
using apex::platform::KeyChanged;
using apex::platform::MouseMoved;
using apex::platform::WindowResized;

namespace {

const std::array<Event, 5> kEvents{
    Event{KeyChanged{Key::W, true, false}},
    Event{KeyChanged{Key::S, false, false}},
    Event{MouseMoved{3.0f, -2.0f}},
    Event{WindowResized{800, 600}},
    Event{CloseRequested{}},
};

} // namespace

TEST_CASE("Nothing is removed when the debug interface captures nothing", "[debug]") {
    const std::vector<Event> kept = filterCapturedEvents(kEvents, false, false);
    REQUIRE(kept.size() == kEvents.size());
}

TEST_CASE("A captured keyboard removes key presses but keeps key releases", "[debug]") {
    const std::vector<Event> kept = filterCapturedEvents(kEvents, true, false);

    REQUIRE(kept.size() == 4);
    const auto* release = std::get_if<KeyChanged>(&kept[0]);
    REQUIRE(release != nullptr);
    REQUIRE(release->key == Key::S);
    REQUIRE_FALSE(release->pressed);
    REQUIRE(std::holds_alternative<MouseMoved>(kept[1]));
}

TEST_CASE("A captured mouse removes mouse motion only", "[debug]") {
    const std::vector<Event> kept = filterCapturedEvents(kEvents, false, true);

    REQUIRE(kept.size() == 4);
    for (const Event& event : kept) {
        REQUIRE_FALSE(std::holds_alternative<MouseMoved>(event));
    }
}

TEST_CASE("Window events are always kept", "[debug]") {
    const std::vector<Event> kept = filterCapturedEvents(kEvents, true, true);

    REQUIRE(kept.size() == 3);
    REQUIRE(std::holds_alternative<KeyChanged>(kept[0]));
    REQUIRE(std::holds_alternative<WindowResized>(kept[1]));
    REQUIRE(std::holds_alternative<CloseRequested>(kept[2]));
}
