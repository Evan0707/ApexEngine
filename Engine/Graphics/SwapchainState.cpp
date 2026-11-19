#include "Graphics/SwapchainState.h"

namespace apex::graphics {

void SwapchainState::markOutOfDate() noexcept {
    m_outOfDate = true;
}

void SwapchainState::recreated(platform::PixelSize size) noexcept {
    m_hasSwapchain = true;
    m_outOfDate = false;
    m_size = size;
}

FrameDecision SwapchainState::decide(platform::PixelSize windowSize) const noexcept {
    if (windowSize.width == 0 || windowSize.height == 0) {
        return FrameDecision::Skip;
    }
    if (!m_hasSwapchain || m_outOfDate || windowSize != m_size) {
        return FrameDecision::RecreateThenRender;
    }
    return FrameDecision::Render;
}

} // namespace apex::graphics
