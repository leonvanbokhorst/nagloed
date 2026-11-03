#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AfterglowLookAndFeel.h"
#include "GrainCloud.h"
#include "Panels.h"
#include "Theme.h"
#include "TopBar.h"
#include "core/Contracts.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// The NAGLOED editor: a dim analogue laboratory at night. Layout:
//
//   +--------------------------------------------------------------+
//   | wordmark · tagline            preset browser      status  FREEZE |
//   +----------+--------------------------------------+-------------+
//   |  SINTEL  |            grain cloud              |     EAR     |
//   |   rail   |   (phosphor field + buffer line)     |    chain    |
//   +----------+--------------------------------------+-------------+
//   | GESTURE (5 macros) | RUIMTE | MOD/LFO | VU IN/OUT             |
//   +--------------------------------------------------------------+
//   | serial · company · parameter count · sample rate              |
//   +--------------------------------------------------------------+
//
// Everything is drawn procedurally; the cloud animates at 60 Hz and only the
// other visualisers are refreshed at 30 Hz.
// -----------------------------------------------------------------------------
class NagloedEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    NagloedEditor (juce::AudioProcessor& audioProcessor,
                   juce::AudioProcessorValueTreeState& apvts,
                   const VisualSource& visualSource,
                   PresetAccess& presetAccess);
    ~NagloedEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class PageCache final : public theme::CachedBackground
    {
    public:
        explicit PageCache (NagloedEditor& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        NagloedEditor& owner;
    };

    void timerCallback() override;
    void layoutAtScale();
    void applyScale (float newScale);
    void refreshOnce();
    void verifyParameterCoverage();
    void renderPage (juce::Graphics&, juce::Rectangle<float>);

    const VisualSource& visual;
    PresetAccess& presets;

    AfterglowLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;

    TopBar topBar;
    GrainCloud grainCloud;
    SintelPanel sintelPanel;
    EarPanel earPanel;
    BottomPanel bottomPanel;
    PageCache pageCache;

    VisualState snapshot;
    double lastTickMs = 0.0;
    int tickCounter = 0;
    float uiScale = 1.0f;

    juce::Rectangle<int> topRect, mainRect, bottomRect, footerRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagloedEditor)
};

} // namespace nagloed::ui
