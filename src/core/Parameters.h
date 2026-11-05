#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Version.h"
#include "ParamIDs.h"
#include "EngineParams.h"
#include "Gloop.h"

namespace nagloed
{

// -----------------------------------------------------------------------------
// Single source of truth for ranges, defaults and choice lists. The GUI, the
// preset validator and the DSP adapter all read from here so they can never
// drift apart.
// -----------------------------------------------------------------------------
struct ParamRange
{
    float min, max, def;
    float centre = 0.0f;   // >0 enables a skewed (log-ish) response
    float step   = 0.0f;   // >0 quantises
};

namespace ranges
{
inline constexpr ParamRange density     { 0.5f,  200.0f, 30.0f,  6.0f };
inline constexpr ParamRange size        { 5.0f,  500.0f, 80.0f,  70.0f };
inline constexpr ParamRange spray       { 0.0f,  1.0f,   0.25f };
inline constexpr ParamRange scan        { -1.0f, 1.0f,   0.0f };
inline constexpr ParamRange pitch       { -24.0f, 24.0f, 0.0f, 0.0f, 0.01f };
inline constexpr ParamRange spread      { 0.0f,  24.0f,  0.0f, 0.0f, 0.01f };
inline constexpr ParamRange reverse     { 0.0f,  1.0f,   0.0f };
inline constexpr ParamRange jitter      { 0.0f,  1.0f,   0.3f };
inline constexpr ParamRange shape       { 0.0f,  1.0f,   0.15f };
inline constexpr ParamRange width       { 0.0f,  1.0f,   0.5f };
inline constexpr ParamRange feedback    { 0.0f,  0.95f,  0.0f };
inline constexpr ParamRange focus       { 0.0f,  1.0f,   0.5f };

inline constexpr ParamRange spaceMix    { 0.0f,  1.0f,   0.35f };
inline constexpr ParamRange spaceSize   { 0.0f,  1.0f,   0.6f };
inline constexpr ParamRange spaceDamp   { 0.0f,  1.0f,   0.4f };
inline constexpr ParamRange spaceMod    { 0.0f,  1.0f,   0.3f };

inline constexpr ParamRange drive       { 0.0f,  30.0f,  6.0f };
inline constexpr ParamRange bias        { 0.0f,  1.0f,   0.15f };
inline constexpr ParamRange tone        { 0.0f,  1.0f,   0.5f };
inline constexpr ParamRange cutoff      { 20.0f, 20000.0f, 9000.0f, 1000.0f };
inline constexpr ParamRange reso        { 0.0f,  1.0f,   0.15f };
inline constexpr ParamRange drift       { 0.0f,  1.0f,   0.25f };
inline constexpr ParamRange wow         { 0.0f,  1.0f,   0.2f };
inline constexpr ParamRange tape        { 0.0f,  1.0f,   0.35f };
inline constexpr ParamRange lofi        { 0.0f,  1.0f,   0.0f };
inline constexpr ParamRange hiss        { 0.0f,  1.0f,   0.06f };
inline constexpr ParamRange input       { -24.0f, 24.0f, 0.0f, 0.0f, 0.1f };
inline constexpr ParamRange output      { -24.0f, 6.0f,  0.0f, 0.0f, 0.1f };
inline constexpr ParamRange mix         { 0.0f,  1.0f,   0.65f };

inline constexpr ParamRange macroGlow    { 0.0f, 1.0f, 0.3f };
inline constexpr ParamRange macroDrift   { 0.0f, 1.0f, 0.3f };
inline constexpr ParamRange macroGrain   { 0.0f, 1.0f, 0.5f };
inline constexpr ParamRange macroSpace   { 0.0f, 1.0f, 0.4f };
inline constexpr ParamRange macroShimmer { 0.0f, 1.0f, 0.2f };

inline constexpr ParamRange lfoRate      { 0.01f, 20.0f, 0.15f, 1.0f };
inline constexpr ParamRange lfoAmount    { -1.0f, 1.0f, 0.0f };
inline constexpr ParamRange autoMotion   { 0.0f, 1.0f, 0.15f };
inline constexpr ParamRange expression   { 0.0f, 1.0f, 1.0f };

inline constexpr ParamRange gloopAmount  { 0.0f, 1.0f, 0.5f };
inline constexpr ParamRange gloopSpeed   { 0.25f, 2.0f, 1.0f };
inline constexpr ParamRange gloopSmooth  { 0.0f, 1.0f, 0.35f };
} // namespace ranges

// --- Choice tables -----------------------------------------------------------
inline juce::StringArray tuneNames()
{
    return { "Free", "Chromatic", "Minor", "Major", "Dorian",
             "Phrygian", "Pentatonic", "Whole-tone" };
}
inline juce::StringArray sourceNames()      { return { "Input", "Noise", "Drone" }; }
inline juce::StringArray lfoShapeNames()    { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }; }
inline juce::StringArray lfoDestNames()     { return { "Off", "Focus", "Pitch", "Cutoff", "Pan", "Space", "Density" }; }
inline juce::StringArray pressureDestNames(){ return { "Density", "Pitch", "Cutoff", "Drive", "Space", "Spread" }; }
inline juce::StringArray slideDestNames()   { return { "Pitch", "Focus", "Cutoff", "Pan" }; }

