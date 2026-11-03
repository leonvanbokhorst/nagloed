#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

#include "AfterglowLookAndFeel.h"
#include "ParamControls.h"
#include "Theme.h"
#include "Visuals.h"
#include "core/Contracts.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// Sintel: the grain engine controls (10 rotaries + tune/source choices) with a
// small live capture-buffer readout at the foot of the rail.
// -----------------------------------------------------------------------------
class SintelPanel final : public juce::Component
{
public:
    SintelPanel (juce::AudioProcessorValueTreeState& apvts, AfterglowLookAndFeel& lookAndFeel);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;

    void collectParameterIds (juce::StringArray& ids) const;

private:
    class BackgroundCache final : public theme::CachedBackground
    {
    public:
        explicit BackgroundCache (SintelPanel& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        SintelPanel& owner;
    };

    void renderBackground (juce::Graphics&, juce::Rectangle<float>);
    void layoutControls();

    ParamControlSet controls;
    BackgroundCache background;

    float uiScale = 1.0f;
    juce::Rectangle<float> headerRect, gridArea, statusArea;
    std::array<juce::Component*, 12> cells {};
    juce::ComboBox* tuneChoice = nullptr;
    juce::ComboBox* sourceChoice = nullptr;

    float bufferFill = 0.0f;
    float grainRate = 0.0f;
    int activeNotes = 0;
    int tickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SintelPanel)
};

// -----------------------------------------------------------------------------
// EAR: the analogue resonance chain, with a live 24 dB filter curve.
// -----------------------------------------------------------------------------
class EarPanel final : public juce::Component
{
public:
    EarPanel (juce::AudioProcessorValueTreeState& apvts, AfterglowLookAndFeel& lookAndFeel);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;

    void collectParameterIds (juce::StringArray& ids) const;

private:
    class BackgroundCache final : public theme::CachedBackground
    {
    public:
        explicit BackgroundCache (EarPanel& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        EarPanel& owner;
    };

    void renderBackground (juce::Graphics&, juce::Rectangle<float>);
    void layoutControls();

    ParamControlSet controls;
    BackgroundCache background;
    FilterCurve filterCurve;

    float uiScale = 1.0f;
    juce::Rectangle<float> headerRect, filterArea, curveArea, gridArea;
    std::array<juce::Slider*, 13> knobs {};
    juce::Slider* cutoffKnob = nullptr;
    juce::Slider* resoKnob = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarPanel)
};

// -----------------------------------------------------------------------------
// Bottom strip: five large gesture macros, Ruimte, the mod/LFO section and a
// pair of analogue VU meters.
// -----------------------------------------------------------------------------
class BottomPanel final : public juce::Component
{
public:
    BottomPanel (juce::AudioProcessorValueTreeState& apvts, AfterglowLookAndFeel& lookAndFeel);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;
    void resized() override;

    void collectParameterIds (juce::StringArray& ids) const;

private:
    class BackgroundCache final : public theme::CachedBackground
    {
    public:
        explicit BackgroundCache (BottomPanel& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        BottomPanel& owner;
    };

    void renderBackground (juce::Graphics&, juce::Rectangle<float>);
    void layoutControls();

    ParamControlSet controls;
    BackgroundCache background;
    LfoVisual lfoVisual;
    SpaceVisual spaceVisual;
    AnalogueMeter meterIn { "IN", true };
    AnalogueMeter meterOut { "OUT", false };

    float uiScale = 1.0f;

    juce::Rectangle<float> gestureArea, ruimteArea, modArea, meterArea;
    juce::Rectangle<float> gestureInner, ruimteInner, modInner, meterInner;
    std::array<juce::Slider*, 5> macroKnobs {};
    std::array<juce::Slider*, 4> ruimteKnobs {};
    std::array<juce::Slider*, 4> modKnobs {};
    std::array<juce::ComboBox*, 4> modChoices {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BottomPanel)
};

} // namespace nagloed::ui
