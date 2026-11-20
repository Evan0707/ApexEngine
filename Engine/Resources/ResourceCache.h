#pragma once

#include "Core/Logger.h"
#include "Resources/AssetPath.h"
#include "Resources/EnvironmentData.h"
#include "Resources/Handles.h"
#include "Resources/MaterialData.h"
#include "Resources/MeshData.h"
#include "Resources/ModelData.h"
#include "Resources/ResourcePool.h"
#include "Resources/TextureData.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace apex::resources {

// One part of a loaded model: a mesh drawn with a material.
struct ModelPartRef {
    MeshHandle mesh;
    MaterialHandle material;
};

struct Model {
    std::vector<ModelPartRef> parts;
};

// Owns every CPU resource (spec 5.5, decision 0014): loads each file once, keyed by its normalized asset
// path, and hands out stateful handles (G-03). Loading is synchronous in phase 4. A load never fails
// from the caller's point of view: the handle's state says Failed, the error is logged with the path,
// and readers get the fallback assets. Not thread-safe.
class ResourceCache {
public:
    // assetsRoot: the folder asset paths are relative to. The logger must outlive the cache.
    ResourceCache(std::filesystem::path assetsRoot, core::Logger& logger);

    [[nodiscard]] ModelHandle loadModel(std::string_view path);
    // A texture used both as a color and as data is loaded twice, once per color space.
    [[nodiscard]] TextureHandle loadTexture(std::string_view path, ColorSpace colorSpace);
    // A Radiance HDR image (.hdr): the light around the scene (decision 0022). No fallback: whoever reads a
    // handle that is not ready lights the scene with the constant ambient term.
    [[nodiscard]] EnvironmentHandle loadEnvironment(std::string_view path);

    // Resources made in code (primitives, materials of the demo): not keyed by a path. An invalid mesh
    // or material is Failed, with a log line naming it.
    [[nodiscard]] MeshHandle addMesh(std::string_view name, MeshData mesh);
    [[nodiscard]] MaterialHandle addMaterial(std::string_view name, const MaterialData& material);
    [[nodiscard]] ModelHandle addModel(std::span<const ModelPartRef> parts);

    // The asset path a model was loaded from. Nothing for a stale handle or a model made in code.
    [[nodiscard]] std::optional<AssetPath> modelPath(ModelHandle model) const;
    // The asset path an environment was loaded from, failed or not. Nothing for a stale or default handle.
    [[nodiscard]] std::optional<AssetPath> environmentPath(EnvironmentHandle environment) const;
    // The folder asset paths are relative to.
    [[nodiscard]] const std::filesystem::path& root() const noexcept;

    [[nodiscard]] ResourceState state(ModelHandle model) const noexcept;
    [[nodiscard]] ResourceState state(TextureHandle texture) const noexcept;
    [[nodiscard]] ResourceState state(EnvironmentHandle environment) const noexcept;
    // The parts of a ready model; the fallback model's otherwise: a 1 m cube in the magenta checker.
    [[nodiscard]] std::span<const ModelPartRef> parts(ModelHandle model) const noexcept;

    // Built in (spec 5.5), always ready, recreated by unloadAll.
    [[nodiscard]] MeshHandle fallbackMesh() const noexcept;
    [[nodiscard]] MaterialHandle defaultMaterial() const noexcept;
    // The default material with the checker: the fallback model's, impossible to miss (AC-11).
    [[nodiscard]] MaterialHandle fallbackMaterial() const noexcept;
    // Given for a texture that is not ready or failed.
    [[nodiscard]] TextureHandle checkerTexture() const noexcept;
    // Given for a material without a texture.
    [[nodiscard]] TextureHandle whiteTexture() const noexcept;
    // Given for a material without a normal map, or whose normal map is not ready or failed (decision 0021).
    [[nodiscard]] TextureHandle flatNormalTexture() const noexcept;

    // Read by the renderer to upload what became ready and release what was unloaded.
    [[nodiscard]] const ResourcePool<MeshHandle, MeshData>& meshes() const noexcept;
    [[nodiscard]] const ResourcePool<TextureHandle, TextureData>& textures() const noexcept;
    [[nodiscard]] const ResourcePool<MaterialHandle, MaterialData>& materials() const noexcept;
    [[nodiscard]] const ResourcePool<EnvironmentHandle, EnvironmentData>& environments() const noexcept;

    // Scene unloading (spec 5.5): every handle given so far becomes stale.
    void unloadAll();

    // Files read since construction: each path once, however many times it is requested.
    [[nodiscard]] std::size_t filesRead() const noexcept;

private:
    void createFallbacks();
    [[nodiscard]] TextureHandle addDecodedTexture(std::string_view name, std::span<const std::byte> bytes,
                                                  ColorSpace colorSpace);
    [[nodiscard]] TextureHandle imageTexture(const AssetPath& model, const ModelData& data,
                                             std::uint32_t image, ColorSpace colorSpace);
    [[nodiscard]] MaterialHandle modelMaterial(const AssetPath& model, const ModelData& data,
                                               std::uint32_t material);

    std::filesystem::path m_root;
    core::Logger* m_logger;
    ResourcePool<MeshHandle, MeshData> m_meshes;
    ResourcePool<TextureHandle, TextureData> m_textures;
    ResourcePool<MaterialHandle, MaterialData> m_materials;
    ResourcePool<ModelHandle, Model> m_models;
    ResourcePool<EnvironmentHandle, EnvironmentData> m_environments;
    std::map<AssetPath, EnvironmentHandle> m_environmentPaths;
    std::map<AssetPath, ModelHandle> m_modelPaths;
    std::map<std::pair<AssetPath, ColorSpace>, TextureHandle> m_texturePaths;
    Model m_fallbackModel;
    MaterialHandle m_defaultMaterial;
    TextureHandle m_checker;
    TextureHandle m_white;
    TextureHandle m_flatNormal;
    std::size_t m_filesRead = 0;
};

} // namespace apex::resources
