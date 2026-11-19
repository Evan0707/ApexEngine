#include "Graphics/SwapchainState.h"

#include <catch2/catch_test_macros.hpp>

using apex::graphics::FrameDecision;
using apex::graphics::SwapchainState;
using apex::platform::PixelSize;

namespace {

constexpr PixelSize kFullHd{1920, 1080};

} // namespace

TEST_CASE("The first frame creates the swapchain", "[graphics]") {
    const SwapchainState state;
    REQUIRE(state.decide(kFullHd) == FrameDecision::RecreateThenRender);
}

TEST_CASE("A swapchain matching the window is reused", "[graphics]") {
    SwapchainState state;
    state.recreated(kFullHd);
    REQUIRE(state.decide(kFullHd) == FrameDecision::Render);
}

TEST_CASE("A window without area skips the frame", "[graphics]") {
    SwapchainState state;
    REQUIRE(state.decide({0, 0}) == FrameDecision::Skip);
    state.recreated(kFullHd);
    REQUIRE(state.decide({0, 0}) == FrameDecision::Skip);
    REQUIRE(state.decide({1920, 0}) == FrameDecision::Skip);
    REQUIRE(state.decide({0, 1080}) == FrameDecision::Skip);
}

TEST_CASE("A resized window recreates the swapchain once", "[graphics]") {
    SwapchainState state;
    state.recreated(kFullHd);

    // Several resizes between two frames: only the last size counts.
    REQUIRE(state.decide({1280, 720}) == FrameDecision::RecreateThenRender);
    state.recreated({1280, 720});
    REQUIRE(state.decide({1280, 720}) == FrameDecision::Render);
}

TEST_CASE("An out of date swapchain is recreated even at the same size", "[graphics]") {
    SwapchainState state;
    state.recreated(kFullHd);
    state.markOutOfDate();

    REQUIRE(state.decide(kFullHd) == FrameDecision::RecreateThenRender);
    state.recreated(kFullHd);
    REQUIRE(state.decide(kFullHd) == FrameDecision::Render);
}

TEST_CASE("Restoring a minimized window renders again without recreation when the size is unchanged",
          "[graphics]") {
    SwapchainState state;
    state.recreated(kFullHd);

    REQUIRE(state.decide({0, 0}) == FrameDecision::Skip);
    REQUIRE(state.decide(kFullHd) == FrameDecision::Render);
}

TEST_CASE("An out of date mark survives a minimization", "[graphics]") {
    SwapchainState state;
    state.recreated(kFullHd);
    state.markOutOfDate();

    REQUIRE(state.decide({0, 0}) == FrameDecision::Skip);
    REQUIRE(state.decide(kFullHd) == FrameDecision::RecreateThenRender);
}
