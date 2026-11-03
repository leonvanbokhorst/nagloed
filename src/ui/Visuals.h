#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AfterglowLookAndFeel.h"
#include "Theme.h"
#include "core/Contracts.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// Analogue VU with proper ballistics and a peak latch.
// -----------------------------------------------------------------------------
class AnalogueMeter final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    AnalogueMeter (const juce::String& meterLabel, bool useInput);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;

private:
    struct Needle
    {
        float db = -30.0f;
        float peakDb = -30.0f;
        float peakHold = 0.0f;
        bool initialised = false;
    };

    void rebuildFonts();

    static float dbFromRms (float rms);
    float angleForDb (float db) const;
    juce::Point<float> pointOnArc (float angle, float radius) const;
    void drawNeedle (juce::Graphics&, const Needle&, float angle, juce::Colour colour,
                     float widthScale) const;

    juce::String label;
    bool inputMeter = true;
    float uiScale = 1.0f;
    Needle left, right;
    juce::Font scaleFont { juce::FontOptions() }, labelFont { juce::FontOptions() };
    juce::Font valueFont { juce::FontOptions() }, tinyFont { juce::FontOptions() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalogueMeter)
};

// -----------------------------------------------------------------------------
// The EAR 24 dB low-pass response, with cutoff marker and live readout.
// -----------------------------------------------------------------------------
class FilterCurve final : public juce::Component
{
public:
    explicit FilterCurve (juce::AudioProcessorValueTreeState& apvts);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;

private:
    void refresh();

    juce::AudioProcessorValueTreeState& apvts;
    float uiScale = 1.0f;
    float cutoffHz = 9000.0f;
    float reso = 0.15f;
    juce::String readout;
    juce::Font readoutFont { juce::FontOptions() };
    int tickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterCurve)
};

// -----------------------------------------------------------------------------
// Compact LFO scope: shape, rate, amount and a moving phosphor phase dot.
// -----------------------------------------------------------------------------
class LfoVisual final : public juce::Component
{
public:
    explicit LfoVisual (juce::AudioProcessorValueTreeState& apvts);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;

private:
    static float lfoValue (int shape, float phase);
    void refresh();
    void rebuildFonts();

    juce::AudioProcessorValueTreeState& apvts;
    float uiScale = 1.0f;
    float phase = 0.0f;
    int shape = 0;
    float rate = 0.15f;
    float amount = 0.0f;
    int tickCounter = 0;
    juce::Font readoutFont { juce::FontOptions() }, tagFont { juce::FontOptions() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoVisual)
};

// -----------------------------------------------------------------------------
// Ruimte: a tiny perspective room whose depth tracks the space size.
// -----------------------------------------------------------------------------
class SpaceVisual final : public juce::Component
{
public:
    explicit SpaceVisual (juce::AudioProcessorValueTreeState& apvts);

    void setUiScale (float newScale);
    void tick (const VisualState& snapshot, double elapsedSeconds);
    void paint (juce::Graphics&) override;

private:
    void refresh();
    void rebuildFonts();

    juce::AudioProcessorValueTreeState& apvts;
    float uiScale = 1.0f;
    float size = 0.6f;
    float damp = 0.4f;
    float mod = 0.3f;
    float phase = 0.0f;
    juce::String readout;
    juce::Font readoutFont { juce::FontOptions() };
    int tickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceVisual)
};

} // namespace nagloed::ui
