#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Graphics/ValidationLog.h"
#include "Platform/Window.h"

#include <memory>
#include <string>

namespace apex::graphics {

class Device;

struct RendererConfig {
    std::string applicationName = "Apex Engine";
    // Debug builds turn it on (decision 0006). Ignored, with a warning, when the layers are not installed.
    bool enableValidation = false;
};

// The only entry point of Graphics for the application. No Vulkan type in this header (G-01, G-10).
class Renderer {
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // The window, the logger and the validation log must outlive the renderer.
    [[nodiscard]] static core::Result<std::unique_ptr<Renderer>> create(platform::Window& window,
                                                                        core::Logger& logger,
                                                                        ValidationLog& validation,
                                                                        const RendererConfig& config);

    Renderer(ConstructionKey key, std::unique_ptr<Device> device) noexcept;
    // Waits for the GPU, then destroys every GPU object before the device.
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    std::unique_ptr<Device> m_device;
};

} // namespace apex::graphics
