#pragma once

#include "Core/FixedTimestep.h"
#include "Core/FramePhases.h"

#include <cstdint>

namespace apex::core {

class MainLoop {
public:
    explicit MainLoop(const FixedTimestepConfig& config = {});

    // Runs one frame of the given duration. Returns false once processInput asked to quit.
    [[nodiscard]] bool runFrame(FramePhases& phases, double frameSeconds);

    // Measures real frame time with a steady clock and runs frames until processInput asks to quit.
    void run(FramePhases& phases);

private:
    FixedTimestep m_timestep;
    std::uint64_t m_nextTick = 0;
};

} // namespace apex::core
