#include "SceneExtraction.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace apex::game;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

void requireNear(const glm::vec3& actual, const glm::vec3& expected) {
    REQUIRE_THAT(actual.x, WithinAbs(expected.x, kTolerance));
    REQUIRE_THAT(actual.y, WithinAbs(expected.y, kTolerance));
    REQUIRE_THAT(actual.z, WithinAbs(expected.z, kTolerance));
}

[[nodiscard]] glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point) {
    return glm::vec3(matrix * glm::vec4(point, 1.0f));
}

} // namespace

TEST_CASE("Objects are placed relative to the camera", "[game]") {
    const ObjectTransform transform{.position = {3.0f, 1.0f, -2.0f}};
    const glm::mat4 model = modelRelativeToCamera(transform, {1.0f, 1.0f, 2.0f});
    requireNear(transformPoint(model, glm::vec3(0.0f)), {2.0f, 0.0f, -4.0f});
}

TEST_CASE("Far from the world origin, positions near the camera stay exact", "[game]") {
    // At 10 km, a float step is about 1 mm: the subtraction must come before any matrix product.
    const ObjectTransform transform{.position = {10000.0f, 0.0f, 10000.0f}};
    const glm::mat4 model = modelRelativeToCamera(transform, {10000.0f, 1.0f, 10005.0f});
    REQUIRE(transformPoint(model, glm::vec3(0.0f)) == glm::vec3(0.0f, -1.0f, -5.0f));
}

TEST_CASE("Rotation and scale apply around the object's origin", "[game]") {
    const ObjectTransform transform{.position = {0.0f, 0.0f, -10.0f},
                                    .rotation = glm::angleAxis(glm::half_pi<float>(), apex::core::kWorldUp),
                                    .scale = glm::vec3(2.0f)};
    const glm::mat4 model = modelRelativeToCamera(transform, glm::vec3(0.0f));
    // +X turned a quarter to the left is -Z, then doubled.
    requireNear(transformPoint(model, {0.5f, 0.0f, 0.0f}), {0.0f, 0.0f, -11.0f});
}

TEST_CASE("The render camera is a pure rotation", "[game]") {
    CameraView camera;
    camera.position = {500.0f, 20.0f, -300.0f};
    camera.orientation = glm::angleAxis(glm::radians(30.0f), apex::core::kWorldUp);
    const apex::graphics::RenderCamera render = makeRenderCamera(camera);
    REQUIRE(render.view[3] == glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    // What the camera faces ends up straight ahead, on -Z.
    const glm::vec3 facing = camera.orientation * apex::core::kWorldForward;
    requireNear(glm::vec3(render.view * glm::vec4(facing, 0.0f)), apex::core::kWorldForward);
}

TEST_CASE("The render camera keeps the projection settings", "[game]") {
    const CameraView camera{.verticalFovRadians = 1.0f, .nearPlane = 0.5f, .farPlane = 50.0f};
    const apex::graphics::RenderCamera render = makeRenderCamera(camera);
    REQUIRE(render.verticalFovRadians == 1.0f);
    REQUIRE(render.nearPlane == 0.5f);
    REQUIRE(render.farPlane == 50.0f);
}
