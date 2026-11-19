#include "CommandLine.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <initializer_list>
#include <span>
#include <string_view>

using apex::core::ErrorCode;
using apex::game::parseCommandLine;

TEST_CASE("Without arguments the demo runs until closed, with vsync", "[game]") {
    const auto options = parseCommandLine({});
    REQUIRE(options.has_value());
    REQUIRE_FALSE(options->maxFrames.has_value());
    REQUIRE(options->vsync);
}

TEST_CASE("The frames option sets the frame limit and no-vsync turns vsync off", "[game]") {
    constexpr std::array<std::string_view, 3> arguments{"--frames", "120", "--no-vsync"};
    const auto options = parseCommandLine(arguments);
    REQUIRE(options.has_value());
    REQUIRE(options->maxFrames == 120u);
    REQUIRE_FALSE(options->vsync);
}

TEST_CASE("The frames option rejects a missing, zero, negative or non-numeric count", "[game]") {
    constexpr std::array<std::string_view, 1> missing{"--frames"};
    constexpr std::array<std::string_view, 2> zero{"--frames", "0"};
    constexpr std::array<std::string_view, 2> negative{"--frames", "-5"};
    constexpr std::array<std::string_view, 2> text{"--frames", "12abc"};

    for (const auto arguments :
         {std::span<const std::string_view>(missing), std::span<const std::string_view>(zero),
          std::span<const std::string_view>(negative), std::span<const std::string_view>(text)}) {
        const auto options = parseCommandLine(arguments);
        REQUIRE_FALSE(options.has_value());
        REQUIRE(options.error().code == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("An unknown option is rejected", "[game]") {
    constexpr std::array<std::string_view, 1> arguments{"--fullscreen"};
    const auto options = parseCommandLine(arguments);
    REQUIRE_FALSE(options.has_value());
    REQUIRE(options.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("The benchmark option sets the measured duration", "[game]") {
    constexpr std::array<std::string_view, 3> arguments{"--no-vsync", "--benchmark", "60"};
    const auto options = parseCommandLine(arguments);
    REQUIRE(options.has_value());
    REQUIRE(options->benchmarkSeconds == 60u);
    REQUIRE_FALSE(options->vsync);
    REQUIRE_FALSE(parseCommandLine(std::array<std::string_view, 1>{"--benchmark"}).has_value());
    REQUIRE_FALSE(parseCommandLine(std::array<std::string_view, 2>{"--benchmark", "0"}).has_value());
    REQUIRE_FALSE(parseCommandLine(std::array<std::string_view, 2>{"--benchmark", "1.5"}).has_value());
}
