#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "DspUtil.h"

// -----------------------------------------------------------------------------
// RUIMTE — the space at the end of the hallway.
//
// An 8-line feedback delay network with:
//   * a normalised 8x8 Hadamard (orthogonal, i.e. lossless) mixing matrix so the
//     tail is dense and never has a preferred direction;
//   * mutually incommensurate delay lengths scaled by space.size;
//   * smoothly gliding, gently modulated delay taps (space.mod) for a wide,
//     breathing stereo field;
//   * per-line one-pole damping (space.damp) and a size-dependent RT60 so the
//     space ranges from a tight room to a long, dark hall.
//
// Stability: the Hadamard matrix is orthonormal and the loop gain is clamped
// below 1, so the network is a contraction; damping only removes energy.
// Modulated reads can add a little energy, so the loop gain is capped at 0.97
// and every state update is finite-checked — the tail decays in every case.
// -----------------------------------------------------------------------------
namespace nagloed::dsp
{

struct ReverbControls
{
    float size        = 0.6f;    // 0..1 -> room size / RT60
    float damp        = 0.4f;    // 0..1 -> damping (0 bright, 1 dark)
    float mod         = 0.3f;    // 0..1 -> delay modulation depth
};

class RuimteReverb
{
public:
    struct Stereo
    {
        float left  = 0.0f;
        float right = 0.0f;
    };

    static constexpr int kLines = 8;

    void prepare (double sampleRate);

    void reset() noexcept;

    // Control rate (every engine control frame): recompute loop gain, damping,
    // delay lengths and modulation depth. Allocation-free.
    void update (const ReverbControls& controls) noexcept;

    // Audio rate: feed the dry grain bus in, get the wet space out.
    Stereo process (float inL, float inR) noexcept;

private:
    static void hadamard8 (float* v) noexcept;
    float readLine (int line, double delaySamples) const noexcept;

    double sampleRate = 48000.0;

    std::array<std::vector<float>, kLines> lines;
    std::array<std::uint32_t, kLines>      masks {};
    std::array<int, kLines>                lineLengths {};
    std::uint64_t writePos = 0;

    // Current controls (updated at control rate).
    std::array<float, kLines> targetDelay {};
    std::array<float, kLines> currentDelay {};
    std::array<float, kLines> modulationPhase {};
    std::array<float, kLines> modulationInc {};
    std::array<float, kLines> dampState {};

    float loopGain        = 0.85f;
    float dampingCoeff    = 0.4f;
    float modDepthSamples = 0.0f;
    float injectionGain   = 0.2f;
};

} // namespace nagloed::dsp
