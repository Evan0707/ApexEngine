#pragma once

#include "Core/Error.h"
#include "Graphics/GpuTimestamps.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace apex::graphics {

// Internal to Graphics. GPU time of each frame (spec 5.9, decision 0023): two timestamps per frame in flight,
// at the start and the end of its commands, read once its fence was waited for, so reading never blocks.
// Does nothing when the queue writes no timestamp.
class GpuTimer {
public:
    [[nodiscard]] static core::Result<GpuTimer> create(VkDevice device, const TimestampFormat& format,
                                                       std::uint32_t framesInFlight);

    GpuTimer() = default;
    ~GpuTimer();
    GpuTimer(GpuTimer&& other) noexcept;
    GpuTimer& operator=(GpuTimer&& other) noexcept;
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    // First and last commands of a frame in flight's command buffer, outside any rendering pass.
    void begin(VkCommandBuffer commands, std::uint32_t frame) noexcept;
    void end(VkCommandBuffer commands, std::uint32_t frame) noexcept;

    // After the frame's fence was waited for: the GPU time of the commands last submitted in this slot.
    // Nothing when nothing was submitted since the last read, or without timestamps.
    [[nodiscard]] std::optional<double> read(std::uint32_t frame);

private:
    void destroy() noexcept;

    VkDevice m_device = VK_NULL_HANDLE;
    VkQueryPool m_pool = VK_NULL_HANDLE;
    TimestampFormat m_format;
    // Per frame in flight: its two queries were written by a submitted command buffer and not read yet.
    std::vector<bool> m_written;
};

} // namespace apex::graphics
