#pragma once

#include "Core/Math.h"

namespace apex::graphics {

// Internal to Graphics, and the only place that knows the Vulkan clip space (spec 3.3): view space is
// right-handed and looks towards -Z; depth goes from 0 at the near plane to 1 at the far plane; Y is
// flipped so that +Y in view space is up on screen. Counter-clockwise faces stay counter-clockwise.
[[nodiscard]] glm::mat4 perspectiveProjection(float verticalFovRadians, float aspectRatio, float nearPlane,
                                              float farPlane) noexcept;

// Same conventions, parallel rays: the box from -halfWidth to halfWidth, -halfHeight to halfHeight, and
// nearPlane to farPlane in front of the view, fills the clip volume. The shadow map's projection.
[[nodiscard]] glm::mat4 orthographicProjection(float halfWidth, float halfHeight, float nearPlane,
                                               float farPlane) noexcept;

} // namespace apex::graphics
