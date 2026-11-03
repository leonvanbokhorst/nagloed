#include "EditorFactory.h"
#include "NagloedEditor.h"

namespace nagloed::ui
{
std::unique_ptr<juce::AudioProcessorEditor> createEditor (juce::AudioProcessor& processor,
                                                          juce::AudioProcessorValueTreeState& apvts,
                                                          const VisualSource& visual,
                                                          PresetAccess& presets)
{
    return std::make_unique<NagloedEditor> (processor, apvts, visual, presets);
}
} // namespace nagloed::ui
