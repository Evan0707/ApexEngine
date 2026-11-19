#pragma once

#include <cstddef>
#include <vector>

namespace apex::debug {

// Milliseconds, except the frame rate.
struct BenchmarkSummary {
    std::size_t frameCount = 0;
    double durationSeconds = 0.0;
    double averageFramesPerSecond = 0.0;
    double averageFrameMs = 0.0;
    // Nearest-rank 99th percentile of the frame times (spec 7.1: at most 20 ms).
    double percentile99FrameMs = 0.0;
    double maxFrameMs = 0.0;
};

// Records every frame time of a measurement (spec 7.1). Like FrameTimeStats, it never reads the clock:
// the application measures the frames.
class BenchmarkRecorder {
public:
    // Negative or non-finite times count as 0.
    void addFrame(double frameSeconds);

    [[nodiscard]] double elapsedSeconds() const noexcept;
    // All zero before the first frame.
    [[nodiscard]] BenchmarkSummary summary() const;

private:
    std::vector<double> m_frameSeconds;
    double m_elapsedSeconds = 0.0;
};

} // namespace apex::debug
