#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "core/EngineParams.h"
#include "core/Contracts.h"

namespace nagloed
{

// -----------------------------------------------------------------------------
// Internal engine implementation.
//
// OWNERSHIP: the DSP workstream. `Engine::Impl` derives from this class, so
// anything you add here (grain scheduler, capture buffer, filter ladder, FDN,
// tape section, modulation) stays private to src/dsp/.
//
// Rules for this file:
//   * prepare()/reset() run on the message thread, process()/setParams()/
//     getVisual() on the audio thread.
//   * process() must not allocate, lock, throw or call any JUCE function that
//     does. Preallocate everything in prepare().
//   * getVisual() is called immediately after process() on the audio thread, so
//     it can read live engine state without synchronisation.
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

protected:
    // The DSP workstream is free to replace everything below.
    EngineParams params {};
    double sampleRate = 48000.0;
    int blockSize = 512;
    int channels = 2;
    float inputGain = 1.0f;
    float visualInL = 0.0f, visualInR = 0.0f;
    float visualOutL = 0.0f, visualOutR = 0.0f;
    float bufferFill = 0.0f;
    mutable juce::uint32 frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineDSP)
};

} // namespace nagloed
