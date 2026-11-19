#pragma once

#include "Resources/ResourcePool.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace apex::graphics {

// Internal to Graphics, without Vulkan. The GPU side of a resources::ResourcePool (decision 0014): one
// entry per slot, created when the resource becomes Ready, released when it is unloaded or its slot is
// reused. A creation that failed is not retried while that resource lives. Nothing points into the
// pool: entries are found again by handle every frame (G-03).
template <typename Handle, typename CpuValue, typename GpuValue>
class ResourceMirror {
public:
    // create(handle, const CpuValue&) -> std::optional<GpuValue>; release(GpuValue&&). Does nothing when
    // the pool did not change since the last call.
    template <typename Create, typename Release>
    void sync(const resources::ResourcePool<Handle, CpuValue>& pool, Create&& create, Release&& release) {
        if (pool.revision() == m_revision) {
            return;
        }
        m_revision = pool.revision();
        std::vector<bool> live(m_entries.size(), false);
        pool.forEach([&](Handle handle, resources::ResourceState state, const CpuValue* value) {
            if (handle.index >= m_entries.size()) {
                m_entries.resize(handle.index + 1);
                live.resize(handle.index + 1, false);
            }
            std::optional<Entry>& entry = m_entries[handle.index];
            if (entry && entry->generation != handle.generation) {
                releaseEntry(entry, release);
            }
            if (!entry && state == resources::ResourceState::Ready) {
                entry = Entry{.generation = handle.generation, .value = create(handle, *value)};
            }
            live[handle.index] = entry.has_value();
        });
        for (std::size_t index = 0; index < m_entries.size(); ++index) {
            if (!live[index] && m_entries[index]) {
                releaseEntry(m_entries[index], release);
            }
        }
    }

    // nullptr when the resource is not ready, failed on the GPU, was unloaded, or was never synced.
    [[nodiscard]] const GpuValue* find(Handle handle) const noexcept {
        if (handle.index >= m_entries.size()) {
            return nullptr;
        }
        const std::optional<Entry>& entry = m_entries[handle.index];
        return entry && entry->generation == handle.generation && entry->value ? &*entry->value : nullptr;
    }

    template <typename Release>
    void releaseAll(Release&& release) {
        for (std::optional<Entry>& entry : m_entries) {
            if (entry) {
                releaseEntry(entry, release);
            }
        }
        m_revision = kNeverSynced;
    }

private:
    static constexpr std::uint64_t kNeverSynced = std::numeric_limits<std::uint64_t>::max();

    struct Entry {
        std::uint32_t generation;
        std::optional<GpuValue> value;
    };

    template <typename Release>
    static void releaseEntry(std::optional<Entry>& entry, Release&& release) {
        if (entry->value) {
            release(std::move(*entry->value));
        }
        entry.reset();
    }

    std::vector<std::optional<Entry>> m_entries;
    std::uint64_t m_revision = kNeverSynced;
};

} // namespace apex::graphics
