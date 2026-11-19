#include "Debug/FrameTimeStats.h"

#include "Core/Assert.h"

#include <algorithm>
#include <cmath>

namespace apex::debug {

FrameTimeStats::FrameTimeStats(double windowSeconds) : m_windowSeconds(windowSeconds) {
    APEX_ASSERT(std::isfinite(windowSeconds) && windowSeconds > 0.0,
                "The window must be a positive duration");
}

void FrameTimeStats::addFrame(double frameSeconds) {
    const double frame = std::isfinite(frameSeconds) ? std::max(frameSeconds, 0.0) : 0.0;
    m_frames.push_back(frame);
    m_totalSeconds += frame;
    // The newest frame always stays, even when it alone is longer than the window.
    while (m_frames.size() > 1 && m_totalSeconds - m_frames.front() >= m_windowSeconds) {
        m_totalSeconds -= m_frames.front();
        m_frames.pop_front();
    }
    // Removing values one by one accumulates rounding: never let the total drift below zero.
    m_totalSeconds = std::max(m_totalSeconds, 0.0);
}

double FrameTimeStats::averageSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : m_totalSeconds / static_cast<double>(m_frames.size());
}

double FrameTimeStats::maxSeconds() const noexcept {
    return m_frames.empty() ? 0.0 : std::ranges::max(m_frames);
}

double FrameTimeStats::framesPerSecond() const noexcept {
    return m_totalSeconds > 0.0 ? static_cast<double>(m_frames.size()) / m_totalSeconds : 0.0;
}

std::size_t FrameTimeStats::frameCount() const noexcept {
    return m_frames.size();
}

} // namespace apex::debug
