#pragma once

#include <atomic>
#include <array>

namespace nagloed
{

// Single-writer / single-reader double buffered snapshot.
// The writer (audio thread) never blocks; the reader (UI) never blocks.
template <typename T>
class SnapshotChannel
{
public:
    void publish (const T& value) noexcept
    {
        const auto current = published.load (std::memory_order_relaxed);
        const auto next    = 1u - current;
        buffers[next] = value;
        published.store (next, std::memory_order_release);
    }

    void read (T& out) const noexcept
    {
        const auto current = published.load (std::memory_order_acquire);
        out = buffers[current];
    }

private:
    std::array<T, 2> buffers {};
    std::atomic<unsigned int> published { 0u };
};

} // namespace nagloed
