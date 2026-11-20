#pragma once

#include "Core/Error.h"
#include "Graphics/GpuAllocator.h"
#include "Graphics/UploadContext.h"
#include "Resources/MeshData.h"

#include <cstdint>
#include <string_view>

namespace apex::graphics {

// Internal to Graphics. A mesh in device-local memory, in the vertex format of resources::Vertex.
struct GpuMesh {
    GpuBuffer vertices;
    GpuBuffer indices;
    std::uint32_t indexCount = 0;
    // In the mesh's own space: frustum culling (decision 0021).
    resources::BoundingSphere bounds;
};

// Validates the mesh, then uploads it. The error names the mesh (spec 5.4).
[[nodiscard]] core::Result<GpuMesh> createGpuMesh(GpuAllocator& allocator, UploadContext& upload,
                                                  std::string_view name, const resources::MeshData& mesh);

// Immediate: the caller makes sure no frame in flight still draws the mesh (DeferredDeleter).
void destroyGpuMesh(GpuAllocator& allocator, const GpuMesh& mesh) noexcept;

} // namespace apex::graphics
