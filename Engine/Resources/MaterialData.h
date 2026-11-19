#pragma once

#include "Core/Math.h"

namespace apex::resources {

// glTF 2.0 metallic-roughness, simplified (spec 5.5). Textures arrive in phase 4. The default values are
// the engine's default material: a light grey, non-metallic, half-rough surface.
struct MaterialData {
    // Linear RGBA. Alpha is ignored: no transparency (spec 5.5).
    glm::vec4 baseColorFactor{0.8f, 0.8f, 0.8f, 1.0f};
    // In [0, 1].
    float metallicFactor = 0.0f;
    // In [0, 1].
    float roughnessFactor = 0.5f;
};

} // namespace apex::resources
