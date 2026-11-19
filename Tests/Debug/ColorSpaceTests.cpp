#include "Debug/ColorSpace.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using apex::debug::srgbToLinear;
using Catch::Approx;

TEST_CASE("sRGB to linear keeps black and white", "[debug]") {
    REQUIRE(srgbToLinear(0.0f) == 0.0f);
    REQUIRE(srgbToLinear(1.0f) == Approx(1.0f));
}

TEST_CASE("sRGB to linear follows the IEC 61966-2-1 curve", "[debug]") {
    // Linear segment near black, then the power curve: sRGB 0.5 is about 21.4 % linear.
    REQUIRE(srgbToLinear(0.04f) == Approx(0.04f / 12.92f));
    REQUIRE(srgbToLinear(0.5f) == Approx(0.214041f).epsilon(1e-5));
}

TEST_CASE("sRGB to linear is continuous at the junction of its two segments", "[debug]") {
    REQUIRE(srgbToLinear(0.04045f) == Approx(srgbToLinear(0.040451f)).epsilon(1e-4));
}
