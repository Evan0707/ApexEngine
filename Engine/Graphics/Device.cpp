#include "Graphics/Device.h"

#include "Core/Version.h"

#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace apex::graphics {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL onValidationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                   VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                                   const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                   void* userData) {
    auto* validation = static_cast<ValidationLog*>(userData);
    const std::string_view message =
        data != nullptr && data->pMessage != nullptr ? data->pMessage : "(no message)";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        validation->report(ValidationSeverity::Error, message);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        validation->report(ValidationSeverity::Warning, message);
    } else {
        validation->report(ValidationSeverity::Info, message);
    }
    // VK_FALSE: the call that triggered the message must not be aborted.
    return VK_FALSE;
}

[[nodiscard]] std::string describe(std::error_code error, VkResult result) {
    if (result == VK_SUCCESS) {
        return error.message();
    }
    return std::format("{} (VkResult {})", error.message(), static_cast<int>(result));
}

template <typename T>
[[nodiscard]] std::unexpected<core::Error> gpuError(core::Logger& logger, std::string_view what,
                                                    const vkb::Result<T>& result) {
    std::string message = std::format("{}: {}", what, describe(result.error(), result.vk_result()));
    for (const std::string& reason : result.detailed_failure_reasons()) {
        message += std::format("\n  {}", reason);
    }
    logger.error("{}", message);
    return core::makeError(core::ErrorCode::GpuFailure, std::move(message));
}

[[nodiscard]] std::string_view deviceTypeName(VkPhysicalDeviceType type) noexcept {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "other";
    }
}

} // namespace

core::Result<std::unique_ptr<Device>> Device::create(platform::Window& window, core::Logger& logger,
                                                     ValidationLog& validation, const DeviceConfig& config) {
    const vkb::Result<vkb::SystemInfo> system = vkb::SystemInfo::get_system_info();
    if (!system) {
        return gpuError(logger, "Cannot load the Vulkan loader", system);
    }

    const bool useValidation = config.enableValidation && system->validation_layers_available;
    if (config.enableValidation && !useValidation) {
        logger.warning(
            "Vulkan validation layers requested but not installed (Vulkan SDK): running without them");
    }

    const core::Result<std::span<const char* const>> windowExtensions = window.vulkanInstanceExtensions();
    if (!windowExtensions) {
        logger.error("{}", windowExtensions.error().message);
        return std::unexpected(windowExtensions.error());
    }

    auto device = std::make_unique<Device>(ConstructionKey{}, logger);

    const core::Version version = core::engineVersion();
    vkb::InstanceBuilder instanceBuilder;
    // Headless: the window extensions come from SDL (decision 0009), not from vk-bootstrap's own list.
    instanceBuilder.set_app_name(config.applicationName)
        .set_engine_name("Apex Engine")
        .set_engine_version(version.major, version.minor, version.patch)
        .require_api_version(1, 3, 0)
        .set_headless(true)
        .enable_extensions(*windowExtensions);
    if (useValidation) {
        instanceBuilder.enable_validation_layers(true)
            .set_debug_callback(&onValidationMessage)
            .set_debug_callback_user_data_pointer(&validation)
            .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    }
    vkb::Result<vkb::Instance> instance = instanceBuilder.build();
    if (!instance) {
        return gpuError(logger, "Cannot create the Vulkan 1.3 instance", instance);
    }
    device->m_instance = std::move(*instance);
    device->m_validationEnabled = useValidation;

    const core::Result<std::uint64_t> surface = window.createVulkanSurface(device->m_instance.instance);
    if (!surface) {
        logger.error("{}", surface.error().message);
        return std::unexpected(surface.error());
    }
    device->m_surface = reinterpret_cast<VkSurfaceKHR>(static_cast<std::uintptr_t>(*surface));

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    vkb::PhysicalDeviceSelector selector(device->m_instance, device->m_surface);
    // require_present is off on a headless instance: turn it back on.
    selector.set_minimum_version(1, 3).set_required_features_13(features13).require_present(true);
    vkb::Result<vkb::PhysicalDevice> physicalDevice = selector.select();
    if (!physicalDevice) {
        return gpuError(logger, "No GPU supports Vulkan 1.3 with dynamic rendering and synchronization2",
                        physicalDevice);
    }

    vkb::Result<vkb::Device> logicalDevice = vkb::DeviceBuilder(*physicalDevice).build();
    if (!logicalDevice) {
        return gpuError(logger, "Cannot create the Vulkan device", logicalDevice);
    }
    device->m_device = std::move(*logicalDevice);

    const vkb::Result<VkQueue> graphicsQueue = device->m_device.get_queue(vkb::QueueType::graphics);
    const vkb::Result<std::uint32_t> graphicsFamily =
        device->m_device.get_queue_index(vkb::QueueType::graphics);
    const vkb::Result<VkQueue> presentQueue = device->m_device.get_queue(vkb::QueueType::present);
    if (!graphicsQueue) {
        return gpuError(logger, "Cannot get the graphics queue", graphicsQueue);
    }
    if (!graphicsFamily) {
        return gpuError(logger, "Cannot get the graphics queue family", graphicsFamily);
    }
    if (!presentQueue) {
        return gpuError(logger, "Cannot get the present queue", presentQueue);
    }
    device->m_graphicsQueue = *graphicsQueue;
    device->m_graphicsQueueFamily = *graphicsFamily;
    device->m_presentQueue = *presentQueue;

    const VkPhysicalDeviceProperties& properties = device->m_device.physical_device.properties;
    logger.info("GPU: {} ({}), Vulkan {}.{}.{}", device->m_device.physical_device.name,
                deviceTypeName(properties.deviceType), VK_API_VERSION_MAJOR(properties.apiVersion),
                VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
    logger.info("Vulkan validation: {}", useValidation ? "enabled" : "disabled");
    return device;
}

Device::Device(ConstructionKey /*key*/, core::Logger& logger) noexcept : m_logger(&logger) {}

Device::~Device() {
    if (m_device.device != VK_NULL_HANDLE) {
        waitIdle();
        vkb::destroy_device(m_device);
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkb::destroy_surface(m_instance, m_surface);
    }
    vkb::destroy_instance(m_instance);
}

VkInstance Device::instance() const noexcept {
    return m_instance.instance;
}

VkPhysicalDevice Device::physicalDevice() const noexcept {
    return m_device.physical_device.physical_device;
}

VkDevice Device::device() const noexcept {
    return m_device.device;
}

VkSurfaceKHR Device::surface() const noexcept {
    return m_surface;
}

const vkb::Device& Device::bootstrapDevice() const noexcept {
    return m_device;
}

VkQueue Device::graphicsQueue() const noexcept {
    return m_graphicsQueue;
}

std::uint32_t Device::graphicsQueueFamily() const noexcept {
    return m_graphicsQueueFamily;
}

VkQueue Device::presentQueue() const noexcept {
    return m_presentQueue;
}

bool Device::validationEnabled() const noexcept {
    return m_validationEnabled;
}

void Device::waitIdle() noexcept {
    if (const VkResult result = vkDeviceWaitIdle(m_device.device); result != VK_SUCCESS) {
        m_logger->error("vkDeviceWaitIdle failed: VkResult {}", static_cast<int>(result));
    }
}

} // namespace apex::graphics
