#include "Debug/DebugUi.h"

#include "Debug/ImGuiBackend.h"

#include <imgui.h>

#include <cmath>
#include <utility>

namespace apex::debug {

namespace {

// IEC 61966-2-1.
[[nodiscard]] float srgbToLinear(float value) noexcept {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

// ImGui colors are sRGB, and the sRGB swapchain encodes what the shader writes: without this, the
// interface is encoded twice and looks washed out.
void convertStyleToLinear(ImGuiStyle& style) noexcept {
    for (ImVec4& color : style.Colors) {
        color.x = srgbToLinear(color.x);
        color.y = srgbToLinear(color.y);
        color.z = srgbToLinear(color.z);
    }
}

} // namespace

core::Result<std::unique_ptr<DebugUi>> DebugUi::create(platform::Window& window, graphics::Renderer& renderer,
                                                       core::Logger& logger) {
    IMGUI_CHECKVERSION();
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Nothing written next to the executable (decision 0008).
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui::StyleColorsDark();
    convertStyleToLinear(ImGui::GetStyle());

    auto ui = std::make_unique<DebugUi>(ConstructionKey{}, renderer, context);
    core::Result<std::unique_ptr<ImGuiBackend>> backend = ImGuiBackend::create(window, renderer);
    if (!backend) {
        logger.error("{}", backend.error().message);
        return std::unexpected(backend.error());
    }
    ui->m_backend = std::move(*backend);
    renderer.setOverlay(ui.get());
    logger.info("Dear ImGui {}", ImGui::GetVersion());
    return ui;
}

DebugUi::DebugUi(ConstructionKey /*key*/, graphics::Renderer& renderer, ImGuiContext* context) noexcept
    : m_renderer(&renderer), m_context(context) {}

DebugUi::~DebugUi() {
    m_renderer->setOverlay(nullptr);
    m_backend.reset();
    ImGui::DestroyContext(m_context);
}

void DebugUi::buildFrame() {
    m_backend->newFrame();
    ImGui::NewFrame();

    // Placeholder until the statistics panel: proves the interface is drawn over the scene.
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Apex Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text("%.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
    ImGui::End();

    ImGui::Render();
}

bool DebugUi::wantsKeyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool DebugUi::wantsMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void DebugUi::record(const graphics::OverlayCommands& commands) {
    m_backend->render(commands);
}

} // namespace apex::debug
