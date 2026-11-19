#pragma once

#include "Platform/Window.h"

namespace apex::graphics {

enum class FrameDecision {
    // The window has no area (minimized): nothing is rendered nor presented (spec 5.2).
    Skip,
    Render,
    RecreateThenRender,
};

// Internal to Graphics, without Vulkan. Decides at the start of each frame whether the swapchain
// must be recreated. Tested without a GPU.
class SwapchainState {
public:
    // Acquisition or presentation returned VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR.
    void markOutOfDate() noexcept;

    // The swapchain was (re)created for this window size.
    void recreated(platform::PixelSize size) noexcept;

    // Only the size at decision time counts: several resizes between two frames give one recreation.
    [[nodiscard]] FrameDecision decide(platform::PixelSize windowSize) const noexcept;

private:
    bool m_hasSwapchain = false;
    bool m_outOfDate = false;
    platform::PixelSize m_size;
};

} // namespace apex::graphics
