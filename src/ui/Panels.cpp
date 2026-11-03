#include "Panels.h"
#include "core/ParamIDs.h"

#include <cmath>

namespace nagloed::ui
{

using namespace theme;

// =============================================================================
// SintelPanel
// =============================================================================
SintelPanel::SintelPanel (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel)
    : controls (state, lookAndFeel), background (*this)
{
    setOpaque (false);
    setTitle ("Sintel grain engine controls");
    setDescription ("The granular engine: density, grain size, spray, pitch, shape, width, "
                    "feedback, focus and tuning.");

    const std::array<const char*, 10> ids {
        pid::density, pid::size, pid::spray, pid::pitch, pid::spread,
        pid::jitter, pid::shape, pid::width, pid::feedback, pid::focus
    };
    const std::array<const char*, 10> captions {
        "DENSITY", "SIZE", "SPRAY", "PITCH", "SPREAD",
        "JITTER", "SHAPE", "WIDTH", "FEEDBACK", "FOCUS"
    };

    for (size_t i = 0; i < ids.size(); ++i)
        cells[i] = &controls.addKnob (*this, ids[i], captions[i]);

    tuneChoice = &controls.addChoice (*this, pid::tune, "TUNE");
    sourceChoice = &controls.addChoice (*this, pid::source, "SOURCE");
    cells[10] = tuneChoice;
    cells[11] = sourceChoice;
}

void SintelPanel::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);
    background.invalidate();
    resized();
    repaint();
}

void SintelPanel::resized()
{
    background.invalidate();
    layoutControls();
}

void SintelPanel::layoutControls()
{
    const auto s = uiScale;
    auto inner = getLocalBounds().toFloat().reduced (10.0f * s);

    headerRect = inner.removeFromTop (22.0f * s);
    inner.removeFromTop (5.0f * s);

    // Tune/source need the full width so their values never truncate.
    const auto comboRow = inner.removeFromBottom (24.0f * s);
    inner.removeFromBottom (6.0f * s);
    gridArea = inner;

    const auto cellW = gridArea.getWidth() / 3.0f;
    const auto cellH = gridArea.getHeight() / 4.0f;

    for (int i = 0; i < 10; ++i)
    {
        auto* cell = cells[(size_t) i];
        if (cell == nullptr)
            continue;

        const auto row = i / 3;
        const auto col = i % 3;
        const juce::Rectangle<float> area (gridArea.getX() + (float) col * cellW,
                                           gridArea.getY() + (float) row * cellH,
                                           cellW, cellH);
        cell->setBounds (area.toNearestInt());
    }

    // The two cells beside the FOCUS knob become the live buffer readout so the
    // rail never ends in dead space.
    statusArea = { gridArea.getX() + cellW, gridArea.getY() + cellH * 3.0f,
                   cellW * 2.0f, cellH };

    const auto comboW = (comboRow.getWidth() - 6.0f * s) * 0.5f;

    if (tuneChoice != nullptr)
        tuneChoice->setBounds (juce::roundToInt (comboRow.getX()),
                               juce::roundToInt (comboRow.getY()),
                               juce::roundToInt (comboW),
                               juce::roundToInt (comboRow.getHeight()));

    if (sourceChoice != nullptr)
        sourceChoice->setBounds (juce::roundToInt (comboRow.getX() + comboW + 6.0f * s),
                                 juce::roundToInt (comboRow.getY()),
                                 juce::roundToInt (comboW),
                                 juce::roundToInt (comboRow.getHeight()));
}

void SintelPanel::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

void SintelPanel::tick (const VisualState& snapshot, double)
{
    if ((tickCounter++ % 15) != 0)
        return;

    const auto newFill = snapshot.bufferFill;
    const auto newRate = snapshot.grainRate;
    const auto newNotes = snapshot.activeNotes;

    const auto changed = std::abs (newFill - bufferFill) > 0.002f
                         || std::abs (newRate - grainRate) > 0.05f
                         || newNotes != activeNotes;

    bufferFill = newFill;
    grainRate = newRate;
    activeNotes = newNotes;

    if (changed)
        repaint (statusArea.toNearestInt().expanded (2));
}

void SintelPanel::BackgroundCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderBackground (g, bounds);
}

