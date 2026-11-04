#include "NagloedEditor.h"

#include <cmath>
#include <cstdio>

#include "core/Version.h"

namespace nagloed::ui
{

using namespace theme;

namespace
{
// Shown only when a parameter in the APVTS has no attached control. This is a
// release-build failure state, not a debug-only assertion.
class CornerResizeGrip final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    explicit CornerResizeGrip (juce::Component& editorToResize)
        : editor (editorToResize)
    {
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
        setTitle ("Resize editor");
        setTooltip ("Drag to resize the editor");
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto s = juce::jmin (b.getWidth(), b.getHeight());
        g.setColour (theme::muted.withAlpha (0.55f));

        for (int i = 0; i < 3; ++i)
        {
            const auto offset = (float) i * 4.5f * (s / 22.0f);
            g.drawLine (b.getRight() - 5.0f - offset, b.getBottom() - 2.0f,
                        b.getRight() - 2.0f, b.getBottom() - 5.0f - offset,
                        1.0f * (s / 22.0f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        startSize = { editor.getWidth(), editor.getHeight() };
        startScreenPosition = e.getScreenPosition();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto delta = e.getScreenPosition() - startScreenPosition;
        const auto w = juce::jlimit (960, 2400, startSize.x + delta.x);
        const auto h = juce::jlimit (560, 1500, startSize.y + delta.y);
        editor.setSize (w, h);
    }

private:
    juce::Component& editor;
    juce::Point<int> startSize, startScreenPosition;
};

class CoverageErrorBanner final : public juce::Component
{
public:
    explicit CoverageErrorBanner (juce::String message)
        : text (std::move (message))
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, false);
        setAlwaysOnTop (true);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (warning.withAlpha (0.92f));
        g.fillRect (b);
        g.setColour (paleEmber);
        g.setFont (monoFont (9.0f, juce::Font::bold, 0.02f));
        g.drawFittedText (text, b.toNearestInt().reduced (10, 2), juce::Justification::centredLeft, 1);
    }

private:
    juce::String text;
};
} // namespace

NagloedEditor::NagloedEditor (juce::AudioProcessor& audioProcessor,
                              juce::AudioProcessorValueTreeState& state,
                              const VisualSource& visualSource,
                              PresetAccess& presetAccess)
    : juce::AudioProcessorEditor (&audioProcessor),
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
    setResizable (true, false);
    setResizeLimits (960, 560, 2400, 1500);
    setSize (1180, 680);

    addAndMakeVisible (topBar);
    addAndMakeVisible (grainCloud);
    addAndMakeVisible (sintelPanel);
    addAndMakeVisible (earPanel);
    addAndMakeVisible (bottomPanel);

    resizeGrip = std::make_unique<CornerResizeGrip> (*this);
    addAndMakeVisible (*resizeGrip);

