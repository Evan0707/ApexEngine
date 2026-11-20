#pragma once

#include "Core/Math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace apex::resources {

// Light coming from every direction (spec 5.4, decision 0022), independent of its file format (G-04): an
// equirectangular image of linear RGB radiance. Rows from the top (+Y, up) to the bottom (-Y). Across a
// row, the direction turns around +Y; the middle column looks towards -Z (environmentUv).
struct EnvironmentData {
    // For error messages: the asset path.
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // width * height texels, row by row.
    std::vector<glm::vec3> texels;
};

// The texture coordinates of a unit direction: u from 0 to 1 around +Y, 0.5 towards -Z; v from 0 at +Y to
// 1 at -Y. Shaders use the same formula.
[[nodiscard]] glm::vec2 environmentUv(const glm::vec3& direction) noexcept;

// The unit direction at the center of the texel (x, y).
[[nodiscard]] glm::vec3 environmentDirection(const EnvironmentData& environment, std::uint32_t x,
                                             std::uint32_t y) noexcept;

} // namespace apex::resources