namespace detail
{
inline juce::NormalisableRange<float> makeRange (const ParamRange& r)
{
    juce::NormalisableRange<float> range (r.min, r.max);
    if (r.step > 0.0f)
        range.interval = r.step;
    if (r.centre > r.min && r.centre < r.max)
        range.setSkewForCentre (r.centre);
    return range;
}

inline juce::AudioParameterFloatAttributes attrs (const char* label)
{
    return juce::AudioParameterFloatAttributes().withLabel (label);
}
} // namespace detail

// -----------------------------------------------------------------------------
// Gloop target table.
//
// This is the single source of truth for what a lane can orbit, for the range
// used to normalise a recorded gesture, and for the parameter id the GUI draws
// its ring around. The order MUST match gloop::Target.
// -----------------------------------------------------------------------------
inline constexpr std::array<const char*, (size_t) gloop::kNumTargetChoices> kGloopTargetNames {
    "Off", "Focus", "Pitch", "Cutoff", "Space", "Density", "Spray",
    "Width", "Shape", "Drive", "Tone", "Reso", "Wow", "Tape",
    "Feedback", "Spread",
    "Grain Size", "Drift",   // appended: existing choice indices keep their meaning
    "Multi"             // the multi-parameter sentinel, always last
};
static_assert (kGloopTargetNames.size() == (size_t) gloop::kNumTargetChoices,
               "Gloop target name table and gloop::Target are out of sync");

inline juce::StringArray gloopTargetNames()
{
    juce::StringArray names;

    for (const auto* name : kGloopTargetNames)
        names.add (name);

    return names;
}

inline const char* gloopTargetName (int target)
{
    if (target < 0 || target >= gloop::kNumTargetChoices)
        return kGloopTargetNames[0];

    return kGloopTargetNames[(size_t) target];
}

// Parameter id whose knob should show the lane's moving ring, or nullptr for
// Off and the Multi sentinel (Multi's concrete target tracks are visualised
// individually in LaneVisual::tracks).
inline const char* gloopTargetParamId (int target)
{
    if (target == gloop::kAutoTarget)
        return nullptr;

    switch ((gloop::Target) target)
    {
        case gloop::Target::Focus:    return pid::focus;
        case gloop::Target::Pitch:    return pid::pitch;
        case gloop::Target::Cutoff:   return pid::cutoff;
        case gloop::Target::Space:    return pid::spaceMix;
        case gloop::Target::Density:  return pid::density;
        case gloop::Target::Spray:    return pid::spray;
        case gloop::Target::Width:    return pid::width;
        case gloop::Target::Shape:    return pid::shape;
        case gloop::Target::Drive:    return pid::drive;
        case gloop::Target::Tone:     return pid::tone;
        case gloop::Target::Reso:     return pid::reso;
        case gloop::Target::Wow:      return pid::wow;
        case gloop::Target::Tape:     return pid::tape;
        case gloop::Target::Feedback: return pid::feedback;
        case gloop::Target::Spread:   return pid::spread;
        case gloop::Target::Grain:    return pid::size;
        case gloop::Target::Drift:    return pid::drift;
        case gloop::Target::Off:
        case gloop::Target::Count:
            break;
    }

    return nullptr;
}

