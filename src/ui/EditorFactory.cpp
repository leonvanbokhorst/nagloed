#include "EditorFactory.h"
#include "NagloedEditor.h"

namespace nagloed::ui
{
std::unique_ptr<juce::AudioProcessorEditor> createEditor (juce::AudioProcessor& processor,
                                                          juce::AudioProcessorValueTreeState& apvts,
                                                          const VisualSource& visual,
                                                          PresetAccess& presets,
                                                          GloopControl& gloop)
{
    return std::make_unique<NagloedEditor> (processor, apvts, visual, presets, gloop);
}
} // namespace nagloed::ui
