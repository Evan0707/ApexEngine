#pragma once

#include <cstdint>

namespace apex::core {

struct FixedStep {
    // 0 for the first step, then +1 per step.
    std::uint64_t tick;
    double deltaSeconds;
};

// The phases of one frame, in call order (spec 5.1). Implemented by the application.
class FramePhases {
public:
    virtual ~FramePhases() = default;

    // Returns false to quit: no other phase runs in that frame.
    [[nodiscard]] virtual bool processInput() = 0;
    // Called zero or more times per frame. Never sees the real frame time (G-05).
    virtual void simulate(const FixedStep& step) = 0;
    virtual void extract(double alpha) = 0;
    virtual void render() = 0;
    virtual void present() = 0;
};

} // namespace apex::core
