#pragma once

#include <cstdint>

namespace apex::graphics {

// GPU memory over the device-local heaps, as reported by VMA (spec 5.9: GPU memory, VMA budget).
struct GpuMemoryStats {
    std::uint64_t usedBytes = 0;
    // Estimate of what the process can use without trouble. Exact only with VK_EXT_memory_budget.
    std::uint64_t budgetBytes = 0;
};

} // namespace apex::graphics
