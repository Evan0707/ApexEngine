#include "Debug/StatsPanel.h"

#include "Core/LogSink.h"
#include "Debug/ColorSpace.h"

#include <imgui.h>

#include <optional>
#include <string>
#include <vector>

namespace apex::debug {

namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kBytesPerMebibyte = 1024.0 * 1024.0;

// Given in sRGB like the rest of the style, written to an sRGB swapchain.
[[nodiscard]] ImVec4 interfaceColor(float red, float green, float blue) noexcept {
    return {srgbToLinear(red), srgbToLinear(green), srgbToLinear(blue), 1.0f};
}

void drawFrameSection(const FrameTimeStats& frameTimes) {
    ImGui::Text("%.0f FPS", frameTimes.framesPerSecond());
    ImGui::Text("CPU time per frame: %.2f ms average, %.2f ms max",
                frameTimes.averageCpuSeconds() * kMillisecondsPerSecond,
                frameTimes.maxCpuSeconds() * kMillisecondsPerSecond);
    if (const std::optional<double> average = frameTimes.averageGpuSeconds()) {
        ImGui::Text("GPU time per frame: %.2f ms average, %.2f ms max", *average * kMillisecondsPerSecond,
                    frameTimes.maxGpuSeconds().value_or(0.0) * kMillisecondsPerSecond);
    } else {
        ImGui::TextUnformatted("GPU time per frame: n/a");
    }
    ImGui::Text("Frame time: %.2f ms average, %.2f ms max",
                frameTimes.averageFrameSeconds() * kMillisecondsPerSecond,
                frameTimes.maxFrameSeconds() * kMillisecondsPerSecond);
    ImGui::TextDisabled("Over the last second (%zu frames)", frameTimes.frameCount());
}

void drawSceneSection(const DebugPanelData& data) {
    ImGui::Text("Entities: %zu", data.entityCount);
    ImGui::Text("Physics bodies: %zu (%zu awake)", data.bodyCount, data.activeBodyCount);
    ImGui::Text("Objects drawn: %u (shadow pass: %u)", data.counters.drawCalls,
                data.counters.shadowDrawCalls);
    ImGui::Text("Triangles: %llu", static_cast<unsigned long long>(data.counters.triangles));
    ImGui::Text("GPU memory: %.0f MiB used, %.0f MiB budget",
                static_cast<double>(data.memory.usedBytes) / kBytesPerMebibyte,
                static_cast<double>(data.memory.budgetBytes) / kBytesPerMebibyte);
}

void drawCameraSection(DebugControls& controls) {
    ImGui::SliderFloat("Field of view", &controls.verticalFovDegrees, controls.minVerticalFovDegrees,
                       controls.maxVerticalFovDegrees, "%.0f degrees", ImGuiSliderFlags_AlwaysClamp);
}

// Ranges of the sliders; the configuration may go beyond them.
constexpr float kMaxConstantBias = 0.2f;
constexpr float kMaxSlopeBias = 8.0f;

void drawShadowSection(DebugControls& controls) {
    ImGui::Checkbox("Shadows (F4)", &controls.shadows);
    ImGui::BeginDisabled(!controls.shadows);
    ImGui::SliderFloat("Constant bias", &controls.shadowConstantBias, 0.0f, kMaxConstantBias, "%.3f m");
    ImGui::SliderFloat("Slope bias", &controls.shadowSlopeBias, 0.0f, kMaxSlopeBias, "%.2f");
    ImGui::EndDisabled();
}

void drawToneMappingSection(DebugControls& controls) {
    ImGui::SliderFloat("Exposure", &controls.exposureStops, controls.minExposureStops,
                       controls.maxExposureStops, "%+.1f stops", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Checkbox("Tonemapping (ACES)", &controls.toneMapping);
}

void drawLogSection(const LogHistory& log) {
    const std::vector<LogEntry> entries = log.snapshot();
    ImGui::Text("Warnings and errors: %zu (last %zu kept)", entries.size(), log.capacity());
    if (!ImGui::BeginChild("Log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                           ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }
    const ImVec4 warningColor = interfaceColor(1.0f, 0.8f, 0.3f);
    const ImVec4 errorColor = interfaceColor(1.0f, 0.4f, 0.4f);
    // Follows new lines only when the view is already at the bottom: reading older lines stays possible.
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
    for (const LogEntry& entry : entries) {
        const std::string line = core::formatRecord({entry.level, entry.time, entry.message});
        const bool isError = entry.level >= core::LogLevel::Error;
        ImGui::PushStyleColor(ImGuiCol_Text, isError ? errorColor : warningColor);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    if (atBottom) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

} // namespace

void drawStatsPanel(const DebugPanelData& data, DebugControls& controls, bool& open) {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug (F1)", &open)) {
        drawFrameSection(data.frameTimes);
        ImGui::Separator();
        drawSceneSection(data);
        ImGui::Separator();
        drawCameraSection(controls);
        ImGui::Separator();
        drawShadowSection(controls);
        ImGui::Separator();
        drawToneMappingSection(controls);
        ImGui::Separator();
        drawLogSection(data.log);
    }
    ImGui::End();
}

} // namespace apex::debug
