#include "Resources/FallbackAssets.h"

#include "Resources/Primitives.h"

#include <array>
#include <cstdint>

namespace apex::resources {

namespace {

constexpr std::uint32_t kCheckerSize = 64;
constexpr std::uint32_t kCheckerSquare = 8;
constexpr std::array<std::uint8_t, 4> kMagenta{255, 0, 255, 255};
constexpr std::array<std::uint8_t, 4> kBlack{0, 0, 0, 255};

} // namespace

TextureData makeCheckerTexture() {
    TextureData texture;
    texture.name = "Fallback checker";
    texture.width = kCheckerSize;
    texture.height = kCheckerSize;
    texture.colorSpace = ColorSpace::Srgb;
    texture.pixels.reserve(std::size_t{kCheckerSize} * kCheckerSize * 4);
    for (std::uint32_t y = 0; y < kCheckerSize; ++y) {
        for (std::uint32_t x = 0; x < kCheckerSize; ++x) {
            const bool magenta = ((x / kCheckerSquare) + (y / kCheckerSquare)) % 2 == 0;
            const std::array<std::uint8_t, 4>& color = magenta ? kMagenta : kBlack;
            texture.pixels.insert(texture.pixels.end(), color.begin(), color.end());
        }
    }
    return texture;
}

TextureData makeWhiteTexture(ColorSpace colorSpace) {
    return TextureData{
        .name = "White", .width = 1, .height = 1, .pixels = {255, 255, 255, 255}, .colorSpace = colorSpace};
}

MeshData makeFallbackMesh() {
    return makeCube(1.0f);
}

MaterialData makeDefaultMaterial() {
    return MaterialData{};
}

} // namespace apex::resources
