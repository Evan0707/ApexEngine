#pragma once

namespace apex::core {

struct FixedTimestepConfig {
    double stepSeconds = 1.0 / 60.0;
    int maxStepsPerFrame = 4;
    double maxFrameSeconds = 0.25;
};

struct FrameAdvance {
    int steps;
    // In [0, 1): how far the current time lies between the last two simulation states.
    double alpha;
};

// Turns variable frame times into a whole number of fixed steps (spec 5.1, G-05).
class FixedTimestep {
public:
    explicit FixedTimestep(const FixedTimestepConfig& config = {});

    // Frame time is clamped to [0, maxFrameSeconds]. Past maxStepsPerFrame, whole steps are dropped:
    // the simulation slows down instead of spiralling.
    [[nodiscard]] FrameAdvance advance(double frameSeconds) noexcept;

    [[nodiscard]] const FixedTimestepConfig& config() const noexcept;

private:
    FixedTimestepConfig m_config;
    double m_accumulator = 0.0;
};

} // namespace apex::core
