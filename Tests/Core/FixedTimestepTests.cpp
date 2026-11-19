#include "Core/FixedTimestep.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>

using apex::core::FixedTimestep;
using apex::core::FixedTimestepConfig;
using apex::core::FrameAdvance;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;

} // namespace

TEST_CASE("The default configuration follows the specification", "[core]") {
    const FixedTimestep timestep;
    REQUIRE(timestep.config().stepSeconds == kStep);
    REQUIRE(timestep.config().maxStepsPerFrame == 4);
    REQUIRE(timestep.config().maxFrameSeconds == 0.25);
}

TEST_CASE("A frame of one step advances one step", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance advance = timestep.advance(kStep);
    REQUIRE(advance.steps == 1);
    REQUIRE_THAT(advance.alpha, WithinAbs(0.0, 1e-9));
}

TEST_CASE("Two half-step frames advance one step on the second", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance first = timestep.advance(kStep / 2.0);
    REQUIRE(first.steps == 0);
    REQUIRE_THAT(first.alpha, WithinAbs(0.5, 1e-9));

    const FrameAdvance second = timestep.advance(kStep / 2.0);
    REQUIRE(second.steps == 1);
    REQUIRE_THAT(second.alpha, WithinAbs(0.0, 1e-9));
}

TEST_CASE("Three third-step frames advance exactly one step", "[core]") {
    FixedTimestep timestep;
    int steps = 0;
    for (int frame = 0; frame < 3; ++frame) {
        steps += timestep.advance(kStep / 3.0).steps;
    }
    REQUIRE(steps == 1);
    REQUIRE(timestep.advance(0.0).steps == 0);
}

TEST_CASE("Alpha is the fraction of a step left in the accumulator", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance advance = timestep.advance(1.5 * kStep);
    REQUIRE(advance.steps == 1);
    REQUIRE_THAT(advance.alpha, WithinAbs(0.5, 1e-9));
}

TEST_CASE("A long frame is clamped and capped at four steps", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance advance = timestep.advance(1.0);
    REQUIRE(advance.steps == 4);
    REQUIRE(advance.alpha >= 0.0);
    REQUIRE(advance.alpha < 1.0);
    REQUIRE(timestep.advance(0.0).steps == 0);
}

TEST_CASE("A frame worth six steps is still capped at four", "[core]") {
    FixedTimestep timestep;
    REQUIRE(timestep.advance(0.1).steps == 4);
    REQUIRE(timestep.advance(0.0).steps == 0);
}

TEST_CASE("Hitting the cap keeps the fraction of a step", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance advance = timestep.advance(5.5 * kStep);
    REQUIRE(advance.steps == 4);
    REQUIRE_THAT(advance.alpha, WithinAbs(0.5, 1e-9));
}

TEST_CASE("A negative or NaN frame time advances nothing", "[core]") {
    FixedTimestep timestep;
    const FrameAdvance negative = timestep.advance(-1.0);
    REQUIRE(negative.steps == 0);
    REQUIRE(negative.alpha == 0.0);

    const FrameAdvance notANumber = timestep.advance(std::nan(""));
    REQUIRE(notANumber.steps == 0);
    REQUIRE(notANumber.alpha == 0.0);

    REQUIRE(timestep.advance(kStep).steps == 1);
}

TEST_CASE("A custom configuration is honoured", "[core]") {
    FixedTimestep timestep(
        FixedTimestepConfig{.stepSeconds = 0.1, .maxStepsPerFrame = 2, .maxFrameSeconds = 1.0});
    REQUIRE(timestep.advance(0.25).steps == 2);
    REQUIRE(timestep.advance(1.0).steps == 2);
}

TEST_CASE("Irregular frames below the cap do not drift", "[core]") {
    FixedTimestep timestep;
    double total = 0.0;
    std::int64_t steps = 0;
    double alpha = 0.0;
    for (int frame = 0; frame < 1000; ++frame) {
        // Deterministic frame times between 1 ms and 50 ms, so never more than three steps.
        const double frameSeconds = 0.001 + static_cast<double>((frame * 37) % 50) * 0.001;
        total += frameSeconds;
        const FrameAdvance advance = timestep.advance(frameSeconds);
        REQUIRE(advance.steps <= 3);
        REQUIRE(advance.alpha >= 0.0);
        REQUIRE(advance.alpha < 1.0);
        steps += advance.steps;
        alpha = advance.alpha;
    }

    const auto expected = static_cast<std::int64_t>(std::floor(total / kStep));
    REQUIRE(steps >= expected - 1);
    REQUIRE(steps <= expected + 1);
    REQUIRE_THAT(static_cast<double>(steps) * kStep + alpha * kStep, WithinAbs(total, 1e-6));
}
