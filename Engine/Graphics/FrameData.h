#pragma once

#include "Core/Math.h"
#include "Graphics/RenderScene.h"

#include <cstddef>
#include <cstdint>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. The C++ side of Shaders/Frame.glsl: same layout, checked by
// the static_asserts below and by tests.

// Uniform buffer, std140.
struct FrameUniforms {
    glm::mat4 viewProjection;
    // Render space to the shadow map's clip space (decision 0021).
    glm::mat4 lightViewProjection;
    // xyz: unit vector towards the light, in render space (world orientation).
    glm::vec4 directionToLight;
    // rgb: light color times intensity.
    glm::vec4 lightRadiance;
    // rgb: constant ambient term.
    glm::vec4 ambient;
    // x: 1 when shadows are on. y: one texel of the shadow map, in texture coordinates. z: where the fading
    // towards the border starts, as a fraction of the half side.
    glm::vec4 shadow;
};
static_assert(sizeof(FrameUniforms) == 192);
static_assert(offsetof(FrameUniforms, lightViewProjection) == 64);
static_assert(offsetof(FrameUniforms, directionToLight) == 128);
static_assert(offsetof(FrameUniforms, ambient) == 160);
static_assert(offsetof(FrameUniforms, shadow) == 176);

// One element of the per-object storage buffer, std430, read with gl_InstanceIndex (G-02).
struct ObjectGpu {
    // Object to render space.
    glm::mat4 model;
    // Inverse transpose of the model's upper 3x3, stored as a mat4 to keep the std430 layout simple.
    glm::mat4 normalMatrix;
    std::uint32_t materialIndex;
    std::uint32_t padding[3];
};
static_assert(sizeof(ObjectGpu) == 144);
static_assert(offsetof(ObjectGpu, normalMatrix) == 64);
static_assert(offsetof(ObjectGpu, materialIndex) == 128);

// Shadows fade out over the last tenth of the area (decision 0021).
inline constexpr float kShadowFadeStart = 0.9f;

// aspectRatio: width over height of the render target. shadowMapSize: pixels per side.
[[nodiscard]] FrameUniforms makeFrameUniforms(const RenderScene& scene, float aspectRatio,
                                              std::uint32_t shadowMapSize) noexcept;

// The light's view and projection of the shadow area: render space to the shadow map's clip space.
[[nodiscard]] glm::mat4 shadowViewProjection(const RenderShadow& shadow) noexcept;

[[nodiscard]] ObjectGpu makeObjectGpu(const glm::mat4& model, std::uint32_t materialIndex) noexcept;

} // namespace apex::graphics