// The normalisable range of the target's knob. Recording stores offsets in this
// normalised space, so a gesture keeps its musical shape when the live centre
// moves (and a cutoff sweep stays a sweep in octaves, not in linear Hz).
inline juce::NormalisableRange<float> gloopTargetRange (int target)
{
    if (target == gloop::kAutoTarget)
        return juce::NormalisableRange<float> (0.0f, 1.0f);

    switch ((gloop::Target) target)
    {
        case gloop::Target::Focus:    return detail::makeRange (ranges::focus);
        case gloop::Target::Pitch:    return detail::makeRange (ranges::pitch);
        case gloop::Target::Cutoff:   return detail::makeRange (ranges::cutoff);
        case gloop::Target::Space:    return detail::makeRange (ranges::spaceMix);
        case gloop::Target::Density:  return detail::makeRange (ranges::density);
        case gloop::Target::Spray:    return detail::makeRange (ranges::spray);
        case gloop::Target::Width:    return detail::makeRange (ranges::width);
        case gloop::Target::Shape:    return detail::makeRange (ranges::shape);
        case gloop::Target::Drive:    return detail::makeRange (ranges::drive);
        case gloop::Target::Tone:     return detail::makeRange (ranges::tone);
        case gloop::Target::Reso:     return detail::makeRange (ranges::reso);
        case gloop::Target::Wow:      return detail::makeRange (ranges::wow);
        case gloop::Target::Tape:     return detail::makeRange (ranges::tape);
        case gloop::Target::Feedback: return detail::makeRange (ranges::feedback);
        case gloop::Target::Spread:   return detail::makeRange (ranges::spread);
        case gloop::Target::Grain:    return detail::makeRange (ranges::size);
        case gloop::Target::Drift:    return detail::makeRange (ranges::drift);
        case gloop::Target::Off:
        case gloop::Target::Count:
            break;
    }

    return juce::NormalisableRange<float> (0.0f, 1.0f);
}

inline float gloopTargetValue (const EngineParams& p, int target)
{
    if (target == gloop::kAutoTarget)
        return 0.0f;

    switch ((gloop::Target) target)
    {
        case gloop::Target::Focus:    return p.focus;
        case gloop::Target::Pitch:    return p.pitchSemis;
        case gloop::Target::Cutoff:   return p.cutoffHz;
        case gloop::Target::Space:    return p.spaceMix;
        case gloop::Target::Density:  return p.density;
        case gloop::Target::Spray:    return p.spray;
        case gloop::Target::Width:    return p.width;
        case gloop::Target::Shape:    return p.shape;
        case gloop::Target::Drive:    return p.driveDb;
        case gloop::Target::Tone:     return p.tone;
        case gloop::Target::Reso:     return p.reso;
        case gloop::Target::Wow:      return p.wow;
        case gloop::Target::Tape:     return p.tape;
        case gloop::Target::Feedback: return p.feedback;
        case gloop::Target::Spread:   return p.pitchSpread;
        case gloop::Target::Grain:    return p.sizeMs;
        case gloop::Target::Drift:    return p.drift;
        case gloop::Target::Off:
        case gloop::Target::Count:
            break;
    }

    return 0.0f;
}

inline float gloopTargetNorm (const EngineParams& p, int target)
{
    return gloopTargetRange (target).convertTo0to1 (gloopTargetValue (p, target));
}

inline float gloopValueToNorm (int target, float value)
{
    return gloopTargetRange (target).convertTo0to1 (value);
}

inline float gloopNormToValue (int target, float norm)
{
    return gloopTargetRange (target).convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm));
}

// Add a normalised offset to a live parameter value and return the result in
// real units. Used by the engine to translate a lane's orbit around the knob's
// current centre while respecting the parameter's own range and skew.
inline float gloopApplyOffset (int target, float value, float offset) noexcept
{
    if (target <= (int) gloop::Target::Off || target >= gloop::kNumTargets
        || offset == 0.0f)
        return value;

    const auto range = gloopTargetRange (target);
    const auto norm = juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (value) + offset);
    return range.convertFrom0to1 (norm);
}

