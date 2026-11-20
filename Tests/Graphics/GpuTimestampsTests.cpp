#include "Graphics/GpuTimestamps.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using apex::graphics::elapsedSeconds;
using apex::graphics::TimestampFormat;
using Catch::Approx;

TEST_CASE("Timestamps convert ticks to seconds with the period of the GPU", "[graphics]") {
    const TimestampFormat format{.validBits = 64, .nanosecondsPerTick = 52.08};
    const auto seconds = elapsedSeconds(1'000, 1'000 + 100'000, format);
    REQUIRE(seconds.has_value());
    REQUIRE(*seconds == Approx(100'000 * 52.08e-9));
}

TEST_CASE("Timestamps count across a counter that wrapped around", "[graphics]") {
    const TimestampFormat format{.validBits = 36, .nanosecondsPerTick = 1.0};
    const std::uint64_t last = (std::uint64_t{1} << 36) - 10;
    // 10 ticks to the wrap, then 30 more.
    const auto seconds = elapsedSeconds(last, 30, format);
    REQUIRE(seconds.has_value());
    REQUIRE(*seconds == Approx(40e-9));
}

TEST_CASE("Timestamps ignore the bits above the valid ones", "[graphics]") {
    const TimestampFormat format{.validBits = 32, .nanosecondsPerTick = 1.0};
    const std::uint64_t garbage = std::uint64_t{0xABCD} << 32;
    const auto seconds = elapsedSeconds(garbage | 100, 150, format);
    REQUIRE(seconds.has_value());
    REQUIRE(*seconds == Approx(50e-9));
}

TEST_CASE("Without timestamps there is no GPU time", "[graphics]") {
    REQUIRE_FALSE(TimestampFormat{}.supported());
    REQUIRE_FALSE(elapsedSeconds(0, 100, TimestampFormat{.validBits = 0, .nanosecondsPerTick = 1.0}));
    REQUIRE_FALSE(elapsedSeconds(0, 100, TimestampFormat{.validBits = 64, .nanosecondsPerTick = 0.0}));
}
