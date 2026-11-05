#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cstdint>

#include "core/Parameters.h"
#include "core/Snapshot.h"
#include "core/Contracts.h"
#include "core/Gloop.h"
#include "core/Version.h"
#include "dsp/Engine.h"

namespace nagloed
{

class NagloedProcessor final : public juce::AudioProcessor,
                               public VisualSource,
                               public PresetAccess,
                               public GloopControl,
                               private juce::Timer
{
public:
    NagloedProcessor();
    ~NagloedProcessor() override;

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

    // --- GloopControl --------------------------------------------------------
    void setGloopMode (int lane, int mode) override;
    int  getGloopMode (int lane) const override;
    void gloopRandomize() override;

    /** Drives the Gloop recorder. Called by the internal timer at 100 Hz; public
        so tests can advance it deterministically without a message loop. */
    void gloopTick (double nowSeconds);

    /** True once a lane has a completed gesture (READY or LOOP). */
    bool hasGloopGesture (int lane) const noexcept;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return apvts; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;
    void republishGloopGestures();
    void startGloopRecording (int lane, double nowSeconds, const EngineParams& params);
    void captureGloopSamples (int lane, double nowSeconds, const EngineParams& params);
    void appendGloopSample (int lane, const EngineParams& params);
    void finishGloopRecording (int lane, int requestedMode);
    void clearGloopState();
    juce::String serialiseGloopState() const;
    bool deserialiseGloopState (const juce::String&);

    Engine engine;
    SnapshotChannel<VisualState> visualChannel;
    int currentPreset = 0;

    // --- Gloop performance state (message thread owns the gesture buffers) ---
    std::array<std::atomic<int>, gloop::kNumLanes> gloopRequestedMode {};
    std::array<std::atomic<int>, gloop::kNumLanes> gloopActualMode {};
    std::array<gloop::Gesture, gloop::kNumLanes> gloopGestures {};
    std::array<bool, gloop::kNumLanes> gloopGestureValid {};
    std::array<bool, gloop::kNumLanes> gloopPublishPending {};
    std::array<bool, gloop::kNumLanes> gloopRecordingActive {};
    std::array<int, gloop::kNumLanes> gloopRecordingTarget {};
    std::array<float, gloop::kNumLanes> gloopRecordingStartNorm {};
    std::array<std::array<float, gloop::kNumTargets>, gloop::kNumLanes> gloopAutoStartNorm {};
    std::array<double, gloop::kNumLanes> gloopRecordingNextTime {};
    std::atomic<std::uint32_t> gloopRandomizeCounter { 0 };
    bool gloopEnginePrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagloedProcessor)
};

} // namespace nagloed
