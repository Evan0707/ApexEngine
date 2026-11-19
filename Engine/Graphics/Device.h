#pragma once

#include "Core/Error.h"
#include "Core/Logger.h"
#include "Graphics/ValidationLog.h"
#include "Platform/Window.h"

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>

namespace apex::graphics {

struct DeviceConfig {
    std::string applicationName = "Apex Engine";
    // Requested only: when the layers are not installed, a warning is logged and the device runs without
    // them.
    bool enableValidation = false;
};

// Internal to Graphics. Owns the instance, the window surface and the logical device (decision 0006).
// Not movable: the validation callback keeps the address of the ValidationLog, given at creation.
class Device {
    // Restricts construction to create() while still allowing std::make_unique.
    struct ConstructionKey {
        explicit ConstructionKey() = default;
    };

public:
    // The window, the logger and the validation log must outlive the device.
    [[nodiscard]] static core::Result<std::unique_ptr<Device>> create(platform::Window& window,
                                                                      core::Logger& logger,
                                                                      ValidationLog& validation,
                                                                      const DeviceConfig& config);

    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;
    // Used by the swapchain builder.
    [[nodiscard]] const vkb::Device& bootstrapDevice() const noexcept;

    [[nodiscard]] VkQueue graphicsQueue() const noexcept;
    [[nodiscard]] std::uint32_t graphicsQueueFamily() const noexcept;
    [[nodiscard]] VkQueue presentQueue() const noexcept;

    [[nodiscard]] bool validationEnabled() const noexcept;
    // VK_EXT_memory_budget: exact GPU memory budget in VMA's statistics.
    [[nodiscard]] bool memoryBudgetEnabled() const noexcept;

    // Waits for the GPU to finish all submitted work. Logged on failure, never fatal.
    void waitIdle() noexcept;

    Device(ConstructionKey key, core::Logger& logger) noexcept;

private:
    core::Logger* m_logger;
    vkb::Instance m_instance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    vkb::Device m_device;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    std::uint32_t m_graphicsQueueFamily = 0;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    bool m_validationEnabled = false;
    bool m_memoryBudgetEnabled = false;
};

} // namespace apex::graphics
