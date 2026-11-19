#pragma once

namespace apex::graphics {

// Defined in Graphics/VulkanBackendAccess.h, which only the ImGui backend of Debug includes (decision 0009).
struct OverlayCommands;

// Something drawn over the scene, at the end of the main pass: the debug interface (decision 0008).
// No Vulkan type here.
class RenderOverlay {
public:
    virtual ~RenderOverlay() = default;

    // Called by the renderer while it records a frame, after the scene, inside the main pass.
    virtual void record(const OverlayCommands& commands) = 0;
};

} // namespace apex::graphics
