#include "Graphics/GpuTimestamps.h"

namespace apex::graphics {

std::optional<double> elapsedSeconds(std::uint64_t begin, std::uint64_t end,
                                     const TimestampFormat& format) noexcept {
    if (!format.supported()) {
        return std::nullopt;
    }
    const std::uint64_t mask =
        format.validBits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << format.validBits) - 1;
    // Unsigned subtraction, then the mask: a counter that wrapped once still gives the right count.
    const std::uint64_t ticks = ((end & mask) - (begin & mask)) & mask;
    constexpr double kSecondsPerNanosecond = 1e-9;
    return static_cast<double>(ticks) * format.nanosecondsPerTick * kSecondsPerNanosecond;
}

} // namespace apex::graphics
