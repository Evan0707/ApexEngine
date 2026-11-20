#include "Graphics/FrameData.h"
#include "World/RenderExtraction.h"
#include "World/ShadowFocus.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace apex::world;
using apex::graphics::RenderScene;
using apex::graphics::RenderShadow;
using apex::graphics::shadowViewProjection;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-4f;
// The demo's sun.
const glm::vec3 kTowardsSun = glm::normalize(glm::vec3(0.4f, 1.0f, 0.35f));
const ShadowSettings kSettings{.mapSize = 2048, .areaSize = 40.0f, .depthRange = 100.0f};
const float kTexel = kSettings.areaSize / static_cast<float>(kSettings.mapSize);

// Where a world point lands in the shadow map's clip space.
[[nodiscard]] glm::vec3 shadowNdc(const RenderShadow& shadow, const glm::vec3& world,
                                  const glm::vec3& camera) {
    const glm::vec4 clip = shadowViewProjection(shadow) * glm::vec4(world - camera, 1.0f);
    return glm::vec3(clip) / clip.w;
}

// The point the light's view sees at the center of the area, in the light's view coordinates.
[[nodiscard]] glm::vec3 inLightView(const RenderShadow& shadow, const glm::vec3& world,
                                    const glm::vec3& camera) {
    return glm::vec3(shadow.lightView * glm::vec4(world - camera, 1.0f));
}

} // namespace

TEST_CASE("The light's view rotation looks along the light", "[world]") {
    const glm::mat3 rotation = lightViewRotation(kTowardsSun);
    // Towards the light is +Z of the view: the view looks the other way, as cameras do.
    const glm::vec3 towardsLight = rotation * kTowardsSun;
    REQUIRE_THAT(towardsLight.z, WithinAbs(1.0, kTolerance));
    REQUIRE_THAT(glm::determinant(rotation), WithinAbs(1.0, kTolerance));
}

TEST_CASE("A vertical light still gives a rotation", "[world]") {
    for (const glm::vec3 direction : {glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)}) {
        const glm::mat3 rotation = lightViewRotation(direction);
        REQUIRE_THAT(glm::determinant(rotation), WithinAbs(1.0, kTolerance));
        REQUIRE_THAT((rotation * direction).z, WithinAbs(1.0, kTolerance));
    }
}

TEST_CASE("The focus lands in the middle of the shadow map", "[world]") {
    const glm::vec3 focus{3.0f, 0.0f, -7.0f};
    const glm::vec3 camera{0.0f, 2.0f, 8.0f};
    const RenderShadow shadow = frameShadow(kTowardsSun, focus, camera, kSettings);
    REQUIRE(shadow.enabled);
    REQUIRE(shadow.halfExtent == 20.0f);
    const glm::vec3 ndc = shadowNdc(shadow, focus, camera);
    // Snapped to the texel grid: at most half a texel away from the center, in clip units.
    REQUIRE(std::abs(ndc.x) <= kTexel / shadow.halfExtent);
    REQUIRE(std::abs(ndc.y) <= kTexel / shadow.halfExtent);
    REQUIRE_THAT(ndc.z, WithinAbs(0.5, kTolerance));
}

TEST_CASE("The whole area around the focus fits in the shadow map", "[world]") {
    const glm::vec3 focus{0.0f};
    const RenderShadow shadow = frameShadow(kTowardsSun, focus, glm::vec3(0.0f), kSettings);
    const glm::mat3 rotation = lightViewRotation(kTowardsSun);
    const glm::vec3 right = glm::transpose(rotation)[0];
    const glm::vec3 up = glm::transpose(rotation)[1];
    for (const float x : {-19.0f, 19.0f}) {
        for (const float y : {-19.0f, 19.0f}) {
            const glm::vec3 ndc = shadowNdc(shadow, focus + right * x + up * y, glm::vec3(0.0f));
            REQUIRE(std::abs(ndc.x) < 1.0f);
            REQUIRE(std::abs(ndc.y) < 1.0f);
        }
    }
}

TEST_CASE("Depth grows away from the light", "[world]") {
    const glm::vec3 focus{0.0f, 1.0f, 0.0f};
    const RenderShadow shadow = frameShadow(kTowardsSun, focus, glm::vec3(0.0f), kSettings);
    const float atFocus = shadowNdc(shadow, focus, glm::vec3(0.0f)).z;
    const float nearer = shadowNdc(shadow, focus + kTowardsSun * 10.0f, glm::vec3(0.0f)).z;
    const float farther = shadowNdc(shadow, focus - kTowardsSun * 10.0f, glm::vec3(0.0f)).z;
    REQUIRE(nearer < atFocus);
    REQUIRE(farther > atFocus);
    REQUIRE(nearer > 0.0f);
    REQUIRE(farther < 1.0f);
}

