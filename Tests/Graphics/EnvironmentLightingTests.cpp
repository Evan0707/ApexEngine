#include "Graphics/EnvironmentLighting.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace apex::graphics;
using apex::resources::EnvironmentData;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

[[nodiscard]] EnvironmentData uniformEnvironment(const glm::vec3& radiance) {
    return EnvironmentData{
        .name = "uniform", .width = 64, .height = 32, .texels = std::vector(64 * 32, radiance)};
}

// Bright above the horizon, black below.
[[nodiscard]] EnvironmentData skyOverBlackGround() {
    EnvironmentData environment = uniformEnvironment(glm::vec3(0.0f));
    for (std::uint32_t y = 0; y < environment.height / 2; ++y) {
        for (std::uint32_t x = 0; x < environment.width; ++x) {
            environment.texels[y * environment.width + x] = glm::vec3(1.0f);
        }
    }
    return environment;
}

} // namespace

TEST_CASE("A uniform environment gives an irradiance of pi times its radiance everywhere", "[graphics]") {
    const IrradianceSh sh = projectIrradiance(uniformEnvironment({0.5f, 1.0f, 2.0f}));
    for (const glm::vec3 normal :
         {glm::vec3(0, 1, 0), glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::normalize(glm::vec3(1, -2, 3))}) {
        const glm::vec3 irradiance = irradianceAt(sh, normal);
        REQUIRE_THAT(irradiance.r, WithinRel(0.5 * glm::pi<double>(), 0.01));
        REQUIRE_THAT(irradiance.g, WithinRel(1.0 * glm::pi<double>(), 0.01));
        REQUIRE_THAT(irradiance.b, WithinRel(2.0 * glm::pi<double>(), 0.01));
    }
}

TEST_CASE("A bright sky over a black ground lights upward faces most", "[graphics]") {
    const IrradianceSh sh = projectIrradiance(skyOverBlackGround());
    const float up = irradianceAt(sh, {0.0f, 1.0f, 0.0f}).r;
    const float side = irradianceAt(sh, {1.0f, 0.0f, 0.0f}).r;
    const float down = irradianceAt(sh, {0.0f, -1.0f, 0.0f}).r;
    // Exactly: pi facing the sky, pi / 2 on a vertical face, 0 facing the ground; nine coefficients come
    // close.
    REQUIRE_THAT(up, WithinRel(glm::pi<double>(), 0.06));
    REQUIRE_THAT(side, WithinRel(glm::half_pi<double>(), 0.06));
    REQUIRE(down < 0.15f);
    REQUIRE(down >= 0.0f);
}

TEST_CASE("The split-sum table stays between 0 and 1", "[graphics]") {
    constexpr std::uint32_t kSize = 16;
    const std::vector<glm::vec2> table = computeBrdfTable(kSize, 128);
    REQUIRE(table.size() == kSize * kSize);
    for (const glm::vec2 entry : table) {
        REQUIRE(entry.x >= 0.0f);
        REQUIRE(entry.y >= 0.0f);
        REQUIRE(entry.x + entry.y <= 1.01f);
    }
}

TEST_CASE("A smooth surface seen head-on reflects F0, rough and grazing reflect less", "[graphics]") {
    constexpr std::uint32_t kSize = 32;
    const std::vector<glm::vec2> table = computeBrdfTable(kSize, 256);
    const auto at = [&](std::uint32_t column, std::uint32_t row) { return table[row * kSize + column]; };
    // n.v close to 1, smooth: scale close to 1, bias close to 0.
    const glm::vec2 smoothHeadOn = at(kSize - 1, 0);
    REQUIRE_THAT(smoothHeadOn.x, WithinAbs(1.0, 0.05));
    REQUIRE_THAT(smoothHeadOn.y, WithinAbs(0.0, 0.02));
    // Rough surfaces lose energy to masking.
    REQUIRE(at(kSize - 1, kSize - 1).x + at(kSize - 1, kSize - 1).y < smoothHeadOn.x + smoothHeadOn.y);
    // At grazing angles, Fresnel moves weight from the scale to the bias.
    REQUIRE(at(0, kSize / 2).y > at(kSize - 1, kSize / 2).y);
}
