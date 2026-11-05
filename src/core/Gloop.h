#pragma once

#include <array>
#include <cstdint>

// -----------------------------------------------------------------------------
// Gloop: eight independent, deliberately unsynchronised gesture loopers.
//
// A lane records the *movement* of one target parameter relative to the value
// the knob had when recording started. Playback adds that relative movement to
// the knob's live value, so moving the knob translates the orbit instead of
// fighting it.
//
// The contract deliberately contains no JUCE types: it is shared by the APVTS
// adapter, the DSP engine and the GUI. Normalisation and parameter ranges live
// in core/Parameters.h (gloopTarget* helpers), which is the single source of
// truth for the target table.
// -----------------------------------------------------------------------------
namespace nagloed::gloop
{

inline constexpr int kNumLanes = 8;

// The recorder samples at a fixed 100 Hz. A 30 s gesture is already a very long
// loop; keeping the bound protects memory (8 lanes x 30 s x 4 B = 96 KB) and
// makes the state blob small.
inline constexpr int   kMaxGestureSamples = 3000;
inline constexpr float kGestureSampleRate = 100.0f;

inline constexpr float kMinSpeed = 0.25f;
inline constexpr float kMaxSpeed = 2.0f;

// Off is intentional: a lane with Off contributes zero modulation. A lane is
// allowed to be empty or motionless; Gloop never invents an oscillator.
enum class Target : int
{
    Off = 0,
    Focus,
    Pitch,
    Cutoff,
    Space,
    Density,
    Spray,
    Width,
    Shape,
    Drive,
    Tone,
    Reso,
    Wow,
    Tape,
    Feedback,
    Spread,
    Count
};

inline constexpr int kNumTargets = (int) Target::Count;

enum class Mode : int
{
    Idle = 0,   // ready for a new REC; contributes nothing
    Record,     // capturing the target knob's movement
    Loop,       // playing the recorded gesture indefinitely
    Fade,       // withdrawing this lane's influence, then Idle
    Count
};

// Per-lane settings handed to the DSP once per block. `mode` is performance
// state (it is not a host parameter); the processor injects it from atomics.
struct LaneParams
{
    int   target = (int) Target::Off;
    float amount = 0.5f;   // 0..1, separate from speed
    float speed  = 1.0f;   // 0.25..2, playback time of the recorded gesture
    int   mode   = (int) Mode::Idle;
};

// A recorded gesture. Offsets are normalised to the target parameter's own
// range and are relative to the knob value at the start of the recording.
// count == 0 means "empty"; a gesture with count >= 2 and all offsets == 0 is
// valid but motionless, and therefore contributes zero modulation.
struct Gesture
{
    int   count = 0;
    float durationSeconds = 0.0f;
    std::array<float, kMaxGestureSamples> offsets {};
};

// Everything the UI needs to draw one lane: a moving playhead, the current
// effective offset, and the lane's actual state.
struct LaneVisual
{
    int   target = (int) Target::Off;
    int   mode   = (int) Mode::Idle;
    bool  hasGesture = false;

    float phase = 0.0f;          // 0..1 position inside the loop
    float offset = 0.0f;         // effective normalised offset this frame
    float amount = 0.0f;         // effective amount incl. fade/randomise
    float centreNorm = 0.0f;     // live knob centre, 0..1 in target range
    float durationSeconds = 0.0f;
};

struct Visual
{
    std::array<LaneVisual, kNumLanes> lanes {};
    std::uint32_t frame = 0;
};

} // namespace nagloed::gloop
