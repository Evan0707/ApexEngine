#include "Graphics/MaterialTable.h"

#include "Core/Assert.h"

#include <cmath>
#include <format>

namespace apex::graphics {

namespace {

[[nodiscard]] bool isFactor(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

[[nodiscard]] MaterialGpu toGpu(const resources::MaterialData& material) noexcept {
    return MaterialGpu{.baseColor = material.baseColorFactor,
                       .metallic = material.metallicFactor,
                       .roughness = material.roughnessFactor,
                       .padding = {}};
}

} // namespace

MaterialTable::MaterialTable(std::uint32_t capacity) : m_capacity(capacity) {
    APEX_ASSERT(capacity >= 1, "The material table needs room for the default material");
    m_gpu.push_back(toGpu(resources::MaterialData{}));
    m_generations.push_back(1);
    m_live.push_back(false);
}

core::Result<MaterialHandle> MaterialTable::add(const resources::MaterialData& material) {
    if (!isFactor(material.metallicFactor) || !isFactor(material.roughnessFactor)) {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("Material factors must be in [0, 1]: metallic {}, roughness {}",
                                           material.metallicFactor, material.roughnessFactor));
    }
    std::uint32_t index = 0;
    if (!m_free.empty()) {
        index = m_free.back();
        m_free.pop_back();
        m_gpu[index] = toGpu(material);
    } else if (m_gpu.size() < m_capacity) {
        index = static_cast<std::uint32_t>(m_gpu.size());
        m_gpu.push_back(toGpu(material));
        m_generations.push_back(1);
        m_live.push_back(false);
    } else {
        return core::makeError(core::ErrorCode::InvalidArgument,
                               std::format("The material table is full ({} materials)", m_capacity - 1));
    }
    m_live[index] = true;
    ++m_size;
    return MaterialHandle{index, m_generations[index]};
}

bool MaterialTable::remove(MaterialHandle handle) noexcept {
    if (!isLive(handle)) {
        return false;
    }
    m_live[handle.index] = false;
    ++m_generations[handle.index];
    m_free.push_back(handle.index);
    --m_size;
    return true;
}

std::uint32_t MaterialTable::gpuIndex(MaterialHandle handle) const noexcept {
    return isLive(handle) ? handle.index : kDefaultIndex;
}

std::span<const MaterialGpu> MaterialTable::gpuData() const noexcept {
    return m_gpu;
}

std::size_t MaterialTable::size() const noexcept {
    return m_size;
}

bool MaterialTable::isLive(MaterialHandle handle) const noexcept {
    return handle.index < m_live.size() && m_live[handle.index] &&
           m_generations[handle.index] == handle.generation;
}

} // namespace apex::graphics
