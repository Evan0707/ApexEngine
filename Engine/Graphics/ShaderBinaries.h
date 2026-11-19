#pragma once

#include <cstdint>
#include <span>

namespace apex::graphics {

// Internal to Graphics. SPIR-V compiled by glslc during the build (cmake/ApexShaders.cmake), embedded in
// the binary.
[[nodiscard]] std::span<const std::uint32_t> triangleVertexShader() noexcept;
[[nodiscard]] std::span<const std::uint32_t> triangleFragmentShader() noexcept;

} // namespace apex::graphics
