#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/EngineParams.h"
#include "core/Gloop.h"

#include "DspUtil.h"

namespace nagloed::dsp
{

// -----------------------------------------------------------------------------
// Gloop playback: eight independent, deliberately unsynchronised gesture lanes.
//
// Ownership
//   Recording happens in the processor (message thread). This class only plays
//   back completed takes. They arrive through a lock-free single-producer /
//   single-consumer queue (juce::AbstractFifo) that is fully preallocated in the
//   constructor, so publish() never allocates and neither does the audio side.
//
// What a lane outputs
//   A *normalised* offset in the target parameter's 0..1 space, relative to the
//   knob's live value. EngineDSP turns that into real units with
//   gloopApplyOffset() at the point where the control is computed, so moving the
//   knob translates the orbit and the APVTS value is never written.
//
// What it deliberately never does
//   Invent motion. No gesture, an empty take, a motionless take, amount 0, a
//   lane that is not playing, or an Off target all produce exactly zero.
//
// Control rate
//   beginBlock() runs once per audio block: it drains the queue, takes a
//   snapshot of EngineParams and applies lane mode transitions. processFrame()
//   runs once per control frame (EngineDSP::kControlInterval samples) and
//   advances phase, fades, glides, smoothing and the per-target sums. Everything
//   time-based therefore depends on seconds, never on the host block size.
//
// Threading
//   publish()          message thread only.
//   beginBlock()/      audio thread only (beginBlock drains the FIFO, so it must
//   processFrame()     be the audio thread or a test harness standing in for it).
//   offsetFor()/       either thread, read-only, but only meaningful after
//   copyVisual()       beginBlock/processFrame have run.
// -----------------------------------------------------------------------------
class GloopEngine
{
public:
    GloopEngine();

    // Message thread. GloopEngine owns no audio buffers, so this only stores the
    // sample rate; the queue and the gesture storage already exist.
    void prepare (double sampleRate) noexcept;

    // Audio thread. Keeps the recorded takes (a reset must not lose a take) and
    // rewinds the playheads / restartable fade state.
    void reset() noexcept;

    // -------------------------------------------------------------------------
    // Message thread -> audio thread gesture hand-off.
    //
    // A take is copied into a preallocated slot and queued. Returns false when
    // the queue is full, in which case the caller should retry later (the
    // processor keeps the take and republishes). count < 2 or numTracks == 0
    // means "empty" and clears the lane, which is also valid.
    // -------------------------------------------------------------------------
    bool publish (int lane, const gloop::Gesture& gesture) noexcept;

    /** Convenience for QA harnesses: wrap one offset track as a Gesture, using
        the lane's current target (Focus if that is Off/Multi). */
    bool publish (int lane, const float* offsets, int count,
                  float durationSeconds) noexcept;

    // -------------------------------------------------------------------------
    // Audio thread.
    // -------------------------------------------------------------------------
    void beginBlock (const EngineParams& params) noexcept;
    void processFrame (double dtSeconds) noexcept;

    // Normalised offset for one target: the sum of every playing lane that
    // targets it, through a bounded, smooth limiter. 0 for Off / no lanes.
    float offsetFor (gloop::Target target) const noexcept;

    // Fills a gloop::Visual snapshot (all eight lanes) for getVisual().
    void copyVisual (gloop::Visual& out, std::uint32_t frame) const noexcept;

    // -------------------------------------------------------------------------
    // QA probes (tests and the CPU/behaviour net). Read-only, no side effects.
    // -------------------------------------------------------------------------
    int   laneMode (int lane) const noexcept;             // actual (not requested) mode
    bool  laneHasGesture (int lane) const noexcept;
    float lanePhase (int lane) const noexcept;            // 0..1 inside the loop
    float laneOffset (int lane) const noexcept;           // first track's effective offset
    float laneEffectiveAmount (int lane) const noexcept;  // amount * randomise * fade
    float laneAmountScale (int lane) const noexcept;      // randomise glide value
    float lanePlaybackRate (int lane) const noexcept;     // smoothed speed * randomise
    float laneFadeGain (int lane) const noexcept;

    int   laneTrackCount (int lane) const noexcept;
    int   laneTrackTarget (int lane, int track) const noexcept;
    float laneTrackOffset (int lane, int track) const noexcept;

private:
    // Empty gestures are a legitimate state, so count < 2 never plays.
    static constexpr int kMinSamples = 2;

    // One burst of eight republishes fits exactly; the processor retries the
    // rest, so a full queue is a "come back later", never a lost take.
    // AbstractFifo keeps one slot free to distinguish full from empty, hence
    // kNumLanes + 1 backing slots for kNumLanes usable ones.
    static constexpr int kQueueSlots = gloop::kNumLanes + 1;

