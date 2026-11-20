#pragma once

#include <cstdint>
#include <optional>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. How the graphics queue counts time (decision 0023).
struct TimestampFormat {
    // VkQueueFamilyProperties::timestampValidBits: 0 when the queue writes no timestamp.
    std::uint32_t validBits = 0;
    // VkPhysicalDeviceLimits::timestampPeriod.
    double nanosecondsPerTick = 0.0;

    [[nodiscard]] bool supported() const noexcept { return validBits > 0 && nanosecondsPerTick > 0.0; }
};

// The time between two timestamps of the same queue. The counter wraps around after validBits bits: an end
// below its begin is a counter that wrapped once. Nothing when the queue writes no timestamp.
[[nodiscard]] std::optional<double> elapsedSeconds(std::uint64_t begin, std::uint64_t end,
                                                   const TimestampFormat& format) noexcept;

} // namespace apex::graphics
