#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

#include "AfterglowLookAndFeel.h"
#include "PresetBrowser.h"
#include "Theme.h"
#include "core/Contracts.h"
#include "core/ParamIDs.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// Top bar: procedural NAGLOED wordmark, tagline, preset browser and the freeze
// ember button, with a quiet engine status readout.
// -----------------------------------------------------------------------------
class TopBar final : public juce::Component
{
public:
    TopBar (juce::AudioProcessorValueTreeState& apvts, PresetAccess& presets,
            AfterglowLookAndFeel& lookAndFeel);
    ~TopBar() override;

    void setUiScale (float newScale);
    void setStatus (float grainRate, int numGrains, bool hasGrains);

    /** Animate the freeze ember while it's engaged. Called from the editor timer. */
    void tick();

    void refreshPresets();
    PresetBrowser& getPresetBrowser() noexcept { return browser; }

    void collectParameterIds (juce::StringArray& ids) const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class BackgroundCache final : public theme::CachedBackground
    {
    public:
        explicit BackgroundCache (TopBar& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        TopBar& owner;
    };

    void renderBackground (juce::Graphics&, juce::Rectangle<float>);
    void layoutAtScale();

    juce::AudioProcessorValueTreeState& apvts;
    AfterglowLookAndFeel& lnf;
    PresetBrowser browser;
    juce::TextButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    BackgroundCache background;

    float uiScale = 1.0f;
    juce::Rectangle<float> wordmarkArea, taglineArea, statusArea,
                           browserCaptionRect, freezeCaptionRect;

    float grainRate = 0.0f;
    int numGrains = 0;
    bool live = false;
    float shownRate = -1.0f;
    int shownGrains = -1;
    int shownLive = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};

} // namespace nagloed::ui