void SintelPanel::renderBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    drawPanelPlate (g, bounds.reduced (0.5f * s), s, 5.0f * s);
    drawHeader (g, headerRect, s, "01", "SINTEL", "GRAIN");

    // Static readout captions and bar base, stacked in the cells beside FOCUS.
    {
        const auto rowH = juce::jmin (16.0f * s, statusArea.getHeight() * 0.22f);
        const auto row1 = statusArea.withHeight (rowH);
        const auto row2 = statusArea.withTrimmedTop (rowH * 1.9f).withHeight (rowH);
        const auto row3 = statusArea.withTrimmedTop (rowH * 2.8f).withHeight (rowH);
        const auto bar = row1.withTrimmedLeft (40.0f * s).withTrimmedRight (30.0f * s)
                             .withSizeKeepingCentre (statusArea.getWidth() - 70.0f * s, 4.0f * s);

        drawMicroText (g, "BUFFER",
                       row1.withWidth (38.0f * s),
                       monoFont (6.6f * fs, juce::Font::plain, 0.14f),
                       muted.withAlpha (0.85f));

        g.setColour (juce::Colour (0xff070a0e));
        g.fillRoundedRectangle (bar, 2.0f * s);
        g.setColour (lineSoft.withAlpha (0.9f));
        g.drawRoundedRectangle (bar, 2.0f * s, 0.7f * s);

        drawMicroText (g, "RATE",
                       row2.withWidth (38.0f * s),
                       monoFont (6.6f * fs, juce::Font::plain, 0.14f),
                       muted.withAlpha (0.8f));

        drawMicroText (g, "ACTIVE",
                       row3.withWidth (38.0f * s),
                       monoFont (6.6f * fs, juce::Font::plain, 0.14f),
                       muted.withAlpha (0.8f));
    }
}

void SintelPanel::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    background.ensure (getWidth(), getHeight(), scale, 4.0e6f);
    drawCached (g, background.getImage(), getLocalBounds());

    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    const auto rowH = juce::jmin (16.0f * s, statusArea.getHeight() * 0.22f);
    const auto row1 = statusArea.withHeight (rowH);
    const auto row2 = statusArea.withTrimmedTop (rowH * 1.9f).withHeight (rowH);
    const auto row3 = statusArea.withTrimmedTop (rowH * 2.8f).withHeight (rowH);

    // Buffer fill.
    {
        const auto bar = row1.withTrimmedLeft (40.0f * s).withTrimmedRight (30.0f * s)
                             .withSizeKeepingCentre (statusArea.getWidth() - 70.0f * s, 4.0f * s);

        const auto fill = juce::jlimit (0.0f, 1.0f, bufferFill);
        if (fill > 0.001f)
        {
            juce::Rectangle<float> area (bar.getX(), bar.getY(), bar.getWidth() * fill, bar.getHeight());
            juce::ColourGradient grad (emberDeep.withAlpha (0.55f), bar.getX(), bar.getY(),
                                       ember.withAlpha (0.95f), bar.getRight(), bar.getY(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (area, 2.0f * s);
        }

        drawMicroText (g, juce::String (juce::roundToInt (fill * 100.0f)) + "%",
                       row1.withTrimmedLeft (row1.getWidth() - 28.0f * s),
                       monoFont (5.8f * fs, juce::Font::plain, 0.04f),
                       muted.withAlpha (0.9f), juce::Justification::centredRight);
    }

    drawMicroText (g, juce::String (grainRate, 1) + " G/S",
                   row2.withTrimmedLeft (38.0f * s),
                   monoFont (6.8f * fs, juce::Font::plain, 0.03f),
                   muted.withAlpha (0.9f));

    drawMicroText (g, juce::String (activeNotes),
                   row3.withTrimmedLeft (38.0f * s),
                   monoFont (6.8f * fs, juce::Font::plain, 0.03f),
                   muted.withAlpha (0.9f));
}

// =============================================================================
// EarPanel
// =============================================================================
EarPanel::EarPanel (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel)
    : controls (state, lookAndFeel),
      background (*this), filterCurve (state)
{
    setOpaque (false);
    setTitle ("EAR analogue section");
    setDescription ("Emotional analogue resonance: drive, bias, tone, the 24 dB low-pass, "
                    "drift, wow, tape, lofi, hiss and final mix.");

    const std::array<const char*, 11> ids {
        pid::drive, pid::bias, pid::tone, pid::drift, pid::wow, pid::tape,
        pid::lofi, pid::hiss, pid::input, pid::output, pid::mix
    };
    const std::array<const char*, 11> captions {
        "DRIVE", "BIAS", "TONE", "DRIFT", "WOW", "TAPE",
        "LOFI", "HISS", "IN", "OUT", "MIX"
    };

    for (size_t i = 0; i < ids.size(); ++i)
        knobs[i] = &controls.addKnob (*this, ids[i], captions[i]);

    cutoffKnob = &controls.addKnob (*this, pid::cutoff, "CUTOFF");
    resoKnob = &controls.addKnob (*this, pid::reso, "RESO");

    addAndMakeVisible (filterCurve);
}

void EarPanel::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);
    filterCurve.setUiScale (uiScale);
    background.invalidate();
    resized();
    repaint();
}

