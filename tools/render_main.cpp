// Renders a demo performance through the real NAGLOED processor: no host, but
// the exact same code path the plugin uses.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "plugin/PluginProcessor.h"
#include "DemoScore.h"

namespace
{
struct AutomationRuntime
{
    std::vector<demo::AutomationPoint> points;
    size_t next = 0;

    void applyUpTo (juce::AudioProcessorValueTreeState& apvts, float t)
    {
        while (next < points.size() && points[next].time <= t)
        {
            const auto& p = points[next++];
            if (auto* param = apvts.getParameter (p.parameter))
                param->setValueNotifyingHost (param->convertTo0to1 (p.value));
        }
    }
};

double rmsOf (const juce::AudioBuffer<float>& b)
{
    double sum = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double s = b.getSample (ch, i);
            sum += s * s;
        }
    return std::sqrt (sum / (double) juce::jmax (1, b.getNumSamples() * b.getNumChannels()));
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const float length = demo::lengthSeconds();

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());

    nagloed::NagloedProcessor processor;
    processor.setBusesLayout (layout);
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    const auto outFile = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("Demo").getChildFile ("Nagloed-Demo.wav");
    outFile.getParentDirectory().createDirectory();
    outFile.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
    if (stream == nullptr)
    {
        std::fprintf (stderr, "cannot open %s\n", outFile.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
        return 1;
    stream.release();

    AutomationRuntime automation;
    for (const auto& p : demo::automation())
        automation.points.push_back (p);
    std::stable_sort (automation.points.begin(), automation.points.end(),
                      [] (const auto& a, const auto& b) { return a.time < b.time; });

    juce::AudioBuffer<float> input (2, blockSize);
    juce::AudioBuffer<float> block (2, blockSize);
    juce::MidiBuffer midi;
    const int totalBlocks = (int) std::ceil (length * sampleRate / blockSize);
    double peak = 0.0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        const float t0 = (float) (b * blockSize / sampleRate);
        automation.applyUpTo (processor.getAPVTS(), t0);

        input.clear();
        demo::renderInput (t0, sampleRate, blockSize, input.getWritePointer (0), input.getWritePointer (1));

        block.makeCopyOf (input);
        processor.processBlock (block, midi);
        writer->writeFromAudioSampleBuffer (block, 0, blockSize);

        peak = std::max (peak, (double) block.getMagnitude (0, blockSize));
    }

    writer.reset();

    const auto info = outFile.getParentDirectory().getChildFile ("Nagloed-Demo.wav");
    std::printf ("wrote %s\n", info.getFullPathName().toRawUTF8());
    std::printf ("peak: %.3f (%.2f dBFS)\n", peak,
                 peak > 0.0 ? 20.0 * std::log10 (peak) : -999.0);
    return 0;
}
