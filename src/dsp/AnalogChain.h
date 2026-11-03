#pragma once

#include <array>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "DspUtil.h"

// -----------------------------------------------------------------------------
// EAR — "emotional analog resonance".
//
// The character section that turns a clean granular cloud into something that
// sounds like it has been through a valve desk and a tape machine:
//
//   input gain -> asymmetric saturation (2x oversampled)
//              -> tone tilt -> 4-pole resonant ladder lowpass (2x oversampled,
//                 self-oscillating but always tamed by a tanh in the loop)
//              -> wow & flutter tape delay -> tape saturation
//              -> LoFi sample-rate/bit reduction -> hiss -> DC blocker
//              -> output soft limiter -> dry/wet mix
//
// Every stage is smoothed upstream (EngineDSP's control frames), so automating
// any knob of the character chain never zippers. The dry/wet mix happens at the
// very end, against a dry signal delayed by the *exact* wet-path latency
// (oversampler latency + the current tape delay including wow/flutter), so a
// 50/50 blend stays phase-coherent instead of comb-filtering. That total
// latency is reported to the host.
// -----------------------------------------------------------------------------
namespace nagloed::dsp
{

class AnalogChain
{
public:
    // Control snapshot for one control frame.
    struct Controls
    {
        float inputGain    = 1.0f;
        float driveGain    = 1.0f;    // drive gain applied before the saturator
        float toneLow      = 1.0f;    // gain of the low band
        float toneHigh     = 1.0f;    // gain of the high band
        float bias         = 0.15f;   // asymmetric saturation bias (0..1)
        float ladderG      = 0.1f;    // prewarped tan(pi*fc/fs) at the oversampled rate
        float ladderK      = 0.6f;    // ladder feedback (0..4, self-oscillation ~4)
        float wowDepth     = 0.0f;    // slow pitch modulation depth (samples)
        float flutterDepth = 0.0f;    // fast pitch modulation depth (samples)
        float driftDepth   = 0.0f;    // analog instability depth (samples)
        float tapeDrive    = 1.0f;    // tape saturation drive
        float tapeComp     = 1.0f;    // 1/tapeDrive, unity small-signal gain
        float lofiHold     = 1.0f;    // sample & hold length (samples)
        float lofiLevels   = 65536.0f;// quantisation levels
        float hissGain     = 0.0f;    // per-sample hiss amplitude
        float outputGain   = 1.0f;
        float mix          = 0.65f;   // 0 = clean dry, 1 = full EAR chain
        bool  lofiActive   = false;
        bool  hissActive   = false;
    };

    // Control frames are supplied as (startSample, controls) groups covering the
    // whole block. The first group always starts at 0.
    struct Group
    {
        int      start = 0;
        Controls controls {};
    };

    AnalogChain();

    // Message thread: allocates the oversampler, delay lines and scratch
    // buffers, and reports the latency the host has to compensate.
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // Audio thread. `wetL/wetR` carry the space output into the chain and are
    // overwritten with the mixed result. `dryL/dryR` are the clean engine input
    // (not yet gain-staged); this class latency-aligns them internally.
    void process (float* wetL, float* wetR,
                  const float* dryL, const float* dryR,
                  int numSamples,
                  const Group* groups, int numGroups) noexcept;

    int latencySamples() const noexcept { return latency; }

private:
    struct LadderState
    {
        float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
        void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }
    };

    static float processLadder (LadderState& state, float x, float g, float k) noexcept;
    double sampleRate         = 48000.0;
    int    maxBlock           = 512;
    int    oversamplerLatency = 0;
    int    latency            = 0;   // reported host latency: oversampler + tape base delay

    // 2x oversampled nonlinear stages (saturation + ladder).
    // factor = 2^1 = 2, polyphase IIR halfband (low latency, adequate
    // anti-imaging for a saturator), integer latency so the dry path can be
    // aligned exactly.
    juce::dsp::Oversampling<float> oversampler {
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true
    };

    juce::AudioBuffer<float> osInput;
    juce::AudioBuffer<float> osOutput;

    // Dry-path latency compensation. The delay line is read with the same
    // fractional delay as the wet tape path (oversampler latency + tape delay),
    // so dry and wet always align sample-for-sample.
    std::vector<float> dryDelayL, dryDelayR;
    std::uint32_t dryMask = 0;
    std::uint32_t dryWrite = 0;

    // Tone tilt, applied at the oversampled rate between saturation and ladder.
    OnePole toneLpL, toneLpR;

    // Ladder state per channel (operating at the oversampled rate).
    LadderState ladderL, ladderR;

    // Tape transport (wow/flutter fractional delay).
    std::vector<float> tapeL, tapeR;
    std::uint32_t tapeMask = 0;
    std::uint32_t tapeWrite = 0;
    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;
    float wowInc = 0.0f;
    float flutterInc = 0.0f;
    float tapeBaseDelay = 48.0f;

    // LoFi sample & hold.
    float lofiCounter = 0.0f;
    float lofiHeldL = 0.0f;
    float lofiHeldR = 0.0f;
    float quantLevels = 65536.0f;

    // Hiss.
    Rng hissRng;
    OnePole hissLpL, hissLpR;

    // Output DC blocking.
    DcBlocker dcL, dcR;
};

} // namespace nagloed::dsp