void EarPanel::resized()
{
    background.invalidate();
    layoutControls();
}

void EarPanel::layoutControls()
{
    const auto s = uiScale;
    auto inner = getLocalBounds().toFloat().reduced (10.0f * s);

    headerRect = inner.removeFromTop (22.0f * s);
    inner.removeFromTop (5.0f * s);

    filterArea = inner.removeFromTop (104.0f * s);
    inner.removeFromTop (6.0f * s);
    gridArea = inner;

    const auto knobColumnW = 96.0f * s;
    curveArea = { filterArea.getX(), filterArea.getY() + 5.0f * s,
                  filterArea.getWidth() - knobColumnW - 2.0f * s, filterArea.getHeight() - 5.0f * s };
    filterCurve.setBounds (curveArea.toNearestInt());

    if (cutoffKnob != nullptr)
        cutoffKnob->setBounds (juce::roundToInt (filterArea.getRight() - knobColumnW),
                               juce::roundToInt (filterArea.getY()),
                               juce::roundToInt (knobColumnW),
                               juce::roundToInt (52.0f * s));

    if (resoKnob != nullptr)
        resoKnob->setBounds (juce::roundToInt (filterArea.getRight() - knobColumnW),
                             juce::roundToInt (filterArea.getY() + 52.0f * s),
                             juce::roundToInt (knobColumnW),
                             juce::roundToInt (52.0f * s));

    const auto cellW = gridArea.getWidth() / 4.0f;
    const auto cellH = gridArea.getHeight() / 3.0f;

    for (int i = 0; i < 11; ++i)
    {
        if (knobs[(size_t) i] == nullptr)
            continue;

        const auto row = i / 4;
        const auto col = i % 4;
        juce::Rectangle<float> area (gridArea.getX() + (float) col * cellW,
                                     gridArea.getY() + (float) row * cellH,
                                     cellW, cellH);
        knobs[(size_t) i]->setBounds (area.toNearestInt());
    }
}

void EarPanel::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

void EarPanel::tick (const VisualState& snapshot, double elapsedSeconds)
{
    filterCurve.tick (snapshot, elapsedSeconds);
}

void EarPanel::BackgroundCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderBackground (g, bounds);
}

void EarPanel::renderBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    drawPanelPlate (g, bounds.reduced (0.5f * s), s, 5.0f * s);
    drawHeader (g, headerRect, s, "03", "EAR", "ANALOG");

    // Filter module chrome.
    {
        drawMicroText (g, "24 DB LOWPASS",
                       { curveArea.getX() + 2.0f * s, filterArea.getY(),
                         curveArea.getWidth(), 9.0f * s },
                       monoFont (5.6f * fs, juce::Font::plain, 0.18f),
                       copper.withAlpha (0.65f));

        g.setColour (lineSoft.withAlpha (0.8f));
        g.fillRect (filterArea.getX(), filterArea.getBottom() - 1.0f * s,
                    filterArea.getWidth(), 0.8f * s);
    }

    // A quiet engraved divider between the filter and the chain.
    {
        g.setColour (lineSoft.withAlpha (0.55f));
        g.fillRect (gridArea.getX(), gridArea.getY() - 3.0f * s,
                    gridArea.getWidth(), 0.6f * s);
    }

    // Little "EAR" charge dots at the foot of the plate.
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto x = bounds.getX() + 10.0f * s + (float) i * 5.0f * s;
            g.setColour (ember.withAlpha (0.25f - 0.06f * (float) i));
            g.fillEllipse (x, bounds.getBottom() - 7.0f * s, 1.6f * s, 1.6f * s);
        }
    }
}

void EarPanel::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    background.ensure (getWidth(), getHeight(), scale, 4.0e6f);
    drawCached (g, background.getImage(), getLocalBounds());
}

