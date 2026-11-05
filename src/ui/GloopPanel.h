#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

#include "AfterglowLookAndFeel.h"
#include "ParamControls.h"
#include "Theme.h"
#include "core/Contracts.h"
#include "core/Gloop.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// Gloop: eight independent, deliberately unsynchronised gesture loopers.
//
// The panel is an overlay over the grain-cloud area only (see NagloedEditor), so
// the Sintel/EAR/bottom knobs stay visible while the loops run: that is the
// whole point of Gloop, you watch the target knobs move.
//
// One column of eight lanes. A lane is a phase ring (moving playhead + current
// offset), its number, the target choice, one state button and the amount/speed
// ribbons. The state button is the *only* performance control that is not a
// host parameter: its label follows the lane's actual state from VisualState and
// clicking it advances IDLE -> RECORD -> LOOP -> FADE through GloopControl.
//
// Everything here is UI-only: no DSP includes, no engine knowledge.
// -----------------------------------------------------------------------------
class GloopPanel final : public juce::Component
{
public:
    GloopPanel (juce::AudioProcessorValueTreeState& apvts,
                AfterglowLookAndFeel& lookAndFeel,
                GloopControl& gloopControl);
    ~GloopPanel() override = default;

    void setUiScale (float newScale);

    /** Called from the editor's tick path (~30 Hz) with the live lane state. */
    void tick (const gloop::Visual& visual, double elapsedSeconds);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Parameter ids attached here (used by the editor's coverage check). */
    void collectParameterIds (juce::StringArray& ids) const;

private:
    class BackgroundCache final : public theme::CachedBackground
    {
    public:
        explicit BackgroundCache (GloopPanel& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        GloopPanel& owner;
    };

    void layoutControls();
    void renderBackground (juce::Graphics&, juce::Rectangle<float>);

    /** Push the live lane state into labels, palettes and enabled states. */
    void syncLaneControls();

    /** Idle fallback: what the engine has not reported yet, we ask for. */
    int effectiveMode (int lane) const;

    /** The lane's target: the engine's report, with the host parameter as a
        fallback so the panel works before the first engine frame arrives. */
    int laneTarget (int lane) const;

    void handleModeClick (int lane);

    void paintLaneRing (juce::Graphics&, int lane, juce::Rectangle<float> area) const;

    static juce::String modeLabel (int mode, bool hasGesture, bool targetOff);

    juce::AudioProcessorValueTreeState& apvts;
    ParamControlSet controls;
    BackgroundCache background;
    GloopControl& gloop;

    float uiScale = 1.0f;

    std::array<juce::ComboBox*, (size_t) gloop::kNumLanes> targetChoices {};
    std::array<juce::TextButton*, (size_t) gloop::kNumLanes> modeButtons {};
    std::array<juce::Slider*, (size_t) gloop::kNumLanes> amountRibbons {};
    std::array<juce::Slider*, (size_t) gloop::kNumLanes> speedRibbons {};
    juce::TextButton* randomizeButton = nullptr;
    juce::Slider* smoothKnob = nullptr;

    std::vector<std::unique_ptr<juce::TextButton>> buttons;

    gloop::Visual visual {};
    juce::Rectangle<int> headerRect, footerRect;
    std::array<juce::Rectangle<int>, (size_t) gloop::kNumLanes> rowRects {};
    std::array<juce::Rectangle<int>, (size_t) gloop::kNumLanes> ringRects {};
    std::array<juce::Rectangle<int>, (size_t) gloop::kNumLanes> numberRects {};
    int tickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GloopPanel)
};

} // namespace nagloed::ui
