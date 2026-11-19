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
    m_frames.push_back({frame, cpu});
    m_totalFrameSeconds += frame;
    m_totalCpuSeconds += cpu;
    // The newest frame always stays, even when it alone is longer than the window.
    while (m_frames.size() > 1 && m_totalFrameSeconds - m_frames.front().frameSeconds >= m_windowSeconds) {
        m_totalFrameSeconds -= m_frames.front().frameSeconds;
        m_totalCpuSeconds -= m_frames.front().cpuSeconds;
        m_frames.pop_front();
    }
    // Removing values one by one accumulates rounding: never let a total drift below zero.
    m_totalFrameSeconds = std::max(m_totalFrameSeconds, 0.0);
    m_totalCpuSeconds = std::max(m_totalCpuSeconds, 0.0);
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

std::size_t FrameTimeStats::frameCount() const noexcept {
    return m_frames.size();
}

} // namespace apex::debug
