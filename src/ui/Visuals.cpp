#include "Visuals.h"
#include "core/ParamIDs.h"

#include <cmath>

namespace nagloed::ui
{

using namespace theme;

namespace
{
constexpr float kMeterMinDb = -30.0f;
constexpr float kMeterMaxDb = 3.0f;
constexpr float kSweepStart = -0.92f;
constexpr float kSweepEnd = 0.92f;

juce::String formatDb (float db)
{
    return juce::String (db, 1).replace ("-", juce::String::fromUTF8 ("−"));
}
} // namespace

// =============================================================================
// AnalogueMeter
// =============================================================================
AnalogueMeter::AnalogueMeter (const juce::String& meterLabel, bool useInput)
    : label (meterLabel), inputMeter (useInput)
{
    setOpaque (false);
    setWantsKeyboardFocus (false);
    setTitle (meterLabel + " level meter");
    setDescription ("Analogue VU meter with peak latch for the " + meterLabel.toLowerCase() + " signal.");
    setTooltip (meterLabel + utf8 (" VU — analogue ballistics with peak hold"));
}

void AnalogueMeter::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.5f, 2.5f, newScale);
    repaint();
}

float AnalogueMeter::dbFromRms (float rms)
{
    return juce::jlimit (kMeterMinDb, kMeterMaxDb,
                         20.0f * std::log10 (juce::jmax (1.0e-4f, rms)));
}

float AnalogueMeter::angleForDb (float db) const
{
    const auto t = juce::jlimit (0.0f, 1.0f, (db - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));
    return kSweepStart + t * (kSweepEnd - kSweepStart);
}

juce::Point<float> AnalogueMeter::pointOnArc (float angle, float radiusScale) const
{
    const auto b = getLocalBounds().toFloat();
    const auto s = uiScale;
    const auto pivot = juce::Point<float> (b.getCentreX(), b.getBottom() - 15.0f * s);
    const auto rx = b.getWidth() * 0.455f;
    const auto ry = (pivot.y - b.getY() - 13.0f * s) * 0.92f;

    return { pivot.x + std::sin (angle) * rx * radiusScale,
             pivot.y - std::cos (angle) * ry * radiusScale };
}

void AnalogueMeter::tick (const VisualState& snapshot, double elapsedSeconds)
{
    const auto dt = (float) juce::jlimit (0.005, 0.1, elapsedSeconds);
    const auto l = inputMeter ? snapshot.inL : snapshot.outL;
    const auto r = inputMeter ? snapshot.inR : snapshot.outR;
    const auto p = inputMeter ? snapshot.inPeak : snapshot.outPeak;

    auto update = [&] (Needle& needle, float rms, bool isLeft)
    {
        const auto target = dbFromRms (rms);

        if (! needle.initialised)
        {
            needle.db = target;
            needle.peakDb = target;
            needle.initialised = true;
            return;
        }

        const auto coefficient = target > needle.db ? 0.50f : 0.14f;
        needle.db += (target - needle.db) * juce::jlimit (0.05f, 0.85f, coefficient * dt * 30.0f);

        const auto peakTarget = dbFromRms (p * (isLeft ? 1.0f : 0.94f));

        if (peakTarget > needle.peakDb)
        {
            needle.peakDb = peakTarget;
            needle.peakHold = 0.75f;
        }
        else if (needle.peakHold > 0.0f)
        {
            needle.peakHold -= dt;
        }
        else
        {
            needle.peakDb -= 7.0f * dt;
            needle.peakDb = juce::jmax (needle.db, needle.peakDb);
        }
    };

    update (left, l, true);
    update (right, r, false);

    repaint();
}

