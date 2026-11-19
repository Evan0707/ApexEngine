#pragma once

#include "Resources/MaterialData.h"
#include "Resources/MeshData.h"
#include "Resources/TextureData.h"

namespace apex::resources {

// Built-in assets (spec 5.5), given in place of a resource that is not ready or failed (G-03).

// Magenta and black squares, sRGB: impossible to miss on screen.
[[nodiscard]] TextureData makeCheckerTexture();

// One white texel: a material without a texture samples it and keeps its factors unchanged.
[[nodiscard]] TextureData makeWhiteTexture(ColorSpace colorSpace);

// A cube of 1 m centered on the origin, drawn in place of a model that is not ready or failed.
[[nodiscard]] MeshData makeFallbackMesh();

// The default material of MaterialData, without textures.
[[nodiscard]] MaterialData makeDefaultMaterial();

} // namespace apex::resources
