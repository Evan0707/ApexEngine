#include "Graphics/ShaderBinaries.h"

namespace apex::graphics {

namespace {

constexpr std::uint32_t kSceneVertex[] =
#include "Shaders/Scene.vert.spv.inc"
    ;

constexpr std::uint32_t kSceneFragment[] =
#include "Shaders/Scene.frag.spv.inc"
    ;

constexpr std::uint32_t kShadowVertex[] =
#include "Shaders/Shadow.vert.spv.inc"
    ;

constexpr std::uint32_t kFullscreenVertex[] =
#include "Shaders/Fullscreen.vert.spv.inc"
    ;

constexpr std::uint32_t kTonemapFragment[] =
#include "Shaders/Tonemap.frag.spv.inc"
    ;

} // namespace

std::span<const std::uint32_t> sceneVertexShader() noexcept {
    return kSceneVertex;
}

std::span<const std::uint32_t> sceneFragmentShader() noexcept {
    return kSceneFragment;
}

std::span<const std::uint32_t> shadowVertexShader() noexcept {
    return kShadowVertex;
}

std::span<const std::uint32_t> fullscreenVertexShader() noexcept {
    return kFullscreenVertex;
}

std::span<const std::uint32_t> tonemapFragmentShader() noexcept {
    return kTonemapFragment;
}

} // namespace apex::graphics
