#pragma once

#include <array>
#include <cstdint>

// -----------------------------------------------------------------------------
// Gloop: eight independent, deliberately unsynchronised gesture loopers.
//
// A lane records the *movement* of one or more parameter knobs relative to the
// values they had when recording started. Playback adds each track's relative
// movement to its own knob's live value, so moving a knob translates its orbit
// instead of fighting it.
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
// loop; keeping the bound protects memory and the state blob. A lane can hold a
// sparse set of parameter tracks (see kMaxTracksPerLane), not a full matrix.
inline constexpr int   kMaxGestureSamples = 3000;
inline constexpr float kGestureSampleRate = 100.0f;

inline constexpr float kMinSpeed = 0.25f;
inline constexpr float kMaxSpeed = 2.0f;

// How many parameters one lane may capture in a single take. Four is already a
// deliberate gesture (e.g. Grain, Drift, Space and Tone), not a modulation
// matrix. Additional movements beyond the cap are ignored.
inline constexpr int kMaxTracksPerLane = 4;

// A target moved by more than this fraction of its own range during REC is
// considered intentional and gets a track in Multi mode. This filters tiny host
// automation/rounding movement while still catching a slow, gentle gesture.
inline constexpr float kAutoRecordThreshold = 0.015f;

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

    // Appended after the original table so existing choice indices/sessions
    // keep their meaning. These are the two extra knobs named in the
    // multi-parameter gesture example (grain size first, then Drift and Space).
    Grain,
    Drift,
    Count
};

inline constexpr int kNumTargets = (int) Target::Count;

// The target choice list has one extra sentinel entry after the concrete
// targets: "Multi". It is appended so every existing choice index (and every
// old preset/session) keeps its meaning.
inline constexpr int kAutoTarget = kNumTargets;
inline constexpr int kNumTargetChoices = kNumTargets + 1;

enum class Mode : int
{
    Idle = 0,   // ready for a new REC; contributes nothing
    Record,     // capturing parameter movement
    Loop,       // playing the recorded gesture indefinitely
    Fade,       // withdrawing this lane's influence, then Idle
    Count
};

// Per-lane settings handed to the DSP once per block. `mode` is performance
// state (it is not a host parameter); the processor injects it from atomics.
struct LaneParams
{
    int   target = (int) Target::Off;   // concrete target, Off, or kAutoTarget
    float amount = 0.5f;                // 0..1, separate from speed
    float speed  = 1.0f;                // 0.25..2, playback time of the gesture
    int   mode   = (int) Mode::Idle;
};

// One recorded parameter track inside a lane. Offsets are normalised to the
// target parameter's own range and are relative to the knob value at the start
// of the recording. Before the track's first movement the offsets are zero, so
// sequential knob gestures share one timeline without inventing motion.
struct Track
{
    int   target = (int) Target::Off;
    std::array<float, kMaxGestureSamples> offsets {};
};

// A recorded gesture: a lane-level timebase plus a sparse set of tracks. All
// tracks share `count` and `durationSeconds`. count < 2 or numTracks == 0 means
// "empty"; a track with all offsets == 0 is valid but motionless and therefore
// contributes zero modulation.
struct Gesture
{
    int   count = 0;
    float durationSeconds = 0.0f;
    int   numTracks = 0;
    std::array<Track, kMaxTracksPerLane> tracks {};
};

// Everything the UI needs to draw one lane: a moving playhead, the current
// effective offset of each recorded track, and the lane's actual state.
struct LaneVisual
{
    struct TrackVisual
    {
        int   target = (int) Target::Off;
        float offset = 0.0f;      // effective normalised offset this frame
        float centreNorm = 0.0f;  // live knob centre, 0..1 in target range
    };

    int   target = (int) Target::Off;  // selected target, or kAutoTarget
    int   mode   = (int) Mode::Idle;
    bool  hasGesture = false;

    float phase = 0.0f;          // 0..1 position inside the loop
    float offset = 0.0f;         // first track's effective offset (legacy view)
    float amount = 0.0f;         // effective amount incl. fade/randomise
    float centreNorm = 0.0f;     // first track's live centre
    float durationSeconds = 0.0f;

    int   numTracks = 0;
    std::array<TrackVisual, kMaxTracksPerLane> tracks {};
};

struct Visual
{
    std::array<LaneVisual, kNumLanes> lanes {};
    std::uint32_t frame = 0;
};

} // namespace nagloed::gloop
