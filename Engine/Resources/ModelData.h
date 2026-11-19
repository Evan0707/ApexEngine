#pragma once

#include "Resources/MaterialData.h"
#include "Resources/MeshData.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apex::resources {

// An image used by a model: a file next to the model, or bytes inside it (GLB, data URI).
struct ModelImage {
    // Relative to the model's folder, already URI-decoded. Empty when the bytes are inside the model.
    std::string uri;
    std::vector<std::byte> bytes;
};

// A material of a model, before its images are loaded: factors, and indices into ModelData::images.
struct ModelMaterial {
    std::string name;
    MaterialData factors;
    std::optional<std::uint32_t> baseColorImage;
    std::optional<std::uint32_t> metallicRoughnessImage;
};

// One mesh drawn with one material.
struct ModelPart {
    MeshData mesh;
    // Index into ModelData::materials.
    std::uint32_t material = 0;
};

// What an importer produces (G-04): independent of the file format. The node hierarchy is flattened
// (spec 5.5): the vertices are in the model's space, and the parts that share a material are merged.
struct ModelData {
    std::vector<ModelPart> parts;
    std::vector<ModelMaterial> materials;
    std::vector<ModelImage> images;
};

} // namespace apex::resources
