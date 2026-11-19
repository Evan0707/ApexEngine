#pragma once

#include <cstdint>
#include <vector>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. One object to draw, once its handles are resolved.
struct DrawItem {
    // Position in RenderScene::objects.
    std::uint32_t objectIndex;
    std::uint32_t materialIndex;
    std::uint32_t meshIndex;
};

// Sorts by material, then by mesh, keeping the scene order inside a group (spec 5.4): the draw
// commands are emitted in this order, and consecutive draws of a mesh share its buffer bindings.
void sortDrawItems(std::vector<DrawItem>& items);

} // namespace apex::graphics
