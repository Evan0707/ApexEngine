#include "Debug/FrameTimeStats.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using apex::debug::FrameTimeStats;
using Catch::Approx;

TEST_CASE("Frame time statistics are zero before the first frame", "[debug]") {
    const FrameTimeStats stats;
    REQUIRE(stats.frameCount() == 0);
    REQUIRE(stats.averageSeconds() == 0.0);
    REQUIRE(stats.maxSeconds() == 0.0);
    REQUIRE(stats.framesPerSecond() == 0.0);
}

TEST_CASE("Frame time statistics give the average, the maximum and the frame rate", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame(0.125);
    stats.addFrame(0.25);
    stats.addFrame(0.125);

    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.averageSeconds() == Approx(0.5 / 3.0));
    REQUIRE(stats.maxSeconds() == 0.25);
    REQUIRE(stats.framesPerSecond() == Approx(6.0));
}

TEST_CASE("Frames older than the window are dropped", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame(0.5); // dropped: the four next frames cover the whole second
    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame(0.25);
    }

    REQUIRE(stats.frameCount() == 4);
    REQUIRE(stats.maxSeconds() == 0.25);
    REQUIRE(stats.averageSeconds() == 0.25);
    REQUIRE(stats.framesPerSecond() == Approx(4.0));
}

TEST_CASE("A frame stays in the window until newer frames cover the whole window", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame(0.5);
    stats.addFrame(0.25);
    stats.addFrame(0.25);

    // The newer frames cover only 0.5 s: the 0.5 s frame still counts.
    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.maxSeconds() == 0.5);
}

TEST_CASE("A frame longer than the window is kept alone", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame(0.25);
    stats.addFrame(2.0);
    stats.addFrame(3.0);

    REQUIRE(stats.frameCount() == 1);
    REQUIRE(stats.maxSeconds() == 3.0);
    REQUIRE(stats.framesPerSecond() == Approx(1.0 / 3.0));
}

TEST_CASE("Negative and non-finite frame times count as zero", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame(-1.0);
    stats.addFrame(std::numeric_limits<double>::quiet_NaN());
    stats.addFrame(std::numeric_limits<double>::infinity());

    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.maxSeconds() == 0.0);
    REQUIRE(stats.averageSeconds() == 0.0);
    REQUIRE(stats.framesPerSecond() == 0.0);
}

TEST_CASE("The window length is configurable", "[debug]") {
    FrameTimeStats stats(0.5);
    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame(0.25);
    }
    REQUIRE(stats.frameCount() == 2);
}
