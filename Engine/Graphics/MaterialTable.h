#pragma once

#include "Core/Error.h"
#include "Core/Math.h"
#include "Graphics/GpuHandles.h"
#include "Resources/MaterialData.h"

#include <cstddef>
#include <cstdint>
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

// Internal to Graphics, without Vulkan. The materials, in the order of the table copied to the GPU each
// frame. Index 0 is the default material: it cannot be removed and replaces any stale handle.
class MaterialTable {
public:
    static constexpr std::uint32_t kDefaultIndex = 0;

    // capacity counts the default material.
    explicit MaterialTable(std::uint32_t capacity);

    // Fails when the table is full or a factor is outside [0, 1].
    [[nodiscard]] core::Result<MaterialHandle> add(const resources::MaterialData& material);
    // False for a stale handle. The slot is reused: a frame already copied its own table.
    bool remove(MaterialHandle handle) noexcept;

    // kDefaultIndex for a stale or default handle.
    [[nodiscard]] std::uint32_t gpuIndex(MaterialHandle handle) const noexcept;
    // Every slot up to the highest one in use; free slots keep stale data that no object refers to.
    [[nodiscard]] std::span<const MaterialGpu> gpuData() const noexcept;
    // Materials added and not removed, without the default one.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    [[nodiscard]] bool isLive(MaterialHandle handle) const noexcept;

    std::uint32_t m_capacity;
    std::vector<MaterialGpu> m_gpu;
    std::vector<std::uint32_t> m_generations;
    std::vector<bool> m_live;
    std::vector<std::uint32_t> m_free;
    std::size_t m_size = 0;
};

} // namespace apex::graphics
