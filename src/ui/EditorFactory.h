#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "core/Contracts.h"

namespace nagloed::ui
{
// The GUI is created through this one seam so the processor never needs to know
// anything about the visual implementation.
std::unique_ptr<juce::AudioProcessorEditor> createEditor (juce::AudioProcessor& processor,
                                                          juce::AudioProcessorValueTreeState& apvts,
                                                          const VisualSource& visual,
                                                          PresetAccess& presets,
                                                          GloopControl& gloop);
} // namespace nagloed::ui
