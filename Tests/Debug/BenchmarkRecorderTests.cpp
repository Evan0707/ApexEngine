#include "Debug/BenchmarkRecorder.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>

using apex::debug::BenchmarkRecorder;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kTolerance = 1e-9;

} // namespace

TEST_CASE("A benchmark without frames reports zeros", "[debug]") {
    const BenchmarkRecorder recorder;
    const auto summary = recorder.summary();
    REQUIRE(summary.frameCount == 0);
    REQUIRE(summary.averageFramesPerSecond == 0.0);
    REQUIRE(summary.percentile99FrameMs == 0.0);
}

TEST_CASE("Average frame rate is frames over total time", "[debug]") {
    BenchmarkRecorder recorder;
    for (int i = 0; i < 30; ++i) {
        recorder.addFrame(0.01);
        recorder.addFrame(0.03);
    }
    const auto summary = recorder.summary();
    REQUIRE(summary.frameCount == 60);
    REQUIRE_THAT(summary.durationSeconds, WithinAbs(1.2, kTolerance));
    REQUIRE_THAT(summary.averageFramesPerSecond, WithinAbs(50.0, kTolerance));
    REQUIRE_THAT(summary.averageFrameMs, WithinAbs(20.0, kTolerance));
    REQUIRE_THAT(summary.maxFrameMs, WithinAbs(30.0, kTolerance));
}

TEST_CASE("The 99th percentile ignores the worst 1 % of frames", "[debug]") {
    BenchmarkRecorder recorder;
    // 990 frames at 10 ms, then 10 hitches at 50 ms: 99 % of the frames take at most 10 ms.
    for (int i = 0; i < 990; ++i) {
        recorder.addFrame(0.010);
    }
    for (int i = 0; i < 10; ++i) {
        recorder.addFrame(0.050);
    }
    REQUIRE_THAT(recorder.summary().percentile99FrameMs, WithinAbs(10.0, kTolerance));

    // One more hitch: the 99th percentile now lands on one of them.
    recorder.addFrame(0.050);
    REQUIRE_THAT(recorder.summary().percentile99FrameMs, WithinAbs(50.0, kTolerance));
}

TEST_CASE("A single frame is its own percentile", "[debug]") {
    BenchmarkRecorder recorder;
    recorder.addFrame(0.016);
    REQUIRE_THAT(recorder.summary().percentile99FrameMs, WithinAbs(16.0, kTolerance));
}

TEST_CASE("Invalid frame times count as zero", "[debug]") {
    BenchmarkRecorder recorder;
    recorder.addFrame(-1.0);
    recorder.addFrame(std::numeric_limits<double>::quiet_NaN());
    recorder.addFrame(0.02);
    REQUIRE(recorder.summary().frameCount == 3);
    REQUIRE_THAT(recorder.elapsedSeconds(), WithinAbs(0.02, kTolerance));
}
