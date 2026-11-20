#include "Debug/FrameTimeStats.h"

#include "Core/Assert.h"

#include <algorithm>
#include <cmath>

namespace apex::debug {

namespace {

[[nodiscard]] double sanitize(double seconds) noexcept {
    return std::isfinite(seconds) ? std::max(seconds, 0.0) : 0.0;
}

} // namespace

FrameTimeStats::FrameTimeStats(double windowSeconds) : m_windowSeconds(windowSeconds) {
    APEX_ASSERT(std::isfinite(windowSeconds) && windowSeconds > 0.0,
                "The window must be a positive duration");
}

void FrameTimeStats::addFrame(const FrameSample& sample) {
    const double frame = sanitize(sample.frameSeconds);
    const double cpu = std::min(sanitize(sample.cpuSeconds), frame);
    std::optional<double> gpu;
    if (sample.gpuSeconds) {
        gpu = sanitize(*sample.gpuSeconds);
        m_totalGpuSeconds += *gpu;
        ++m_gpuFrameCount;
    }
    m_frames.push_back({frame, cpu, gpu});
    m_totalFrameSeconds += frame;
    m_totalCpuSeconds += cpu;
    // The newest frame always stays, even when it alone is longer than the window.
    while (m_frames.size() > 1 && m_totalFrameSeconds - m_frames.front().frameSeconds >= m_windowSeconds) {
        m_totalFrameSeconds -= m_frames.front().frameSeconds;
        m_totalCpuSeconds -= m_frames.front().cpuSeconds;
        if (m_frames.front().gpuSeconds) {
            m_totalGpuSeconds -= *m_frames.front().gpuSeconds;
            --m_gpuFrameCount;
        }
        m_frames.pop_front();
    }
    // Removing values one by one accumulates rounding: never let a total drift below zero.
    m_totalFrameSeconds = std::max(m_totalFrameSeconds, 0.0);
    m_totalCpuSeconds = std::max(m_totalCpuSeconds, 0.0);
    m_totalGpuSeconds = m_gpuFrameCount > 0 ? std::max(m_totalGpuSeconds, 0.0) : 0.0;
}

double FrameTimeStats::framesPerSecond() const noexcept {
    return m_totalFrameSeconds > 0.0 ? static_cast<double>(m_frames.size()) / m_totalFrameSeconds : 0.0;
}

double FrameTimeStats::averageFrameSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : m_totalFrameSeconds / static_cast<double>(m_frames.size());
}

double FrameTimeStats::maxFrameSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : std::ranges::max(m_frames, {}, &FrameSample::frameSeconds).frameSeconds;
}

double FrameTimeStats::averageCpuSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : m_totalCpuSeconds / static_cast<double>(m_frames.size());
}

double FrameTimeStats::maxCpuSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : std::ranges::max(m_frames, {}, &FrameSample::cpuSeconds).cpuSeconds;
}

std::optional<double> FrameTimeStats::averageGpuSeconds() const noexcept {
    if (m_gpuFrameCount == 0) {
        return std::nullopt;
    }
    return m_totalGpuSeconds / static_cast<double>(m_gpuFrameCount);
}

std::optional<double> FrameTimeStats::maxGpuSeconds() const noexcept {
    std::optional<double> result;
    for (const FrameSample& sample : m_frames) {
        if (sample.gpuSeconds && (!result || *sample.gpuSeconds > *result)) {
            result = sample.gpuSeconds;
        }
    }
    return result;
}

std::size_t FrameTimeStats::frameCount() const noexcept {
    return m_frames.size();
}

} // namespace apex::debug
