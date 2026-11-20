#pragma once

#include <bit>
#include <cstdint>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. Levels of a full mip chain, down to 1 x 1 (decisions 0021 and 0022).
[[nodiscard]] constexpr std::uint32_t mipLevelCount(std::uint32_t width, std::uint32_t height) noexcept {
    const std::uint32_t largest = width > height ? width : height;
    return largest == 0 ? 0 : static_cast<std::uint32_t>(std::bit_width(largest));
}

// The size of a level: halved at each level, never below 1.
[[nodiscard]] constexpr std::uint32_t mipSize(std::uint32_t size, std::uint32_t level) noexcept {
    const std::uint32_t halved = level >= 32 ? 0 : size >> level;
    return halved > 0 ? halved : 1;
}

} // namespace apex::graphics
