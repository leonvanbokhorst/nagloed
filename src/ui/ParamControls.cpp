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
    // One knob can carry several Gloop lanes (a common gesture: two orbits on
    // Cutoff). The knob must still read at a glance, so all of their effective
    // offsets are summed into a single influence value. The LookAndFeel draws
    // that as one light-blue arc around the knob's orange live-value ring; it
    // never draws per-lane playheads here (phase belongs in the Gloop panel).
    struct Influence
    {
        float offset = 0.0f;
        float amount = 0.0f;
    };

    std::vector<std::pair<ParamSlider*, Influence>> affected;

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto& l = visual.lanes[(size_t) lane];
        const auto mode = (gloop::Mode) l.mode;

        // Only a lane with actual playback influence draws. READY (Idle) and
        // RECORD contribute zero by contract; FADE contributes what is left of
        // its gain, so it keeps drawing while it withdraws.
        if (mode != gloop::Mode::Loop && mode != gloop::Mode::Fade)
            continue;

        for (int t = 0; t < l.numTracks; ++t)
        {
            const auto& track = l.tracks[(size_t) t];
            auto* slider = findKnob (gloopTargetParamId (track.target));

            if (slider == nullptr)
                continue;   // target without a knob in this panel

            Influence* data = nullptr;

            for (auto& entry : affected)
                if (entry.first == slider)
                    data = &entry.second;

            if (data == nullptr)
            {
                affected.emplace_back (slider, Influence {});
                data = &affected.back().second;
            }

            // LaneVisual::offset is already the *effective* normalised offset
            // (gesture * amount * fade * random) for this track: sum it as-is.
            // Scaling by amount here would apply the lane's gain twice. `amount`
            // only drives the arc intensity.
            data->offset += track.offset;
            data->amount = juce::jmax (data->amount,
                                       juce::jlimit (0.0f, 1.0f, l.amount));
        }
    }

    // Knobs that dropped out of the active set lose their blue influence once.
    for (auto* slider : gloopRinged)
    {
        bool stillActive = false;

        for (const auto& entry : affected)
            if (entry.first == slider)
                stillActive = true;

        if (! stillActive)
        {
            auto& props = slider->getProperties();
            props.remove ("gloopOffset");
            props.remove ("gloopAmount");
            props.remove ("gloopActive");
            slider->repaint();
        }
    }

    gloopRinged.clear();

    for (auto& entry : affected)
    {
        auto* slider = entry.first;
        const auto& data = entry.second;

        auto& props = slider->getProperties();
        props.set ("gloopOffset", juce::var (juce::jlimit (-1.0f, 1.0f, data.offset)));
        props.set ("gloopAmount", juce::var (juce::jlimit (0.0f, 1.0f, data.amount)));
        props.set ("gloopActive", juce::var (true));

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
