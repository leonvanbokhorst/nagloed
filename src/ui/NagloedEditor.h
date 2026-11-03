#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Contracts.h"
#include "core/Parameters.h"

namespace nagloed::ui
{

// SCAFFOLD editor: the UI workstream replaces this with the real NAGLOED face.
class NagloedEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    NagloedEditor (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&,
                   const VisualSource&, PresetAccess&);
    ~NagloedEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
    const VisualSource& visual;
    PresetAccess& presets;
    VisualState snapshot;
    juce::uint32 lastFrame = 0;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagloedEditor)
};

} // namespace nagloed::ui
