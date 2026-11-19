#include "Resources/FallbackAssets.h"
#include "Resources/FileReader.h"
#include "Resources/ImageDecoder.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdlib>
#include <filesystem>
#include <vector>

using namespace apex::resources;
using Catch::Matchers::ContainsSubstring;

namespace {

[[nodiscard]] std::vector<std::byte> testFile(const char* relative) {
    const auto bytes = readFile(std::filesystem::path(APEX_TEST_DATA_DIR) / relative);
    REQUIRE(bytes.has_value());
    return *bytes;
}

} // namespace

TEST_CASE("A PNG decodes to its exact RGBA pixels, top row first", "[resources]") {
    const auto texture = decodeImage(testFile("Images/rgba_2x2.png"), "rgba_2x2.png", ColorSpace::Srgb);
    REQUIRE(texture.has_value());
    REQUIRE(texture->width == 2);
    REQUIRE(texture->height == 2);
    REQUIRE(texture->colorSpace == ColorSpace::Srgb);
    const std::vector<std::uint8_t> expected{255, 0, 0,   255, 0,   255, 0,   255,
                                             0,   0, 255, 255, 255, 255, 255, 0};
    REQUIRE(texture->pixels == expected);
}

TEST_CASE("A JPG decodes to RGBA with an opaque alpha", "[resources]") {
    const auto texture = decodeImage(testFile("Images/gray_8x8.jpg"), "gray_8x8.jpg", ColorSpace::Linear);
    REQUIRE(texture.has_value());
    REQUIRE(texture->width == 8);
    REQUIRE(texture->byteSize() == 8 * 8 * 4);
    REQUIRE(texture->colorSpace == ColorSpace::Linear);
    for (std::size_t i = 0; i < texture->pixels.size(); i += 4) {
        // Lossy: within a few levels of the gray it was written with.
        REQUIRE(std::abs(texture->pixels[i] - 128) <= 3);
        REQUIRE(texture->pixels[i + 3] == 255);
    }
}

TEST_CASE("A corrupt or empty image is refused with its name", "[resources]") {
    const auto corrupt =
        decodeImage(testFile("Images/corrupt.png"), "Textures/corrupt.png", ColorSpace::Srgb);
    REQUIRE_FALSE(corrupt.has_value());
    REQUIRE_THAT(corrupt.error().message, ContainsSubstring("Textures/corrupt.png"));
    REQUIRE_FALSE(decodeImage({}, "empty.png", ColorSpace::Srgb).has_value());
}

TEST_CASE("Reading a missing file names it", "[resources]") {
    const auto missing = readFile(std::filesystem::path(APEX_TEST_DATA_DIR) / "Images/absent.png");
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == apex::core::ErrorCode::NotFound);
    REQUIRE_THAT(missing.error().message, ContainsSubstring("Images/absent.png"));
}

TEST_CASE("The fallback texture is a magenta checker", "[resources]") {
    const TextureData checker = makeCheckerTexture();
    REQUIRE(checker.width == 64);
    REQUIRE(checker.byteSize() == 64 * 64 * 4);
    REQUIRE(checker.colorSpace == ColorSpace::Srgb);
    const auto texel = [&](std::uint32_t x, std::uint32_t y) {
        const std::size_t offset = (std::size_t{y} * checker.width + x) * 4;
        return std::vector<std::uint8_t>(checker.pixels.begin() + static_cast<std::ptrdiff_t>(offset),
                                         checker.pixels.begin() + static_cast<std::ptrdiff_t>(offset + 4));
    };
    REQUIRE(texel(0, 0) == std::vector<std::uint8_t>{255, 0, 255, 255});
    REQUIRE(texel(8, 0) == std::vector<std::uint8_t>{0, 0, 0, 255});
    REQUIRE(texel(8, 8) == std::vector<std::uint8_t>{255, 0, 255, 255});
}

TEST_CASE("The white texture keeps material factors unchanged", "[resources]") {
    const TextureData white = makeWhiteTexture(ColorSpace::Linear);
    REQUIRE(white.width == 1);
    REQUIRE(white.pixels == std::vector<std::uint8_t>{255, 255, 255, 255});
    REQUIRE(white.colorSpace == ColorSpace::Linear);
    REQUIRE(validate(makeFallbackMesh(), "fallback"));
}
