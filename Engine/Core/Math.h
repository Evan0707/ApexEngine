#pragma once

// The only file that includes GLM (decision 0010): stable extensions only, never glm/gtx.
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace apex::core {

// Right-handed, +Y up, meters (spec 3.3). An unrotated camera looks towards -Z.
inline constexpr glm::vec3 kWorldRight{1.0f, 0.0f, 0.0f};
inline constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};
inline constexpr glm::vec3 kWorldForward{0.0f, 0.0f, -1.0f};

} // namespace apex::core
