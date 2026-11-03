#include "NagloedEditor.h"

#include <cmath>

#include "core/Version.h"

namespace nagloed::ui
{

using namespace theme;

NagloedEditor::NagloedEditor (juce::AudioProcessor& processor,
                              juce::AudioProcessorValueTreeState& state,
                              const VisualSource& visualSource,
                              PresetAccess& presetAccess)
    : juce::AudioProcessorEditor (&processor),
      apvts (state),
      visual (visualSource),
      presets (presetAccess),
      lookAndFeel(),
      tooltipWindow (this, 650),
      topBar (state, presetAccess, lookAndFeel),
      grainCloud (state, lookAndFeel),
      sintelPanel (state, lookAndFeel),
      earPanel (state, lookAndFeel),
      bottomPanel (state, lookAndFeel),
      pageCache (*this)
{
    setLookAndFeel (&lookAndFeel);

    setOpaque (true);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (960, 560, 2400, 1500);
    setSize (1180, 680);

    addAndMakeVisible (topBar);
    addAndMakeVisible (grainCloud);
    addAndMakeVisible (sintelPanel);
    addAndMakeVisible (earPanel);
    addAndMakeVisible (bottomPanel);

    refreshOnce();
    verifyParameterCoverage();
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

NagloedEditor::~NagloedEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// -----------------------------------------------------------------------------
void NagloedEditor::verifyParameterCoverage()
{
    // Every parameter the processor exposes must have exactly one attached
    // control somewhere in the editor; this catches additions to the contract
    // that the UI forgets to wire up.
    juce::StringArray covered;
    topBar.collectParameterIds (covered);
    grainCloud.collectParameterIds (covered);
    sintelPanel.collectParameterIds (covered);
    earPanel.collectParameterIds (covered);
    bottomPanel.collectParameterIds (covered);

    juce::StringArray missing;

    if (auto* processor = getAudioProcessor())
    {
        for (auto* parameter : processor->getParameters())
        {
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                if (! covered.contains (withId->paramID))
                    missing.add (withId->paramID);
        }
    }

    jassert (missing.isEmpty());

    if (missing.isEmpty())
        DBG ("NAGLOED UI: all " << covered.size() << " parameters are attached");
    else
        DBG ("NAGLOED UI: unattached parameters: " << missing.joinIntoString (", "));
}

// -----------------------------------------------------------------------------
void NagloedEditor::refreshOnce()
{
    visual.copyVisual (snapshot);

    constexpr double dt = 1.0 / 60.0;
    grainCloud.tick (snapshot, dt);
    sintelPanel.tick (snapshot, dt);
    earPanel.tick (snapshot, dt);
    bottomPanel.tick (snapshot, dt);
    topBar.setStatus (snapshot.grainRate, snapshot.numGrains, snapshot.numGrains > 0);
    topBar.refreshPresets();
}

void NagloedEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    auto dt = (now - lastTickMs) / 1000.0;
    lastTickMs = now;

    if (! isShowing())
        return;   // hidden editor: no snapshot reads, no repaints

    dt = juce::jlimit (0.001, 0.1, dt);

    // One snapshot per frame, however many visualisers read it.
    visual.copyVisual (snapshot);

    grainCloud.tick (snapshot, dt);

    if ((tickCounter & 1) == 0)
    {
        sintelPanel.tick (snapshot, dt * 2.0);
        earPanel.tick (snapshot, dt * 2.0);
        bottomPanel.tick (snapshot, dt * 2.0);
        topBar.setStatus (snapshot.grainRate, snapshot.numGrains, snapshot.numGrains > 0);
    }

    if (tickCounter % 12 == 0)
        topBar.refreshPresets();

    if (tickCounter % 4 == 0)
        topBar.tick();

    ++tickCounter;
}

// -----------------------------------------------------------------------------
void NagloedEditor::resized()
{
    const auto scale = theme::scaleFor (getLocalBounds());
    applyScale (scale);
    layoutAtScale();
    pageCache.invalidate();
}

void NagloedEditor::applyScale (float newScale)
{
    newScale = juce::jlimit (theme::minUiScale, theme::maxUiScale, newScale);

    if (std::abs (newScale - uiScale) < 0.004f && uiScale > 0.0f)
        return;

    uiScale = newScale;

    lookAndFeel.setUiScale (uiScale);
    topBar.setUiScale (uiScale);
    grainCloud.setUiScale (uiScale);
    sintelPanel.setUiScale (uiScale);
    earPanel.setUiScale (uiScale);
    bottomPanel.setUiScale (uiScale);
    pageCache.invalidate();
}

void NagloedEditor::layoutAtScale()
{
    const auto s = uiScale;
    const auto b = getLocalBounds();
    const auto margin = juce::roundToInt (14.0f * s);
    const auto topH = juce::roundToInt (54.0f * s);
    const auto bottomH = juce::roundToInt (156.0f * s);
    const auto footerH = juce::roundToInt (14.0f * s);
    const auto gapTop = juce::roundToInt (8.0f * s);
    const auto gapMain = juce::roundToInt (8.0f * s);
    const auto gapBottom = juce::roundToInt (6.0f * s);

    const auto innerW = b.getWidth() - margin * 2;

    topRect = { margin, margin, innerW, topH };

    footerRect = { margin, b.getBottom() - margin - footerH, innerW, footerH };
    bottomRect = { margin, footerRect.getY() - gapBottom - bottomH, innerW, bottomH };

    mainRect = { margin, topRect.getBottom() + gapTop, innerW,
                 bottomRect.getY() - gapMain - (topRect.getBottom() + gapTop) };

    const auto sintelW = juce::roundToInt (236.0f * s);
    const auto earW = juce::roundToInt (300.0f * s);
    const auto mainGap = juce::roundToInt (8.0f * s);
    const auto cloudW = juce::jmax (240, mainRect.getWidth() - sintelW - earW - mainGap * 2);

    sintelPanel.setBounds (mainRect.getX(), mainRect.getY(), sintelW, mainRect.getHeight());
    grainCloud.setBounds (mainRect.getX() + sintelW + mainGap, mainRect.getY(),
                          cloudW, mainRect.getHeight());
    earPanel.setBounds (mainRect.getX() + sintelW + mainGap + cloudW + mainGap,
                        mainRect.getY(), earW, mainRect.getHeight());

    topBar.setBounds (topRect);
    bottomPanel.setBounds (bottomRect);
}

// -----------------------------------------------------------------------------
void NagloedEditor::PageCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderPage (g, bounds);
}

