#include "GloopPanel.h"

#include <cmath>

#include "core/ParamIDs.h"
#include "core/Parameters.h"

namespace nagloed::ui
{

using namespace theme;

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

// Ring drawing starts at 12 o'clock and runs clockwise, which is angle 0 for
// JUCE's centred arcs (the same convention AfterglowLookAndFeel uses).
juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
{
    return { centre.x + std::sin (angle) * radius, centre.y - std::cos (angle) * radius };
}

int laneIndex (int lane) { return juce::jlimit (0, gloop::kNumLanes - 1, lane); }
} // namespace

// =============================================================================
GloopPanel::GloopPanel (juce::AudioProcessorValueTreeState& state,
                        AfterglowLookAndFeel& lookAndFeel,
                        GloopControl& gloopControl)
    : apvts (state), controls (state, lookAndFeel), background (*this), gloop (gloopControl)
{
    setOpaque (true);
    setTitle ("Gloop gesture lanes");
    setDescription ("Eight independent gesture loopers: each lane records the movement of one "
                    "knob and orbits it. The target knobs stay visible behind this panel.");

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto n = juce::String (lane + 1);

        // The target choice carries no engraved caption: the lane number and the
        // value itself are enough, and it keeps the row usable at 380 px wide.
        targetChoices[(size_t) lane] = &controls.addChoice (*this, pid::gloopTarget[lane],
                                                            juce::String());

        auto button = std::make_unique<juce::TextButton>();
        button->getProperties().set ("role", "gloopMode");
        button->getProperties().set ("laneArgb", (int) theme::gloopLaneColour (lane).getARGB());
        button->getProperties().set ("gloopState", (int) gloop::Mode::Idle);
        button->getProperties().set ("gloopHasGesture", false);
        button->setTitle ("Gloop lane " + n + " state");
        button->getProperties().set ("uiScale", (double) uiScale);
        button->onClick = [this, lane] { handleModeClick (lane); };
        addAndMakeVisible (*button);
        modeButtons[(size_t) lane] = button.get();
        buttons.push_back (std::move (button));

        amountRibbons[(size_t) lane] = &controls.addRibbon (*this, pid::gloopAmount[lane], "AMT");
        speedRibbons[(size_t) lane] = &controls.addRibbon (*this, pid::gloopSpeed[lane], "SPD");
    }

    auto randomize = std::make_unique<juce::TextButton> ("RANDOMIZE");
    randomize->getProperties().set ("role", "gloopRandomize");
    randomize->setTitle ("Randomize Gloop lanes");
    randomize->setTooltip ("Gently transform the lanes that are already moving");
    randomize->getProperties().set ("uiScale", (double) uiScale);
    randomize->onClick = [this] { gloop.gloopRandomize(); };
    addAndMakeVisible (*randomize);
    randomizeButton = randomize.get();
    buttons.push_back (std::move (randomize));

    smoothKnob = &controls.addKnob (*this, pid::gloopSmooth, "SMOOTH");

    syncLaneControls();
}

void GloopPanel::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);

    for (auto& button : buttons)
        button->getProperties().set ("uiScale", (double) uiScale);

    resized();
    repaint();
}

// -----------------------------------------------------------------------------
juce::String GloopPanel::modeLabel (int mode, bool hasGesture, bool targetOff)
{
    // State labels, not action labels: the button says where the lane *is*, so
    // a looping lane never reads FADE. The tooltip carries the click action
    // (REC -> record, STOP -> loop it, LOOP -> fade it out).
    if (targetOff)
        return "OFF";

    switch ((gloop::Mode) mode)
    {
        case gloop::Mode::Idle:   return hasGesture ? "READY" : "REC";
        case gloop::Mode::Record: return "STOP";
        case gloop::Mode::Loop:   return "LOOP";
        case gloop::Mode::Fade:   return utf8 ("FADE\u2026");
        case gloop::Mode::Count:  break;
    }

    return utf8 ("\u2014");
}

