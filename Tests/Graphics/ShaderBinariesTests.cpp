#include "Graphics/ShaderBinaries.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>

namespace {

// SPIR-V specification, section 2.3: magic number, then version, generator, bound and schema.
constexpr std::uint32_t kSpirvMagic = 0x07230203;
constexpr std::size_t kSpirvHeaderWords = 5;

void requireSpirv(std::span<const std::uint32_t> words) {
    REQUIRE(words.size() > kSpirvHeaderWords);
    REQUIRE(words[0] == kSpirvMagic);
    // Vulkan 1.3 accepts SPIR-V up to 1.6: version word 0x00MMmm00.
    REQUIRE(words[1] <= 0x00010600u);
    REQUIRE(words[4] == 0u);
}

} // namespace

TEST_CASE("The scene shaders are embedded as SPIR-V", "[graphics]") {
    requireSpirv(apex::graphics::sceneVertexShader());
    requireSpirv(apex::graphics::sceneFragmentShader());
}
