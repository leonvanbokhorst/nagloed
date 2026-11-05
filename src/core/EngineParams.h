#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Gloop.h"

namespace nagloed
{

// Plain-old-data parameter snapshot handed to the DSP engine once per block.
// The DSP layer never touches APVTS / juce::AudioProcessor: it only sees this.
// All values are in real-world units (Hz, ms, dB, semitones, 0..1).
struct EngineParams
{
    // --- Sintel: grain engine -----------------------------------------------
    float density      = 30.0f;   // grains per second
    float sizeMs       = 80.0f;   // grain length in ms
    float spray        = 0.25f;   // 0..1 source position scatter
    float scan         = 0.0f;    // -1..1 source playhead speed (0 = frozen)
    float pitchSemis   = 0.0f;    // -24..+24
    float pitchSpread  = 0.0f;    // 0..24 semitones of per-grain randomness
    float reverse      = 0.0f;    // 0..1 probability a grain plays backwards
    float jitter       = 0.3f;    // 0..1 timing randomness
    float shape        = 0.15f;   // 0..1 Hann -> expodec window morph
    float width        = 0.5f;    // 0..1 stereo width
    float feedback     = 0.0f;    // 0..0.95 grain feedback into capture buffer
    float focus        = 0.5f;    // 0..1 centre of capture buffer
    bool  freeze       = false;   // stop recording new input
    int   tuneMode     = 0;       // index into kTuneNames
    int   sourceMode   = 0;       // 0 input, 1 noise, 2 drone

    // --- Ruimte: space -------------------------------------------------------
    float spaceMix     = 0.35f;
    float spaceSize    = 0.6f;
    float spaceDamp    = 0.4f;
    float spaceMod     = 0.3f;

    // --- EAR: analog character ----------------------------------------------
    float driveDb      = 6.0f;
    float bias         = 0.15f;
    float tone         = 0.5f;
    float cutoffHz     = 9000.0f;
    float reso         = 0.15f;
    float drift        = 0.25f;
    float wow          = 0.2f;
    float tape         = 0.35f;
    float lofi         = 0.0f;
    float hiss         = 0.06f;
    float inputDb      = 0.0f;
    float outputDb     = 0.0f;
    float mix          = 0.65f;

    // --- Gesture -------------------------------------------------------------
    float macroGlow    = 0.3f;
    float macroDrift   = 0.3f;
    float macroGrain   = 0.5f;
    float macroSpace   = 0.4f;
    float macroShimmer = 0.2f;

    // --- Modulation ----------------------------------------------------------
    float lfoRate      = 0.15f;
    int   lfoShape     = 0;
    int   lfoDest      = 0;
    float lfoAmount    = 0.0f;
    float autoMotion   = 0.15f;
    int   pressureDest = 0;
    int   slideDest    = 0;
    float expression   = 1.0f;

    // --- Gloop: eight asynchronous gesture loopers --------------------------
    // target/amount/speed arrive from the APVTS; mode is performance state that
    // the processor injects from atomics (it is deliberately not automatable).
    std::array<gloop::LaneParams, gloop::kNumLanes> gloopLanes {};
    float gloopSmoothing = 0.35f;      // 0..1 -> ~8..260 ms glide
    std::uint32_t gloopRandomize = 0;  // increments on a Randomize press

    // Convenience derived values used by the engine.
    float sizeSeconds() const noexcept { return std::clamp (sizeMs, 1.0f, 2000.0f) * 0.001f; }
    float densityHz()   const noexcept { return std::clamp (density, 0.1f, 400.0f); }
    float driveGain()   const noexcept { return std::pow (10.0f, driveDb / 20.0f); }
    float inputGain()   const noexcept { return std::pow (10.0f, inputDb / 20.0f); }
    float outputGain()  const noexcept { return std::pow (10.0f, outputDb / 20.0f); }
    bool  tuneActive()  const noexcept { return tuneMode > 0; }
};

} // namespace nagloed
