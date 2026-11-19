#include "Graphics/Swapchain.h"

#include "Core/Assert.h"

#include <format>
#include <string>
#include <utility>

namespace apex::graphics {

namespace {

template <typename T>
[[nodiscard]] std::unexpected<core::Error> swapchainError(std::string_view what,
                                                          const vkb::Result<T>& result) {
    std::string message = std::format("{}: {}", what, result.error().message());
    if (result.vk_result() != VK_SUCCESS) {
        message += std::format(" (VkResult {})", static_cast<int>(result.vk_result()));
    }
    return core::makeError(core::ErrorCode::GpuFailure, std::move(message));
}

} // namespace

core::Result<Swapchain> Swapchain::create(const Device& device, const SwapchainConfig& config,
                                          VkSwapchainKHR oldSwapchain) {
    APEX_ASSERT(config.width > 0 && config.height > 0, "A swapchain cannot be empty: skip the frame instead");

    // sRGB swapchain (spec 3.3): the hardware converts the linear colors written by the shaders.
    vkb::SwapchainBuilder builder(device.bootstrapDevice());
    builder.set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .add_fallback_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_extent(config.width, config.height)
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .set_old_swapchain(oldSwapchain);
    if (config.vsync) {
        builder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
    } else {
        builder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
    }

    vkb::Result<vkb::Swapchain> built = builder.build();
    if (!built) {
        return swapchainError("Cannot create the swapchain", built);
    }
    Swapchain swapchain;
    swapchain.m_swapchain = std::move(*built);
    swapchain.m_vsync = config.vsync;

    vkb::Result<std::vector<VkImage>> images = swapchain.m_swapchain.get_images();
    if (!images) {
        return swapchainError("Cannot get the swapchain images", images);
    }
    swapchain.m_images = std::move(*images);

    vkb::Result<std::vector<VkImageView>> views = swapchain.m_swapchain.get_image_views();
    if (!views) {
        return swapchainError("Cannot create the swapchain image views", views);
    }
    swapchain.m_imageViews = std::move(*views);

    const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (std::size_t index = 0; index < swapchain.m_images.size(); ++index) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        if (const VkResult result = vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &semaphore);
            result != VK_SUCCESS) {
            return core::makeError(
                core::ErrorCode::GpuFailure,
                std::format("Cannot create the swapchain semaphores: VkResult {}", static_cast<int>(result)));
        }
        swapchain.m_renderFinished.push_back(semaphore);
    }
    return swapchain;
}

Swapchain::~Swapchain() {
    destroy();
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : m_swapchain(std::exchange(other.m_swapchain, {})), m_images(std::move(other.m_images)),
      m_imageViews(std::move(other.m_imageViews)), m_renderFinished(std::move(other.m_renderFinished)),
      m_vsync(other.m_vsync) {
    other.m_images.clear();
    other.m_imageViews.clear();
    other.m_renderFinished.clear();
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if (this != &other) {
        destroy();
        m_swapchain = std::exchange(other.m_swapchain, {});
        m_images = std::exchange(other.m_images, {});
        m_imageViews = std::exchange(other.m_imageViews, {});
        m_renderFinished = std::exchange(other.m_renderFinished, {});
        m_vsync = other.m_vsync;
    }
    return *this;
}

void Swapchain::destroy() noexcept {
    for (const VkSemaphore semaphore : m_renderFinished) {
        vkDestroySemaphore(m_swapchain.device, semaphore, nullptr);
    }
    m_renderFinished.clear();
    if (!m_imageViews.empty()) {
        m_swapchain.destroy_image_views(m_imageViews);
        m_imageViews.clear();
    }
    m_images.clear();
    if (m_swapchain.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(m_swapchain);
        m_swapchain = {};
    }
}

VkSwapchainKHR Swapchain::handle() const noexcept {
    return m_swapchain.swapchain;
}

VkFormat Swapchain::format() const noexcept {
    return m_swapchain.image_format;
}

VkExtent2D Swapchain::extent() const noexcept {
    return m_swapchain.extent;
}

bool Swapchain::vsync() const noexcept {
    return m_vsync;
}

std::uint32_t Swapchain::imageCount() const noexcept {
    return static_cast<std::uint32_t>(m_images.size());
}

VkImage Swapchain::image(std::uint32_t index) const noexcept {
    APEX_ASSERT(index < m_images.size(), "Swapchain image index out of range");
    return m_images[index];
}

VkImageView Swapchain::imageView(std::uint32_t index) const noexcept {
    APEX_ASSERT(index < m_imageViews.size(), "Swapchain image index out of range");
    return m_imageViews[index];
}

VkSemaphore Swapchain::renderFinished(std::uint32_t index) const noexcept {
    APEX_ASSERT(index < m_renderFinished.size(), "Swapchain image index out of range");
    return m_renderFinished[index];
}

} // namespace apex::graphics
