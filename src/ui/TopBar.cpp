#include "TopBar.h"

#include <cmath>

namespace nagloed::ui
{

using namespace theme;

TopBar::TopBar (juce::AudioProcessorValueTreeState& state, PresetAccess& presets,
                AfterglowLookAndFeel& lookAndFeel)
    : apvts (state), lnf (lookAndFeel), browser (presets), freezeButton ("FREEZE"),
      background (*this)
{
    setOpaque (false);
    setTitle ("NAGLOED header");
    setDescription ("NAGLOED wordmark, preset browser, engine status and capture freeze.");

    freezeButton.setOpaque (false);
    freezeButton.setClickingTogglesState (true);
    freezeButton.setWantsKeyboardFocus (true);
    freezeButton.getProperties().set ("role", "freeze");
    freezeButton.getProperties().set ("uiScale", 1.0);
    freezeButton.setTitle ("Freeze");
    freezeButton.setDescription ("Freeze the capture buffer so the current grains become a held afterglow.");
    freezeButton.setTooltip (utf8 ("FREEZE — hold the capture buffer; the ember field keeps glowing"));

    if (apvts.getParameter (pid::freeze) != nullptr)
    {
        freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, pid::freeze, freezeButton);
        freezeAttached = (freezeAttachment != nullptr);
    }

    addAndMakeVisible (browser);
    addAndMakeVisible (freezeButton);
}

TopBar::~TopBar()
{
    freezeAttachment.reset();
}

void TopBar::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    freezeButton.getProperties().set ("uiScale", (double) uiScale);
    browser.setUiScale (uiScale);
    resized();
    repaint();
}

void TopBar::layoutAtScale()
{
    const auto s = uiScale;
    const auto b = getLocalBounds().toFloat();
    const auto h = b.getHeight();

    const auto freezeW = 122.0f * s;
    const auto freezeH = 28.0f * s;
    const auto freezeX = b.getRight() - freezeW;
    const auto freezeY = (h - freezeH) * 0.5f;
    freezeButton.setBounds (juce::roundToInt (freezeX), juce::roundToInt (freezeY),
                            juce::roundToInt (freezeW), juce::roundToInt (freezeH));

    statusArea = { freezeX - 134.0f * s, (h - 30.0f * s) * 0.5f, 120.0f * s, 30.0f * s };

    const auto browserLeft = b.getX() + 336.0f * s;
    const auto browserRight = statusArea.getX() - 16.0f * s;
    const auto available = juce::jmax (160.0f * s, browserRight - browserLeft);
    const auto browserW = juce::jmin (470.0f * s, available);
    const auto browserX = browserLeft + (available - browserW) * 0.5f;
    const auto browserY = (h - 38.0f * s) * 0.5f;

    browser.setBounds (juce::roundToInt (browserX),
                       juce::roundToInt (browserY),
                       juce::roundToInt (browserW),
                       juce::roundToInt (38.0f * s));

    browserCaptionRect = { browserX, b.getY() + 0.5f * s, browserW, 7.0f * s };
    freezeCaptionRect = { freezeX, b.getY() + 0.5f * s, freezeW, 7.0f * s };
}

void TopBar::resized()
{
    layoutAtScale();
    background.invalidate();
    lnf.setUiScale (uiScale);
}

// --- cached chrome -----------------------------------------------------------
void TopBar::BackgroundCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderBackground (g, bounds);
}

