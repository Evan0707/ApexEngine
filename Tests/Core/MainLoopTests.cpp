#include "Core/MainLoop.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using apex::core::FixedStep;
using apex::core::FramePhases;
using apex::core::MainLoop;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kStep = 1.0 / 60.0;

class RecordingPhases final : public FramePhases {
public:
    bool processInput() override {
        calls.emplace_back("input");
        ++inputCount;
        return inputCount <= framesBeforeQuit;
    }
    void simulate(const FixedStep& step) override {
        calls.emplace_back("simulate");
        steps.push_back(step);
    }
    void extract(double alpha) override {
        calls.emplace_back("extract");
        alphas.push_back(alpha);
    }
    void render() override { calls.emplace_back("render"); }
    void present() override { calls.emplace_back("present"); }

    int framesBeforeQuit = 1000;
    int inputCount = 0;
    std::vector<std::string> calls;
    std::vector<FixedStep> steps;
    std::vector<double> alphas;
};

} // namespace

TEST_CASE("A frame runs its phases in order", "[core]") {
    RecordingPhases phases;
    MainLoop loop;

    REQUIRE(loop.runFrame(phases, 2.5 * kStep));

    const std::vector<std::string> expected{"input", "simulate", "simulate", "extract", "render", "present"};
    REQUIRE(phases.calls == expected);
    REQUIRE(phases.alphas.size() == 1);
    REQUIRE_THAT(phases.alphas[0], WithinAbs(0.5, 1e-9));
}

TEST_CASE("A frame shorter than a step skips the simulation", "[core]") {
    RecordingPhases phases;
    MainLoop loop;

    REQUIRE(loop.runFrame(phases, kStep / 4.0));

    const std::vector<std::string> expected{"input", "extract", "render", "present"};
    REQUIRE(phases.calls == expected);
}

TEST_CASE("Fixed steps are numbered from zero and carry the fixed delta", "[core]") {
    RecordingPhases phases;
    MainLoop loop;

    REQUIRE(loop.runFrame(phases, kStep));
    REQUIRE(loop.runFrame(phases, 0.0));
    REQUIRE(loop.runFrame(phases, 3.0 * kStep));
    REQUIRE(loop.runFrame(phases, 1.0));

    REQUIRE(phases.steps.size() == 8);
    for (std::size_t index = 0; index < phases.steps.size(); ++index) {
        REQUIRE(phases.steps[index].tick == static_cast<std::uint64_t>(index));
        REQUIRE(phases.steps[index].deltaSeconds == kStep);
    }
}

TEST_CASE("Quitting in the input phase skips the rest of the frame", "[core]") {
    RecordingPhases phases;
    phases.framesBeforeQuit = 0;
    MainLoop loop;

    REQUIRE_FALSE(loop.runFrame(phases, 1.0));

    const std::vector<std::string> expected{"input"};
    REQUIRE(phases.calls == expected);
}

TEST_CASE("The real-time loop stops when the input phase asks to quit", "[core]") {
    RecordingPhases phases;
    phases.framesBeforeQuit = 3;
    MainLoop loop;

    loop.run(phases);

    REQUIRE(phases.inputCount == 4);
    REQUIRE(phases.alphas.size() == 3);
    REQUIRE(phases.calls.back() == "input");
}
