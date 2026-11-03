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

// Applies one object of {parameterId: value} pairs. Values are real-world units.
// Choice parameters may also be given by their display string ("Minor").
static bool applyParamsObject (juce::AudioProcessorValueTreeState& apvts, const juce::var& paramsVar)
{
    auto* obj = paramsVar.getDynamicObject();
    if (obj == nullptr)
        return false;

    bool appliedAny = false;

    for (const auto& prop : obj->getProperties())
    {
        const auto id = prop.name.toString();

        // Metadata keys are not parameters: skip silently.
        if (id == "name" || id == "category" || id == "description" || id == "params")
            continue;

        auto* param = apvts.getParameter (id);
        if (param == nullptr)
        {
            DBG ("NAGLOED preset: unknown parameter '" << id << "'");
            continue;
        }

        float value = 0.0f;
        if (prop.value.isString())
        {
            // Choice parameters can be authored by name for readability.
            const auto names = param->getAllValueStrings();
            const auto index = names.indexOf (prop.value.toString());
            if (index < 0)
            {
                DBG ("NAGLOED preset: '" << prop.value.toString()
                     << "' is not a choice of '" << id << "'");
                continue;
            }
            value = (float) index;
        }
        else if (prop.value.isBool())
        {
            value = (bool) prop.value ? 1.0f : 0.0f;
        }
        else
        {
            value = static_cast<float> (static_cast<double> (prop.value));
        }

        param->setValueNotifyingHost (param->convertTo0to1 (value));
        appliedAny = true;
    }

    return appliedAny;
}

static bool applyJson (juce::AudioProcessorValueTreeState& apvts, const char* jsonText)
{
    const auto parsed = juce::JSON::parse (juce::String::fromUTF8 (jsonText));
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    // Presets are authored as {name, category, description, params:{...}}.
    // Flat {parameterId: value} objects are also accepted.
    const auto paramsVar = obj->getProperty ("params");
    if (paramsVar.isObject())
        return applyParamsObject (apvts, paramsVar);

    return applyParamsObject (apvts, parsed);
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