void AnalogueMeter::drawNeedle (juce::Graphics& g, const Needle& needle, float angle,
                                juce::Colour colour, float widthScale) const
{
    const auto s = uiScale;
    const auto b = getLocalBounds().toFloat();
    const auto pivot = juce::Point<float> (b.getCentreX(), b.getBottom() - 15.0f * s);
    const auto tip = pointOnArc (angle, 0.965f);
    const auto delta = tip - pivot;
    const auto distance = juce::jmax (1.0e-3f, delta.getDistanceFromOrigin());
    const auto start = pivot + delta * (5.0f * s / distance);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine ({ start.translated (0.8f * s, 0.9f * s),
                  tip.translated (0.8f * s, 0.9f * s) },
                1.4f * s * widthScale);

    g.setColour (colour.withAlpha (0.20f));
    g.drawLine ({ start, tip }, 3.0f * s * widthScale);
    g.setColour (colour.withAlpha (0.98f));
    g.drawLine ({ start, tip }, 1.15f * s * widthScale);

    g.setColour (colour);
    g.fillEllipse (tip.x - 1.0f * s, tip.y - 1.0f * s, 2.0f * s, 2.0f * s);

    // Peak latch marker, on the arc itself.
    const auto peakAngle = angleForDb (needle.peakDb);
    const auto peakPoint = pointOnArc (peakAngle, 0.94f);
    g.setColour (paleEmber.withAlpha (0.92f));
    g.fillEllipse (peakPoint.x - 1.3f * s, peakPoint.y - 1.3f * s, 2.6f * s, 2.6f * s);
}

void AnalogueMeter::paint (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto b = getLocalBounds().toFloat();
    const auto pivot = juce::Point<float> (b.getCentreX(), b.getBottom() - 15.0f * s);
    const auto rx = b.getWidth() * 0.455f;
    const auto ry = (pivot.y - b.getY() - 13.0f * s) * 0.92f;

    // Quiet inset screen.
    {
        juce::ColourGradient well (juce::Colour (0xff0a0e14), b.getX(), b.getY(),
                                   juce::Colour (0xff05070a), b.getX(), b.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (b.reduced (0.5f * s), 2.0f * s);

        g.setColour (lineSoft.withAlpha (0.65f));
        g.drawRoundedRectangle (b.reduced (0.5f * s), 2.0f * s, 0.8f * s);
    }

    // Elliptical scale.
    {
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, rx, ry, 0.0f, kSweepStart, kSweepEnd, true);
        g.setColour (copper.withAlpha (0.60f));
        g.strokePath (arc, juce::PathStrokeType (0.8f * s));

        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, rx, ry, 0.0f, angleForDb (0.0f), kSweepEnd, true);
        g.setColour (warning.withAlpha (0.65f));
        g.strokePath (red, juce::PathStrokeType (1.5f * s));

        for (int i = 0; i <= 33; ++i)
        {
            const auto t = (float) i / 33.0f;
            const auto angle = kSweepStart + t * (kSweepEnd - kSweepStart);
            const auto major = (i % 5 == 0);
            const auto inner = pointOnArc (angle, major ? 0.955f : 0.975f);
            const auto outer = pointOnArc (angle, 1.005f);
            g.setColour (major ? copper.withAlpha (0.95f) : copperDim.withAlpha (0.6f));
            g.drawLine ({ inner, outer }, (major ? 0.9f : 0.55f) * s);
        }

        for (auto db : { -20.0f, -10.0f, -6.0f, -3.0f, 0.0f, 3.0f })
        {
            const auto angle = angleForDb (db);
            const auto p = pointOnArc (angle, 0.86f);
            g.setFont (monoFont (5.4f * fs, juce::Font::plain, 0.02f));
            g.setColour (db >= 0.0f ? warning.withAlpha (0.95f) : muted.withAlpha (0.9f));
            g.drawText (juce::String ((int) db),
                        juce::Rectangle<float> (p.x - 8.0f * s, p.y - 4.0f * s,
                                                16.0f * s, 8.0f * s),
                        juce::Justification::centred, false);
        }
    }

    // The lit wedge below the needle, like a valve meter's warm glow.
    {
        const auto angle = angleForDb (left.db);

        if (angle > kSweepStart + 0.01f)
        {
            juce::Path wedge;
            wedge.startNewSubPath (pivot);
            wedge.lineTo (pointOnArc (kSweepStart, 1.0f));
            wedge.addCentredArc (pivot.x, pivot.y, rx, ry, 0.0f,
                                 kSweepStart, angle, false);
            wedge.closeSubPath();

            juce::ColourGradient glow (ember.withAlpha (0.10f), pivot.x, pivot.y,
                                       ember.withAlpha (0.0f), pivot.x, pivot.y - ry, true);
            g.setGradientFill (glow);
            g.fillPath (wedge);
        }
    }

    // Needles.
    drawNeedle (g, left, angleForDb (left.db), paleEmber, 1.10f);
    drawNeedle (g, right, angleForDb (right.db), gold, 0.85f);

    // Pivot cap.
    {
        g.setColour (juce::Colour (0xff13181f));
        g.fillEllipse (pivot.x - 4.2f * s, pivot.y - 4.2f * s, 8.4f * s, 8.4f * s);
        g.setColour (copper.withAlpha (0.6f));
        g.drawEllipse (pivot.x - 4.2f * s, pivot.y - 4.2f * s, 8.4f * s, 8.4f * s, 0.9f * s);
        g.setColour (ember.withAlpha (0.45f));
        g.fillEllipse (pivot.x - 1.1f * s, pivot.y - 1.1f * s, 2.2f * s, 2.2f * s);
    }

    // Glass sheen.
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.030f),
                                    b.getX(), b.getY(),
                                    juce::Colours::white.withAlpha (0.0f),
                                    b.getRight(), b.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (b.reduced (0.5f * s), 2.0f * s);
    }

    // Labels.
    {
        drawMicroText (g, label,
                       { b.getX() + 6.0f * s, b.getY() + 4.0f * s,
                         b.getWidth() * 0.5f, 10.0f * s },
                       monoFont (6.9f * fs, juce::Font::plain, 0.20f),
                       inputMeter ? copper.withAlpha (0.95f) : gold.withAlpha (0.95f));

        drawMicroText (g, "VU",
                       { b.getCentreX(), b.getY() + 4.0f * s,
                         b.getWidth() * 0.5f - 6.0f * s, 10.0f * s },
                       monoFont (5.6f * fs, juce::Font::plain, 0.14f),
                       dim.withAlpha (0.9f), juce::Justification::centredRight);

        drawMicroText (g, formatDb (left.db) + " dB",
                       { b.getX() + 5.0f * s, pivot.y - 13.0f * s,
                         b.getWidth() * 0.55f, 9.0f * s },
                       monoFont (5.6f * fs, juce::Font::plain, 0.02f),
                       muted.withAlpha (0.85f));

        // L/R needle legend.
        const auto legendY = pivot.y - 11.0f * s;
        g.setColour (paleEmber.withAlpha (0.8f));
        g.fillRect (b.getX() + b.getWidth() * 0.62f, legendY + 3.0f * s, 5.0f * s, 0.9f * s);
        g.setColour (gold.withAlpha (0.8f));
        g.fillRect (b.getX() + b.getWidth() * 0.62f, legendY + 5.5f * s, 5.0f * s, 0.9f * s);
        g.setFont (monoFont (5.0f * fs, juce::Font::plain, 0.02f));
        g.setColour (dim.withAlpha (0.9f));
        g.drawText ("L R", juce::Rectangle<float> (b.getX() + b.getWidth() * 0.68f, legendY,
                                                   18.0f * s, 10.0f * s),
                    juce::Justification::centredLeft, false);
    }
}