TEST_CASE("The shadow texels stay put while the focus moves less than a texel", "[world]") {
    const glm::mat3 rotation = lightViewRotation(kTowardsSun);
    const glm::vec3 right = glm::transpose(rotation)[0];
    // On the texel grid, then a third of a texel across the light.
    const glm::vec3 onGrid = glm::transpose(rotation) * glm::vec3(10.0f * kTexel, -4.0f * kTexel, 2.0f);
    const RenderShadow before = frameShadow(kTowardsSun, onGrid, glm::vec3(0.0f), kSettings);
    const RenderShadow after =
        frameShadow(kTowardsSun, onGrid + right * (kTexel / 3.0f), glm::vec3(0.0f), kSettings);
    const glm::vec3 point{5.0f, 0.0f, 5.0f};
    const glm::vec3 a = inLightView(before, point, glm::vec3(0.0f));
    const glm::vec3 b = inLightView(after, point, glm::vec3(0.0f));
    REQUIRE_THAT(a.x, WithinAbs(b.x, kTolerance));
    REQUIRE_THAT(a.y, WithinAbs(b.y, kTolerance));

    // A whole texel: everything shifts by exactly one texel.
    const RenderShadow moved = frameShadow(kTowardsSun, onGrid + right * kTexel, glm::vec3(0.0f), kSettings);
    REQUIRE_THAT(inLightView(moved, point, glm::vec3(0.0f)).x, WithinAbs(a.x - kTexel, kTolerance));
}

TEST_CASE("The shadow area does not depend on where the camera is", "[world]") {
    const glm::vec3 focus{2.0f, 0.0f, 1.0f};
    const glm::vec3 point{-3.0f, 0.5f, 4.0f};
    const glm::vec3 cameraA{0.0f, 2.0f, 8.0f};
    const glm::vec3 cameraB{-30.0f, 5.0f, 12.0f};
    const glm::vec3 a = inLightView(frameShadow(kTowardsSun, focus, cameraA, kSettings), point, cameraA);
    const glm::vec3 b = inLightView(frameShadow(kTowardsSun, focus, cameraB, kSettings), point, cameraB);
    REQUIRE_THAT(a.x, WithinAbs(b.x, kTolerance));
    REQUIRE_THAT(a.y, WithinAbs(b.y, kTolerance));
    REQUIRE_THAT(a.z, WithinAbs(b.z, kTolerance));
}

TEST_CASE("The extraction frames the shadow on the focus entity, or under the camera", "[world]") {
    entt::registry registry;
    const entt::entity camera = registry.create();
    registry.emplace<Transform>(camera, Transform{.position = {0.0f, 2.0f, 8.0f}});
    registry.emplace<Camera>(camera);
    const entt::entity player = registry.create();
    registry.emplace<Transform>(player, Transform{.position = {6.0f, 0.0f, -2.0f}});
    RenderScene scene;
    scene.light.directionToLight = kTowardsSun;
    scene.light.intensity = 3.0f;

    extractShadow(registry, camera, player, 1.0f, kSettings, scene);
    REQUIRE(scene.shadow.enabled);
    const glm::vec3 onPlayer = shadowNdc(scene.shadow, {6.0f, 0.0f, -2.0f}, {0.0f, 2.0f, 8.0f});
    REQUIRE(std::abs(onPlayer.x) <= kTexel / 20.0f);

    extractShadow(registry, camera, entt::null, 1.0f, kSettings, scene);
    const glm::vec3 underCamera = shadowNdc(scene.shadow, {0.0f, 2.0f, 8.0f}, {0.0f, 2.0f, 8.0f});
    REQUIRE(std::abs(underCamera.x) <= kTexel / 20.0f);
    REQUIRE(std::abs(underCamera.y) <= kTexel / 20.0f);
}

TEST_CASE("No shadow without light or when turned off", "[world]") {
    entt::registry registry;
    RenderScene scene;
    scene.light.intensity = 0.0f;
    extractShadow(registry, entt::null, entt::null, 1.0f, kSettings, scene);
    REQUIRE_FALSE(scene.shadow.enabled);

    scene.light.intensity = 1.0f;
    ShadowSettings off = kSettings;
    off.enabled = false;
    extractShadow(registry, entt::null, entt::null, 1.0f, off, scene);
    REQUIRE_FALSE(scene.shadow.enabled);
}