// -----------------------------------------------------------------------------
// The parameter layout. Order matters only for browsing ergonomics.
// -----------------------------------------------------------------------------
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace detail;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&layout] (const char* id, const char* name, const ParamRange& r,
                           const char* label = "", juce::String (*fmt) (float, int) = nullptr)
    {
        auto a = attrs (label);
        if (fmt != nullptr)
            a = a.withStringFromValueFunction (fmt);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, kVersionHint }, name, makeRange (r), r.def, a));
    };
    auto addC = [&layout] (const char* id, const char* name, const juce::StringArray& choices, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, kVersionHint }, name, choices, def));
    };

    const auto hz = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                                                : juce::String (v, 0) + " Hz"; };
    const auto ms = [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; };
    const auto st = [] (float v, int) { return (v >= 0.0f ? "+" : "") + juce::String (v, 2) + " st"; };
    const auto db = [] (float v, int) { return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
    const auto pc = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; };
    const auto gps = [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " g/s"; };
    const auto spd = [] (float v, int)
    {
        return juce::String (v, 2) + juce::String::charToString ((juce::juce_wchar) 0x00d7);
    };

    // Sintel
    addF (pid::density,  "Density",     ranges::density,  "g/s", gps);
    addF (pid::size,     "Grain",       ranges::size,     "ms",  ms);
    addF (pid::spray,    "Spray",       ranges::spray,    "%",   pc);
    addF (pid::scan,     "Scan",        ranges::scan,     "",    pc);
    addF (pid::pitch,    "Pitch",       ranges::pitch,    "st",  st);
    addF (pid::spread,   "Spread",      ranges::spread,   "st",  st);
    addF (pid::reverse,  "Reverse",     ranges::reverse,  "%",   pc);
    addF (pid::jitter,   "Jitter",      ranges::jitter,   "%",   pc);
    addF (pid::shape,    "Shape",       ranges::shape,    "%",   pc);
    addF (pid::width,    "Width",       ranges::width,    "%",   pc);
    addF (pid::feedback, "Feed",        ranges::feedback, "%",   pc);
    addF (pid::focus,    "Focus",       ranges::focus,    "",    pc);
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::freeze, kVersionHint }, "Freeze", false));
    addC (pid::tune,   "Tune",   tuneNames(), 0);
    addC (pid::source, "Source", sourceNames(), 0);

    // Ruimte
    addF (pid::spaceMix,  "Space", ranges::spaceMix,  "%", pc);
    addF (pid::spaceSize, "Size",  ranges::spaceSize, "%", pc);
    addF (pid::spaceDamp, "Damp",  ranges::spaceDamp, "%", pc);
    addF (pid::spaceMod,  "Mod",   ranges::spaceMod,  "%", pc);

    // EAR
    addF (pid::drive,  "Drive",  ranges::drive,  "dB", db);
    addF (pid::bias,   "Bias",   ranges::bias,   "%",  pc);
    addF (pid::tone,   "Tone",   ranges::tone,   "%",  pc);
    addF (pid::cutoff, "Cutoff", ranges::cutoff, "Hz", hz);
    addF (pid::reso,   "Reso",   ranges::reso,   "%",  pc);
    addF (pid::drift,  "Drift",  ranges::drift,  "%",  pc);
    addF (pid::wow,    "Wow",    ranges::wow,    "%",  pc);
    addF (pid::tape,   "Tape",   ranges::tape,   "%",  pc);
    addF (pid::lofi,   "LoFi",   ranges::lofi,   "%",  pc);
    addF (pid::hiss,   "Hiss",   ranges::hiss,   "%",  pc);
    addF (pid::input,  "In",     ranges::input,  "dB", db);
    addF (pid::output, "Out",    ranges::output, "dB", db);
    addF (pid::mix,    "Mix",    ranges::mix,    "%",  pc);

    // Gesture macros
    addF (pid::macroGlow,    "Glow",    ranges::macroGlow,    "%", pc);
    addF (pid::macroDrift,   "Drift",   ranges::macroDrift,   "%", pc);
    addF (pid::macroGrain,   "Grain",   ranges::macroGrain,   "%", pc);
    addF (pid::macroSpace,   "Space",   ranges::macroSpace,   "%", pc);
    addF (pid::macroShimmer, "Shimmer", ranges::macroShimmer, "%", pc);

    // Modulation
    addF (pid::lfoRate,    "Rate",   ranges::lfoRate,    "Hz", hz);
    addC (pid::lfoShape,   "Shape",  lfoShapeNames(), 0);
    addC (pid::lfoDest,    "Dest",   lfoDestNames(), 0);
    addF (pid::lfoAmount,  "Amount", ranges::lfoAmount, "%", pc);
    addF (pid::autoMotion, "Auto",   ranges::autoMotion, "%", pc);
    addC (pid::pressureDest, "Pressure", pressureDestNames(), 0);
    addC (pid::slideDest,    "Slide",    slideDestNames(), 0);
    addF (pid::expression,   "Expression", ranges::expression, "%", pc);

    // Gloop: eight independent lanes. Target/amount/speed are ordinary
    // parameters (so a preset can prepare the orbits); REC/LOOP/FADE state and
    // the recorded gesture itself are performance state, persisted separately.
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto n = juce::String (lane + 1);
        addC (pid::gloopTarget[lane], ("Gloop " + n + " Target").toRawUTF8(),
              gloopTargetNames(), 0);
        addF (pid::gloopAmount[lane], ("Gloop " + n + " Amount").toRawUTF8(),
              ranges::gloopAmount, "%", pc);
        addF (pid::gloopSpeed[lane], ("Gloop " + n + " Speed").toRawUTF8(),
              ranges::gloopSpeed, "x", spd);
    }
    addF (pid::gloopSmooth, "Gloop Smooth", ranges::gloopSmooth, "%", pc);

    return layout;
}

