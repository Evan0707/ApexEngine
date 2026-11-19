#pragma once

namespace apex::debug {

// sRGB transfer function (IEC 61966-2-1), for one channel in [0, 1]. The interface colors are given
// in sRGB and written to an sRGB swapchain, which encodes again: they are converted to linear first.
[[nodiscard]] float srgbToLinear(float value) noexcept;

} // namespace apex::debug
