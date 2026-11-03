#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AfterglowLookAndFeel.h"
#include "Theme.h"
#include "core/Contracts.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// Preset browser: name, category and description with prev/next arrows and a
// grouped popup menu. Reads everything through the PresetAccess contract.
// -----------------------------------------------------------------------------
class PresetBrowser final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    explicit PresetBrowser (PresetAccess& presets);

    void setUiScale (float newScale);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void load (int index);
    void step (int delta);
    void showMenu();
    juce::Rectangle<float> nameArea() const;

    PresetAccess& presets;
    juce::TextButton prevButton, nextButton;

    float uiScale = 1.0f;
    int currentIndex = -1;
    int numPresets = 0;
    juce::String presetName, category, description;
    juce::Point<float> mousePosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace nagloed::ui
