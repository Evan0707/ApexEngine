#include "Graphics/Renderer.h"

#include "Graphics/Device.h"

#include <utility>

namespace apex::graphics {

core::Result<std::unique_ptr<Renderer>> Renderer::create(platform::Window& window, core::Logger& logger,
                                                         ValidationLog& validation,
                                                         const RendererConfig& config) {
    core::Result<std::unique_ptr<Device>> device = Device::create(
        window, logger, validation, DeviceConfig{config.applicationName, config.enableValidation});
    if (!device) {
        return std::unexpected(device.error());
    }
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(*device));
}

Renderer::Renderer(ConstructionKey /*key*/, std::unique_ptr<Device> device) noexcept
    : m_device(std::move(device)) {}

Renderer::~Renderer() = default;

} // namespace apex::graphics
