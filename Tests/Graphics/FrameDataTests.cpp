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
    STATIC_REQUIRE(offsetof(FrameUniforms, inverseViewProjection) == 64);
    STATIC_REQUIRE(offsetof(FrameUniforms, lightViewProjection) == 128);
    STATIC_REQUIRE(offsetof(FrameUniforms, directionToLight) == 192);
    STATIC_REQUIRE(offsetof(FrameUniforms, lightRadiance) == 208);
    STATIC_REQUIRE(offsetof(FrameUniforms, ambient) == 224);
    STATIC_REQUIRE(offsetof(FrameUniforms, shadow) == 240);
    STATIC_REQUIRE(offsetof(FrameUniforms, toneMapping) == 256);
    // std140 array of vec4: a stride of 16 bytes.
    STATIC_REQUIRE(offsetof(FrameUniforms, irradiance) == 272);
    STATIC_REQUIRE(offsetof(FrameUniforms, environment) == 416);
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
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f, 2048);

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
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f, 2048);
    const glm::vec4 clip = uniforms.viewProjection * glm::vec4(-10.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE_THAT(clip.x / clip.w, WithinAbs(0.0, kTolerance));
    REQUIRE(clip.z / clip.w > 0.0f);
    REQUIRE(clip.z / clip.w < 1.0f);
}

TEST_CASE("Frame uniforms turn shadows on only when the scene asks for them", "[graphics]") {
    RenderScene scene;
    REQUIRE(makeFrameUniforms(scene, 1.0f, 2048).shadow.x == 0.0f);
    scene.shadow.enabled = true;
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f, 2048);
    REQUIRE(uniforms.shadow.x == 1.0f);
    REQUIRE(uniforms.shadow.y == 1.0f / 2048.0f);
    REQUIRE(uniforms.shadow.z == kShadowFadeStart);
    REQUIRE(uniforms.lightViewProjection == shadowViewProjection(scene.shadow));
}

TEST_CASE("Exposure stops double or halve the light", "[graphics]") {
    REQUIRE(exposureMultiplier(0.0f) == 1.0f);
    REQUIRE(exposureMultiplier(1.0f) == 2.0f);
    REQUIRE(exposureMultiplier(-2.0f) == 0.25f);
    REQUIRE(exposureMultiplier(4.0f) == 16.0f);
}

TEST_CASE("Frame uniforms carry the exposure and the tonemapping switch", "[graphics]") {
    RenderScene scene;
    REQUIRE(makeFrameUniforms(scene, 1.0f, 2048).toneMapping == glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));
    scene.toneMapping = RenderToneMapping{.exposureStops = -1.0f, .enabled = false};
    REQUIRE(makeFrameUniforms(scene, 1.0f, 2048).toneMapping == glm::vec4(0.5f, 0.0f, 0.0f, 0.0f));
}

TEST_CASE("Frame uniforms carry the environment's irradiance only when it lights the frame", "[graphics]") {
    const RenderScene scene;
    REQUIRE(makeFrameUniforms(scene, 1.0f, 2048).environment.x == 0.0f);

    IrradianceSh sh;
    sh.coefficients[0] = {1.0f, 2.0f, 3.0f};
    sh.coefficients[8] = {-1.0f, 0.5f, 0.0f};
    const FrameUniforms uniforms = makeFrameUniforms(
        scene, 1.0f, 2048, FrameEnvironment{.irradiance = &sh, .intensity = 0.5f, .lastLevel = 5.0f});
    REQUIRE(uniforms.environment == glm::vec4(1.0f, 0.5f, 5.0f, 0.0f));
    REQUIRE(uniforms.irradiance[0] == glm::vec4(1.0f, 2.0f, 3.0f, 0.0f));
    REQUIRE(uniforms.irradiance[8] == glm::vec4(-1.0f, 0.5f, 0.0f, 0.0f));
}

TEST_CASE("The inverse view-projection takes the far plane back to directions of the view", "[graphics]") {
    RenderScene scene;
    scene.camera.verticalFovRadians = glm::radians(90.0f);
    const FrameUniforms uniforms = makeFrameUniforms(scene, 1.0f, 2048);
    const glm::vec4 far = uniforms.inverseViewProjection * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    const glm::vec3 direction = glm::normalize(glm::vec3(far) / far.w);
    REQUIRE_THAT(direction.z, WithinAbs(-1.0, kTolerance));
    // Top of the screen (Vulkan's -1) is up.
    const glm::vec4 top = uniforms.inverseViewProjection * glm::vec4(0.0f, -1.0f, 1.0f, 1.0f);
    REQUIRE(top.y / top.w > 0.0f);
}
