#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "DspUtil.h"
#include "core/Contracts.h"

// -----------------------------------------------------------------------------
// SINTEL — the granular core of NAGLOED.
//
// A 12-second stereo capture ring buffer continuously records whatever the
// engine is generating (the audio input, or — in Noise/Drone source modes — a
// self-generated texture), and a pool of 256 preallocated grain voices reads
// from it. Grains are scheduled in samples with fractional start positions and
// every parameter is sampled from a control frame provided by EngineDSP, so the
// per-sample loop only ever does arithmetic.
//
// Musical intent
//  * The capture buffer is the "room" that holds the last 12 seconds; Freeze
//    stops writing to it, turning NAGLOED into a granular instrument that plays
//    the material it has already captured.
//  * In Drone source mode the oscillator cluster is tuned to the grain pitch
//    itself (quantised by grain.tune when enabled), so changing Pitch plays the
//    drone like an instrument; the grains only add spread/shimmer texture on
//    top instead of transposing the drone a second time.
//  * grain.focus chooses where in the captured window grains are born, grain
//    spray scatters them around that point and grain.scan drifts the read head
//    through the window: 0 locks the read head to the captured material at the
//    current focus offset (fully frozen once Freeze stops the capture), positive
//    values scan towards newer material, negative values towards older
//    material — which is what lets the demo score scrub backwards while
//    frozen.
//  * grain.feedback feeds the summed grain output back into the capture buffer
//    through a damped, softly clipped path so clouds can regenerate themselves;
//    the feedback is additionally tamed by the pitch ratio so upward pitch
//    shifts can never turn the loop into a scream.
//
// Realtime contract
//  * prepare() allocates; process() only reads/writes preallocated memory.
//  * The grain pool is fixed at kMaxGrains (256) — voices are stolen instead of
//    allocated when the density is too high for the pool.
//  * The loop bounds are all compile-time constants.
// -----------------------------------------------------------------------------
namespace nagloed::dsp
{

// A control-rate snapshot consumed by GrainEngine. EngineDSP fills one of
// these per control frame (every kControlInterval samples).
struct GrainControls
{
    float densityHz     = 30.0f;   // new grains per second
    float grainSeconds  = 0.08f;   // grain duration (fixed, independent of pitch)
    float spray         = 0.25f;   // 0..1 position scatter around the read head
    float scan          = 0.0f;    // -1..1 read-head drift in samples/sample
    float pitchSemis    = 0.0f;    // base pitch (manual + shimmer/drift/mod)
    float pitchSpread   = 0.0f;    // 0..24 semitones per-grain randomness
    float reverse       = 0.0f;    // probability a grain reads backwards
    float jitter        = 0.3f;    // 0..1 timing randomness
    float shape         = 0.15f;   // 0..1 Hann -> Tukey -> expodec
    float width         = 0.5f;    // 0..1 per-grain stereo spread
    float feedback      = 0.0f;    // 0..0.95 effective grain feedback
    float focus         = 0.5f;    // 0..1 centre of the capture window
    float panOffset     = 0.0f;    // -1..1 LFO/auto pan offset
    float shimmer       = 0.0f;    // 0..1 probability of an octave-up grain
    float gain          = 0.5f;    // per-grain gain (density normalisation)
    float feedbackScale = 1.0f;    // pitch-dependent feedback safety scale
    float brightness    = 0.5f;    // normalised ladder cutoff (visual only)
    bool  freeze        = false;   // stop writing to the capture buffer
    int   tuneMode      = 0;       // 0 = free, see dsp::TuneMode
    int   sourceMode    = 0;       // 0 input, 1 noise, 2 drone
};

class GrainEngine
{
public:
    struct Stereo
    {
        float left  = 0.0f;
        float right = 0.0f;
    };

    GrainEngine();
    ~GrainEngine();

    // Message thread. Allocates the capture buffer, window tables and grains.
    void prepare (double sampleRate);

    // Audio thread. Clears state without allocating.
    void reset() noexcept;

    // Audio thread, once per sample.
    Stereo process (float inL, float inR, const GrainControls& controls) noexcept;

