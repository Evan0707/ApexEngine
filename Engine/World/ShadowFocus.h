#pragma once

#include "Core/Math.h"
#include "Core/WorldPosition.h"
#include "Graphics/RenderScene.h"

#include <entt/entity/registry.hpp>

#include <cstdint>

namespace apex::world {

// How the shadow area is framed and biased (spec 5.4, decision 0021).
struct ShadowSettings {
    bool enabled = true;
    // Pixels per side of the shadow map: the area's center moves by whole texels.
    std::uint32_t mapSize = 2048;
    // Meters: the side of the square area, and its depth along the light, centered on the focus.
    float areaSize = 40.0f;
    float depthRange = 100.0f;
    // Depth bias of the shadow pass: constant in meters, slope factor in units of the depth slope.
    float constantBias = 0.02f;
    float slopeBias = 2.0f;
};

// World to the light's view orientation: rows right, up and directionToLight, so that the view looks along
// the light, towards -Z. Up is +Y in the world, or -Z for a vertical light.
[[nodiscard]] glm::mat3 lightViewRotation(const glm::vec3& directionToLight) noexcept;

// The shadow area centered on `focus`, in render space relative to `cameraPosition` (G-07). Across the light,
// the center moves to the nearest multiple of a texel in world space: the texels stay put when the focus
// moves, and shadow edges do not shimmer.
[[nodiscard]] graphics::RenderShadow frameShadow(const glm::vec3& directionToLight,
                                                 const core::WorldPosition& focus,
                                                 const core::WorldPosition& cameraPosition,
                                                 const ShadowSettings& settings) noexcept;

// After extractRenderScene: frames scene.shadow on the interpolated position of `focus` (the character), or
// on the camera's when `focus` has no Transform. Off when settings.enabled is false or the scene has no
// light.
void extractShadow(const entt::registry& registry, entt::entity camera, entt::entity focus, float alpha,
                   const ShadowSettings& settings, graphics::RenderScene& scene);

} // namespace apex::world
