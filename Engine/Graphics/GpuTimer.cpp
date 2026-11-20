#include "Graphics/GpuTimer.h"

#include "Core/Assert.h"

#include <array>
#include <format>
#include <utility>

namespace apex::graphics {

core::Result<GpuTimer> GpuTimer::create(VkDevice device, const TimestampFormat& format,
                                        std::uint32_t framesInFlight) {
    GpuTimer timer;
    timer.m_format = format;
    timer.m_written.assign(framesInFlight, false);
    if (!format.supported()) {
        return timer;
    }
    const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                     .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                     .queryCount = 2 * framesInFlight};
    if (const VkResult result = vkCreateQueryPool(device, &info, nullptr, &timer.m_pool);
        result != VK_SUCCESS) {
        return core::makeError(
            core::ErrorCode::GpuFailure,
            std::format("Cannot create the GPU timer's query pool: VkResult {}", static_cast<int>(result)));
    }
    timer.m_device = device;
    return timer;
}

GpuTimer::~GpuTimer() {
    destroy();
}

GpuTimer::GpuTimer(GpuTimer&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)), m_format(other.m_format),
      m_written(std::move(other.m_written)) {}

GpuTimer& GpuTimer::operator=(GpuTimer&& other) noexcept {
    if (this != &other) {
        destroy();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
        m_format = other.m_format;
        m_written = std::move(other.m_written);
    }
    return *this;
}

void GpuTimer::destroy() noexcept {
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

void GpuTimer::begin(VkCommandBuffer commands, std::uint32_t frame) noexcept {
    if (m_pool == VK_NULL_HANDLE) {
        return;
    }
    APEX_ASSERT(frame < m_written.size(), "No such frame in flight");
    vkCmdResetQueryPool(commands, m_pool, 2 * frame, 2);
    vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, m_pool, 2 * frame);
}

void GpuTimer::end(VkCommandBuffer commands, std::uint32_t frame) noexcept {
    if (m_pool == VK_NULL_HANDLE) {
        return;
    }
    vkCmdWriteTimestamp2(commands, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, m_pool, 2 * frame + 1);
    m_written[frame] = true;
}

std::optional<double> GpuTimer::read(std::uint32_t frame) {
    if (m_pool == VK_NULL_HANDLE || !m_written[frame]) {
        return std::nullopt;
    }
    m_written[frame] = false;
    std::array<std::uint64_t, 2> ticks{};
    // No wait flag: the fence was waited for, the results are there. VK_NOT_READY would give nothing.
    if (vkGetQueryPoolResults(m_device, m_pool, 2 * frame, 2, sizeof(ticks), ticks.data(),
                              sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS) {
        return std::nullopt;
    }
    return elapsedSeconds(ticks[0], ticks[1], m_format);
}

} // namespace apex::graphics
