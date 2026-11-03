#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "core/EngineParams.h"
#include "core/Contracts.h"

#include "AnalogChain.h"
#include "DspUtil.h"
#include "GrainEngine.h"
#include "Modulation.h"
#include "RuimteReverb.h"

namespace nagloed
{

// -----------------------------------------------------------------------------
// Internal engine implementation.
//
// OWNERSHIP: the DSP workstream. `Engine::Impl` derives from this class, so the
// grain engine, capture buffer, FDN, tape section and modulation matrix stay
// private to src/dsp/.
//
// Signal flow (see docs/ARCHITECTURE.md):
//
//   input -> Sintel (capture + grains) -> Ruimte (FDN) -> EAR (analog) -> out
//     |
//     +-> VisualState (once per block)
//
// Rules for this file:
//   * prepare()/reset() run on the message thread, process()/setParams()/
//     getVisual() on the audio thread.
//   * process() must not allocate, lock, throw or call any JUCE function that
//     does. Every buffer below is allocated in prepare().
//   * getVisual() is called immediately after process() on the audio thread, so
//     it can read live engine state without synchronisation.
//
// Control-rate design
//   Continuous parameters are smoothed by one-pole Smoothed values and the
//   expensive derived values (ladder coefficients, FDN delays/RT60, tape
//   depths) are recomputed once every kControlInterval samples. That interval is
//   tied to an absolute sample counter rather than the block start, so the
//   engine audio result does not depend on the host block size.
// -----------------------------------------------------------------------------
class EngineDSP
{
public:
    EngineDSP();
    virtual ~EngineDSP();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setParams (const EngineParams&) noexcept;
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&) noexcept;
    void getVisual (VisualState&) const noexcept;
    int  getLatencySamples() const noexcept;

private:
    static constexpr int kControlInterval = 16;

    // --- smoothed parameter set ---------------------------------------------
    struct SmoothedParams
    {
        dsp::Smoothed density, sizeMs, spray, scan, pitch, spread, reverse, jitter,
                       shape, width, feedback, focus;
        dsp::Smoothed spaceMix, spaceSize, spaceDamp, spaceMod;
        dsp::Smoothed driveDb, bias, tone, cutoff, reso, drift, wow, tape, lofi,
                       hiss, inputDb, outputDb, mix;
        dsp::Smoothed macroGlow, macroDrift, macroGrain, macroSpace, macroShimmer;
        dsp::Smoothed lfoRate, lfoAmount, autoMotion;

        void prepare (double sr, int intervalSamples) noexcept;
        void setTargets (const EngineParams& p) noexcept;
        void snap (const EngineParams& p) noexcept;
        void tick() noexcept;
    };

    void prepareBuffers();
    void updateControlFrame() noexcept;
    void processChunk (float* ioL, float* ioR, int numSamples, bool mono) noexcept;

    // --- configuration -------------------------------------------------------
    double sampleRate = 48000.0;
    int    maxBlock   = 512;
    bool   prepared   = false;
    bool   needsSnap  = true;

    EngineParams params {};

    // --- DSP sections --------------------------------------------------------
    dsp::GrainEngine    grainEngine;
    dsp::RuimteReverb   reverb;
    dsp::AnalogChain    analogChain;
    dsp::Modulation     modulation;
    SmoothedParams      smooth;

    // --- per-chunk buffers (allocated in prepare) ----------------------------
    std::vector<float> inputL, inputR;   // latency-aligned dry copy of the input
    std::vector<float> earL, earR;       // wet chain (space output -> EAR -> mix)
    std::vector<dsp::AnalogChain::Group> controlGroups;

    // --- current control frame ----------------------------------------------
    dsp::GrainControls         grainControls {};
    dsp::ReverbControls        reverbControls {};
    dsp::AnalogChain::Controls earControls {};
    float currentSpaceMix = 0.35f;

    // --- state ---------------------------------------------------------------
    std::uint64_t sampleCounter = 0;

    // --- visual state (written once per process() call) ----------------------
    float visualInL = 0.0f, visualInR = 0.0f;
    float visualOutL = 0.0f, visualOutR = 0.0f;
    float visualInPeak = 0.0f, visualOutPeak = 0.0f;
    float visualFocus = 0.5f;
    float visualSpray = 0.25f;
    float visualLfo = 0.0f;
    float visualCutoff = 9000.0f;
    float visualDensity = 30.0f;
    float visualDriveDb = 6.0f;
    float visualLofi = 0.0f;
    std::uint32_t visualFrame = 0;

    // --- meters for the current process() call -------------------------------
    double accumInL = 0.0, accumInR = 0.0, accumOutL = 0.0, accumOutR = 0.0;
    int    accumSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineDSP)
};

} // namespace nagloed
