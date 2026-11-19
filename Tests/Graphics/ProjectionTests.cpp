#include "Graphics/Projection.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

using apex::graphics::perspectiveProjection;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;
constexpr float kNear = 0.1f;
constexpr float kFar = 100.0f;

[[nodiscard]] glm::vec3 toNdc(const glm::mat4& projection, const glm::vec3& viewPosition) {
    const glm::vec4 clip = projection * glm::vec4(viewPosition, 1.0f);
    return glm::vec3(clip) / clip.w;
}

[[nodiscard]] glm::mat4 squareProjection() {
    return perspectiveProjection(glm::radians(90.0f), 1.0f, kNear, kFar);
}

} // namespace

TEST_CASE("Depth goes from 0 at the near plane to 1 at the far plane", "[graphics]") {
    const glm::mat4 projection = squareProjection();
    REQUIRE_THAT(toNdc(projection, {0.0f, 0.0f, -kNear}).z, WithinAbs(0.0, kTolerance));
    REQUIRE_THAT(toNdc(projection, {0.0f, 0.0f, -kFar}).z, WithinAbs(1.0, kTolerance));
    const float middle = toNdc(projection, {0.0f, 0.0f, -10.0f}).z;
    REQUIRE(middle > 0.0f);
    REQUIRE(middle < 1.0f);
}

TEST_CASE("Up in view space is up on screen, where Vulkan's Y is -1", "[graphics]") {
    const glm::mat4 projection = squareProjection();
    // 90 degrees vertically: at 1 m, the top of the view is 1 m above the axis.
    REQUIRE_THAT(toNdc(projection, {0.0f, 1.0f, -1.0f}).y, WithinAbs(-1.0, kTolerance));
    REQUIRE_THAT(toNdc(projection, {1.0f, 0.0f, -1.0f}).x, WithinAbs(1.0, kTolerance));
}

TEST_CASE("The aspect ratio widens the horizontal field of view", "[graphics]") {
    const glm::mat4 projection = perspectiveProjection(glm::radians(90.0f), 2.0f, kNear, kFar);
    REQUIRE_THAT(toNdc(projection, {2.0f, 0.0f, -1.0f}).x, WithinAbs(1.0, kTolerance));
    REQUIRE_THAT(toNdc(projection, {0.0f, 1.0f, -1.0f}).y, WithinAbs(-1.0, kTolerance));
}

TEST_CASE("A counter-clockwise face towards the camera stays front-facing for Vulkan", "[graphics]") {
    // Counter-clockwise seen from the camera, which looks towards -Z.
    const std::array<glm::vec3, 3> triangle{
        {{-1.0f, -1.0f, -5.0f}, {1.0f, -1.0f, -5.0f}, {0.0f, 1.0f, -5.0f}}};
    const glm::mat4 projection = squareProjection();
    constexpr float kWidth = 800.0f;
    constexpr float kHeight = 600.0f;
    std::array<glm::vec2, 3> framebuffer{};
    for (std::size_t i = 0; i < triangle.size(); ++i) {
        const glm::vec3 ndc = toNdc(projection, triangle[i]);
        framebuffer[i] = {(ndc.x + 1.0f) * 0.5f * kWidth, (ndc.y + 1.0f) * 0.5f * kHeight};
    }
    // Vulkan specification, "Basic Polygon Rasterization": a > 0 is counter-clockwise.
    float area = 0.0f;
    for (std::size_t i = 0; i < framebuffer.size(); ++i) {
        const glm::vec2& current = framebuffer[i];
        const glm::vec2& next = framebuffer[(i + 1) % framebuffer.size()];
        area += current.x * next.y - next.x * current.y;
    }
    area *= -0.5f;
    REQUIRE(area > 0.0f);
}
