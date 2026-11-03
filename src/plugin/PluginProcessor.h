#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "core/Parameters.h"
#include "core/Snapshot.h"
#include "core/Contracts.h"
#include "core/Version.h"
#include "dsp/Engine.h"

namespace nagloed
{

class NagloedProcessor final : public juce::AudioProcessor,
                               public VisualSource,
                               public PresetAccess
{
public:
    NagloedProcessor();
    ~NagloedProcessor() override = default;

    // --- AudioProcessor ------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kProductName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool supportsMPE() const override  { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // --- VisualSource --------------------------------------------------------
    void copyVisual (VisualState& out) const override { visualChannel.read (out); }

    // --- PresetAccess --------------------------------------------------------
    int        getNumPresets() const override;
    PresetInfo getPresetInfo (int index) const override;
    int        getCurrentPreset() const override { return currentPreset; }
    void       loadPreset (int index) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return apvts; }

    juce::AudioProcessorValueTreeState apvts;

private:
    Engine engine;
    SnapshotChannel<VisualState> visualChannel;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagloedProcessor)
};

} // namespace nagloed