    // FADE: the documented 2.5 s smooth withdraw. LOOP re-entry uses the same
    // slope to glide back in, so a lane never jumps back to full influence.
    static constexpr float kFadeSeconds = 2.5f;

    // Seam crossfade: the recorded start and end rarely match, so the last 45 ms
    // of the loop crossfades into the first 45 ms. The loop period shortens by
    // that amount, which is what makes the wrap continuous (see readLane()).
    static constexpr float kSeamSeconds = 0.045f;

    // Parameter speed glides (instant enough to feel played, slow enough that an
    // automated speed lane cannot click) and the Randomize glide (~3-4 s to
    // settle, so a per-frame amount change stays far below 0.01).
    static constexpr float kSpeedGlideSeconds  = 0.25f;
    static constexpr float kRandomGlideSeconds = 1.0f;

    // One-pole offset smoothing: 8 ms at smoothing 0, 260 ms at 1, 40 ms at the
    // 0.35 default (tau = min + s^2 * (max - min)).
    static constexpr float kSmoothMinSeconds = 0.008f;
    static constexpr float kSmoothMaxSeconds = 0.260f;

    // Cross-lane limiter: linear up to 0.75 normalised, then a smooth tanh knee
    // that asymptotes at 1.25. Two lanes on one target therefore sum to a
    // bounded, click-free result instead of clipping or fighting.
    static constexpr float kOffsetLinear  = 0.75f;
    static constexpr float kOffsetCeiling = 1.25f;

    struct Slot
    {
        int   lane = 0;
        gloop::Gesture gesture {};
    };

    struct Lane
    {
        gloop::Gesture gesture {};
        bool  hasGesture = false;
        int   target = (int) gloop::Target::Off;   // selected target or kAutoTarget
        int   actualMode = (int) gloop::Mode::Idle;
        int   lastRequested = -1;      // -1 = no block seen yet

        double phase = 0.0;            // 0..1 inside the loop period
        double gestureDuration = 0.0;  // recorded length in seconds
        double seamSeconds = 0.0;      // crossfade length for this take
        double loopPeriod = 0.0;       // gestureDuration - seamSeconds

        float fadeGain = 0.0f;         // 1 = full influence, 0 = withdrawn
        float amountParam = 0.5f;      // params.gloopLanes[i].amount
        float amountScale = 1.0f;      // Randomize glide, 0.35..1
        float amountScaleTarget = 1.0f;
        float speedParam = 1.0f;       // params.gloopLanes[i].speed
        float speedSmoothed = 1.0f;
        float randomSpeed = 1.0f;      // Randomize glide, 0.6..1.4
        float randomSpeedTarget = 1.0f;
        bool  speedInitialised = false;

        float effectiveAmount = 0.0f;  // amount * amountScale * fadeGain

        // Per-track playback/smoothing state. A lane can carry several sparse
        // parameter tracks; each has its own offset smoother and live centre.
        std::array<float, gloop::kMaxTracksPerLane> rawTrack {};
        std::array<float, gloop::kMaxTracksPerLane> offsetTrack {};
        std::array<float, gloop::kMaxTracksPerLane> centreTrack {};

        // First-track projections retained for the existing single-target
        // visual/QA surface.
        float rawOffset = 0.0f;
        float offset = 0.0f;
        float centreNorm = 0.0f;
    };

    void drainGestures() noexcept;
    void installFromSlot (const Slot& slot) noexcept;
    void clearLaneForRecording (Lane& lane) noexcept;
    void refreshActualMode (Lane& lane) noexcept;
    void randomiseTargets (std::uint32_t epoch) noexcept;
    void updateCoefficients (double dtSeconds) noexcept;

    static float softLimitOffset (float offset) noexcept;
    float readTrack (const Lane& lane, const gloop::Track& track) const noexcept;

    // Heap-allocated so an EngineDSP constructed on a test/main-thread stack
    // stays small: eight lanes x four 30 s tracks is ~384 KB, and the hand-off
    // queue is another ~432 KB. Allocated once in the constructor (message
    // thread), never on the audio thread.
    std::vector<Lane> lanes;
    std::vector<Slot> slots;
    std::array<float, (size_t) gloop::kNumTargets> offsetByTarget {};

    // Preallocated SPSC hand-off. slots[] is owned by the writer until
    // finishedWrite() and by the reader after, which is what makes the copy safe
    // without a lock.
    juce::AbstractFifo fifo { kQueueSlots };

    EngineParams settings {};
    dsp::Rng random;

    double sampleRate = 48000.0;
    double lastDtSeconds = 0.0;
    float  lastSmoothing = -1.0f;
    float  offsetCoeff = 0.0f;
    float  speedCoeff = 0.0f;
    float  randomCoeff = 0.0f;
    std::uint32_t lastRandomize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GloopEngine)
};

} // namespace nagloed::dsp
