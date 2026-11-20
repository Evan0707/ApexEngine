#pragma once

#include <cstdint>
#include <optional>

namespace apex::graphics {

// What the last rendered frame drew (spec 5.9: objects drawn, triangles).
struct FrameCounters {
    std::uint32_t drawCalls = 0;
    std::uint64_t triangles = 0;
    // Time the CPU spent blocked on the GPU or the presentation engine (fence, acquire, present):
    // with vsync on, most of the frame. Subtracted from the frame time to get the CPU time.
    double waitSeconds = 0.0;
    // From the first to the last command of a frame, measured by the GPU (decision 0023). It belongs to an
    // earlier frame, the last one that used the same frame in flight. Nothing when the GPU writes no
    // timestamp.
    std::optional<double> gpuSeconds;
};

} // namespace apex::graphics
