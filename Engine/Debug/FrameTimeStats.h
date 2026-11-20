#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace apex::debug {

struct FrameSample {
    // From the start of the previous frame to the start of this one.
    double frameSeconds = 0.0;
    // Part of the frame the CPU spent working, without waiting for the GPU or the presentation engine.
    double cpuSeconds = 0.0;
    // Measured by the GPU, for an earlier frame (graphics::FrameCounters). Nothing when unknown.
    std::optional<double> gpuSeconds;
};

// Frame statistics over a sliding window of frame time, one second by default (spec 5.9: FPS, CPU time
// per frame, average and maximum over one second; P1: GPU time).
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
    // Over the frames of the window that have a GPU time. Nothing when none has.
    [[nodiscard]] std::optional<double> averageGpuSeconds() const noexcept;
    [[nodiscard]] std::optional<double> maxGpuSeconds() const noexcept;
    [[nodiscard]] std::size_t frameCount() const noexcept;

private:
    double m_windowSeconds;
    std::deque<FrameSample> m_frames;
    double m_totalFrameSeconds = 0.0;
    double m_totalCpuSeconds = 0.0;
    double m_totalGpuSeconds = 0.0;
    std::size_t m_gpuFrameCount = 0;
};

} // namespace apex::debug
