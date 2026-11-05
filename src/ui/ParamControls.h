#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <utility>
#include <vector>

#include "AfterglowLookAndFeel.h"
#include "core/Gloop.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// A rotary slider with the NAGLOED feel:
//  * horizontal-drag rotary (steady, DAW-friendly),
//  * shift-drag is five times finer,
//  * double-click jumps back to the parameter default,
//  * hover/drag swaps the engraved label for the live value readout.
// All drawing happens in AfterglowLookAndFeel; this class only handles input.
// -----------------------------------------------------------------------------
class ParamSlider final : public juce::Slider
{
public:
    ParamSlider();

    void setUiScale (float newScale) noexcept;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void focusGained (juce::Component::FocusChangeType) override;
    void focusLost (juce::Component::FocusChangeType) override;

private:
    float scaleFromProperties() const noexcept;

    juce::Point<float> dragStartPosition;
    double dragStartValue = 0.0;
    bool fineDrag = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamSlider)
};

// -----------------------------------------------------------------------------
// Builds, attaches and owns every control for a panel. Attachments are stored
// here so the parameters stay in sync with the widgets for their whole life.
// -----------------------------------------------------------------------------
class ParamControlSet
{
public:
    ParamControlSet (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel);

    /** Rotary knob in a parameter's own units. `caption` is the engraved label. */
    ParamSlider& addKnob (juce::Component& parent, const char* paramId,
                          const juce::String& caption);

    /** Thin horizontal ribbon (used for scan / reverse). */
    juce::Slider& addRibbon (juce::Component& parent, const char* paramId,
                             const juce::String& caption);

    /** Drop-down choice with an engraved caption. */
    juce::ComboBox& addChoice (juce::Component& parent, const char* paramId,
                               const juce::String& caption);

    /** Called on every resize; keeps painted geometry in sync with the scale. */
    void setUiScale (float newScale);

    /** Feed the live Gloop state to the knobs that are a lane's target.
        Each affected slider receives the component properties the LookAndFeel
        reads to draw its blue influence arc ("gloopOffset", "gloopAmount",
        "gloopActive"); sliders whose lanes all stopped are cleared and
        repainted once. Offsets from several lanes on one knob are summed into
        one combined display, not drawn as separate rings. */
    void applyGloopVisual (const gloop::Visual& visual);

    /** Parameter ids that have a live attachment in this set (for coverage tests). */
    void collectParameterIds (juce::StringArray& ids) const;

    AfterglowLookAndFeel& lookAndFeel() noexcept { return lnf; }

    /** Tooltip text for a parameter, e.g. "Density (g/s) — drag to adjust…". */
    static juce::String tooltipFor (const juce::RangedAudioParameter& param);

private:
    juce::RangedAudioParameter* findParam (const char* id) const;

    ParamSlider* findKnob (const char* paramId) const;

    juce::AudioProcessorValueTreeState& apvts;
    AfterglowLookAndFeel& lnf;
    float uiScale = 1.0f;

    std::vector<std::unique_ptr<juce::Component>> controls;
    juce::StringArray attachedIds;
    std::vector<std::pair<juce::String, ParamSlider*>> knobsByParamId;
    std::vector<ParamSlider*> gloopRinged;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControlSet)
};

} // namespace nagloed::ui
