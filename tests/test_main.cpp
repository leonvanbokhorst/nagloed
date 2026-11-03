// NAGLOED test runner. Dependency-free mini harness: the DSP and QA workstreams
// extend this file (or add more translation units under tests/).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <string>

#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "core/Parameters.h"

namespace
{
int gFailures = 0;
int gChecks = 0;

void check (bool condition, const std::string& what)
{
    ++gChecks;
    if (! condition)
    {
        ++gFailures;
        std::printf ("  FAIL  %s\n", what.c_str());
    }
}
} // namespace

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
    processor.setStateInformation (state.getData(), (int) state.getSize());

    std::printf ("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
