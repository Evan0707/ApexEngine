#include "Core/FixedTimestep.h"

#include "Core/Assert.h"

#include <algorithm>
#include <cmath>

namespace apex::core {

namespace {

// Frame times that should add up to a step can land a few ulps below it: accept them as a full step.
constexpr double kStepTolerance = 1e-6;

} // namespace

FixedTimestep::FixedTimestep(const FixedTimestepConfig& config) : m_config(config) {
    APEX_ASSERT(m_config.stepSeconds > 0.0, "Fixed step must be positive");
    APEX_ASSERT(m_config.maxStepsPerFrame >= 1, "At least one step per frame must be allowed");
    APEX_ASSERT(m_config.maxFrameSeconds > 0.0, "Maximum frame time must be positive");
}

FrameAdvance FixedTimestep::advance(double frameSeconds) noexcept {
    // Also rejects NaN.
    if (!(frameSeconds > 0.0)) {
        frameSeconds = 0.0;
    }
    m_accumulator += std::min(frameSeconds, m_config.maxFrameSeconds);

    const double step = m_config.stepSeconds;
    const double threshold = step * (1.0 - kStepTolerance);
    int steps = 0;
    while (steps < m_config.maxStepsPerFrame && m_accumulator >= threshold) {
        m_accumulator -= step;
        ++steps;
    }

    if (m_accumulator >= threshold) {
        m_accumulator = std::fmod(m_accumulator, step);
        if (m_accumulator >= threshold) {
            m_accumulator -= step;
        }
    }

    // The accumulator may sit slightly below zero after a tolerated step.
    return {steps, std::max(m_accumulator / step, 0.0)};
}

const FixedTimestepConfig& FixedTimestep::config() const noexcept {
    return m_config;
}

} // namespace apex::core
