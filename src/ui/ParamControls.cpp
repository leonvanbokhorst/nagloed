#include "ParamControls.h"
#include "Theme.h"
#include "core/Parameters.h"

namespace nagloed::ui
{

// =============================================================================
ParamSlider::ParamSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalDrag);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                         juce::MathConstants<float>::pi * 2.75f, true);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setScrollWheelEnabled (false);
    setSliderSnapsToMousePosition (false);
    setWantsKeyboardFocus (true);
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ParamSlider::setUiScale (float newScale) noexcept
{
    getProperties().set ("uiScale", (double) newScale);
}

float ParamSlider::scaleFromProperties() const noexcept
{
    return juce::jlimit (0.5f, 3.0f,
                         (float) (double) getProperties().getWithDefault ("uiScale", 1.0));
}

void ParamSlider::mouseDown (const juce::MouseEvent& e)
{
    dragStartPosition = e.position;
    dragStartValue = getValue();
    fineDrag = e.mods.isShiftDown();

    juce::Slider::mouseDown (e);

    dragStartValue = getValue();
}

void ParamSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    // Re-anchor when shift is pressed or released mid-drag, so the value never
    // jumps: the new sensitivity applies from the current position onwards.
    const auto fine = e.mods.isShiftDown();
    if (fine != fineDrag)
    {
        fineDrag = fine;
        dragStartPosition = e.position;
        dragStartValue = getValue();
    }

    const auto pixelsForFullRange = (fineDrag ? 1250.0 : 260.0) * (double) scaleFromProperties();
    const auto startNorm = juce::jlimit (0.0, 1.0, valueToProportionOfLength (dragStartValue));
    const auto deltaNorm = (double) (e.position.x - dragStartPosition.x) / juce::jmax (1.0, pixelsForFullRange);
    const auto newNorm = juce::jlimit (0.0, 1.0, startNorm + deltaNorm);

    setValue (proportionOfLengthToValue (newNorm), juce::sendNotificationSync);
}

void ParamSlider::mouseUp (const juce::MouseEvent& e)
{
    juce::Slider::mouseUp (e);
}

void ParamSlider::mouseEnter (const juce::MouseEvent& e)
{
    juce::Slider::mouseEnter (e);
    repaint();
}

void ParamSlider::mouseExit (const juce::MouseEvent& e)
{
    juce::Slider::mouseExit (e);
    repaint();
}

void ParamSlider::focusGained (juce::Component::FocusChangeType type)
{
    juce::Slider::focusGained (type);
    repaint();
}

void ParamSlider::focusLost (juce::Component::FocusChangeType type)
{
    juce::Slider::focusLost (type);
    repaint();
}

// =============================================================================
ParamControlSet::ParamControlSet (juce::AudioProcessorValueTreeState& state,
                                  AfterglowLookAndFeel& lookAndFeel)
    : apvts (state), lnf (lookAndFeel)
{
}

juce::RangedAudioParameter* ParamControlSet::findParam (const char* id) const
{
    auto* param = apvts.getParameter (id);
    return dynamic_cast<juce::RangedAudioParameter*> (param);
}

juce::String ParamControlSet::tooltipFor (const juce::RangedAudioParameter& param)
{
    auto tip = param.getName (48);
    const auto label = param.getLabel();

    if (label.isNotEmpty())
        tip << " (" << label << ")";

    if (dynamic_cast<const juce::AudioParameterChoice*> (&param) != nullptr)
        tip << theme::utf8 (" — click to choose");
    else
        tip << theme::utf8 (" — drag to adjust · shift-drag for fine · double-click to reset");

    return tip;
}

ParamSlider& ParamControlSet::addKnob (juce::Component& parent, const char* paramId,
                                       const juce::String& caption)
{
    auto* param = findParam (paramId);
    jassert (param != nullptr);

    auto slider = std::make_unique<ParamSlider>();
    auto& s = *slider;

    s.getProperties().set ("uiScale", (double) uiScale);
    s.getProperties().set ("caption", caption);
    s.getProperties().set ("paramId", juce::String (paramId));

    if (param != nullptr)
    {
        s.setName (param->getName (48));
        s.setTitle (param->getName (48));
        s.setDescription (tooltipFor (*param));
        s.setTooltip (tooltipFor (*param));
    }

    s.textFromValueFunction = [param] (double value)
    {
        if (param == nullptr)
            return juce::String (value, 2);
        return param->getText (param->convertTo0to1 ((float) value), 32);
    };

    if (param != nullptr)
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    else
        s.setDoubleClickReturnValue (true, 0.0);

    if (param != nullptr)
    {
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, s);

        if (attachment != nullptr)
        {
            sliderAttachments.push_back (std::move (attachment));
            attachedIds.add (paramId);
        }
    }

    knobsByParamId.emplace_back (juce::String (paramId), &s);

    parent.addAndMakeVisible (s);
    controls.push_back (std::move (slider));

    return s;
}

juce::Slider& ParamControlSet::addRibbon (juce::Component& parent, const char* paramId,
                                          const juce::String& caption)
{
    auto* param = findParam (paramId);
    jassert (param != nullptr);

    auto slider = std::make_unique<ParamSlider>();
    auto& s = *slider;

    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.getProperties().set ("uiScale", (double) uiScale);
    s.getProperties().set ("caption", caption);
    s.getProperties().set ("paramId", juce::String (paramId));

    if (param != nullptr)
    {
        s.setName (param->getName (48));
        s.setTitle (param->getName (48));
        s.setDescription (tooltipFor (*param));
        s.setTooltip (tooltipFor (*param));
    }

    s.textFromValueFunction = [param] (double value)
    {
        if (param == nullptr)
            return juce::String (value, 2);
        return param->getText (param->convertTo0to1 ((float) value), 32);
    };

    if (param != nullptr)
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    else
        s.setDoubleClickReturnValue (true, 0.0);

    if (param != nullptr)
    {
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, s);

        if (attachment != nullptr)
        {
            sliderAttachments.push_back (std::move (attachment));
            attachedIds.add (paramId);
        }
    }

    parent.addAndMakeVisible (s);
    controls.push_back (std::move (slider));

    return s;
}

