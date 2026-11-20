#include "Core/Profiling.h"
#include "Graphics/RendererState.h"

#include <chrono>
#include <cstddef>
#include <optional>

namespace apex::graphics {

void Renderer::syncResources(const resources::ResourceCache& cache) {
    State& state = *m_state;
    APEX_PROFILE_SCOPE("Renderer::syncResources");
    state.fallbackMesh = cache.fallbackMesh();
    std::size_t uploadedMeshes = 0;
    std::size_t uploadedTextures = 0;
    state.meshes.sync(
        cache.meshes(),
        [&state, &uploadedMeshes](resources::MeshHandle,
                                  const resources::MeshData& mesh) -> std::optional<GpuMesh> {
            core::Result<GpuMesh> created = createGpuMesh(*state.allocator, state.upload, mesh.name, mesh);
            if (!created) {
                state.logger->error("{}", created.error().message);
                return std::nullopt;
            }
            ++uploadedMeshes;
            return *created;
        },
        [&state](GpuMesh&& mesh) {
            // A frame in flight may still draw it.
            state.deleter.enqueue(
                [allocator = state.allocator.get(), mesh] { destroyGpuMesh(*allocator, mesh); });
        });
    state.textures.sync(
        cache.textures(),
        [&state, &uploadedTextures](resources::TextureHandle,
                                    const resources::TextureData& texture) -> std::optional<GpuTexture> {
            core::Result<GpuTexture> created = createGpuTexture(state.device->device(), *state.allocator,
                                                                state.upload, texture.name, texture);
            if (!created) {
                state.logger->error("{}", created.error().message);
                return std::nullopt;
            }
            ++uploadedTextures;
            return *created;
        },
        [&state](GpuTexture&& texture) {
            state.deleter.enqueue([device = state.device->device(), allocator = state.allocator.get(),
                                   texture] { destroyGpuTexture(device, *allocator, texture); });
        });
    state.environments.sync(
        cache.environments(),
        [&state](resources::EnvironmentHandle,
                 const resources::EnvironmentData& environment) -> std::optional<GpuEnvironment> {
            const auto start = std::chrono::steady_clock::now();
            core::Result<GpuEnvironment> created =
                createGpuEnvironment(state.device->device(), *state.allocator, state.upload, state.prefilter,
                                     state.lightingSets, state.brdfTable.view, environment.name, environment);
            if (!created) {
                // The constant ambient term lights the scene instead (decision 0022).
                state.logger->error("Cannot upload environment '{}': {}", environment.name,
                                    created.error().message);
                return std::nullopt;
            }
            const std::chrono::duration<double, std::milli> elapsed =
                std::chrono::steady_clock::now() - start;
            state.logger->info("GPU upload: environment '{}', {}x{} pixels, prefiltered in {:.0f} ms",
                               environment.name, environment.width, environment.height, elapsed.count());
            return *created;
        },
        [&state](GpuEnvironment&& environment) {
            state.deleter.enqueue([&state, environment] {
                destroyGpuEnvironment(state.device->device(), *state.allocator, state.lightingSets,
                                      environment);
            });
        });
    state.syncMaterialSets(cache);
    if (uploadedMeshes + uploadedTextures > 0) {
        const GpuMemoryStats memory = state.allocator->memoryStats();
        state.logger->info("GPU upload: {} mesh(es), {} texture(s); GPU memory {:.0f} MiB used",
                           uploadedMeshes, uploadedTextures,
                           static_cast<double>(memory.usedBytes) / (1024.0 * 1024.0));
    }
    if (const std::size_t leftOut = state.materials.sync(cache.materials()); leftOut > 0) {
        state.logger->warning("{} material(s) beyond the table's {} entries get the default material",
                              leftOut, kMaxMaterials);
    }
}

VkImageView Renderer::State::resolveTexture(resources::TextureHandle texture,
                                            resources::TextureHandle checker,
                                            resources::TextureHandle white) const noexcept {
    const resources::TextureHandle wanted = texture == resources::TextureHandle{} ? white : texture;
    if (const GpuTexture* found = textures.find(wanted)) {
        return found->view;
    }
    const GpuTexture* fallback = textures.find(checker);
    return fallback != nullptr ? fallback->view : VK_NULL_HANDLE;
}

void Renderer::State::syncMaterialSets(const resources::ResourceCache& cache) {
    const auto freeLater = [this](VkDescriptorSet&& set) {
        deleter.enqueue([this, set] { materialSets.free(set); });
    };
    if (cache.textures().revision() != textureRevision) {
        textureRevision = cache.textures().revision();
        materialSetMirror.releaseAll(freeLater);
        if (defaultMaterialSet) {
            freeLater(std::move(*defaultMaterialSet));
        }
        const VkImageView white = resolveTexture({}, cache.checkerTexture(), cache.whiteTexture());
        defaultMaterialSet = white != VK_NULL_HANDLE ? materialSets.allocate(white, white) : std::nullopt;
    }
    materialSetMirror.sync(
        cache.materials(),
        [this, &cache](resources::MaterialHandle,
                       const resources::MaterialData& material) -> std::optional<VkDescriptorSet> {
            const VkImageView baseColor =
                resolveTexture(material.baseColorTexture, cache.checkerTexture(), cache.whiteTexture());
            const VkImageView metallicRoughness = resolveTexture(
                material.metallicRoughnessTexture, cache.checkerTexture(), cache.whiteTexture());
            if (baseColor == VK_NULL_HANDLE || metallicRoughness == VK_NULL_HANDLE) {
                return std::nullopt;
            }
            std::optional<VkDescriptorSet> set = materialSets.allocate(baseColor, metallicRoughness);
            if (!set) {
                logger->warning("No room for another material descriptor set: the default material is used");
            }
            return set;
        },
        freeLater);
}

} // namespace apex::graphics
