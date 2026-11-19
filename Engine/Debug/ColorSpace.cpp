#include "Debug/ColorSpace.h"

#include <cmath>

namespace apex::debug {

float srgbToLinear(float value) noexcept {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace apex::debug