juce::ComboBox& ParamControlSet::addChoice (juce::Component& parent, const char* paramId,
                                            const juce::String& caption)
{
    auto* param = findParam (paramId);
    jassert (param != nullptr);

    auto combo = std::make_unique<juce::ComboBox>();
    auto& c = *combo;

    c.getProperties().set ("uiScale", (double) uiScale);
    c.getProperties().set ("caption", caption);
    if (param != nullptr)
    {
        c.setName (param->getName (48));
        c.setTitle (param->getName (48));
        c.setDescription (tooltipFor (*param));
        c.setTooltip (tooltipFor (*param));
    }
    c.setWantsKeyboardFocus (true);
    c.setJustificationType (juce::Justification::centredLeft);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        c.addItemList (choice->choices, 1);
    else
        c.addItem (theme::utf8 ("—"), 1);

    if (param != nullptr)
    {
        auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, paramId, c);

        if (attachment != nullptr)
        {
            comboAttachments.push_back (std::move (attachment));
            attachedIds.add (paramId);
        }
    }

    parent.addAndMakeVisible (c);
    controls.push_back (std::move (combo));

    return c;
}

void ParamControlSet::collectParameterIds (juce::StringArray& ids) const
{
    for (const auto& id : attachedIds)
        ids.addIfNotAlreadyThere (id);
}

ParamSlider* ParamControlSet::findKnob (const char* paramId) const
{
    if (paramId == nullptr)
        return nullptr;

    const juce::String wanted (paramId);

    for (const auto& entry : knobsByParamId)
        if (entry.first == wanted)
            return entry.second;

    return nullptr;
}

void ParamControlSet::applyGloopVisual (const gloop::Visual& visual)
{
    // One knob can carry several lanes (a common gesture: two orbits on Cutoff).
    // The ring data is therefore a fixed 8-slot array per knob, indexed by lane,
    // so the LookAndFeel can draw one dot per active lane and sum the influence.
    struct RingData
    {
        std::array<float, (size_t) gloop::kNumLanes> phase {};
        std::array<float, (size_t) gloop::kNumLanes> amount {};
        float offset = 0.0f;
        std::array<bool, (size_t) gloop::kNumLanes> used {};

        RingData()
        {
            phase.fill (-1.0f);
            amount.fill (0.0f);
        }
    };

    std::vector<std::pair<ParamSlider*, RingData>> affected;

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto& l = visual.lanes[(size_t) lane];
        const auto mode = (gloop::Mode) l.mode;

        // Only a lane that is actually moving modulates its target; a READY
        // (Idle + gesture) lane contributes nothing, so it draws no ring.
        if (mode != gloop::Mode::Record && mode != gloop::Mode::Loop && mode != gloop::Mode::Fade)
            continue;

        auto* slider = findKnob (gloopTargetParamId (l.target));

        if (slider == nullptr)
            continue;   // target without a knob in this panel

        RingData* data = nullptr;

        for (auto& entry : affected)
            if (entry.first == slider)
                data = &entry.second;

        if (data == nullptr)
        {
            affected.emplace_back (slider, RingData {});
            data = &affected.back().second;
        }

        const auto idx = (size_t) lane;
        data->phase[idx] = juce::jlimit (-1.0f, 1.0f, l.phase);
        data->amount[idx] = juce::jlimit (0.0f, 1.0f, l.amount);
        // LaneVisual::offset is already the *effective* normalised offset
        // (gesture * amount * fade * random): sum it as-is. Scaling by amount
        // here would apply the lane's gain twice. `amount` only drives the
        // dot/track intensity.
        data->offset += l.offset;
        data->used[idx] = true;
    }

    // Knobs that dropped out of the active set lose their ring once.
    for (auto* slider : gloopRinged)
    {
        bool stillActive = false;

        for (const auto& entry : affected)
            if (entry.first == slider)
                stillActive = true;

        if (! stillActive)
        {
            auto& props = slider->getProperties();
            props.remove ("gloopPhases");
            props.remove ("gloopAmounts");
            props.remove ("gloopOffset");
            slider->repaint();
        }
    }

    gloopRinged.clear();

    for (auto& entry : affected)
    {
        auto* slider = entry.first;
        const auto& data = entry.second;

        juce::Array<juce::var> phases, amounts;

        for (int lane = 0; lane < gloop::kNumLanes; ++lane)
        {
            const auto idx = (size_t) lane;
            phases.add (juce::var (data.used[idx] ? data.phase[idx] : -1.0f));
            amounts.add (juce::var (data.amount[idx]));
        }

        auto& props = slider->getProperties();
        props.set ("gloopPhases", juce::var (phases));
        props.set ("gloopAmounts", juce::var (amounts));
        props.set ("gloopOffset", juce::var (juce::jlimit (-1.0f, 1.0f, data.offset)));

        gloopRinged.push_back (slider);
        slider->repaint();
    }
}

void ParamControlSet::setUiScale (float newScale)
{
    uiScale = newScale;

    for (auto& control : controls)
        control->getProperties().set ("uiScale", (double) newScale);
}

} // namespace nagloed::ui
