#pragma once

#include "Core/Math.h"

namespace apex::core {

// A position in world space, in meters (G-07). Converted to render space in one place only, the
// extraction, as matrices relative to the camera. May become double precision (spec 4.5).
using WorldPosition = glm::vec3;

} // namespace apex::core