// =============================================================================
// BottomPanel
// =============================================================================
BottomPanel::BottomPanel (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel)
    : controls (state, lookAndFeel), background (*this),
      lfoVisual (state), spaceVisual (state)
{
    setOpaque (false);
    setTitle ("Gesture, Ruimte, modulation and meters");

    const std::array<const char*, 5> macroIds {
        pid::macroGlow, pid::macroDrift, pid::macroGrain, pid::macroSpace, pid::macroShimmer
    };
    const std::array<const char*, 5> macroCaptions {
        "GLOW", "DRIFT", "GRAIN", "SPACE", "SHIMMER"
    };

    for (size_t i = 0; i < macroIds.size(); ++i)
        macroKnobs[i] = &controls.addKnob (*this, macroIds[i], macroCaptions[i]);

    const std::array<const char*, 4> ruimteIds {
        pid::spaceMix, pid::spaceSize, pid::spaceDamp, pid::spaceMod
    };
    const std::array<const char*, 4> ruimteCaptions {
        "MIX", "SIZE", "DAMP", "MOD"
    };

    for (size_t i = 0; i < ruimteIds.size(); ++i)
        ruimteKnobs[i] = &controls.addKnob (*this, ruimteIds[i], ruimteCaptions[i]);

    modKnobs[0] = &controls.addKnob (*this, pid::lfoRate, "RATE");
    modKnobs[1] = &controls.addKnob (*this, pid::lfoAmount, "AMT");
    modKnobs[2] = &controls.addKnob (*this, pid::autoMotion, "AUTO");
    modKnobs[3] = &controls.addKnob (*this, pid::expression, "EXPR");

    modChoices[0] = &controls.addChoice (*this, pid::lfoShape, "SHAPE");
    modChoices[1] = &controls.addChoice (*this, pid::lfoDest, "DEST");
    modChoices[2] = &controls.addChoice (*this, pid::pressureDest, "PRESS");
    modChoices[3] = &controls.addChoice (*this, pid::slideDest, "SLIDE");

    addAndMakeVisible (lfoVisual);
    addAndMakeVisible (spaceVisual);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);
}

void BottomPanel::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);
    lfoVisual.setUiScale (uiScale);
    spaceVisual.setUiScale (uiScale);
    meterIn.setUiScale (uiScale);
    meterOut.setUiScale (uiScale);
    background.invalidate();
    resized();
    repaint();
}

void BottomPanel::resized()
{
    background.invalidate();
    layoutControls();
}

