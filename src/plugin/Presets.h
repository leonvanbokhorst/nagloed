#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Contracts.h"

namespace nagloed::presets
{
int        count();
PresetInfo info (int index);      // index 0 is always "Init"
bool       applyTo (juce::AudioProcessorValueTreeState& apvts, int index);
} // namespace nagloed::presets