int GloopPanel::effectiveMode (int lane) const
{
    lane = laneIndex (lane);
    auto mode = visual.lanes[(size_t) lane].mode;

    // The engine owns the actual state, but it may not have seen a fresh
    // REC/LOOP request yet. Until it reports one, show what we asked for so the
    // panel never looks unresponsive.
    if (mode == (int) gloop::Mode::Idle)
    {
        const auto requested = gloop.getGloopMode (lane);

        if (requested == (int) gloop::Mode::Record || requested == (int) gloop::Mode::Loop)
            mode = requested;
    }

    return mode;
}

int GloopPanel::laneTarget (int lane) const
{
    lane = laneIndex (lane);

    const auto fromVisual = visual.lanes[(size_t) lane].target;

    if (fromVisual > (int) gloop::Target::Off)
        return fromVisual;

    // Before the engine has published a lane frame the visual target is still
    // Off; the host parameter is authoritative for what the lane may record.
    if (auto* raw = apvts.getRawParameterValue (pid::gloopTarget[lane]))
        return juce::jlimit (0, gloop::kNumTargets - 1, juce::roundToInt (raw->load()));

    return (int) gloop::Target::Off;
}

void GloopPanel::handleModeClick (int lane)
{
    lane = laneIndex (lane);

    // A lane with no target has nothing to record; the button is disabled in
    // that case, so this is only a safety net against a stale visual frame.
    if (laneTarget (lane) <= (int) gloop::Target::Off
        && effectiveMode (lane) == (int) gloop::Mode::Idle)
        return;

    switch ((gloop::Mode) effectiveMode (lane))
    {
        case gloop::Mode::Idle:   gloop.setGloopMode (lane, (int) gloop::Mode::Record); break;
        case gloop::Mode::Record: gloop.setGloopMode (lane, (int) gloop::Mode::Loop);   break;
        case gloop::Mode::Loop:   gloop.setGloopMode (lane, (int) gloop::Mode::Fade);   break;
        case gloop::Mode::Fade:   break;   // withdrawing: no further action
        case gloop::Mode::Count:  break;
    }

    // React on this frame instead of waiting for the next visual snapshot.
    syncLaneControls();
    repaint (rowRects[(size_t) lane].expanded (2));
}

void GloopPanel::syncLaneControls()
{
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        const auto& l = visual.lanes[idx];
        const auto mode = effectiveMode (lane);
        const auto targetOff = laneTarget (lane) <= (int) gloop::Target::Off;
        const auto hasGesture = l.hasGesture;
        const auto fading = mode == (int) gloop::Mode::Fade;

        if (auto* button = modeButtons[idx])
        {
            auto& props = button->getProperties();
            props.set ("gloopState", mode);
            props.set ("gloopHasGesture", hasGesture);
            props.set ("laneArgb", (int) theme::gloopLaneColour (lane).getARGB());

            // Target, not parameter state, decides whether a lane can start.
            const auto label = modeLabel (targetOff ? (int) gloop::Mode::Idle : mode,
                                          hasGesture, targetOff);

            if (button->getButtonText() != label)
                button->setButtonText (label);

            button->setEnabled (! targetOff && ! fading);

            juce::String tip ("Lane ");
            tip << (lane + 1) << ": ";

            if (targetOff)
                tip << "choose a target knob to record";
            else if (mode == (int) gloop::Mode::Record)
                tip << "recording the target knob's movement — click to loop it";
            else if (mode == (int) gloop::Mode::Loop)
                tip << "looping — click to fade this lane out";
            else if (fading)
                tip << "fading out";
            else if (hasGesture)
                tip << "gesture ready — click to record a new one";
            else
                tip << "empty — click, then move the target knob";

            button->setTooltip (tip);
        }

        if (auto* combo = targetChoices[idx])
        {
            // A lane whose target was reset to Off (Init / a preset) must always
            // be able to choose a new target, even before the engine's next
            // visual frame has settled the lane to Idle.
            combo->setEnabled (targetOff || mode == (int) gloop::Mode::Idle);
        }
    }
}

