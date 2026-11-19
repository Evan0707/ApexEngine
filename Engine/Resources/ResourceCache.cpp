#include "Resources/ResourceCache.h"

#include "Core/Profiling.h"
#include "Resources/FallbackAssets.h"
#include "Resources/FileReader.h"
#include "Resources/GltfImporter.h"
#include "Resources/ImageDecoder.h"

#include <cmath>
#include <format>
#include <string>

namespace apex::resources {

namespace {

[[nodiscard]] bool isFactor(float value) noexcept {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

} // namespace

ResourceCache::ResourceCache(std::filesystem::path assetsRoot, core::Logger& logger)
    : m_root(std::move(assetsRoot)), m_logger(&logger) {
    createFallbacks();
}

void ResourceCache::createFallbacks() {
    m_checker = m_textures.add(makeCheckerTexture());
    m_white = m_textures.add(makeWhiteTexture(ColorSpace::Srgb));
    m_defaultMaterial = m_materials.add(makeDefaultMaterial());
    MaterialData fallback = makeDefaultMaterial();
    fallback.baseColorFactor = glm::vec4(1.0f);
    fallback.baseColorTexture = m_checker;
    m_fallbackModel.parts = {
        ModelPartRef{.mesh = m_meshes.add(makeFallbackMesh()), .material = m_materials.add(fallback)}};
}

ModelHandle ResourceCache::loadModel(std::string_view path) {
    APEX_PROFILE_SCOPE("ResourceCache::loadModel");
    const core::Result<AssetPath> assetPath = AssetPath::parse(path);
    if (!assetPath) {
        m_logger->error("Cannot load model: {}", assetPath.error().message);
        const ModelHandle failed = m_models.reserve();
        m_models.setFailed(failed);
        return failed;
    }
    if (const auto found = m_modelPaths.find(*assetPath); found != m_modelPaths.end()) {
        return found->second;
    }

    const ModelHandle handle = m_models.reserve();
    m_modelPaths.emplace(*assetPath, handle);
    ++m_filesRead;
    core::Result<ModelData> data = importGltf(m_root / assetPath->str(), assetPath->str());
    if (!data) {
        // The fallback cube is drawn instead (AC-11).
        m_logger->error("Cannot load model '{}': {}", assetPath->str(), data.error().message);
        m_models.setFailed(handle);
        return handle;
    }

    Model model;
    for (std::size_t i = 0; i < data->parts.size(); ++i) {
        ModelPart& part = data->parts[i];
        model.parts.push_back(ModelPartRef{
            .mesh = addMesh(std::format("{} part {}", assetPath->str(), i), std::move(part.mesh)),
            .material = modelMaterial(*assetPath, *data, part.material),
        });
    }
    m_models.setReady(handle, std::move(model));
    m_logger->info("Model '{}' loaded: {} part(s), {} texture(s)", assetPath->str(), data->parts.size(),
                   data->images.size());
    return handle;
}

MaterialHandle ResourceCache::modelMaterial(const AssetPath& model, const ModelData& data,
                                            std::uint32_t material) {
    const ModelMaterial& source = data.materials[material];
    MaterialData converted = source.factors;
    if (source.baseColorImage) {
        converted.baseColorTexture = imageTexture(model, data, *source.baseColorImage, ColorSpace::Srgb);
    }
    if (source.metallicRoughnessImage) {
        converted.metallicRoughnessTexture =
            imageTexture(model, data, *source.metallicRoughnessImage, ColorSpace::Linear);
    }
    return addMaterial(std::format("{} material '{}'", model.str(), source.name), converted);
}

TextureHandle ResourceCache::imageTexture(const AssetPath& model, const ModelData& data, std::uint32_t image,
                                          ColorSpace colorSpace) {
    const ModelImage& source = data.images[image];
    if (!source.uri.empty()) {
        const core::Result<AssetPath> path = model.resolveSibling(source.uri);
        return loadTexture(path ? std::string_view(path->str()) : std::string_view(source.uri), colorSpace);
    }
    // Inside the model: not shared, not keyed by a path.
    return addDecodedTexture(std::format("{} image {}", model.str(), image), source.bytes, colorSpace);
}

TextureHandle ResourceCache::loadTexture(std::string_view path, ColorSpace colorSpace) {
    APEX_PROFILE_SCOPE("ResourceCache::loadTexture");
    const core::Result<AssetPath> assetPath = AssetPath::parse(path);
    if (!assetPath) {
        m_logger->error("Cannot load texture: {}", assetPath.error().message);
        const TextureHandle failed = m_textures.reserve();
        m_textures.setFailed(failed);
        return failed;
    }
    const std::pair key{*assetPath, colorSpace};
    if (const auto found = m_texturePaths.find(key); found != m_texturePaths.end()) {
        return found->second;
    }

    ++m_filesRead;
    const core::Result<std::vector<std::byte>> bytes = readFile(m_root / assetPath->str());
    if (!bytes) {
        m_logger->error("Cannot load texture '{}': {}", assetPath->str(), bytes.error().message);
        const TextureHandle failed = m_textures.reserve();
        m_textures.setFailed(failed);
        m_texturePaths.emplace(key, failed);
        return failed;
    }
    const TextureHandle handle = addDecodedTexture(assetPath->str(), *bytes, colorSpace);
    m_texturePaths.emplace(key, handle);
    return handle;
}

TextureHandle ResourceCache::addDecodedTexture(std::string_view name, std::span<const std::byte> bytes,
                                               ColorSpace colorSpace) {
    const TextureHandle handle = m_textures.reserve();
    core::Result<TextureData> texture = decodeImage(bytes, name, colorSpace);
    if (!texture) {
        // The checker is drawn instead (AC-11).
        m_logger->error("Cannot load texture '{}': {}", name, texture.error().message);
        m_textures.setFailed(handle);
        return handle;
    }
    m_textures.setReady(handle, std::move(*texture));
    return handle;
}

MeshHandle ResourceCache::addMesh(std::string_view name, MeshData mesh) {
    const MeshHandle handle = m_meshes.reserve();
    mesh.name = name;
    if (const core::Result<void> valid = validate(mesh, name); !valid) {
        m_logger->error("{}", valid.error().message);
        m_meshes.setFailed(handle);
        return handle;
    }
    m_meshes.setReady(handle, std::move(mesh));
    return handle;
}

MaterialHandle ResourceCache::addMaterial(std::string_view name, const MaterialData& material) {
    const MaterialHandle handle = m_materials.reserve();
    if (!isFactor(material.metallicFactor) || !isFactor(material.roughnessFactor)) {
        m_logger->error("Material '{}': factors must be in [0, 1] (metallic {}, roughness {})", name,
                        material.metallicFactor, material.roughnessFactor);
        m_materials.setFailed(handle);
        return handle;
    }
    m_materials.setReady(handle, material);
    return handle;
}

ModelHandle ResourceCache::addModel(std::span<const ModelPartRef> parts) {
    return m_models.add(Model{.parts = {parts.begin(), parts.end()}});
}

std::optional<AssetPath> ResourceCache::modelPath(ModelHandle model) const {
    for (const auto& [path, handle] : m_modelPaths) {
        if (handle == model) {
            return path;
        }
    }
    return std::nullopt;
}

const std::filesystem::path& ResourceCache::root() const noexcept {
    return m_root;
}

ResourceState ResourceCache::state(ModelHandle model) const noexcept {
    return m_models.state(model);
}

ResourceState ResourceCache::state(TextureHandle texture) const noexcept {
    return m_textures.state(texture);
}

std::span<const ModelPartRef> ResourceCache::parts(ModelHandle model) const noexcept {
    const Model* found = m_models.find(model);
    return found != nullptr ? std::span<const ModelPartRef>(found->parts) : m_fallbackModel.parts;
}

MeshHandle ResourceCache::fallbackMesh() const noexcept {
    return m_fallbackModel.parts.front().mesh;
}

MaterialHandle ResourceCache::defaultMaterial() const noexcept {
    return m_defaultMaterial;
}

MaterialHandle ResourceCache::fallbackMaterial() const noexcept {
    return m_fallbackModel.parts.front().material;
}

TextureHandle ResourceCache::checkerTexture() const noexcept {
    return m_checker;
}

TextureHandle ResourceCache::whiteTexture() const noexcept {
    return m_white;
}

const ResourcePool<MeshHandle, MeshData>& ResourceCache::meshes() const noexcept {
    return m_meshes;
}

const ResourcePool<TextureHandle, TextureData>& ResourceCache::textures() const noexcept {
    return m_textures;
}

const ResourcePool<MaterialHandle, MaterialData>& ResourceCache::materials() const noexcept {
    return m_materials;
}

void ResourceCache::unloadAll() {
    m_models.clear();
    m_meshes.clear();
    m_textures.clear();
    m_materials.clear();
    m_modelPaths.clear();
    m_texturePaths.clear();
    createFallbacks();
    m_logger->info("Resources unloaded");
}

std::size_t ResourceCache::filesRead() const noexcept {
    return m_filesRead;
}

} // namespace apex::resources
