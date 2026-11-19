#include "Graphics/ShaderBinaries.h"

namespace apex::graphics {

namespace {

constexpr std::uint32_t kTriangleVertex[] =
#include "Shaders/Triangle.vert.spv.inc"
    ;

constexpr std::uint32_t kTriangleFragment[] =
#include "Shaders/Triangle.frag.spv.inc"
    ;

} // namespace

std::span<const std::uint32_t> triangleVertexShader() noexcept {
    return kTriangleVertex;
}

std::span<const std::uint32_t> triangleFragmentShader() noexcept {
    return kTriangleFragment;
}

} // namespace apex::graphics
