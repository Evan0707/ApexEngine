#pragma once

#include "Core/Math.h"
#include "Resources/Handles.h"

namespace apex::resources {

// glTF 2.0 metallic-roughness, simplified (spec 5.5). The default values are the engine's default
// material: a light grey, non-metallic, half-rough surface, without textures.
struct MaterialData {
    // Linear RGBA, multiplied by the base color texture. Alpha is ignored: no transparency (spec 5.5).
    glm::vec4 baseColorFactor{0.8f, 0.8f, 0.8f, 1.0f};
    // In [0, 1], multiplied by the blue channel of the metallic-roughness texture.
    float metallicFactor = 0.0f;
    // In [0, 1], multiplied by the green channel of the metallic-roughness texture.
    float roughnessFactor = 0.5f;
    // sRGB. A default handle means no texture: the factor alone.
    TextureHandle baseColorTexture;
    // Linear (glTF: metallic in blue, roughness in green). A default handle means no texture.
    TextureHandle metallicRoughnessTexture;
};

} // namespace apex::resources
