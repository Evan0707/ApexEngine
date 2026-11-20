#include "Graphics/EnvironmentLighting.h"

#include "Core/Assert.h"
#include "Core/Profiling.h"

#include <algorithm>
#include <cmath>

namespace apex::graphics {

namespace {

// Convolution of each band with the clamped cosine: pi, 2 pi / 3, pi / 4.
constexpr std::array<float, 9> kCosineLobe{glm::pi<float>(),
                                           2.0f * glm::pi<float>() / 3.0f,
                                           2.0f * glm::pi<float>() / 3.0f,
                                           2.0f * glm::pi<float>() / 3.0f,
                                           glm::pi<float>() / 4.0f,
                                           glm::pi<float>() / 4.0f,
                                           glm::pi<float>() / 4.0f,
                                           glm::pi<float>() / 4.0f,
                                           glm::pi<float>() / 4.0f};

// Van der Corput radical inverse in base 2: the second coordinate of the Hammersley set.
[[nodiscard]] float radicalInverse(std::uint32_t bits) noexcept {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

// Same formula as visibilitySmithGgxCorrelated in Shaders/Pbr.glsl.
[[nodiscard]] float visibility(float nDotV, float nDotL, float alpha) noexcept {
    const float alpha2 = alpha * alpha;
    const float viewTerm = nDotL * std::sqrt(nDotV * nDotV * (1.0f - alpha2) + alpha2);
    const float lightTerm = nDotV * std::sqrt(nDotL * nDotL * (1.0f - alpha2) + alpha2);
    return 0.5f / (viewTerm + lightTerm);
}

} // namespace

std::array<float, 9> shBasis(const glm::vec3& direction) noexcept {
    const float x = direction.x;
    const float y = direction.y;
    const float z = direction.z;
    return {0.282095f,
            0.488603f * y,
            0.488603f * z,
            0.488603f * x,
            1.092548f * x * y,
            1.092548f * y * z,
            0.315392f * (3.0f * z * z - 1.0f),
            1.092548f * x * z,
            0.546274f * (x * x - y * y)};
}

IrradianceSh projectIrradiance(const resources::EnvironmentData& environment) {
    APEX_PROFILE_SCOPE("Graphics::projectIrradiance");
    APEX_ASSERT(environment.texels.size() == std::size_t{environment.width} * environment.height,
                "Environment of the wrong size");
    std::array<glm::dvec3, 9> sums{};
    const double texelAngle =
        (glm::two_pi<double>() / environment.width) * (glm::pi<double>() / environment.height);
    for (std::uint32_t y = 0; y < environment.height; ++y) {
        // Rows near the poles cover less of the sphere.
        const double polar = (y + 0.5) / environment.height * glm::pi<double>();
        const double solidAngle = texelAngle * std::sin(polar);
        for (std::uint32_t x = 0; x < environment.width; ++x) {
            const glm::dvec3 radiance = environment.texels[std::size_t{y} * environment.width + x];
            const std::array<float, 9> basis = shBasis(resources::environmentDirection(environment, x, y));
            for (std::size_t i = 0; i < basis.size(); ++i) {
                sums[i] += radiance * (basis[i] * solidAngle);
            }
        }
    }
    IrradianceSh sh;
    for (std::size_t i = 0; i < sums.size(); ++i) {
        sh.coefficients[i] = glm::vec3(sums[i]) * kCosineLobe[i];
    }
    return sh;
}

glm::vec3 irradianceAt(const IrradianceSh& sh, const glm::vec3& normal) noexcept {
    const std::array<float, 9> basis = shBasis(normal);
    glm::vec3 irradiance{0.0f};
    for (std::size_t i = 0; i < basis.size(); ++i) {
        irradiance += sh.coefficients[i] * basis[i];
    }
    return glm::max(irradiance, glm::vec3(0.0f));
}

std::vector<glm::vec2> computeBrdfTable(std::uint32_t size, std::uint32_t samples) {
    APEX_PROFILE_SCOPE("Graphics::computeBrdfTable");
    APEX_ASSERT(size > 0 && samples > 0, "Empty BRDF table");
    std::vector<glm::vec2> table;
    table.reserve(std::size_t{size} * size);
    for (std::uint32_t row = 0; row < size; ++row) {
        const float roughness = (static_cast<float>(row) + 0.5f) / static_cast<float>(size);
        const float alpha = roughness * roughness;
        for (std::uint32_t column = 0; column < size; ++column) {
            const float nDotV = (static_cast<float>(column) + 0.5f) / static_cast<float>(size);
            // The normal is +Z; the view in the XZ plane.
            const glm::vec3 view{std::sqrt(1.0f - nDotV * nDotV), 0.0f, nDotV};
            glm::vec2 sum{0.0f};
            for (std::uint32_t i = 0; i < samples; ++i) {
                // GGX importance sampling of the half vector, Hammersley set.
                const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(samples);
                const float v = radicalInverse(i);
                const float phi = glm::two_pi<float>() * u;
                const float cosTheta = std::sqrt((1.0f - v) / (1.0f + (alpha * alpha - 1.0f) * v));
                const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
                const glm::vec3 half{sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta};
                const float vDotH = glm::dot(view, half);
                const glm::vec3 light = 2.0f * vDotH * half - view;
                const float nDotL = light.z;
                if (nDotL <= 0.0f || vDotH <= 0.0f) {
                    continue;
                }
                // The sampled BRDF times n.l, divided by the probability of the sample.
                const float weight = visibility(nDotV, nDotL, alpha) * 4.0f * nDotL * vDotH / half.z;
                const float fresnel = std::pow(1.0f - vDotH, 5.0f);
                sum += glm::vec2((1.0f - fresnel) * weight, fresnel * weight);
            }
            table.push_back(sum / static_cast<float>(samples));
        }
    }
    return table;
}

} // namespace apex::graphics
