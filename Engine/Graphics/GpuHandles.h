#pragma once

#include <cstdint>
#include <limits>

namespace apex::graphics {

// Designates a GPU object owned by the renderer. The generation makes a handle stale once its object
// is destroyed, even if the slot is reused. A default handle is never valid.
template <typename Tag>
struct GpuHandle {
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;

    [[nodiscard]] bool operator==(const GpuHandle&) const noexcept = default;
};

using MeshHandle = GpuHandle<struct MeshTag>;
using MaterialHandle = GpuHandle<struct MaterialTag>;

} // namespace apex::graphics
