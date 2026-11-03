#include "Presets.h"
#include "PresetsData.h"   // generated at build time by scripts/gen_presets.py
#include <juce_audio_processors/juce_audio_processors.h>

namespace nagloed::presets
{

int count() { return 1 + generated::count(); }

PresetInfo info (int index)
{
    if (index <= 0)
        return { "Init", "Factory", "Clean starting point: no grains, no colour, no space." };

    const auto& d = generated::data()[index - 1];
    return { juce::String::fromUTF8 (d.name),
             juce::String::fromUTF8 (d.category),
             juce::String::fromUTF8 (d.description) };
}

static void resetToDefaults (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* p : apvts.processor.getParameters())
        if (p != nullptr)
            p->setValueNotifyingHost (p->getDefaultValue());
}

static bool applyJson (juce::AudioProcessorValueTreeState& apvts, const char* jsonText)
{
    const auto parsed = juce::JSON::parse (juce::String::fromUTF8 (jsonText));
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    for (const auto& prop : obj->getProperties())
    {
        const auto id = prop.name.toString();
        auto* param = apvts.getParameter (id);
        if (param == nullptr)
        {
            DBG ("NAGLOED preset: unknown parameter '" << id << "'");
            continue;
        }

        const auto value = static_cast<float> (static_cast<double> (prop.value));
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
    return true;
}

bool applyTo (juce::AudioProcessorValueTreeState& apvts, int index)
{
    index = juce::jlimit (0, count() - 1, index);
    if (index == 0)
    {
        resetToDefaults (apvts);
        return true;
    }
    return applyJson (apvts, generated::data()[index - 1].json);
}

} // namespace nagloed::presets
