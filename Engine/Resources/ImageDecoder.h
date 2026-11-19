#pragma once

#include "Core/Error.h"
#include "Resources/TextureData.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace apex::resources {

// Images larger than this on either side are refused (spec 6.1 allows 2048).
inline constexpr std::uint32_t kMaxImageSize = 4096;

// Decodes a PNG or JPG held in memory into RGBA 8 bits (decision 0013). The error names the image and
// says why it was refused.
[[nodiscard]] core::Result<TextureData> decodeImage(std::span<const std::byte> bytes, std::string_view name,
                                                    ColorSpace colorSpace);

} // namespace apex::resources
