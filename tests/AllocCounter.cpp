// Counting replacements for the global allocation functions.
//
// Only active when a test enables tracking, so the rest of the test binary
// behaves normally. This is the empirical proof of the engine's "no allocation
// on the audio thread" contract.
#include "AllocCounter.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{
std::atomic<long long> gAllocationCount { 0 };
std::atomic<bool>      gTracking { false };

inline void noteAllocation() noexcept
{
    if (gTracking.load (std::memory_order_relaxed))
        gAllocationCount.fetch_add (1, std::memory_order_relaxed);
}
} // namespace

namespace nagl_test
{
void setAllocationTracking (bool shouldTrack) noexcept { gTracking.store (shouldTrack); }
void resetAllocationCount() noexcept { gAllocationCount.store (0); }
long long allocationCount() noexcept { return gAllocationCount.load(); }
} // namespace nagl_test

void* operator new (std::size_t size)
{
    noteAllocation();
    void* p = std::malloc (size == 0 ? 1 : size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[] (std::size_t size) { return ::operator new (size); }

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

void* operator new (std::size_t size, std::align_val_t alignment)
{
    noteAllocation();
    const std::size_t a = (std::size_t) alignment;
    const std::size_t request = (size == 0 ? a : size);
    void* p = std::aligned_alloc (a, ((request + a - 1) / a) * a);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
