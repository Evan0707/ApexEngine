#include "Debug/FrameTimeStats.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using apex::debug::FrameSample;
using apex::debug::FrameTimeStats;
using Catch::Approx;

TEST_CASE("Frame time statistics are zero before the first frame", "[debug]") {
    const FrameTimeStats stats;
    REQUIRE(stats.frameCount() == 0);
    REQUIRE(stats.framesPerSecond() == 0.0);
    REQUIRE(stats.averageFrameSeconds() == 0.0);
    REQUIRE(stats.maxFrameSeconds() == 0.0);
    REQUIRE(stats.averageCpuSeconds() == 0.0);
    REQUIRE(stats.maxCpuSeconds() == 0.0);
}

TEST_CASE("Frame time statistics give the frame rate, and the average and maximum times", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({0.125, 0.0625});
    stats.addFrame({0.25, 0.125});
    stats.addFrame({0.125, 0.03125});

    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.framesPerSecond() == Approx(6.0));
    REQUIRE(stats.averageFrameSeconds() == Approx(0.5 / 3.0));
    REQUIRE(stats.maxFrameSeconds() == 0.25);
    REQUIRE(stats.averageCpuSeconds() == Approx(0.21875 / 3.0));
    REQUIRE(stats.maxCpuSeconds() == 0.125);
}

TEST_CASE("Frames older than the window are dropped", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({0.5, 0.5}); // dropped: the four next frames cover the whole second
    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame({0.25, 0.0625});
    }

    REQUIRE(stats.frameCount() == 4);
    REQUIRE(stats.maxFrameSeconds() == 0.25);
    REQUIRE(stats.maxCpuSeconds() == 0.0625);
    REQUIRE(stats.averageCpuSeconds() == Approx(0.0625));
    REQUIRE(stats.framesPerSecond() == Approx(4.0));
}

TEST_CASE("A frame stays in the window until newer frames cover the whole window", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({0.5, 0.25});
    stats.addFrame({0.25, 0.0});
    stats.addFrame({0.25, 0.0});

    // The newer frames cover only 0.5 s: the 0.5 s frame still counts.
    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.maxFrameSeconds() == 0.5);
    REQUIRE(stats.maxCpuSeconds() == 0.25);
}

TEST_CASE("A frame longer than the window is kept alone", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({0.25, 0.25});
    stats.addFrame({2.0, 1.0});
    stats.addFrame({3.0, 1.5});

    REQUIRE(stats.frameCount() == 1);
    REQUIRE(stats.maxFrameSeconds() == 3.0);
    REQUIRE(stats.framesPerSecond() == Approx(1.0 / 3.0));
}

TEST_CASE("The CPU time is capped by the frame time", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({0.01, 0.5});
    REQUIRE(stats.maxCpuSeconds() == 0.01);
}

TEST_CASE("Negative and non-finite times count as zero", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({-1.0, -1.0});
    stats.addFrame({std::numeric_limits<double>::quiet_NaN(), 0.0});
    stats.addFrame({std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    REQUIRE(stats.frameCount() == 3);
    REQUIRE(stats.maxFrameSeconds() == 0.0);
    REQUIRE(stats.maxCpuSeconds() == 0.0);
    REQUIRE(stats.framesPerSecond() == 0.0);
}

TEST_CASE("The window length is configurable", "[debug]") {
    FrameTimeStats stats(0.5);
    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame({0.25, 0.0});
    }
    REQUIRE(stats.frameCount() == 2);
}

TEST_CASE("The GPU time is unknown until a frame brings one", "[debug]") {
    FrameTimeStats stats;
    REQUIRE_FALSE(stats.averageGpuSeconds());
    stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.125});
    REQUIRE_FALSE(stats.averageGpuSeconds());
    REQUIRE_FALSE(stats.maxGpuSeconds());
}

TEST_CASE("The GPU time is averaged over the frames that have one", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.0, .gpuSeconds = 0.125});
    stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.0});
    stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.0, .gpuSeconds = 0.0625});

    REQUIRE(stats.averageGpuSeconds() == Approx(0.09375));
    REQUIRE(stats.maxGpuSeconds() == 0.125);
}

TEST_CASE("GPU times leave the window with their frame", "[debug]") {
    FrameTimeStats stats;
    stats.addFrame({.frameSeconds = 0.5, .cpuSeconds = 0.0, .gpuSeconds = 0.5});
    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.0, .gpuSeconds = 0.25});
    }
    REQUIRE(stats.averageGpuSeconds() == Approx(0.25));
    REQUIRE(stats.maxGpuSeconds() == 0.25);

    for (int frame = 0; frame < 4; ++frame) {
        stats.addFrame({.frameSeconds = 0.25, .cpuSeconds = 0.0});
    }
    REQUIRE_FALSE(stats.averageGpuSeconds());
}
