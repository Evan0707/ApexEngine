#pragma once

#include "Core/Error.h"
#include "Resources/ModelData.h"

#include <filesystem>
#include <string_view>

namespace apex::resources {

// Imports a glTF 2.0 file (.gltf with its external files, or .glb) with cgltf (decision 0012). Images
// are not decoded: the cache loads them. The error names the model (`name`, its asset path) and the
// faulty element.
[[nodiscard]] core::Result<ModelData> importGltf(const std::filesystem::path& file, std::string_view name);

} // namespace apex::resources
