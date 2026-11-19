#include "Graphics/MaterialTable.h"

#include "Core/Assert.h"
#include "Resources/FallbackAssets.h"

namespace apex::graphics {

namespace {

[[nodiscard]] MaterialGpu toGpu(const resources::MaterialData& material) noexcept {
    return MaterialGpu{.baseColor = material.baseColorFactor,
                       .metallic = material.metallicFactor,
                       .roughness = material.roughnessFactor,
                       .padding = {}};
}

} // namespace

MaterialTable::MaterialTable(std::uint32_t capacity) : m_capacity(capacity) {
    APEX_ASSERT(capacity >= 1, "The material table needs room for the default material");
    m_gpu.push_back(toGpu(resources::makeDefaultMaterial()));
}

std::size_t
MaterialTable::sync(const resources::ResourcePool<resources::MaterialHandle, resources::MaterialData>& pool) {
    if (pool.revision() == m_revision) {
        return 0;
    }
    m_revision = pool.revision();
    m_gpu.resize(1);
    m_generations.clear();
    std::size_t leftOut = 0;
    pool.forEach([&](resources::MaterialHandle handle, resources::ResourceState state,
                     const resources::MaterialData* material) {
        if (state != resources::ResourceState::Ready) {
            return;
        }
        const std::size_t entry = std::size_t{handle.index} + 1;
        if (entry >= m_capacity) {
            ++leftOut;
            return;
        }
        if (entry >= m_gpu.size()) {
            m_gpu.resize(entry + 1, m_gpu.front());
            m_generations.resize(entry, kNoGeneration);
        }
        m_gpu[entry] = toGpu(*material);
        m_generations[handle.index] = handle.generation;
    });
    return leftOut;
}

std::uint32_t MaterialTable::gpuIndex(resources::MaterialHandle handle) const noexcept {
    const bool ready = handle.index < m_generations.size() &&
                       m_generations[handle.index] == handle.generation && handle.generation != kNoGeneration;
    return ready ? handle.index + 1 : kDefaultIndex;
}

std::span<const MaterialGpu> MaterialTable::gpuData() const noexcept {
    return m_gpu;
}

} // namespace apex::graphics