// =============================================================================
// FilterCurve
// =============================================================================
FilterCurve::FilterCurve (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    setOpaque (false);
    setWantsKeyboardFocus (false);
    setTitle ("EAR low-pass response");
    setDescription ("Live 24 dB low-pass response curve for the EAR analogue section.");
    refresh();
}

void FilterCurve::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.5f, 2.5f, newScale);
    repaint();
}

void FilterCurve::refresh()
{
    if (auto* p = apvts.getParameter (pid::cutoff))
    {
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto value = r->convertFrom0to1 (r->getValue());
            if (std::abs (value - cutoffHz) > 0.5f)
            {
                cutoffHz = value;
            }
        }
    }

    if (auto* p = apvts.getParameter (pid::reso))
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            reso = r->getValue();

    const auto q = 0.5f + reso * 9.0f;
    readout = (cutoffHz >= 1000.0f ? juce::String (cutoffHz / 1000.0f, 2) + " kHz"
                                   : juce::String (cutoffHz, 0) + " Hz")
              + utf8 ("  ·  Q ") + juce::String (q, 1);
}

void FilterCurve::tick (const VisualState&, double)
{
    if (++tickCounter % 3 != 0)
        return;

    const auto oldCutoff = cutoffHz;
    const auto oldReso = reso;
    refresh();

    if (std::abs (oldCutoff - cutoffHz) > 0.5f || std::abs (oldReso - reso) > 0.0005f)
        repaint();
}

