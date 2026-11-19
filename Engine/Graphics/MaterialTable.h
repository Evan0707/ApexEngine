#pragma once

#include "Core/Math.h"
#include "Resources/Handles.h"
#include "Resources/MaterialData.h"
#include "Resources/ResourcePool.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace apex::graphics {

// One entry of the material table read by the shaders (decision 0011), std430 layout: see Material.glsl.
struct MaterialGpu {
    glm::vec4 baseColor;
    float metallic;
    float roughness;
    float padding[2];
};
static_assert(sizeof(MaterialGpu) == 32);

// Internal to Graphics, without Vulkan. The factors of the cache's materials, in the order of the table
// copied to the GPU each frame: material slot i of the cache is entry i + 1. Entry 0 is the default
// material, given to any material that is not ready, stale, or beyond the capacity (G-03).
class MaterialTable {
public:
    static constexpr std::uint32_t kDefaultIndex = 0;

    // capacity counts the default material.
    explicit MaterialTable(std::uint32_t capacity);

    // Rebuilds the table when the pool changed. Returns the number of ready materials left out for lack
    // of room (they get the default material).
    std::size_t sync(const resources::ResourcePool<resources::MaterialHandle, resources::MaterialData>& pool);

    [[nodiscard]] std::uint32_t gpuIndex(resources::MaterialHandle handle) const noexcept;
    [[nodiscard]] std::span<const MaterialGpu> gpuData() const noexcept;

private:
    static constexpr std::uint32_t kNoGeneration = 0;

    std::uint32_t m_capacity;
    std::vector<MaterialGpu> m_gpu;
    // Generation of the ready material in each cache slot, kNoGeneration otherwise.
    std::vector<std::uint32_t> m_generations;
    std::uint64_t m_revision = std::numeric_limits<std::uint64_t>::max();
};

} // namespace apex::graphics
