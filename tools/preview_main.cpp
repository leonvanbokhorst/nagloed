// Renders the real editor offscreen to a PNG so the UI can be reviewed without
// opening a DAW.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include <memory>

#include "plugin/PluginProcessor.h"
#include "ui/EditorFactory.h"

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const double sampleRate = 48000.0;
    const int blockSize = 512;

    nagloed::NagloedProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    // Push some audio through so any live meters have something to show.
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    for (int i = 0; i < 40; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < blockSize; ++s)
                buffer.setSample (ch, s, 0.2f * std::sin ((i * blockSize + s) * 0.03f));
        processor.processBlock (buffer, midi);
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setSize (1180, 680);
    editor->setVisible (true);

    const int width = (argc > 1) ? juce::String (argv[1]).getIntValue() : 1180;
    const int height = (argc > 2) ? juce::String (argv[2]).getIntValue() : 680;
    editor->setSize (width, height);

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("docs")
                         .getChildFile ("ui-preview.png");
    out.getParentDirectory().createDirectory();

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());
    if (stream == nullptr || ! png.writeImageToStream (image, *stream))
    {
        std::fprintf (stderr, "failed to write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
