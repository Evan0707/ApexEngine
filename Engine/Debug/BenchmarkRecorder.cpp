#include "Debug/BenchmarkRecorder.h"

#include <algorithm>
#include <cmath>

namespace apex::debug {

namespace {

constexpr double kMillisecondsPerSecond = 1000.0;

} // namespace

void BenchmarkRecorder::addFrame(double frameSeconds) {
    const double valid = std::isfinite(frameSeconds) && frameSeconds > 0.0 ? frameSeconds : 0.0;
    m_frameSeconds.push_back(valid);
    m_elapsedSeconds += valid;
}

double BenchmarkRecorder::elapsedSeconds() const noexcept {
    return m_elapsedSeconds;
}

BenchmarkSummary BenchmarkRecorder::summary() const {
    BenchmarkSummary summary;
    summary.frameCount = m_frameSeconds.size();
    summary.durationSeconds = m_elapsedSeconds;
    if (m_frameSeconds.empty()) {
        return summary;
    }
    const auto count = static_cast<double>(m_frameSeconds.size());
    if (m_elapsedSeconds > 0.0) {
        summary.averageFramesPerSecond = count / m_elapsedSeconds;
    }
    summary.averageFrameMs = m_elapsedSeconds / count * kMillisecondsPerSecond;

    std::vector<double> sorted = m_frameSeconds;
    std::ranges::sort(sorted);
    // Nearest rank, ceil(0.99 n) in integers: the smallest time that at least 99 % of the frames do not
    // exceed.
    const std::size_t rank = (sorted.size() * 99 + 99) / 100;
    summary.percentile99FrameMs = sorted[rank - 1] * kMillisecondsPerSecond;
    summary.maxFrameMs = sorted.back() * kMillisecondsPerSecond;
    return summary;
}

} // namespace apex::debug
