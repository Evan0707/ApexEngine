#include "Resources/ImageDecoder.h"

// The only file that includes stb_image (decisions 0013 and 0022): PNG, JPEG and Radiance HDR only, no file
// access.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_HDR
#define STBI_NO_STDIO
#include <stb_image.h>

#include <climits>
#include <format>
#include <memory>

namespace apex::resources {

namespace {

constexpr int kRgba = 4;

constexpr int kRgb = 3;

struct StbiFree {
    void operator()(void* pixels) const noexcept { stbi_image_free(pixels); }
};

[[nodiscard]] std::unexpected<core::Error> invalidImage(std::string_view name, std::string_view reason) {
    return core::makeError(core::ErrorCode::InvalidArgument, std::format("Image '{}': {}", name, reason));
}

} // namespace

core::Result<TextureData> decodeImage(std::span<const std::byte> bytes, std::string_view name,
                                      ColorSpace colorSpace) {
    if (bytes.empty()) {
        return invalidImage(name, "empty file");
    }
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        return invalidImage(name, "file too large");
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto size = static_cast<int>(bytes.size());
    // stb_image would turn an HDR image into 8 bits: refused, it is an environment's format.
    if (stbi_is_hdr_from_memory(data, size) != 0) {
        return invalidImage(name, "a Radiance HDR image is not a texture");
    }

    // Checked before decoding: a huge image is refused without allocating it.
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(data, size, &width, &height, &channels) == 0) {
        return invalidImage(name, std::format("not a valid PNG or JPG ({})", stbi_failure_reason()));
    }
    if (width <= 0 || height <= 0 || static_cast<std::uint32_t>(width) > kMaxImageSize ||
        static_cast<std::uint32_t>(height) > kMaxImageSize) {
        return invalidImage(
            name, std::format("{}x{} pixels, the limit is {} per side", width, height, kMaxImageSize));
    }

    const std::unique_ptr<stbi_uc, StbiFree> pixels(
        stbi_load_from_memory(data, size, &width, &height, &channels, kRgba));
    if (!pixels) {
        return invalidImage(name, std::format("cannot decode it ({})", stbi_failure_reason()));
    }

    TextureData texture;
    texture.name = name;
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.colorSpace = colorSpace;
    const std::size_t byteCount = std::size_t{texture.width} * texture.height * kRgba;
    texture.pixels.assign(pixels.get(), pixels.get() + byteCount);
    return texture;
}

core::Result<EnvironmentData> decodeHdrImage(std::span<const std::byte> bytes, std::string_view name) {
    if (bytes.empty()) {
        return invalidImage(name, "empty file");
    }
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        return invalidImage(name, "file too large");
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto size = static_cast<int>(bytes.size());
    if (stbi_is_hdr_from_memory(data, size) == 0) {
        return invalidImage(name, "not a Radiance HDR image");
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(data, size, &width, &height, &channels) == 0) {
        return invalidImage(name, std::format("not a valid Radiance HDR image ({})", stbi_failure_reason()));
    }
    if (width <= 0 || height <= 0 || static_cast<std::uint32_t>(width) > kMaxImageSize ||
        static_cast<std::uint32_t>(height) > kMaxImageSize) {
        return invalidImage(
            name, std::format("{}x{} pixels, the limit is {} per side", width, height, kMaxImageSize));
    }
    const std::unique_ptr<float, StbiFree> pixels(
        stbi_loadf_from_memory(data, size, &width, &height, &channels, kRgb));
    if (!pixels) {
        return invalidImage(name, std::format("cannot decode it ({})", stbi_failure_reason()));
    }

    EnvironmentData environment;
    environment.name = name;
    environment.width = static_cast<std::uint32_t>(width);
    environment.height = static_cast<std::uint32_t>(height);
    const std::size_t count = std::size_t{environment.width} * environment.height;
    environment.texels.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const float* texel = pixels.get() + i * kRgb;
        environment.texels[i] = glm::vec3(texel[0], texel[1], texel[2]);
    }
    return environment;
}

} // namespace apex::resources