void NagloedEditor::renderPage (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    // Base gradient.
    {
        juce::ColourGradient base (pageTop, bounds.getX(), bounds.getY(),
                                   pageBottom, bounds.getX(), bounds.getBottom(), false);
        base.addColour (0.55, juce::Colour (0xff090c12));
        g.setGradientFill (base);
        g.fillRect (bounds);
    }

    drawAtmosphere (g, bounds, s, 1.0f);

    // A hairline frame and four registration ticks.
    {
        g.setColour (copper.withAlpha (0.16f));
        g.drawRect (bounds.reduced (0.5f * s), juce::jmax (0.8f, 0.8f * s));

        g.setColour (copper.withAlpha (0.30f));
        const auto inset = 7.0f * s;
        const auto len = 10.0f * s;
        const auto pen = 0.9f * s;

        g.fillRect (bounds.getX() + inset, bounds.getY() + inset, len, pen);
        g.fillRect (bounds.getX() + inset, bounds.getY() + inset, pen, len);

        g.fillRect (bounds.getRight() - inset - len, bounds.getY() + inset, len, pen);
        g.fillRect (bounds.getRight() - inset - pen, bounds.getY() + inset, pen, len);

        g.fillRect (bounds.getX() + inset, bounds.getBottom() - inset - pen, len, pen);
        g.fillRect (bounds.getX() + inset, bounds.getBottom() - inset - len, pen, len);

        g.fillRect (bounds.getRight() - inset - len, bounds.getBottom() - inset - pen, len, pen);
        g.fillRect (bounds.getRight() - inset - pen, bounds.getBottom() - inset - len, pen, len);
    }

    // Footer engraving.
    {
        const auto audioProcessor = getAudioProcessor();
        const auto numParams = audioProcessor != nullptr
                                   ? (int) audioProcessor->getParameters().size()
                                   : 0;
        const auto sampleRate = audioProcessor != nullptr ? audioProcessor->getSampleRate() : 0.0;
        const auto srText = sampleRate > 0.0
                                ? juce::String (sampleRate / 1000.0, 1) + " KHZ"
                                : utf8 ("— KHZ");
        const auto numPresets = presets.getNumPresets();

        const auto footer = footerRect.toFloat();
        const auto font = monoFont (5.9f * fs, juce::Font::plain, 0.13f);

        drawMicroText (g, juce::String ("NAGLOED ") + kVersion + utf8 ("  ·  UNLIKENOISE LABS"),
                       footer.withTrimmedRight (footer.getWidth() * 0.66f),
                       font, muted.withAlpha (0.8f));

        drawMicroText (g,
                       juce::String (numParams) + utf8 (" PARAMETERS  ·  ")
                           + juce::String (numPresets) + (numPresets == 1 ? " PRESET" : " PRESETS")
                           + utf8 ("  ·  ") + srText,
                       footer.withTrimmedLeft (footer.getWidth() * 0.50f),
                       font, dim.withAlpha (0.9f), juce::Justification::centredRight);

        drawMicroText (g, utf8 ("EINDHOVEN · NL"),
                       footer.withTrimmedLeft (footer.getWidth() * 0.30f)
                             .withTrimmedRight (footer.getWidth() * 0.50f),
                       font, copper.withAlpha (0.55f), juce::Justification::centred);

        drawFadeRule (g, { footer.getX(), footer.getY() - 2.0f * s,
                           footer.getWidth(), juce::jmax (0.7f, 0.8f * s) },
                      copper, 0.18f);
    }
}

void NagloedEditor::paint (juce::Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    pageCache.ensure (getWidth(), getHeight(), scale);
    drawCached (g, pageCache.getImage(), getLocalBounds());
}

} // namespace nagloed::ui
