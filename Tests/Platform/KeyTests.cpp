#include "Platform/Event.h"
#include "Platform/Key.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>
#include <string_view>
#include <variant>

using apex::platform::Key;
using apex::platform::keyFromName;
using apex::platform::keyName;

static_assert(std::variant_size_v<apex::platform::Event> == 7);

TEST_CASE("Every key round-trips through its name", "[platform]") {
    REQUIRE(apex::platform::allKeys().size() == 63);
    for (const Key key : apex::platform::allKeys()) {
        CAPTURE(static_cast<std::uint16_t>(key));
        REQUIRE(key != Key::Unknown);
        REQUIRE(keyName(key) != "Unknown");
        REQUIRE(keyFromName(keyName(key)) == key);
    }
}

TEST_CASE("Key names and values are unique", "[platform]") {
    std::set<std::string_view> names;
    std::set<Key> keys;
    for (const Key key : apex::platform::allKeys()) {
        names.insert(keyName(key));
        keys.insert(key);
    }
    REQUIRE(names.size() == apex::platform::allKeys().size());
    REQUIRE(keys.size() == apex::platform::allKeys().size());
}

TEST_CASE("Key values are USB HID usage codes", "[platform]") {
    REQUIRE(static_cast<std::uint16_t>(Key::A) == 4);
    REQUIRE(static_cast<std::uint16_t>(Key::Z) == 29);
    REQUIRE(static_cast<std::uint16_t>(Key::Digit1) == 30);
    REQUIRE(static_cast<std::uint16_t>(Key::Digit0) == 39);
    REQUIRE(static_cast<std::uint16_t>(Key::Escape) == 41);
    REQUIRE(static_cast<std::uint16_t>(Key::Space) == 44);
    REQUIRE(static_cast<std::uint16_t>(Key::F1) == 58);
    REQUIRE(static_cast<std::uint16_t>(Key::F12) == 69);
    REQUIRE(static_cast<std::uint16_t>(Key::Up) == 82);
    REQUIRE(static_cast<std::uint16_t>(Key::LeftShift) == 225);
    REQUIRE(static_cast<std::uint16_t>(Key::RightAlt) == 230);
}

TEST_CASE("Names are the enumerator names", "[platform]") {
    REQUIRE(keyName(Key::A) == "A");
    REQUIRE(keyName(Key::Digit1) == "Digit1");
    REQUIRE(keyName(Key::Space) == "Space");
    REQUIRE(keyName(Key::LeftShift) == "LeftShift");
    REQUIRE(keyName(Key::F1) == "F1");
    REQUIRE(keyFromName("Escape") == Key::Escape);
}

TEST_CASE("Unknown names give no key", "[platform]") {
    REQUIRE_FALSE(keyFromName("Banana").has_value());
    REQUIRE_FALSE(keyFromName("").has_value());
    REQUIRE_FALSE(keyFromName("a").has_value());
    REQUIRE_FALSE(keyFromName("space").has_value());
    REQUIRE_FALSE(keyFromName("Unknown").has_value());
}

TEST_CASE("Unlisted keys are named Unknown", "[platform]") {
    REQUIRE(keyName(Key::Unknown) == "Unknown");
    REQUIRE(keyName(static_cast<Key>(200)) == "Unknown");
}