void GloopPanel::tick (const gloop::Visual& newVisual, double)
{
    visual = newVisual;
    ++tickCounter;

    syncLaneControls();

    // The rings are the only thing that moves: repaint those strips (plus the
    // pulsing record buttons) instead of the whole panel at 30 Hz.
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        repaint (ringRects[(size_t) lane].expanded (2));

        if (effectiveMode (lane) == (int) gloop::Mode::Record)
            if (auto* button = modeButtons[(size_t) lane])
                button->repaint();
    }
}

// -----------------------------------------------------------------------------
void GloopPanel::resized()
{
    layoutControls();
    repaint();
}

void GloopPanel::layoutControls()
{
    const auto s = uiScale;
    auto inner = getLocalBounds().toFloat().reduced (10.0f * s);

    headerRect = inner.removeFromTop (juce::jmax (14.0f, 20.0f * s)).toNearestInt();
    inner.removeFromTop (4.0f * s);

    // The footer keeps the Randomize action and the smoothing knob; it never
    // eats more than a ninth of the height, so the eight rows stay first.
    const auto footerH = juce::jlimit (30.0f * s, 46.0f * s, inner.getHeight() * 0.135f);
    footerRect = inner.removeFromBottom (footerH).toNearestInt();
    inner.removeFromBottom (3.0f * s);

    const auto rowH = inner.getHeight() / (float) gloop::kNumLanes;

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        const juce::Rectangle<float> row (inner.getX(), inner.getY() + (float) lane * rowH,
                                          inner.getWidth(), rowH);
        rowRects[idx] = row.toNearestInt();

        auto r = row.reduced (1.0f * s, 1.0f * s);
        const auto contentH = juce::jmax (10.0f, r.getHeight());

        // 1. phase ring
        const auto ringSize = juce::jmin (contentH, 22.0f * s);
        ringRects[idx] = juce::Rectangle<float> (r.getX(), r.getCentreY() - ringSize * 0.5f,
                                                 ringSize, ringSize).toNearestInt();
        r.removeFromLeft (ringSize + 4.0f * s);

        // 2. lane number (engraved in the cached background)
        numberRects[idx] = r.removeFromLeft (juce::jmax (8.0f, 12.0f * s)).toNearestInt();

        // 3. target choice
        const auto comboW = juce::jlimit (70.0f * s, 128.0f * s, r.getWidth() * 0.30f);
        const auto fieldH = juce::jmin (contentH, 20.0f * s);

        if (auto* combo = targetChoices[idx])
            combo->setBounds (r.removeFromLeft (comboW)
                               .withSizeKeepingCentre (comboW, fieldH).toNearestInt());
        r.removeFromLeft (3.0f * s);

        // 4. state button
        const auto buttonW = juce::jlimit (40.0f * s, 56.0f * s, r.getWidth() * 0.16f);

        if (auto* button = modeButtons[idx])
            button->setBounds (r.removeFromLeft (buttonW)
                                .withSizeKeepingCentre (buttonW, fieldH).toNearestInt());
        r.removeFromLeft (4.0f * s);

        // 5./6. amount and speed ribbons
        const auto gap = 3.0f * s;
        const auto ribbonW = (r.getWidth() - gap) * 0.5f;
        const auto ribbonH = juce::jmin (contentH, 18.0f * s);

        if (auto* amount = amountRibbons[idx])
            amount->setBounds (r.removeFromLeft (ribbonW)
                                .withSizeKeepingCentre (ribbonW, ribbonH).toNearestInt());
        r.removeFromLeft (gap);

        if (auto* speed = speedRibbons[idx])
            speed->setBounds (r.withSizeKeepingCentre (r.getWidth(), ribbonH).toNearestInt());
    }

    // Footer: RANDOMIZE left, SMOOTH right, a quiet state legend between them.
    {
        auto f = footerRect.toFloat().reduced (2.0f * s, 1.0f * s);

        if (randomizeButton != nullptr)
        {
            const auto w = juce::jlimit (70.0f * s, 122.0f * s, f.getWidth() * 0.28f);
            const auto h = juce::jmin (f.getHeight(), 20.0f * s);
            randomizeButton->setBounds (f.removeFromLeft (w).withSizeKeepingCentre (w, h)
                                         .toNearestInt());
        }

        if (smoothKnob != nullptr)
        {
            const auto w = juce::jlimit (56.0f * s, 92.0f * s, f.getWidth() * 0.32f);
            smoothKnob->setBounds (f.removeFromRight (w).toNearestInt());
        }
    }
}

