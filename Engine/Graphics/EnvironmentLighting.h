#pragma once

#include "Core/Math.h"
#include "Resources/EnvironmentData.h"

#include <array>
#include <cstdint>
#include <vector>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. The parts of image-based lighting computed on the CPU (decision
// 0022), tested without a GPU. Shaders/Ibl.glsl evaluates them with the same formulas.

// The nine real spherical harmonics of bands 0 to 2 at a unit direction, in the order of irradianceAt.
[[nodiscard]] std::array<float, 9> shBasis(const glm::vec3& direction) noexcept;

// Irradiance as nine spherical harmonics coefficients, already convolved with the clamped cosine
// (Ramamoorthi and Hanrahan): the irradiance at a normal n is the sum of coefficients[i] * shBasis(n)[i].
struct IrradianceSh {
    std::array<glm::vec3, 9> coefficients{};
};

// Projects the environment, each texel weighted by its solid angle.
[[nodiscard]] IrradianceSh projectIrradiance(const resources::EnvironmentData& environment);

// The irradiance reaching a surface of unit normal `normal`. A diffuse surface reflects albedo / pi of it.
[[nodiscard]] glm::vec3 irradianceAt(const IrradianceSh& sh, const glm::vec3& normal) noexcept;

// Sides of the split-sum table, and samples per entry.
inline constexpr std::uint32_t kBrdfTableSize = 64;
inline constexpr std::uint32_t kBrdfTableSamples = 256;

// The split-sum table of the specular term (Karis), for the GGX distribution and the height-correlated Smith
// visibility of Shaders/Pbr.glsl: entry (x, y), row by row, gives the scale and the bias of F0 for
// n.v = (x + 0.5) / size and perceptual roughness (y + 0.5) / size.
[[nodiscard]] std::vector<glm::vec2> computeBrdfTable(std::uint32_t size, std::uint32_t samples);

} // namespace apex::graphics
