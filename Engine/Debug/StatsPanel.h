#pragma once

#include "Debug/FrameTimeStats.h"
#include "Debug/LogHistory.h"
#include "Graphics/FrameCounters.h"
#include "Graphics/GpuMemoryStats.h"

#include <cstddef>

namespace apex::debug {

// What the debug panel shows (spec 5.9). Filled by the application each frame.
struct DebugPanelData {
    const FrameTimeStats& frameTimes;
    graphics::FrameCounters counters;
    graphics::GpuMemoryStats memory;
    // 0 until the World module (phase 5).
    std::size_t entityCount = 0;
    // Physics bodies, and those that are not asleep.
    std::size_t bodyCount = 0;
    std::size_t activeBodyCount = 0;
    const LogHistory& log;
};

// Settings the panel can change (decision 0011). Owned by the application, which applies them.
struct DebugControls {
    float verticalFovDegrees = 70.0f;
    float minVerticalFovDegrees = 50.0f;
    float maxVerticalFovDegrees = 100.0f;
};

// Internal to Debug: draws the panel with Dear ImGui, inside DebugUi::buildFrame. open becomes false when
// the panel is closed with its own button.
void drawStatsPanel(const DebugPanelData& data, DebugControls& controls, bool& open);

} // namespace apex::debug
