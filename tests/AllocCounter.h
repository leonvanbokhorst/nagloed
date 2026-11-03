#pragma once

// Allocation tracking used by tests to prove the realtime contract: process(),
// setParams() and getVisual() must not allocate. A counting global operator new
// is installed by AllocCounter.cpp; tracking is only active inside the test.
namespace nagl_test
{
void setAllocationTracking (bool shouldTrack) noexcept;
void resetAllocationCount() noexcept;
long long allocationCount() noexcept;
} // namespace nagl_test