void FilterCurve::paint (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto b = getLocalBounds().toFloat();
    const auto plot = b.reduced (2.0f * s, 3.0f * s);

    if (plot.isEmpty())
        return;

    // Grid: decade lines and dB lines.
    {
        for (auto freq : { 100.0, 1000.0, 10000.0 })
        {
            const auto t = std::log (freq / 20.0) / std::log (1000.0);
            const auto x = plot.getX() + (float) t * plot.getWidth();
            g.setColour (line.withAlpha (0.8f));
            g.fillRect (x - 0.25f * s, plot.getY(), 0.5f * s, plot.getHeight());
        }

        for (auto db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
        {
            const auto y = plot.getBottom() - (db + 48.0f) / 66.0f * plot.getHeight();
            if (y < plot.getY() || y > plot.getBottom())
                continue;
            g.setColour (line.withAlpha (db == 0.0f ? 0.9f : 0.55f));
            g.fillRect (plot.getX(), y - 0.25f * s, plot.getWidth(), 0.5f * s);
        }
    }

    // Response curve.
    const auto q = 0.5f + reso * 9.0f;
    juce::Path curve;

    const int steps = juce::jmax (32, (int) plot.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const auto t = (float) i / (float) steps;
        const auto freq = 20.0f * std::pow (1000.0f, t);
        const auto ratio = (double) freq / (double) juce::jmax (10.0f, cutoffHz);
        const auto qd = (double) q;
        auto mag = 1.0 / std::sqrt ((1.0 - ratio * ratio) * (1.0 - ratio * ratio)
                                    + (ratio / qd) * (ratio / qd));
        mag *= mag;   // 24 dB / oct

        const auto db = 20.0 * std::log10 (juce::jmax (1.0e-5, mag));
        const auto norm = juce::jlimit (0.0, 1.0, (db + 48.0) / 66.0);
        const auto x = plot.getX() + t * plot.getWidth();
        const auto y = plot.getBottom() - (float) norm * plot.getHeight();

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    // Fill under curve.
    {
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        juce::ColourGradient grad (ember.withAlpha (0.16f), plot.getX(), plot.getY(),
                                   ember.withAlpha (0.005f), plot.getX(), plot.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (fill);
    }

    // Glow + crisp curve.
    {
        g.setColour (ember.withAlpha (0.14f));
        g.strokePath (curve, juce::PathStrokeType (3.2f * s, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        g.setColour (gold.withAlpha (0.9f));
        g.strokePath (curve, juce::PathStrokeType (1.4f * s, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Cutoff marker.
    {
        const auto tCut = juce::jlimit (0.0f, 1.0f,
                                        (std::log (cutoffHz / 20.0f) / std::log (1000.0f)));
        const auto x = plot.getX() + tCut * plot.getWidth();

        g.setColour (copper.withAlpha (0.30f));
        for (float y = plot.getY(); y < plot.getBottom(); y += 5.0f * s)
            g.fillRect (x - 0.3f * s, y, 0.6f * s, 2.4f * s);

        const auto markerY = [&]
        {
            const auto ratio = 1.0;
            const auto qd = (double) q;
            auto mag = 1.0 / std::sqrt (std::pow (1.0 - (double) ratio * ratio, 2.0)
                                        + std::pow (ratio / qd, 2.0));
            mag *= mag;
            const auto db = 20.0 * std::log10 (juce::jmax (1.0e-5, mag));
            return plot.getBottom() - (float) juce::jlimit (0.0, 1.0, (db + 48.0) / 66.0) * plot.getHeight();
        }();

        drawGlowDot (g, x, markerY, 1.8f * s, ember, 0.9f, s);
    }

    // Readout.
    drawMicroText (g, readout,
                   { b.getX() + 4.0f * s, b.getBottom() - 10.0f * s,
                     b.getWidth() - 8.0f * s, 9.0f * s },
                   monoFont (5.7f * fs, juce::Font::plain, 0.03f),
                   muted.withAlpha (0.9f), juce::Justification::bottomRight);
}

// =============================================================================
// LfoVisual
// =============================================================================
LfoVisual::LfoVisual (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    setOpaque (false);
    setWantsKeyboardFocus (false);
    setTitle ("LFO shape");
    setDescription ("Live modulation scope showing shape, rate and amount.");
    refresh();
}

void LfoVisual::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.5f, 2.5f, newScale);
    repaint();
}

float LfoVisual::lfoValue (int currentShape, float t)
{
    t = t - std::floor (t);

    switch (currentShape)
    {
        case 1:  return 1.0f - 4.0f * std::abs (t - 0.5f);                  // triangle
        case 2:  return 2.0f * t - 1.0f;                                     // saw up
        case 3:  return 1.0f - 2.0f * t;                                     // saw down
        case 4:  return t < 0.5f ? 1.0f : -1.0f;                             // square
        case 5:                                                              // sample & hold
        {
            const auto step = std::floor (t * 8.0f);
            const auto hash = std::sin (step * 12.9898f) * 43758.5453f;
            return (hash - std::floor (hash)) * 2.0f - 1.0f;
        }
        default: return std::sin (juce::MathConstants<float>::twoPi * t);
    }
}

void LfoVisual::refresh()
{
    if (auto* p = apvts.getParameter (pid::lfoShape))
        shape = juce::roundToInt (p->getValue() * 5.0f + 0.001f);

    if (auto* p = apvts.getParameter (pid::lfoRate))
    {
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            rate = r->convertFrom0to1 (r->getValue());
    }

    if (auto* p = apvts.getParameter (pid::lfoAmount))
        amount = p->getValue() * 2.0f - 1.0f;
}

void LfoVisual::tick (const VisualState&, double elapsedSeconds)
{
    if (++tickCounter % 3 == 0)
        refresh();

    phase += rate * (float) juce::jlimit (0.001, 0.1, elapsedSeconds);
    phase -= std::floor (phase);
    repaint();
}

void LfoVisual::paint (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto b = getLocalBounds().toFloat();
    const auto plot = b.reduced (2.0f * s, 4.0f * s);

    if (plot.isEmpty())
        return;

    const auto centreY = plot.getCentreY();
    const auto amplitude = plot.getHeight() * 0.34f * (0.25f + 0.75f * std::abs (amount));
    const auto alpha = amount == 0.0f ? 0.35f : 1.0f;

    // Zero axis.
    g.setColour (lineSoft.withAlpha (0.9f));
    g.fillRect (plot.getX(), centreY - 0.3f * s, plot.getWidth(), 0.6f * s);

    // Two cycles of the shape.
    juce::Path wave;
    const int steps = juce::jmax (48, (int) plot.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const auto t = 2.0f * (float) i / (float) steps;
        const auto x = plot.getX() + (float) i / (float) steps * plot.getWidth();
        const auto y = centreY - lfoValue (shape, t) * amplitude;

        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    {
        juce::Path fillPath (wave);
        fillPath.lineTo (plot.getRight(), centreY);
        fillPath.lineTo (plot.getX(), centreY);
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad (mint.withAlpha (0.075f * alpha), plot.getX(), centreY,
                                       mint.withAlpha (0.01f * alpha), plot.getX(), plot.getBottom(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);
    }

    g.setColour (mint.withAlpha (0.12f * alpha));
    g.strokePath (wave, juce::PathStrokeType (3.4f * s, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour (mint.withAlpha (0.85f * alpha));
    g.strokePath (wave, juce::PathStrokeType (1.15f * s, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Moving phase dot.
    {
        const auto cycle = phase;
        const auto x = plot.getX() + cycle * plot.getWidth() * 0.5f;
        const auto y = centreY - lfoValue (shape, cycle) * amplitude;

        g.setColour (mint.withAlpha (0.28f));
        g.fillRect (x - 0.4f * s, plot.getY(), 0.8f * s, plot.getHeight());
        drawGlowDot (g, x, y, 1.7f * s, mint, 0.95f, s);
    }

    // Readout.
    drawMicroText (g, juce::String (rate, rate < 1.0f ? 2 : 1) + " HZ",
                   { plot.getX(), plot.getBottom() - 8.0f * s, plot.getWidth() * 0.5f, 8.0f * s },
                   monoFont (5.4f * fs, juce::Font::plain, 0.06f),
                   dim.withAlpha (0.9f));

    drawMicroText (g, "LFO",
                   { plot.getX() + plot.getWidth() * 0.5f, plot.getBottom() - 8.0f * s,
                     plot.getWidth() * 0.5f, 8.0f * s },
                   monoFont (5.4f * fs, juce::Font::plain, 0.16f),
                   mint.withAlpha (0.7f), juce::Justification::centredRight);
}

// =============================================================================
// SpaceVisual
// =============================================================================
SpaceVisual::SpaceVisual (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    setOpaque (false);
    setWantsKeyboardFocus (false);
    setTitle ("Ruimte space depth");
    setDescription ("Perspective diagram of the reverb space: size, damping and modulation.");
    refresh();
}

void SpaceVisual::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.5f, 2.5f, newScale);
    repaint();
}

void SpaceVisual::refresh()
{
    auto norm = [] (juce::AudioProcessorValueTreeState& state, const char* id)
    {
        if (auto* p = state.getParameter (id))
            return p->getValue();
        return 0.0f;
    };

    size = norm (apvts, pid::spaceSize);
    damp = norm (apvts, pid::spaceDamp);
    mod = norm (apvts, pid::spaceMod);
    readout = "SIZE " + juce::String (juce::roundToInt (size * 100.0f)) + "%"
              + utf8 ("  ·  DAMP ") + juce::String (juce::roundToInt (damp * 100.0f)) + "%";
}

void SpaceVisual::tick (const VisualState&, double elapsedSeconds)
{
    if (++tickCounter % 4 == 0)
        refresh();

    phase += (float) juce::jlimit (0.001, 0.1, elapsedSeconds) * (0.4f + mod * 1.4f);
    repaint();
}

void SpaceVisual::paint (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto b = getLocalBounds().toFloat();
    const auto plot = b.reduced (2.0f * s, 2.0f * s);

    if (plot.isEmpty())
        return;

    const auto cx = plot.getCentreX();
    const auto cy = plot.getCentreY() * 0.94f;
    const auto maxW = plot.getWidth() * 0.40f;
    const auto maxH = plot.getHeight() * 0.34f;
    const auto depth = 0.22f + 0.78f * size;

    // Perspective rings receding towards the source.
    const int rings = 6;
    for (int i = rings; i >= 1; --i)
    {
        const auto t = (float) i / (float) rings;
        const auto wobble = std::sin (phase * 1.3f + (float) i * 0.9f) * mod * 1.8f * s;
        const auto rx = maxW * (0.20f + 0.80f * t * depth);
        const auto ry = maxH * (0.16f + 0.84f * t * depth);
        const auto alpha = (0.16f + 0.46f * (1.0f - t)) * (1.0f - 0.40f * damp);

        g.setColour (ember.withAlpha (juce::jlimit (0.0f, 1.0f, alpha * 0.5f)));
        g.drawEllipse (cx - rx + wobble, cy - ry, rx * 2.0f, ry * 2.0f, 1.0f * s);
    }

    // A taste of reverb tail beyond the rings.
    {
        const auto tail = 1.0f - damp * 0.6f;
        for (int i = 0; i < 3; ++i)
        {
            const auto x = cx + (maxW * (0.55f + 0.16f * (float) i)) * (0.6f + 0.4f * size);
            const auto a = (0.20f - 0.055f * (float) i) * tail;
            g.setColour (ember.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x, cy - 0.5f * s, 1.2f * s, 1.0f * s);
            g.fillEllipse (cx - (x - cx), cy - 0.5f * s, 1.2f * s, 1.0f * s);
        }
    }

    // Room axis and reflection ticks.
    {
        g.setColour (copper.withAlpha (0.28f));
        g.fillRect (cx - maxW * depth, cy - 0.3f * s, maxW * depth * 2.0f, 0.6f * s);

        for (int i = -2; i <= 2; ++i)
        {
            if (i == 0)
                continue;
            const auto x = cx + (float) i * maxW * 0.34f * depth;
            g.setColour (copperDim.withAlpha (0.32f));
            g.fillRect (x, cy - 3.0f * s, 0.5f * s, 6.0f * s);
        }
    }

    // Source ember at the vanishing point.
    drawGlowDot (g, cx, cy, 1.8f * s, ember, 0.85f, s);

    // Readout.
    drawMicroText (g, readout,
                   { plot.getX(), plot.getBottom() - 9.0f * s, plot.getWidth(), 9.0f * s },
                   monoFont (5.4f * fs, juce::Font::plain, 0.04f),
                   dim.withAlpha (0.9f), juce::Justification::centredRight);
}

} // namespace nagloed::ui
