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

constexpr std::uint32_t kPrefilterVertex[] =
#include "Shaders/Prefilter.vert.spv.inc"
    ;

constexpr std::uint32_t kPrefilterFragment[] =
#include "Shaders/Prefilter.frag.spv.inc"
    ;

constexpr std::uint32_t kSkyVertex[] =
#include "Shaders/Sky.vert.spv.inc"
    ;

constexpr std::uint32_t kSkyFragment[] =
#include "Shaders/Sky.frag.spv.inc"
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

std::span<const std::uint32_t> prefilterVertexShader() noexcept {
    return kPrefilterVertex;
}

std::span<const std::uint32_t> prefilterFragmentShader() noexcept {
    return kPrefilterFragment;
}

std::span<const std::uint32_t> skyVertexShader() noexcept {
    return kSkyVertex;
}

std::span<const std::uint32_t> skyFragmentShader() noexcept {
    return kSkyFragment;
}

} // namespace apex::graphics
