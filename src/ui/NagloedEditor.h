#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AfterglowLookAndFeel.h"
#include "GloopPanel.h"
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
                            private juce::Timer,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    NagloedEditor (juce::AudioProcessor& audioProcessor,
                   juce::AudioProcessorValueTreeState& apvts,
                   const VisualSource& visualSource,
                   PresetAccess& presetAccess,
                   GloopControl& gloopControl);
    ~NagloedEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Deterministic preview/test hook: stop the wall-clock timer, reset the
        animated/random visual state, advance a fixed number of frames and pin the
        visualiser phases. The editor's timer stays stopped afterwards, so take the
        snapshot immediately. Used by nagl_preview for the byte-reproducible hero. */
    void advanceForPreview (int frames, double dtSeconds = 1.0 / 60.0);

    /** Deterministic frame advance without stopping the timer or resetting state.
        Used by the soak harness so it does not depend on runner timer delivery. */
    void stepVisualsForPreview (int frames, double dtSeconds = 1.0 / 60.0);

    /** Open/close the Gloop overlay. GrainCloud hides while it is open, so the
        target knob rings in the Sintel/EAR/bottom panels stay watchable. */
    void setGloopPanelOpen (bool shouldBeOpen);
    bool isGloopPanelOpen() const noexcept { return gloopOpen; }

    /** The performance interface the Gloop panel drives (modes, randomize). */
    GloopControl& getGloopControl() const noexcept { return gloop; }

    /** Deterministic-preview hook: hand the panels (and the Gloop overlay) a
        fixed lane frame. Used by `nagl_preview --gloop` for the layout/ring
        documentation image; it does not touch the engine or the timer. */
    void setGloopVisualForPreview (const gloop::Visual& visual);

    /** QA hook: is the animation clock running? (Regression test for the 1.0.0
        host-mount bug, where a visibility change stopped it for good.) */
    bool isAnimationTimerRunningForTest() const noexcept { return isTimerRunning(); }
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

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

    // APVTS listener: any parameter change (mouse or host automation) schedules
    // one async visual refresh, so the curve/scope/meters never wait for a tick.
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    void layoutAtScale();
    void layoutGloopToggle();
    void applyScale (float newScale);
    void refreshOnce();
    void verifyParameterCoverage();
    void renderPage (juce::Graphics&, juce::Rectangle<float>);

    juce::AudioProcessorValueTreeState& apvts;
    const VisualSource& visual;
    PresetAccess& presets;
    GloopControl& gloop;

    AfterglowLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;

    TopBar topBar;
    GrainCloud grainCloud;
    SintelPanel sintelPanel;
    EarPanel earPanel;
    BottomPanel bottomPanel;
    GloopPanel gloopPanel;
    juce::TextButton gloopToggle;
    PageCache pageCache;

    VisualState snapshot;
    double lastTickMs = 0.0;
    int tickCounter = 0;
    float uiScale = 1.0f;
    bool gloopOpen = false;

    juce::Rectangle<int> topRect, mainRect, bottomRect, footerRect;
    juce::String coverageError;
    std::unique_ptr<juce::Component> errorBanner;
    std::unique_ptr<juce::Component> resizeGrip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NagloedEditor)
};

} // namespace nagloed::ui
