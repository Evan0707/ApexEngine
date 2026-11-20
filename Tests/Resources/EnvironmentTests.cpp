#include "Core/Logger.h"
#include "Resources/EnvironmentData.h"
#include "Resources/ImageDecoder.h"
#include "Resources/ResourceCache.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace apex::resources;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

constexpr float kTolerance = 1e-5f;

// A flat (not run-length encoded) Radiance image: a header, then 4 bytes per pixel.
[[nodiscard]] std::vector<std::byte> radianceImage(std::uint32_t width, std::uint32_t height,
                                                   const std::vector<std::uint8_t>& rgbePixels) {
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " + std::to_string(height) + " +X " +
                               std::to_string(width) + "\n";
    std::vector<std::byte> bytes;
    for (const char c : header) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    for (const std::uint8_t value : rgbePixels) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

} // namespace

TEST_CASE("A Radiance image decodes to linear floats, top row first", "[resources]") {
    // Exponent 129: a mantissa of 128 is 1.0.
    const auto environment = decodeHdrImage(radianceImage(2, 2,
                                                          {128, 64, 32, 129, 0, 0, 0, 0, //
                                                           128, 128, 128, 130, 64, 0, 0, 129}),
                                            "tiny.hdr");
    REQUIRE(environment.has_value());
    REQUIRE(environment->width == 2);
    REQUIRE(environment->height == 2);
    REQUIRE(environment->texels.size() == 4);
    REQUIRE(environment->name == "tiny.hdr");
    REQUIRE_THAT(environment->texels[0].r, WithinAbs(1.0, kTolerance));
    REQUIRE_THAT(environment->texels[0].g, WithinAbs(0.5, kTolerance));
    REQUIRE_THAT(environment->texels[0].b, WithinAbs(0.25, kTolerance));
    REQUIRE(environment->texels[1] == glm::vec3(0.0f));
    REQUIRE_THAT(environment->texels[2].r, WithinAbs(2.0, kTolerance));
    REQUIRE_THAT(environment->texels[3].r, WithinAbs(0.5, kTolerance));
}

TEST_CASE("Only Radiance images are environments, and only environments are Radiance images", "[resources]") {
    const auto notHdr = decodeHdrImage(std::vector<std::byte>(16, std::byte{0x42}), "fake.hdr");
    REQUIRE_FALSE(notHdr.has_value());
    REQUIRE_THAT(notHdr.error().message, ContainsSubstring("fake.hdr"));
    REQUIRE_THAT(notHdr.error().message, ContainsSubstring("not a Radiance HDR image"));
    REQUIRE_FALSE(decodeHdrImage({}, "empty.hdr").has_value());

    const auto asTexture =
        decodeImage(radianceImage(1, 1, {128, 128, 128, 129}), "sky.hdr", ColorSpace::Srgb);
    REQUIRE_FALSE(asTexture.has_value());
    REQUIRE_THAT(asTexture.error().message, ContainsSubstring("not a texture"));
}

TEST_CASE("A truncated run-length encoded Radiance image is refused with its name", "[resources]") {
    // The start of a run-length encoded scanline of 8 pixels, then nothing.
    const auto truncated = decodeHdrImage(radianceImage(8, 2, {2, 2, 0, 8}), "short.hdr");
    REQUIRE_FALSE(truncated.has_value());
    REQUIRE_THAT(truncated.error().message, ContainsSubstring("short.hdr"));
}

TEST_CASE("A Radiance image larger than the limit is refused", "[resources]") {
    const auto huge = decodeHdrImage(radianceImage(kMaxImageSize + 1, 1, {128, 128, 128, 129}), "huge.hdr");
    REQUIRE_FALSE(huge.has_value());
    REQUIRE_THAT(huge.error().message, ContainsSubstring("the limit is"));
}

TEST_CASE("Environment coordinates: up at the top, -Z in the middle", "[resources]") {
    REQUIRE_THAT(environmentUv({0.0f, 1.0f, 0.0f}).y, WithinAbs(0.0, kTolerance));
    REQUIRE_THAT(environmentUv({0.0f, -1.0f, 0.0f}).y, WithinAbs(1.0, kTolerance));
    const glm::vec2 front = environmentUv({0.0f, 0.0f, -1.0f});
    REQUIRE_THAT(front.x, WithinAbs(0.5, kTolerance));
    REQUIRE_THAT(front.y, WithinAbs(0.5, kTolerance));
    // Turning from -Z towards +X increases u.
    REQUIRE_THAT(environmentUv({1.0f, 0.0f, 0.0f}).x, WithinAbs(0.75, kTolerance));

    EnvironmentData environment{.name = "grid", .width = 64, .height = 32, .texels = {}};
    for (const auto [x, y] :
         {std::pair{0u, 0u}, std::pair{17u, 9u}, std::pair{40u, 30u}, std::pair{63u, 16u}}) {
        const glm::vec3 direction = environmentDirection(environment, x, y);
        REQUIRE_THAT(glm::length(direction), WithinAbs(1.0, kTolerance));
        const glm::vec2 uv = environmentUv(direction);
        REQUIRE_THAT(uv.x * 64.0f, WithinAbs(x + 0.5, 1e-3));
        REQUIRE_THAT(uv.y * 32.0f, WithinAbs(y + 0.5, 1e-3));
    }
}

TEST_CASE("The demo's sky loads once, bright above and dark below", "[resources]") {
    apex::core::Logger logger;
    ResourceCache cache(std::filesystem::path(APEX_ASSETS_DIR), logger);
    const EnvironmentHandle sky = cache.loadEnvironment("Environments/sky.hdr");
    REQUIRE(cache.loadEnvironment("Environments\\.\\sky.hdr") == sky);
    REQUIRE(cache.filesRead() == 1);
    REQUIRE(cache.state(sky) == ResourceState::Ready);
    const EnvironmentData* data = cache.environments().find(sky);
    REQUIRE(data != nullptr);
    REQUIRE(data->width == 2 * data->height);
    const glm::vec3 zenith = data->texels[data->width / 4];
    const glm::vec3 ground = data->texels[(data->height - 1) * data->width];
    REQUIRE(zenith.b > ground.b * 4.0f);
    REQUIRE(zenith.b > zenith.r);

    cache.unloadAll();
    REQUIRE(cache.state(sky) == ResourceState::Failed);
}

TEST_CASE("A missing or invalid environment fails with its path", "[resources]") {
    apex::core::Logger logger;
    ResourceCache cache(std::filesystem::path(APEX_TEST_DATA_DIR), logger);
    REQUIRE(cache.state(cache.loadEnvironment("Environments/missing.hdr")) == ResourceState::Failed);
    REQUIRE(cache.state(cache.loadEnvironment("Images/rgba_2x2.png")) == ResourceState::Failed);
    REQUIRE(cache.state(cache.loadEnvironment("../outside.hdr")) == ResourceState::Failed);
}