    // Every parameter change (mouse or host automation) schedules a visual
    // refresh; the timer is the steady animation clock on top of that.
    for (auto* parameter : audioProcessor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            apvts.addParameterListener (withId->paramID, this);

    refreshOnce();
    verifyParameterCoverage();
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

NagloedEditor::~NagloedEditor()
{
    stopTimer();
    cancelPendingUpdate();

    for (auto* parameter : getAudioProcessor()->getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            apvts.removeParameterListener (withId->paramID, this);

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

    const auto total = covered.size();
    juce::ignoreUnused (total);
    jassert (missing.isEmpty());

    if (missing.isEmpty())
    {
        coverageError.clear();

        if (errorBanner != nullptr)
        {
            removeChildComponent (errorBanner.get());
            errorBanner.reset();
        }

        DBG ("NAGLOED UI: all " << total << " parameters are attached");
        return;
    }

    // Hard failure in release builds too: visible banner + clear stderr line.
    coverageError = utf8 ("UI PARAMETER COVERAGE ERROR — unattached: ")
                    + missing.joinIntoString (", ");
    std::fprintf (stderr, "NAGLOED UI ERROR: %s\n", coverageError.toRawUTF8());

    if (errorBanner == nullptr)
    {
        errorBanner = std::make_unique<CoverageErrorBanner> (coverageError);
        addAndMakeVisible (*errorBanner);
    }
    else
    {
        errorBanner->repaint();
    }

    errorBanner->toFront (false);
    resized();
}

// -----------------------------------------------------------------------------
void NagloedEditor::advanceForPreview (int frames, double dtSeconds)
{
    stopTimer();   // no wall-clock frames mixed into the deterministic run

    grainCloud.resetForDeterministicPreview();
    visual.copyVisual (snapshot);

    for (int i = 0; i < frames; ++i)
    {
        grainCloud.tick (snapshot, dtSeconds);

        if ((i & 1) == 0)
        {
            sintelPanel.tick (snapshot, dtSeconds * 2.0);
            earPanel.tick (snapshot, dtSeconds * 2.0);
            bottomPanel.tick (snapshot, dtSeconds * 2.0);
            topBar.setStatus (snapshot.grainRate, snapshot.numGrains > 0);
        }

        if (i % 12 == 0)
            topBar.refreshPresets();

        if (i % 4 == 0)
            topBar.tick();
    }

    // Pin the wall-clock-driven visualiser phases so the frame is reproducible.
    bottomPanel.setPhasesForPreview (0.37f, 0.21f);
    repaint();
}

void NagloedEditor::refreshOnce()
{
    visual.copyVisual (snapshot);

    constexpr double dt = 1.0 / 60.0;
    grainCloud.tick (snapshot, dt);
    sintelPanel.tick (snapshot, dt);
    earPanel.tick (snapshot, dt);
    bottomPanel.tick (snapshot, dt);
    topBar.setStatus (snapshot.grainRate, snapshot.numGrains > 0);
    topBar.refreshPresets();
}

void NagloedEditor::visibilityChanged()
{
    // Hosts mount the editor as a child of their own view and can report
    // isShowing() false transiently while reparenting. Stopping the timer here
    // loses the animation clock for the rest of the session (the 1.0.0 frozen
    // visualiser bug). The timer stays alive; timerCallback() is a cheap no-op
    // while the editor is not visible.
    if (isVisible() && ! isTimerRunning())
        startTimerHz (60);
}

void NagloedEditor::parentHierarchyChanged()
{
    // Defensive restart for hosts that reparent the editor view after showing
    // it: the timer must survive any mount order.
    if (isVisible() && ! isTimerRunning())
        startTimerHz (60);
}

void NagloedEditor::parameterChanged (const juce::String&, float)
{
    // Called on the audio thread for automation, hence the async hop.
    triggerAsyncUpdate();
}

void NagloedEditor::handleAsyncUpdate()
{
    refreshOnce();
}

void NagloedEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    auto dt = (now - lastTickMs) / 1000.0;
    lastTickMs = now;

    if (! isVisible())
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
        topBar.setStatus (snapshot.grainRate, snapshot.numGrains > 0);
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

    if (errorBanner != nullptr)
        errorBanner->setBounds (juce::Rectangle<int> (margin, margin,
                                                      innerW, juce::roundToInt (24.0f * s)));

    if (resizeGrip != nullptr)
    {
        const auto grip = juce::roundToInt (22.0f * s);
        resizeGrip->setBounds (getWidth() - grip - 2, getHeight() - grip - 2, grip, grip);
        resizeGrip->toFront (false);
    }
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
        g.setColour (muted.withAlpha (0.16f));
        g.drawRect (bounds.reduced (0.5f * s), juce::jmax (0.8f, 0.8f * s));

        g.setColour (muted.withAlpha (0.28f));
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
        const auto font = monoFont (7.6f * fs, juce::Font::plain, 0.11f);

        drawMicroText (g, juce::String ("NAGLOED ") + kVersion + utf8 ("  ·  MASTER LONN & LITTLE WAN"),
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
                       font, muted.withAlpha (0.75f), juce::Justification::centred);

        drawFadeRule (g, { footer.getX(), footer.getY() - 2.0f * s,
                           footer.getWidth(), juce::jmax (0.7f, 0.8f * s) },
                      muted, 0.18f);
    }
}

void NagloedEditor::paint (juce::Graphics& g)
{
    // The page texture is comparatively expensive. When a child repaints inside
    // the panel area the expensive atmospheric layer never shows through, so
    // skip it and just fill the base colour.
    const auto clip = g.getClipBounds();

    // Opaque panel children paint over these regions; the page texture and the
    // base fill are invisible there. Skipping them keeps the 60 Hz cloud
    // repaint cheap on large, high-DPI windows.
    if (sintelPanel.getBounds().contains (clip)
        || grainCloud.getBounds().contains (clip)
        || earPanel.getBounds().contains (clip)
        || bottomPanel.getBounds().contains (clip))
        return;

    g.fillAll (pageBottom);

    const auto coveredByPanels = mainRect.contains (clip) || bottomRect.contains (clip);
    if (coveredByPanels)
        return;

    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    pageCache.ensure (getWidth(), getHeight(), scale, 6.0e6f);
    drawCached (g, pageCache.getImage(), getLocalBounds());
}

} // namespace nagloed::ui
