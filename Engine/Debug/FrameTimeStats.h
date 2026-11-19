#pragma once

#include <cstddef>
#include <deque>

namespace apex::debug {

// Frame times over a sliding window, one second by default (spec 5.9: average and maximum over one second).
class FrameTimeStats {
public:
    explicit FrameTimeStats(double windowSeconds = 1.0);

    // A negative or non-finite duration counts as 0.
    void addFrame(double frameSeconds);

    // 0 until the first frame.
    [[nodiscard]] double averageSeconds() const noexcept;
    [[nodiscard]] double maxSeconds() const noexcept;
    [[nodiscard]] double framesPerSecond() const noexcept;
    [[nodiscard]] std::size_t frameCount() const noexcept;

private:
    double m_windowSeconds;
    std::deque<double> m_frames;
    double m_totalSeconds = 0.0;
};

} // namespace apex::debug