void BottomPanel::layoutControls()
{
    const auto s = uiScale;
    const auto b = getLocalBounds().toFloat();
    const auto gap = 8.0f * s;

    // Derive the section widths from the space that actually exists. The
    // weights are proportional, so the row always fits exactly: 3 VU pixels
    // never spill over the right edge at any editor size.
    const auto available = juce::jmax (0.0f, b.getWidth() - gap * 3.0f);
    const auto unit = available / 1140.0f;   // sum of the four weights

    const auto w0 = 400.0f * unit;
    const auto w1 = 230.0f * unit;
    const auto w2 = 320.0f * unit;
    const auto w3 = available - w0 - w1 - w2;   // absorb rounding in the last section

    gestureArea = { b.getX(), b.getY(), w0, b.getHeight() };
    ruimteArea = { gestureArea.getRight() + gap, b.getY(), w1, b.getHeight() };
    modArea = { ruimteArea.getRight() + gap, b.getY(), w2, b.getHeight() };
    meterArea = { modArea.getRight() + gap, b.getY(), w3, b.getHeight() };

    // --- gesture macros -----------------------------------------------------
    {
        gestureInner = gestureArea.reduced (10.0f * s);
        gestureInner.removeFromTop (20.0f * s);
        gestureInner.removeFromTop (5.0f * s);

        const auto cellW = gestureInner.getWidth() / 5.0f;

        for (int i = 0; i < 5; ++i)
        {
            if (macroKnobs[(size_t) i] == nullptr)
                continue;

            juce::Rectangle<float> area (gestureInner.getX() + (float) i * cellW,
                                         gestureInner.getY(), cellW, gestureInner.getHeight());
            macroKnobs[(size_t) i]->setBounds (area.toNearestInt());
        }
    }

    // --- ruimte -------------------------------------------------------------
    {
        ruimteInner = ruimteArea.reduced (10.0f * s);
        ruimteInner.removeFromTop (20.0f * s);
        ruimteInner.removeFromTop (3.0f * s);

        const auto visualH = 52.0f * s;
        spaceVisual.setBounds (ruimteInner.removeFromTop (visualH).toNearestInt());
        ruimteInner.removeFromTop (2.0f * s);

        const auto cellW = ruimteInner.getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            if (ruimteKnobs[(size_t) i] == nullptr)
                continue;

            juce::Rectangle<float> area (ruimteInner.getX() + (float) i * cellW,
                                         ruimteInner.getY(), cellW, ruimteInner.getHeight());
            ruimteKnobs[(size_t) i]->setBounds (area.toNearestInt());
        }
    }

    // --- mod / LFO ----------------------------------------------------------
    {
        modInner = modArea.reduced (10.0f * s);
        modInner.removeFromTop (20.0f * s);
        modInner.removeFromTop (4.0f * s);

        auto row1 = modInner.removeFromTop (56.0f * s);
        lfoVisual.setBounds (juce::roundToInt (row1.getX()),
                             juce::roundToInt (row1.getY() + 5.0f * s),
                             juce::roundToInt (112.0f * s),
                             juce::roundToInt (50.0f * s));

        auto knobArea = row1.withTrimmedLeft (118.0f * s);
        const auto knobW = knobArea.getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            if (modKnobs[(size_t) i] != nullptr)
                modKnobs[(size_t) i]->setBounds (juce::roundToInt (knobArea.getX() + (float) i * knobW),
                                                 juce::roundToInt (knobArea.getY()),
                                                 juce::roundToInt (knobW),
                                                 juce::roundToInt (knobArea.getHeight()));
        }

        modInner.removeFromTop (4.0f * s);
        const auto comboH = 22.0f * s;
        const auto comboW = modInner.getWidth() / 2.0f;

        for (int i = 0; i < 4; ++i)
        {
            if (modChoices[(size_t) i] == nullptr)
                continue;

            const auto col = i % 2;
            const auto row = i / 2;
            juce::Rectangle<float> area (modInner.getX() + (float) col * comboW,
                                         modInner.getY() + (float) row * (comboH + 4.0f * s),
                                         comboW, comboH);
            modChoices[(size_t) i]->setBounds (area.reduced (2.0f * s, 0.0f).toNearestInt());
        }
    }

    // --- meters -------------------------------------------------------------
    {
        meterInner = meterArea.reduced (10.0f * s);
        meterInner.removeFromTop (20.0f * s);
        meterInner.removeFromTop (3.0f * s);

        const auto meterW = (meterInner.getWidth() - 6.0f * s) * 0.5f;
        meterIn.setBounds (juce::roundToInt (meterInner.getX()),
                           juce::roundToInt (meterInner.getY()),
                           juce::roundToInt (meterW),
                           juce::roundToInt (meterInner.getHeight()));
        meterOut.setBounds (juce::roundToInt (meterInner.getX() + meterW + 6.0f * s),
                            juce::roundToInt (meterInner.getY()),
                            juce::roundToInt (meterW),
                            juce::roundToInt (meterInner.getHeight()));
    }
}

void BottomPanel::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

void BottomPanel::tick (const VisualState& snapshot, double elapsedSeconds)
{
    meterIn.tick (snapshot, elapsedSeconds);
    meterOut.tick (snapshot, elapsedSeconds);
    lfoVisual.tick (snapshot, elapsedSeconds);
    spaceVisual.tick (snapshot, elapsedSeconds);
}

void BottomPanel::BackgroundCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderBackground (g, bounds);
}

void BottomPanel::renderBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ignoreUnused (bounds);

    const auto s = uiScale;

    struct Section
    {
        juce::Rectangle<float> area;
        const char* index;
        const char* title;
        const char* right;
    };

    const std::array<Section, 4> sections {
        Section { gestureArea, "04", "GESTURE", "MACRO" },
        Section { ruimteArea, "05", "RUIMTE", "SPACE" },
        Section { modArea, "06", "MOD", "LFO" },
        Section { meterArea, "07", "VU", "IN / OUT" }
    };

    for (const auto& section : sections)
    {
        drawPanelPlate (g, section.area.reduced (0.5f * s), s, 5.0f * s);

        auto header = section.area.reduced (10.0f * s).removeFromTop (20.0f * s);
        drawHeader (g, header, s, section.index, section.title, section.right);
    }

    // A single engraved divider between the two meters.
    {
        const auto x = meterInner.getX() + (meterInner.getWidth() - 6.0f * s) * 0.5f + 3.0f * s;
        g.setColour (lineSoft.withAlpha (0.8f));
        g.fillRect (x - 0.4f * s, meterInner.getY() + 2.0f * s,
                    0.8f * s, meterInner.getHeight() - 2.0f * s);
    }
}

void BottomPanel::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    background.ensure (getWidth(), getHeight(), scale, 4.0e6f);
    drawCached (g, background.getImage(), getLocalBounds());
}

} // namespace nagloed::ui