void GloopPanel::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

// -----------------------------------------------------------------------------
void GloopPanel::BackgroundCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderBackground (g, bounds);
}

void GloopPanel::renderBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    // Opaque: the panel hides the grain cloud while it is open.
    g.fillAll (pageBottom);

    drawPanelPlate (g, bounds.reduced (0.5f * s), s, 5.0f * s);

    // The right of the header stays clear for the editor's GLOOP toggle.
    drawHeader (g, headerRect.toFloat().withTrimmedRight (72.0f * s), s,
                "02", "GLOOP", utf8 ("LANES 1\u20138"));

    // Per-lane chrome: number, row rule and the idle ring seat.
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        const auto row = rowRects[idx].toFloat();

        if (lane > 0)
        {
            g.setColour (lineSoft.withAlpha (0.55f));
            g.fillRect (row.getX(), row.getY(), row.getWidth(), juce::jmax (0.6f, 0.6f * s));
        }

        drawMicroText (g, juce::String (lane + 1),
                       numberRects[idx].toFloat(),
                       monoFont (7.4f * fs, juce::Font::plain, 0.04f),
                       theme::gloopLaneColour (lane).withAlpha (0.75f),
                       juce::Justification::centredRight);
    }

    // Footer rule and legend.
    {
        const auto f = footerRect.toFloat();
        g.setColour (lineSoft.withAlpha (0.6f));
        g.fillRect (f.getX(), f.getY(), f.getWidth(), juce::jmax (0.6f, 0.6f * s));

        auto centre = f.withTrimmedLeft (f.getWidth() * 0.33f)
                       .withTrimmedRight (f.getWidth() * 0.33f);

        // The colour key is the mental model: orange belongs to the live knob,
        // blue belongs to Gloop. The state cycle sits beneath it when the footer
        // has room; at the minimum editor size the key is dropped rather than
        // squeezing the lane rows.
        const auto showKey = f.getHeight() >= 38.0f * s;
        auto stateLine = showKey ? centre.removeFromTop (centre.getHeight() * 0.48f)
                                 : centre;

        drawMicroText (g, utf8 ("REC \u25b8 LOOP \u25b8 FADE \u25b8 READY"),
                       stateLine,
                       monoFont (6.4f * fs, juce::Font::plain, 0.10f),
                       dim.withAlpha (0.85f), juce::Justification::centred);

        if (showKey)
        {
            const auto keyFont = monoFont (5.8f * fs, juce::Font::plain, 0.08f);
            const auto key = centre.withTrimmedTop (1.0f * s);
            auto orangeKey = key.withTrimmedRight (key.getWidth() * 0.50f + 3.0f * s);
            auto blueKey = key.withTrimmedLeft (key.getWidth() * 0.50f + 3.0f * s);

            drawMicroText (g, "ORANGE = LIVE", orangeKey, keyFont,
                           theme::ember.withAlpha (0.85f), juce::Justification::centredRight);
            drawMicroText (g, "BLUE = GLOOP", blueKey, keyFont,
                           theme::gloopMotion().withAlpha (0.95f),
                           juce::Justification::centredLeft);
        }
    }
}

void GloopPanel::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    background.ensure (getWidth(), getHeight(), scale, 4.0e6f);
    drawCached (g, background.getImage(), getLocalBounds());

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
        paintLaneRing (g, lane, ringRects[(size_t) lane].toFloat());
}

