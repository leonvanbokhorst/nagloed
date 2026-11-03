#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace nagloed
{

// -----------------------------------------------------------------------------
// One grain, as the GUI wants to see it. This is a *visualisation* record, not
// the audio-thread grain: the engine copies whatever it wants into it.
// -----------------------------------------------------------------------------
struct GrainView
{
    float pos       = 0.0f;  // 0..1 position inside the capture buffer
    float progress  = 0.0f;  // 0..1 envelope progress (age)
    float pan       = 0.0f;  // -1..1
    float pitch     = 0.0f;  // -24..+24 semitones
    float env       = 0.0f;  // 0..1 current amplitude
    float brightness = 0.5f; // 0..1 filter/normalised Hz for colour
};

// -----------------------------------------------------------------------------
// A lock-free frame of everything the UI needs to draw. Written by the audio
// thread once per block, read by the UI at frame rate.
// -----------------------------------------------------------------------------
struct VisualState
{
    static constexpr int kMaxGrainViews = 256;

    std::array<GrainView, kMaxGrainViews> grains {};
    int   numGrains   = 0;

    float inL = 0.0f, inR = 0.0f;    // RMS 0..1
    float outL = 0.0f, outR = 0.0f;  // RMS 0..1
    float inPeak = 0.0f, outPeak = 0.0f;

    float focus        = 0.5f;
    float spray        = 0.2f;
    float lfo          = 0.0f;   // -1..1
    float cutoffNorm   = 0.5f;   // 0..1 log-normalised
    float densityNorm  = 0.2f;
    float grainRate    = 0.0f;   // grains / second actually emitted
    float bufferFill   = 0.0f;   // 0..1
    float reduce       = 0.0f;   // 0..1 visual "dust" amount (lofi)
    float drive        = 0.0f;   // 0..1 normalised
    float mpePressure  = 0.0f;
    int   activeNotes  = 0;
    juce::uint32 frame = 0;
};

// The GUI only ever sees this interface; it never links against the engine.
struct VisualSource
{
    virtual ~VisualSource () = default;
    virtual void copyVisual (VisualState& out) const = 0;
};

// -----------------------------------------------------------------------------
// Presets, as exposed to the GUI.
// -----------------------------------------------------------------------------
struct PresetInfo
{
    juce::String name;
    juce::String category;
    juce::String description;
};

struct PresetAccess
{
    virtual ~PresetAccess () = default;
    virtual int         getNumPresets() const = 0;
    virtual PresetInfo  getPresetInfo (int index) const = 0;
    virtual int         getCurrentPreset() const = 0;
    virtual void        loadPreset (int index) = 0;
};

} // namespace nagloed
