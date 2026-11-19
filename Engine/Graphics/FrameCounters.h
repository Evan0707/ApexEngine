#pragma once

#include <cstdint>

namespace apex::graphics {

// What the last rendered frame drew (spec 5.9: objects drawn, triangles).
struct FrameCounters {
    std::uint32_t drawCalls = 0;
    std::uint64_t triangles = 0;
};

} // namespace apex::graphics
