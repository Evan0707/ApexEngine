#include "Graphics/GpuMesh.h"

#include <array>
#include <format>
#include <span>

namespace apex::graphics {

core::Result<GpuMesh> createGpuMesh(GpuAllocator& allocator, UploadContext& upload, std::string_view name,
                                    const resources::MeshData& mesh) {
    if (core::Result<void> valid = resources::validate(mesh, name); !valid) {
        return std::unexpected(valid.error());
    }
    const std::span<const std::byte> vertexBytes = std::as_bytes(std::span(mesh.vertices));
    const std::span<const std::byte> indexBytes = std::as_bytes(std::span(mesh.indices));

    GpuMesh gpuMesh;
    gpuMesh.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
    core::Result<GpuBuffer> vertices =
        allocator.createDeviceBuffer(std::format("{} (vertices)", name), vertexBytes.size(),
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (!vertices) {
        return std::unexpected(vertices.error());
    }
    gpuMesh.vertices = *vertices;
    core::Result<GpuBuffer> indices =
        allocator.createDeviceBuffer(std::format("{} (indices)", name), indexBytes.size(),
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (!indices) {
        allocator.destroyBuffer(gpuMesh.vertices);
        return std::unexpected(indices.error());
    }
    gpuMesh.indices = *indices;

    const std::array copies{BufferCopy{gpuMesh.vertices.buffer, vertexBytes},
                            BufferCopy{gpuMesh.indices.buffer, indexBytes}};
    if (core::Result<void> uploaded = upload.upload(allocator, name, copies); !uploaded) {
        destroyGpuMesh(allocator, gpuMesh);
        return std::unexpected(uploaded.error());
    }
    return gpuMesh;
}

void destroyGpuMesh(GpuAllocator& allocator, const GpuMesh& mesh) noexcept {
    allocator.destroyBuffer(mesh.vertices);
    allocator.destroyBuffer(mesh.indices);
}

} // namespace apex::graphics
