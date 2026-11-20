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
    // Shadows (decision 0021): on or off (also F4), depth bias in meters and slope factor.
    bool shadows = true;
    float shadowConstantBias = 0.02f;
    float shadowSlopeBias = 2.0f;
    // HDR output (decision 0021): exposure in stops and the ACES curve.
    float exposureStops = -1.0f;
    float minExposureStops = -4.0f;
    float maxExposureStops = 4.0f;
    bool toneMapping = true;
    // Image-based lighting and sky (decision 0022); off: the constant ambient term.
    bool imageBasedLighting = true;
    // Wireframe of the physics shapes (decision 0023), also F2.
    bool colliders = false;
};

// Internal to Debug: draws the panel with Dear ImGui, inside DebugUi::buildFrame. open becomes false when
// the panel is closed with its own button.
void drawStatsPanel(const DebugPanelData& data, DebugControls& controls, bool& open);

} // namespace apex::debug
