#pragma once

#include "Resources/MeshData.h"

#include <cstdint>

namespace apex::resources {

// Axis-aligned cube centered on the origin, `size` meters wide: 24 vertices (flat normals), 12 triangles.
// Each face maps the whole texture.
[[nodiscard]] MeshData makeCube(float size);

// UV sphere centered on the origin, poles on the Y axis. segments >= 3 around Y, rings >= 2 from pole to
// pole. u goes around from +Z towards +X, v from the north pole (0) to the south pole (1).
[[nodiscard]] MeshData makeSphere(float radius, std::uint32_t segments, std::uint32_t rings);

} // namespace apex::resources
