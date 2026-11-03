#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include "core/EngineParams.h"
#include "core/Contracts.h"

namespace nagloed
{

// -----------------------------------------------------------------------------
// The NAGLOED engine: "Sintel" (grains) + "Ruimte" (space) + "EAR" (analog).
//
// Contract with the rest of the plugin:
//  * prepare() is called from prepareToPlay, never while processing.
//  * setParams()/process() are audio-thread only, allocation free, and must not
//    lock or block for any reason.
//  * getVisual() is audio-thread only (called right after process()).
//  * reset() is called when the host flushes or stops.
//
// The implementation lives in src/dsp/Engine.cpp and is deliberately pimpl'd so
// the UI never links against engine internals.
// -----------------------------------------------------------------------------
class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setParams (const EngineParams& newParams) noexcept;
    void process (juce::AudioBuffer<float>& io, const juce::MidiBuffer& midi) noexcept;

    void getVisual (VisualState& out) const noexcept;
    int  getLatencySamples() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

} // namespace nagloed
