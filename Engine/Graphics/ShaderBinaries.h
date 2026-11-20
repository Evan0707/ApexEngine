#pragma once

#include <cstdint>
#include <span>

namespace apex::graphics {

// Internal to Graphics. SPIR-V compiled by glslc during the build (cmake/ApexShaders.cmake), embedded in
// the binary.
[[nodiscard]] std::span<const std::uint32_t> sceneVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> sceneFragmentShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> shadowVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> fullscreenVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> tonemapFragmentShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> prefilterVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> prefilterFragmentShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> skyVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> skyFragmentShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> lineVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> lineFragmentShader() noexcept;

} // namespace apex::graphics
