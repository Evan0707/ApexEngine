#pragma once

#include <cstdint>
#include <limits>

namespace apex::resources {

// Designates a resource of the cache (G-03). The generation makes a handle stale once its resource is
// unloaded, even if the slot is reused. A default handle is never valid.
template <typename Tag>
struct Handle {
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t generation = 0;

    [[nodiscard]] bool operator==(const Handle&) const noexcept = default;
};

using MeshHandle = Handle<struct MeshTag>;
using TextureHandle = Handle<struct TextureTag>;
using MaterialHandle = Handle<struct MaterialTag>;
using ModelHandle = Handle<struct ModelTag>;

// Spec 5.5. A stale handle reads as Failed: whoever uses it gets the fallback asset.
enum class ResourceState { Loading, Ready, Failed };

} // namespace apex::resources
