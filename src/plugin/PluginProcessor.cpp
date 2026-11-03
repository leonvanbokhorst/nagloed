#include "PluginProcessor.h"
#include "Presets.h"
#include "ui/EditorFactory.h"

namespace nagloed
{

NagloedProcessor::NagloedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAGLOED", createParameterLayout())
{
    VisualState init;
    visualChannel.publish (init);
}

void NagloedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples (engine.getLatencySamples());
    VisualState init;
    visualChannel.publish (init);
}

void NagloedProcessor::releaseResources() {}

bool NagloedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void NagloedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setParams (readParams (apvts));
    engine.process (buffer, midi);

    VisualState v;
    engine.getVisual (v);
    visualChannel.publish (v);
}

juce::AudioProcessorEditor* NagloedProcessor::createEditor()
{
#if defined (NAGLOED_HEADLESS) && NAGLOED_HEADLESS
    return nullptr;   // console tools link the processor without the GUI
#else
    return ui::createEditor (*this, apvts, *this, *this).release();
#endif
}

int NagloedProcessor::getNumPrograms() { return presets::count(); }
int NagloedProcessor::getCurrentProgram() { return currentPreset; }

void NagloedProcessor::setCurrentProgram (int index)
{
    loadPreset (index);
}

const juce::String NagloedProcessor::getProgramName (int index)
{
    return presets::info (index).name;
}

void NagloedProcessor::loadPreset (int index)
{
    if (presets::applyTo (apvts, index))
        currentPreset = juce::jlimit (0, presets::count() - 1, index);
}

int NagloedProcessor::getNumPresets() const { return presets::count(); }
PresetInfo NagloedProcessor::getPresetInfo (int index) const { return presets::info (index); }

void NagloedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.state.createCopy();
    state.setProperty ("currentPreset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NagloedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
            apvts.replaceState (tree);
            currentPreset = juce::jlimit (0, presets::count() - 1,
                                          (int) tree.getProperty ("currentPreset", 0));
        }
    }
}

} // namespace nagloed

// -----------------------------------------------------------------------------
// Plugin entry point.
// -----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nagloed::NagloedProcessor();
}
