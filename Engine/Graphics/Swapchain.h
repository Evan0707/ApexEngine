#pragma once

#include "Core/Error.h"
#include "Graphics/Device.h"

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace apex::graphics {

struct SwapchainConfig {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // On: FIFO, the only mode every driver supports. Off: mailbox, then immediate, then FIFO.
    bool vsync = true;
};

// Internal to Graphics. The swapchain, its image views and one "render finished" semaphore per image.
// The semaphore is per image, not per frame in flight: the presentation engine may still wait on it
// when the frame slot comes back, as long as the image has not been acquired again.
class Swapchain {
public:
    // oldSwapchain may be VK_NULL_HANDLE. It stays valid and is destroyed by its owner.
    [[nodiscard]] static core::Result<Swapchain> create(const Device& device, const SwapchainConfig& config,
                                                        VkSwapchainKHR oldSwapchain);

    Swapchain() = default;
    ~Swapchain();
    Swapchain(Swapchain&& other) noexcept;
    Swapchain& operator=(Swapchain&& other) noexcept;
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept;
    [[nodiscard]] VkFormat format() const noexcept;
    [[nodiscard]] VkExtent2D extent() const noexcept;
    [[nodiscard]] bool vsync() const noexcept;
    [[nodiscard]] std::uint32_t imageCount() const noexcept;
    [[nodiscard]] VkImage image(std::uint32_t index) const noexcept;
    [[nodiscard]] VkImageView imageView(std::uint32_t index) const noexcept;
    [[nodiscard]] VkSemaphore renderFinished(std::uint32_t index) const noexcept;

private:
    void destroy() noexcept;

    vkb::Swapchain m_swapchain;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkSemaphore> m_renderFinished;
    bool m_vsync = true;
};

} // namespace apex::graphics
