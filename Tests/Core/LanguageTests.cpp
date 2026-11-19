// Fails to compile if the toolchain lacks C++23.

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <format>
#include <string>
#include <version>

static_assert(__cpp_lib_expected >= 202202L, "Apex Engine requires the C++23 standard library");

namespace {

std::expected<int, std::string> half(int value) {
    if (value % 2 != 0) {
        return std::unexpected(std::format("{} is odd", value));
    }
    return value / 2;
}

} // namespace

TEST_CASE("std::expected is available", "[language]") {
    REQUIRE(half(8).value() == 4);
    REQUIRE_FALSE(half(7).has_value());
    REQUIRE(half(7).error() == "7 is odd");
}