void TopBar::renderBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto h = bounds.getHeight();

    // --- sigil: an afterglow mark — a bright ember and its fading tail ------
    {
        const auto cx = bounds.getX() + 16.0f * s;
        const auto cy = bounds.getY() + h * 0.46f;

        // Tail fading to the right.
        juce::ColourGradient tail (gold.withAlpha (0.30f), cx, cy,
                                   gold.withAlpha (0.0f), cx + 24.0f * s, cy, false);
        g.setGradientFill (tail);
        g.fillRect (cx, cy - 0.8f * s, 24.0f * s, 1.6f * s);

        for (int i = 0; i < 4; ++i)
        {
            const auto x = cx + (4.5f + (float) i * 3.6f) * s;
            const auto r = (1.5f - (float) i * 0.28f) * s;
            g.setColour ((i < 2 ? paleEmber : gold).withAlpha (0.32f - (float) i * 0.07f));
            g.fillEllipse (x - r, cy - r, r * 2.0f, r * 2.0f);
        }

        drawGlowDot (g, cx, cy, 1.9f * s, ember, 0.95f, s);

        g.setColour (gold.withAlpha (0.28f));
        g.drawEllipse (cx - 7.2f * s, cy - 7.2f * s, 14.4f * s, 14.4f * s, 0.7f * s);
    }

    // --- procedurally drawn wordmark ----------------------------------------
    {
        const auto font = displayFont (27.0f * fs, juce::Font::bold, 0.035f);
        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (font, "NAGLOED", 0.0f, 0.0f);

        juce::Path wordmark;
        arrangement.createPath (wordmark);

        const auto pathBounds = wordmark.getBounds();
        const auto x = bounds.getX() + 33.0f * s;
        const auto cy = bounds.getY() + h * 0.40f;
        wordmark.applyTransform (juce::AffineTransform::translation (
            x - pathBounds.getX(), cy - pathBounds.getCentreY()));

        // Warm halo, then a copper engraving, then the crisp gradient fill.
        g.setColour (ember.withAlpha (0.035f));
        g.strokePath (wordmark, juce::PathStrokeType (7.0f * s, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour (ember.withAlpha (0.06f));
        g.strokePath (wordmark, juce::PathStrokeType (3.6f * s, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour (gold.withAlpha (0.10f));
        g.strokePath (wordmark, juce::PathStrokeType (1.6f * s, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        juce::ColourGradient fill (paleEmber, x, cy,
                                   gold, x + 200.0f * s, cy, false);
        fill.addColour (0.55, paleEmber.interpolatedWith (gold, 0.35f));
        g.setGradientFill (fill);
        g.fillPath (wordmark);
    }

    // --- tagline: the brand sentence, then the Eindhoven line ---------------
    {
        const auto x = bounds.getX() + 34.0f * s;

        drawMicroText (g, "THE SOUND AFTER THE SOUND",
                       { x, bounds.getY() + h * 0.55f, 300.0f * s, 10.0f * s },
                       monoFont (7.4f * fs, juce::Font::plain, 0.22f),
                       paleEmber.withAlpha (0.78f));

        drawMicroText (g, utf8 ("UNLIKENOISE · GRANULAR AFTERGLOW · EINDHOVEN NL"),
                       { x, bounds.getY() + h * 0.72f, 330.0f * s, 10.0f * s },
                       monoFont (6.4f * fs, juce::Font::plain, 0.14f),
                       muted.withAlpha (0.75f));
    }

    // --- status captions ----------------------------------------------------
    {
        drawMicroText (g, "ENGINE",
                       { statusArea.getX(), statusArea.getY(), statusArea.getWidth(), 14.0f * s },
                       monoFont (6.8f * fs, juce::Font::plain, 0.16f),
                       dim.withAlpha (0.95f));

        drawMicroText (g, "GRAIN RATE",
                       { statusArea.getX(), statusArea.getY() + 15.0f * s,
                         statusArea.getWidth(), 14.0f * s },
                       monoFont (6.8f * fs, juce::Font::plain, 0.16f),
                       dim.withAlpha (0.95f));
    }

    // --- small engraved captions -------------------------------------------
    {
        drawMicroText (g, "PRESET",
                       browserCaptionRect,
                       monoFont (6.4f * fs, juce::Font::plain, 0.20f),
                       muted.withAlpha (0.7f));

        drawMicroText (g, "CAPTURE",
                       freezeCaptionRect,
                       monoFont (5.5f * fs, juce::Font::plain, 0.22f),
                       muted.withAlpha (0.65f), juce::Justification::centredRight);
    }

    // --- ruler and bottom rule ---------------------------------------------
    {
        const auto y = bounds.getBottom() - 1.0f * s;
        drawFadeRule (g, { bounds.getX(), y, bounds.getWidth(), juce::jmax (0.7f, 0.8f * s) },
                      muted, 0.35f);

        for (float x = bounds.getX(); x < bounds.getRight(); x += 9.0f * s)
        {
            const auto major = std::fmod (x, 45.0f * s) < 0.5f;
            g.setColour (muted.withAlpha (major ? 0.30f : 0.13f));
            g.fillRect (x, bounds.getBottom() - (major ? 4.0f : 2.0f) * s,
                        0.6f * s, (major ? 3.0f : 1.5f) * s);
        }
    }
}

void TopBar::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    background.ensure (getWidth(), getHeight(), scale, 4.0e6f);
    drawCached (g, background.getImage(), getLocalBounds());

    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto row1 = statusArea.withHeight (statusArea.getHeight() * 0.5f);
    const auto row2 = statusArea.withTrimmedTop (statusArea.getHeight() * 0.5f);

    // Live / idle state.
    {
        const auto dotColour = live ? gold : muted;
        drawGlowDot (g, statusArea.getX() + 3.0f * s, row1.getCentreY(), 1.3f * s,
                     dotColour, live ? 0.9f : 0.45f, s);

        drawMicroText (g, live ? "LIVE" : "IDLE",
                       row1.withTrimmedLeft (8.0f * s),
                       monoFont (7.2f * fs, juce::Font::plain, 0.16f),
                       live ? gold.withAlpha (0.95f) : muted.withAlpha (0.9f),
                       juce::Justification::centredRight);
    }

    // Grain rate.
    drawMicroText (g, juce::String (grainRate, 1) + " G/S",
                   row2,
                   monoFont (7.2f * fs, juce::Font::plain, 0.06f),
                   muted.withAlpha (0.9f), juce::Justification::centredRight);
}

void TopBar::setStatus (float rate, bool hasGrains)
{
    const auto rounded = std::round (rate * 10.0f) * 0.1f;
    const auto liveFlag = hasGrains ? 1 : 0;

    if (std::abs (rounded - shownRate) < 0.001f && liveFlag == shownLive)
        return;

    grainRate = rounded;
    live = hasGrains;
    shownRate = rounded;
    shownLive = liveFlag;

    repaint (statusArea.toNearestInt().expanded (2));
}

void TopBar::tick()
{
    if (freezeButton.getToggleState())
        freezeButton.repaint();
}

void TopBar::refreshPresets()
{
    browser.refresh();
}

void TopBar::collectParameterIds (juce::StringArray& ids) const
{
    if (freezeAttached)
        ids.addIfNotAlreadyThere (pid::freeze);
}

} // namespace nagloed::ui
