#include "Core/MainLoop.h"

#include <chrono>

namespace apex::core {

MainLoop::MainLoop(const FixedTimestepConfig& config) : m_timestep(config) {}

bool MainLoop::runFrame(FramePhases& phases, double frameSeconds) {
    if (!phases.processInput()) {
        return false;
    }

    const FrameAdvance advance = m_timestep.advance(frameSeconds);
    for (int step = 0; step < advance.steps; ++step) {
        phases.simulate({m_nextTick, m_timestep.config().stepSeconds});
        ++m_nextTick;
    }

    phases.extract(advance.alpha);
    phases.render();
    phases.present();
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