void GloopPanel::paintLaneRing (juce::Graphics& g, int lane, juce::Rectangle<float> area) const
{
    if (area.isEmpty())
        return;

    const auto s = uiScale;
    const auto idx = (size_t) lane;
    const auto& l = visual.lanes[idx];
    const auto mode = effectiveMode (lane);
    const auto active = mode == (int) gloop::Mode::Record
                        || mode == (int) gloop::Mode::Loop
                        || mode == (int) gloop::Mode::Fade;
    const auto colour = theme::gloopLaneColour (lane);
    const auto centre = area.getCentre();
    const auto radius = juce::jmax (2.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f
                                            - 1.6f * s);
    const auto stroke = juce::jmax (0.7f, 0.8f * s);
    constexpr float startAngle = 0.0f;   // 12 o'clock, clockwise

    // Record pulses; the pulse is driven by the (deterministic) tick counter, so
    // a stepped preview frame is reproducible.
    const auto pulse = (float) (0.5 + 0.5 * std::sin ((double) tickCounter * 0.55));

    // Seat: dim ring for empty / idle lanes.
    {
        const auto alpha = active ? 0.55f : (l.hasGesture ? 0.40f : 0.24f);
        const auto c = active ? colour : (l.hasGesture ? theme::copper : theme::line);

        g.setColour (c.withAlpha (alpha));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, stroke);
    }

    if (active)
    {
        // Loop progress: from 12 o'clock to the playhead.
        const auto phase = juce::jlimit (0.0f, 1.0f, l.phase);
        const auto dotAngle = startAngle + phase * kTwoPi;

        juce::Path progress;
        progress.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                startAngle, dotAngle, true);
        g.setColour (colour.withAlpha (mode == (int) gloop::Mode::Fade ? 0.35f : 0.80f));
        g.strokePath (progress, juce::PathStrokeType (juce::jmax (0.9f, 1.2f * s),
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        // Current offset: the inner arc is how far the lane has moved the knob
        // right now (normalised offset of the target range).
        {
            const auto innerR = radius * 0.52f;
            const auto offset = juce::jlimit (-1.0f, 1.0f, l.offset);

            if (std::abs (offset) > 0.01f)
            {
                juce::Path offsetArc;
                offsetArc.addCentredArc (centre.x, centre.y, innerR, innerR, 0.0f,
                                         startAngle, startAngle + offset * kTwoPi, true);

                g.setColour (paleEmber.withAlpha (0.20f));
                g.strokePath (offsetArc, juce::PathStrokeType (2.6f * s, juce::PathStrokeType::curved,
                                                               juce::PathStrokeType::rounded));
                g.setColour (emberRamp (0.35f + 0.45f * juce::jmin (1.0f, std::abs (offset)))
                                 .withAlpha (0.9f));
                g.strokePath (offsetArc, juce::PathStrokeType (1.2f * s, juce::PathStrokeType::curved,
                                                               juce::PathStrokeType::rounded));
            }
        }

        // Playhead: a dot that really moves with the phase.
        const auto dot = polar (centre, radius, dotAngle);
        const auto dotR = (mode == (int) gloop::Mode::Record ? 1.6f + 0.5f * pulse : 1.5f) * s;

        drawGlowDot (g, dot.x, dot.y, juce::jmax (1.0f, dotR * 0.8f), colour,
                     0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, l.amount), s);
        g.setColour (colour.interpolatedWith (paleEmber, 0.35f)
                          .withAlpha (mode == (int) gloop::Mode::Fade ? 0.55f : 1.0f));
        g.fillEllipse (dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
    }
    else if (l.hasGesture)
    {
        // READY: a still, empty seat with a copper collar and a zero mark.
        g.setColour (theme::copper.withAlpha (0.55f));
        g.drawEllipse (centre.x - radius * 0.45f, centre.y - radius * 0.45f,
                       radius * 0.9f, radius * 0.9f, juce::jmax (0.7f, 0.8f * s));
    }
}

} // namespace nagloed::ui