    // Audio thread visualisation hooks (called from EngineDSP::getVisual()).
    int   copyGrainViews (GrainView* destination, int maxViews) const noexcept;
    float bufferFill() const noexcept;
    float grainRate() const noexcept { return grainRateEstimate; }
    int   activeGrainCount() const noexcept { return activeCount; }

    static constexpr int kMaxGrains = 256;

private:
    // -------------------------------------------------------------------------
    // One grain voice.
    // -------------------------------------------------------------------------
    struct Grain
    {
        bool   active     = false;
        double sourcePos  = 0.0;   // absolute fractional buffer position at birth
        double rate       = 1.0;   // source samples per output sample (signed)
        float  age        = 0.0f;  // output samples since birth
        float  length     = 1.0f;  // duration in output samples
        float  gain       = 0.5f;
        float  shape      = 0.0f;
        float  panL       = 0.7071f;
        float  panR       = 0.7071f;
        float  pan        = 0.0f;   // -1..1 (for the visualiser)
        float  pitch      = 0.0f;  // effective semitones (for the visualiser)
        float  env        = 0.0f;  // last window amplitude (for the visualiser)
        float  brightness = 0.5f;  // normalised filter brightness (visual only)
    };

    // --- scheduling ----------------------------------------------------------
    void  spawnGrain (const GrainControls& controls) noexcept;
    int   findVoiceToUse() noexcept;
    float nextSpawnInterval (const GrainControls& controls) noexcept;

    // --- capture -------------------------------------------------------------
    void   writeCapture (float srcL, float srcR, const GrainControls& controls) noexcept;
    void   advanceReadHead (const GrainControls& controls) noexcept;

    // --- source synthesis (self-generated material) --------------------------
    void generateNoise (float& outL, float& outR) noexcept;
    void generateDrone (float& outL, float& outR, const GrainControls& controls) noexcept;

    // --- interpolation -------------------------------------------------------
    float readInterpolated (const float* buffer, double position) const noexcept;

    // --- helpers -------------------------------------------------------------
    static int nextPowerOfTwo (int v) noexcept;
    float windowValue (float phase, float shape) const noexcept;

    // --- configuration (set in prepare) --------------------------------------
    double sampleRate   = 48000.0;
    int    nominalLen   = 576000;   // ~12 s at the current sample rate
    int    ringLen      = 1048576;  // power of two >= nominalLen
    std::uint32_t ringMask = 1048575u;

    // --- capture ring --------------------------------------------------------
    std::vector<float> captureL;
    std::vector<float> captureR;
    double writePos  = 0.0;   // absolute sample counter (frozen when Freeze is on)
    double filled    = 0.0;   // number of valid captured samples (<= nominalLen)
    double readHead  = 0.0;   // relative position inside the filled window (0..filled)
    float  feedbackLpL = 0.0f;
    float  feedbackLpR = 0.0f;
    float  feedbackDampCoeff = 0.4f;

    // --- grain pool ----------------------------------------------------------
    std::array<Grain, kMaxGrains> grains {};
    int activeCount = 0;

    // --- scheduler -----------------------------------------------------------
    float  spawnCountdown = 0.0f;   // samples until the next grain
    int    rateCounter    = 0;      // for the grainRate estimate
    int    rateSpawnCount = 0;
    float  grainRateEstimate = 0.0f;

    // --- source synthesis state ----------------------------------------------
    Rng   rng;
    float noiseWhiteL = 0.0f, noiseWhiteR = 0.0f;
    OnePole noiseLowL, noiseLowR;      // ~900 Hz colour
    OnePole noiseMidL, noiseMidR;      // ~4.5 kHz colour
    float noiseBreathPhase = 0.0f;
    float noiseBreathInc   = 0.0f;

    std::array<float, 6> dronePhase {};
    std::array<float, 6> droneInc {};
    float dronePitch = -999.0f;        // last pitch the increments were built for
    OnePole droneToneL, droneToneR;

    // --- windows -------------------------------------------------------------
    WindowMorph windows;
};

} // namespace nagloed::dsp
