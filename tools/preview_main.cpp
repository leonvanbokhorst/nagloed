// Renders the real editor offscreen to a PNG so the UI can be reviewed without
// opening a DAW. Used for docs/ui-preview.png and for design iteration.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <memory>

#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "ui/EditorFactory.h"

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const double sampleRate = 48000.0;
    const int blockSize = 512;

    nagloed::NagloedProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    // The documentation image should show the instrument playing, not an idle
    // instance: load a spacious factory pad and feed it a warm chord so the
    // capture buffer fills and the grain field comes alive.
    const int presetIndex = 7;   // 07 - Wolkenkaart
    processor.loadPreset (presetIndex);

    // Ordinary parameter moves, so the knobs in the picture show exactly what
    // the engine is doing: a dense, wide, glowing cloud rather than the sparse
    // default. Nothing here is faked after rendering.
    auto setParam = [&processor] (const char* id, float value)
    {
        if (auto* p = processor.getAPVTS().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    setParam (nagloed::pid::density,    150.0f);
    setParam (nagloed::pid::size,       140.0f);
    setParam (nagloed::pid::spray,      0.55f);
    setParam (nagloed::pid::spaceMix,   0.72f);
    setParam (nagloed::pid::macroGrain, 0.85f);
    setParam (nagloed::pid::macroGlow,  0.80f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    const float chordHz[] = { 110.0f, 130.81f, 164.81f, 220.0f, 261.63f };
    const int totalSamples = (int) (3.2 * sampleRate);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - start);
        buffer.clear();

        for (int s = 0; s < n; ++s)
        {
            const double t = (start + s) / sampleRate;
            float l = 0.0f, r = 0.0f;

            for (int note = 0; note < 5; ++note)
            {
                const auto phase = (float) (t * chordHz[note] * 2.0 * juce::MathConstants<double>::pi);
                const float detune = (note % 2 == 0) ? 1.0f : 1.0015f;
                const float swell = 0.6f + 0.4f * (float) std::sin (t * 0.7 + note);
                const float voice = 0.11f * swell * (std::sin (phase * detune)
                                                   + 0.35f * std::sin (phase * 2.0f * detune)
                                                   + 0.18f * std::sin (phase * 3.0f));
                l += voice * ((note % 2 == 0) ? 0.9f : 0.6f);
                r += voice * ((note % 2 == 0) ? 0.6f : 0.9f);
            }

            buffer.setSample (0, s, l);
            buffer.setSample (1, s, r);
        }

        processor.processBlock (buffer, midi);
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    const int width  = (argc > 1) ? juce::String (argv[1]).getIntValue() : 1180;
    const int height = (argc > 2) ? juce::String (argv[2]).getIntValue() : 680;
    editor->setSize (width, height);
    editor->setVisible (true);

    // Let the editor's timers ingest the engine's visual state and repaint
    // before we snapshot, so the grain field is alive in the capture.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("docs")
                         .getChildFile ("ui-preview.png");
    out.getParentDirectory().createDirectory();

    // JUCE's POSIX FileOutputStream appends. If an older PNG is left in place,
    // decoders read the stale image in front of the new bytes.
    out.deleteFile();

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());
    if (stream == nullptr || ! png.writeImageToStream (image, *stream))
    {
        std::fprintf (stderr, "failed to write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("wrote %s (%d x %d, preset %d \"%s\")\n",
                 out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight(),
                 processor.getCurrentPreset(),
                 processor.getProgramName (processor.getCurrentPreset()).toRawUTF8());
    return 0;
}
