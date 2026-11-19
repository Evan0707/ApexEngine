#include "Graphics/FrameData.h"
#include "Graphics/MaterialTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>

using namespace apex::graphics;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

} // namespace

TEST_CASE("The per-frame structures match the GLSL layouts", "[graphics]") {
    // std140 uniform block of Frame.glsl.
    STATIC_REQUIRE(offsetof(FrameUniforms, viewProjection) == 0);
    STATIC_REQUIRE(offsetof(FrameUniforms, directionToLight) == 64);
    STATIC_REQUIRE(offsetof(FrameUniforms, lightRadiance) == 80);
    STATIC_REQUIRE(offsetof(FrameUniforms, ambient) == 96);
    // std430 arrays: the stride of a struct is its size rounded up to its alignment (16 bytes here).
    STATIC_REQUIRE(sizeof(ObjectGpu) == 144);
    STATIC_REQUIRE(offsetof(ObjectGpu, materialIndex) == 128);
    STATIC_REQUIRE(sizeof(MaterialGpu) == 32);
    STATIC_REQUIRE(offsetof(MaterialGpu, metallic) == 16);
    STATIC_REQUIRE(offsetof(MaterialGpu, roughness) == 20);
}

TEST_CASE("The normal matrix keeps normals perpendicular under non-uniform scale", "[graphics]") {
    const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 1.0f));
    const ObjectGpu object = makeObjectGpu(model, 3);
    REQUIRE(object.materialIndex == 3);

    // A 45 degree slope: its normal and a direction along its surface.
    const glm::vec3 normal = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
    const glm::vec3 alongSurface{1.0f, -1.0f, 0.0f};
    const glm::vec3 scaledSurface = glm::vec3(model * glm::vec4(alongSurface, 0.0f));
    const glm::vec3 transformedNormal = glm::vec3(object.normalMatrix * glm::vec4(normal, 0.0f));
    REQUIRE_THAT(glm::dot(transformedNormal, scaledSurface), WithinAbs(0.0, kTolerance));
}

TEST_CASE("The normal matrix ignores the translation relative to the camera", "[graphics]") {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(1000.0f, 5.0f, -3.0f));
    const ObjectGpu object = makeObjectGpu(model, 0);
    REQUIRE(object.normalMatrix == glm::mat4(1.0f));
}

TEST_CASE("Frame uniforms carry the projection, the light and the ambient term", "[graphics]") {
    RenderScene scene;
    scene.camera.verticalFovRadians = glm::radians(90.0f);
    scene.light =
        RenderLight{.directionToLight = {0.0f, 2.0f, 0.0f}, .color = {1.0f, 0.5f, 0.25f}, .intensity = 2.0f};
    scene.ambient = {0.1f, 0.2f, 0.3f};
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f);

    // Straight ahead of the camera: the center of the screen.
    const glm::vec4 clip = uniforms.viewProjection * glm::vec4(0.0f, 0.0f, -10.0f, 1.0f);
    REQUIRE_THAT(clip.x / clip.w, WithinAbs(0.0, kTolerance));
    REQUIRE_THAT(clip.y / clip.w, WithinAbs(0.0, kTolerance));

    REQUIRE(uniforms.directionToLight == glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
    REQUIRE(uniforms.lightRadiance == glm::vec4(2.0f, 1.0f, 0.5f, 0.0f));
    REQUIRE(uniforms.ambient == glm::vec4(0.1f, 0.2f, 0.3f, 0.0f));
}

TEST_CASE("The view rotation applies before the projection", "[graphics]") {
    RenderScene scene;
    // Camera turned a quarter turn to the left: it looks towards -X.
    const glm::quat orientation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    scene.camera.view = glm::mat4_cast(glm::inverse(orientation));
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f);
    const glm::vec4 clip = uniforms.viewProjection * glm::vec4(-10.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE_THAT(clip.x / clip.w, WithinAbs(0.0, kTolerance));
    REQUIRE(clip.z / clip.w > 0.0f);
    REQUIRE(clip.z / clip.w < 1.0f);
}