// -----------------------------------------------------------------------------
// APVTS -> EngineParams adapter. Called once per block on the audio thread;
// only atomic loads and arithmetic, no allocation.
// -----------------------------------------------------------------------------
inline EngineParams readParams (const juce::AudioProcessorValueTreeState& apvts)
{
    auto raw = [&apvts] (const char* id) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load (std::memory_order_relaxed);
        return 0.0f;
    };

    EngineParams e;
    e.density      = raw (pid::density);
    e.sizeMs       = raw (pid::size);
    e.spray        = raw (pid::spray);
    e.scan         = raw (pid::scan);
    e.pitchSemis   = raw (pid::pitch);
    e.pitchSpread  = raw (pid::spread);
    e.reverse      = raw (pid::reverse);
    e.jitter       = raw (pid::jitter);
    e.shape        = raw (pid::shape);
    e.width        = raw (pid::width);
    e.feedback     = raw (pid::feedback);
    e.focus        = raw (pid::focus);
    e.freeze       = raw (pid::freeze) > 0.5f;
    e.tuneMode     = juce::roundToInt (raw (pid::tune));
    e.sourceMode   = juce::roundToInt (raw (pid::source));

    e.spaceMix     = raw (pid::spaceMix);
    e.spaceSize    = raw (pid::spaceSize);
    e.spaceDamp    = raw (pid::spaceDamp);
    e.spaceMod     = raw (pid::spaceMod);

    e.driveDb      = raw (pid::drive);
    e.bias         = raw (pid::bias);
    e.tone         = raw (pid::tone);
    e.cutoffHz     = raw (pid::cutoff);
    e.reso         = raw (pid::reso);
    e.drift        = raw (pid::drift);
    e.wow          = raw (pid::wow);
    e.tape         = raw (pid::tape);
    e.lofi         = raw (pid::lofi);
    e.hiss         = raw (pid::hiss);
    e.inputDb      = raw (pid::input);
    e.outputDb     = raw (pid::output);
    e.mix          = raw (pid::mix);

    e.macroGlow    = raw (pid::macroGlow);
    e.macroDrift   = raw (pid::macroDrift);
    e.macroGrain   = raw (pid::macroGrain);
    e.macroSpace   = raw (pid::macroSpace);
    e.macroShimmer = raw (pid::macroShimmer);

    e.lfoRate      = raw (pid::lfoRate);
    e.lfoShape     = juce::roundToInt (raw (pid::lfoShape));
    e.lfoDest      = juce::roundToInt (raw (pid::lfoDest));
    e.lfoAmount    = raw (pid::lfoAmount);
    e.autoMotion   = raw (pid::autoMotion);
    e.pressureDest = juce::roundToInt (raw (pid::pressureDest));
    e.slideDest    = juce::roundToInt (raw (pid::slideDest));
    e.expression   = raw (pid::expression);

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        auto& l = e.gloopLanes[(size_t) lane];
        l.target = juce::roundToInt (raw (pid::gloopTarget[lane]));
        l.amount = juce::jlimit (0.0f, 1.0f, raw (pid::gloopAmount[lane]));
        l.speed  = juce::jlimit (gloop::kMinSpeed, gloop::kMaxSpeed,
                                 raw (pid::gloopSpeed[lane]));
        // l.mode is injected by the processor from its performance-state atomics.
    }
    e.gloopSmoothing = juce::jlimit (0.0f, 1.0f, raw (pid::gloopSmooth));

    return e;
}

} // namespace nagloed
