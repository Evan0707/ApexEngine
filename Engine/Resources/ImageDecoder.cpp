#include "Resources/ImageDecoder.h"

// The only file that includes stb_image (decision 0013): PNG and JPEG only, no file access.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include <stb_image.h>

#include <climits>
#include <format>
#include <memory>

namespace apex::resources {

namespace {

constexpr int kRgba = 4;

struct StbiFree {
    void operator()(stbi_uc* pixels) const noexcept { stbi_image_free(pixels); }
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
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.colorSpace = colorSpace;
    const std::size_t byteCount = std::size_t{texture.width} * texture.height * kRgba;
    texture.pixels.assign(pixels.get(), pixels.get() + byteCount);
    return texture;
}

} // namespace apex::resources
