#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace apex::resources {

// How the texels are to be read (spec 3.3): colors are stored in sRGB, data (metallic, roughness) in
// linear values.
enum class ColorSpace { Srgb, Linear };

// A decoded image, independent of its file format (G-04): RGBA, 8 bits per channel, rows from the top
// of the image down.
struct TextureData {
    // For error messages: the asset path, or where the image sits inside its model.
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;
    ColorSpace colorSpace = ColorSpace::Srgb;

    [[nodiscard]] std::size_t byteSize() const noexcept { return pixels.size(); }
};

} // namespace apex::resources
