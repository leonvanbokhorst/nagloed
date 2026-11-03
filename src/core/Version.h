#pragma once

// -----------------------------------------------------------------------------
// NAGLOED — the light that stays in the room after the source is gone.
// -----------------------------------------------------------------------------

namespace nagloed
{
inline constexpr const char* kProductName   = "NAGLOED";
inline constexpr const char* kCompanyName   = "UnlikeNoise Labs";
inline constexpr const char* kVersion       = "1.0.0";
inline constexpr const char* kBundleId      = "com.unlikenoise.nagloed";
inline constexpr int         kVersionHint   = 1;

inline constexpr int         kMaxGrains     = 256;   // ceiling for the grain pool
inline constexpr int         kMaxCaptureSec = 12;    // capture buffer length
inline constexpr int         kMaxBlockSize  = 4096;
} // namespace nagloed
