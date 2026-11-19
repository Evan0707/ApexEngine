#include "Core/MainLoop.h"

#include "Core/Profiling.h"

#include <chrono>

namespace apex::core {

MainLoop::MainLoop(const FixedTimestepConfig& config) : m_timestep(config) {}

bool MainLoop::runFrame(FramePhases& phases, double frameSeconds) {
    {
        APEX_PROFILE_SCOPE("Input");
        if (!phases.processInput()) {
            return false;
        }
    }

    const FrameAdvance advance = m_timestep.advance(frameSeconds);
    {
        APEX_PROFILE_SCOPE("Simulation");
        for (int step = 0; step < advance.steps; ++step) {
            phases.simulate({m_nextTick, m_timestep.config().stepSeconds});
            ++m_nextTick;
        }
    }
    {
        APEX_PROFILE_SCOPE("Extraction");
        phases.extract(advance.alpha);
    }
    {
        APEX_PROFILE_SCOPE("Rendering");
        phases.render();
    }
    {
        APEX_PROFILE_SCOPE("Presentation");
        phases.present();
    }

    APEX_PROFILE_FRAME();
    return true;
}

void MainLoop::run(FramePhases& phases) {
    using Clock = std::chrono::steady_clock;

    Clock::time_point previous = Clock::now();
    bool running = true;
    while (running) {
        const Clock::time_point now = Clock::now();
        const std::chrono::duration<double> frameTime = now - previous;
        previous = now;
        running = runFrame(phases, frameTime.count());
    }
}

} // namespace apex::core
