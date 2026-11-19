#pragma once

#include <cstddef>
#include <deque>

namespace apex::debug {

struct FrameSample {
    // From the start of the previous frame to the start of this one.
    double frameSeconds = 0.0;
    // Part of the frame the CPU spent working, without waiting for the GPU or the presentation engine.
    double cpuSeconds = 0.0;
};

// Frame statistics over a sliding window of frame time, one second by default (spec 5.9: FPS, CPU time
// per frame, average and maximum over one second).
class FrameTimeStats {
public:
    explicit FrameTimeStats(double windowSeconds = 1.0);

    // A negative or non-finite duration counts as 0. The CPU time is capped by the frame time.
    void addFrame(const FrameSample& sample);

    // 0 until the first frame.
    [[nodiscard]] double framesPerSecond() const noexcept;
    [[nodiscard]] double averageFrameSeconds() const noexcept;
    [[nodiscard]] double maxFrameSeconds() const noexcept;
    [[nodiscard]] double averageCpuSeconds() const noexcept;
    [[nodiscard]] double maxCpuSeconds() const noexcept;
    [[nodiscard]] std::size_t frameCount() const noexcept;

private:
    double m_windowSeconds;
    std::deque<FrameSample> m_frames;
    double m_totalFrameSeconds = 0.0;
    double m_totalCpuSeconds = 0.0;
};

} // namespace apex::debug
