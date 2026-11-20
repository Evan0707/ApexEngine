#include "Graphics/FrameData.h"
#include "Graphics/Frustum.h"
#include "Graphics/Projection.h"
#include "Resources/Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace apex::graphics;
using apex::resources::BoundingSphere;
using Catch::Matchers::WithinAbs;

namespace {

// 90 degrees, square: at depth d, the view spans from -d to d on both axes. Near 0.1 m, far 100 m.
[[nodiscard]] Frustum squareView() {
    return frustumFromMatrix(perspectiveProjection(glm::radians(90.0f), 1.0f, 0.1f, 100.0f));
}

[[nodiscard]] BoundingSphere sphere(const glm::vec3& center, float radius) {
    return BoundingSphere{.center = center, .radius = radius};
}

} // namespace

TEST_CASE("A sphere in front of the camera is kept, one behind it is not", "[graphics]") {
    const Frustum frustum = squareView();
    REQUIRE(intersects(frustum, sphere({0.0f, 0.0f, -10.0f}, 0.5f)));
    REQUIRE_FALSE(intersects(frustum, sphere({0.0f, 0.0f, 10.0f}, 0.5f)));
    // Behind, but large enough to reach in front of the near plane.
    REQUIRE(intersects(frustum, sphere({0.0f, 0.0f, 1.0f}, 2.0f)));
}

TEST_CASE("Each side of the frustum culls what is entirely outside it", "[graphics]") {
    const Frustum frustum = squareView();
    // At 10 m, the sides are at 10 m from the axis. 0.5 m beyond: outside; touching across: kept.
    for (const glm::vec3 side :
         {glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, -1, 0)}) {
        CAPTURE(side.x, side.y);
        REQUIRE_FALSE(intersects(frustum, sphere(side * 11.5f + glm::vec3(0, 0, -10), 1.0f)));
        REQUIRE(intersects(frustum, sphere(side * 10.5f + glm::vec3(0, 0, -10), 1.0f)));
    }
    // Beyond the far plane, and straddling it.
    REQUIRE_FALSE(intersects(frustum, sphere({0.0f, 0.0f, -102.0f}, 1.0f)));
    REQUIRE(intersects(frustum, sphere({0.0f, 0.0f, -100.5f}, 1.0f)));
}

TEST_CASE("The frustum's planes are unit length", "[graphics]") {
    for (const glm::vec4& plane : squareView().planes) {
        REQUIRE_THAT(glm::length(glm::vec3(plane)), WithinAbs(1.0, 1e-5));
    }
}

TEST_CASE("An orthographic shadow volume culls like a box", "[graphics]") {
    const glm::mat4 projection = orthographicProjection(20.0f, 20.0f, 0.0f, 100.0f);
    const Frustum frustum = frustumFromMatrix(projection);
    REQUIRE(intersects(frustum, sphere({19.0f, 0.0f, -50.0f}, 0.5f)));
    REQUIRE_FALSE(intersects(frustum, sphere({21.0f, 0.0f, -50.0f}, 0.5f)));
    REQUIRE_FALSE(intersects(frustum, sphere({0.0f, 0.0f, 2.0f}, 1.0f)));
    REQUIRE_FALSE(intersects(frustum, sphere({0.0f, 0.0f, -102.0f}, 1.0f)));
}

TEST_CASE("The sphere follows the model matrix, scaled by its largest axis", "[graphics]") {
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, -3.0f)),
                                       glm::vec3(100.0f, 0.2f, 100.0f));
    const BoundingSphere moved = transformSphere(model, sphere({0.0f, 0.5f, 0.0f}, 0.866f));
    REQUIRE_THAT(moved.center.x, WithinAbs(5.0, 1e-4));
    REQUIRE_THAT(moved.center.y, WithinAbs(0.1, 1e-4));
    REQUIRE_THAT(moved.radius, WithinAbs(86.6, 1e-3));
}

TEST_CASE("A cube's bounding sphere reaches its corners", "[resources]") {
    const BoundingSphere bounds = apex::resources::boundingSphere(apex::resources::makeCube(1.0f));
    REQUIRE(bounds.center == glm::vec3(0.0f));
    REQUIRE_THAT(bounds.radius, WithinAbs(std::sqrt(3.0) / 2.0, 1e-5));
    REQUIRE(apex::resources::boundingSphere({}) == BoundingSphere{});
}
