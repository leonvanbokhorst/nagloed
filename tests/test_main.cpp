// NAGLOED test runner. Dependency-free mini harness: the DSP and QA workstreams
// extend this file (or add more translation units under tests/).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <string>

#include "TestHarness.h"
#include "DspTests.h"

#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "core/Parameters.h"

using nagl_test::check;

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("NAGLOED tests\n");

    nagloed::NagloedProcessor processor;

    // --- Parameters ----------------------------------------------------------
    int paramCount = 0;
    for (auto* p : processor.getAPVTS().processor.getParameters())
    {
        if (p != nullptr)
            ++paramCount;
    }
    check (paramCount >= 40, "parameter layout has at least 40 parameters");

    for (const char* id : { nagloed::pid::density, nagloed::pid::cutoff, nagloed::pid::mix,
                            nagloed::pid::freeze, nagloed::pid::tune, nagloed::pid::lfoDest })
        check (processor.getAPVTS().getParameter (id) != nullptr,
               std::string ("parameter resolves: ") + id);

    // --- Presets -------------------------------------------------------------
    check (nagloed::presets::count() >= 1, "at least the Init preset exists");
    check (nagloed::presets::info (0).name == "Init", "preset 0 is Init");

    // --- DSP smoke test ------------------------------------------------------
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool finite = true;
    for (int block = 0; block < 32; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.1f * std::sin ((block * 512 + i) * 0.05f));
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    finite = false;
    }
    check (finite, "processor output is finite");

    // --- State round trip ----------------------------------------------------
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    check (state.getSize() > 0, "state serialises");

    // Recall, not just serialisation: change values, save, change them again,
    // restore and check the values come back. This is the bug pluginval caught
    // when the APVTS timer had not flushed before serialisation.
    {
        auto& apvts = processor.getAPVTS();
        auto setReal = [&apvts] (const char* id, float value)
        {
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        };

        setReal (nagloed::pid::density, 123.0f);
        setReal (nagloed::pid::freeze,  1.0f);

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        check (saved.getSize() > 0, "state saves after a parameter change");

        setReal (nagloed::pid::density, 30.0f);
        setReal (nagloed::pid::freeze,  0.0f);

        processor.setStateInformation (saved.getData(), (int) saved.getSize());
        const float restoredDensity = apvts.getRawParameterValue (nagloed::pid::density)->load();
        const float restoredFreeze  = apvts.getRawParameterValue (nagloed::pid::freeze)->load();
        check (std::abs (restoredDensity - 123.0f) < 0.5f, "density is recalled from saved state");
        check (restoredFreeze > 0.5f, "freeze is recalled from saved state");
    }

    processor.setStateInformation (state.getData(), (int) state.getSize());

    // --- DSP suite -----------------------------------------------------------
    runDspTests();

    std::printf ("\n%d checks, %d failures\n", nagl_test::gChecks, nagl_test::gFailures);
    return nagl_test::gFailures == 0 ? 0 : 1;
}
