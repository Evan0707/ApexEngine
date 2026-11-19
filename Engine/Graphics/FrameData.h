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
    // xyz: unit vector towards the light, in render space (world orientation).
    glm::vec4 directionToLight;
    // rgb: light color times intensity.
    glm::vec4 lightRadiance;
    // rgb: constant ambient term.
    glm::vec4 ambient;
};
static_assert(sizeof(FrameUniforms) == 112);
static_assert(offsetof(FrameUniforms, directionToLight) == 64);
static_assert(offsetof(FrameUniforms, ambient) == 96);

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

// aspectRatio: width over height of the render target.
[[nodiscard]] FrameUniforms makeFrameUniforms(const RenderScene& scene, float aspectRatio) noexcept;

[[nodiscard]] ObjectGpu makeObjectGpu(const glm::mat4& model, std::uint32_t materialIndex) noexcept;

} // namespace apex::graphics
