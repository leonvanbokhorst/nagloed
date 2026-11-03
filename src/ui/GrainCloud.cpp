#include "GrainCloud.h"
#include <cstdio>

#include <algorithm>
#include <array>
#include <cmath>

namespace nagloed::ui
{

using namespace theme;

namespace
{
constexpr int   kMaxMotes      = 400;
constexpr int   kPrewarmFrames = 96;
constexpr float kFadeAlpha     = 0.075f;

juce::String percentText (float normalised)
{
    return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, normalised) * 100.0f)) + "%";
}
} // namespace

// =============================================================================
// PhosphorField: additive ember buffer with slow decay.
// =============================================================================
bool GrainCloud::PhosphorField::ensure (int logicalWidth, int logicalHeight, float scale)
{
    if (logicalWidth <= 0 || logicalHeight <= 0)
        return false;

    scale = juce::jlimit (0.5f, 3.0f, scale);

    if (buffer.isValid() && logicalWidth == logicalW && logicalHeight == logicalH
        && std::abs (scale - pixelScale) < 0.01f)
        return false;

    logicalW = logicalWidth;
    logicalH = logicalHeight;
    pixelScale = scale;

    const auto physicalW = juce::jmax (1, juce::roundToInt ((float) logicalWidth * scale));
    const auto physicalH = juce::jmax (1, juce::roundToInt ((float) logicalHeight * scale));

    buffer = juce::Image (juce::Image::ARGB, physicalW, physicalH, true);
    buffer.clear (buffer.getBounds(), juce::Colour (0xff000000));   // opaque black screen

    pixels = std::make_unique<juce::Image::BitmapData> (buffer, juce::Image::BitmapData::writeOnly);
    return true;
}

void GrainCloud::PhosphorField::clear()
{
    if (buffer.isValid())
        buffer.clear (buffer.getBounds());
}

void GrainCloud::PhosphorField::fade (float amount)
{
    if (! pixels)
        return;

    // Decay in place on the raw RGB buffer. Doing it manually (rather than
    // drawing a translucent black rectangle) is both faster and independent of
    // the image's alpha semantics.
    const auto keep = juce::jlimit (0.0f, 1.0f, 1.0f - amount);
    const auto factor = (juce::uint32) juce::jlimit (0, 256,
                                                     juce::roundToInt (keep * 256.0f));

    auto* data = pixels.get();
    const auto stride = data->pixelStride;

    for (int y = 0; y < data->height; ++y)
    {
        auto* rowPtr = data->getLinePointer (y);

        for (int x = 0; x < data->width; ++x)
        {
            auto* p = rowPtr + x * stride;
            p[0] = (juce::uint8) ((p[0] * factor) >> 8);
            p[1] = (juce::uint8) ((p[1] * factor) >> 8);
            p[2] = (juce::uint8) ((p[2] * factor) >> 8);
            p[3] = 255;
        }
    }
}

void GrainCloud::PhosphorField::splat (float cx, float cy, float radius,
                                       juce::Colour colour, float energy)
{
    if (energy <= 0.0005f || ! pixels || radius <= 0.0f)
        return;

    const auto* data = pixels.get();
    const auto w = data->width;
    const auto h = data->height;
    const auto stride = data->pixelStride;

    const auto px = cx * pixelScale;
    const auto py = cy * pixelScale;
    const auto r = juce::jmax (0.6f, radius * pixelScale);
    const auto r2 = r * r;

    const auto x0 = juce::jmax (0, (int) std::floor (px - r));
    const auto x1 = juce::jmin (w - 1, (int) std::ceil (px + r));
    const auto y0 = juce::jmax (0, (int) std::floor (py - r));
    const auto y1 = juce::jmin (h - 1, (int) std::ceil (py + r));

    if (x0 > x1 || y0 > y1)
        return;

    const auto invR2 = 1.0f / r2;
    const auto cr = (float) colour.getRed() * energy;
    const auto cg = (float) colour.getGreen() * energy;
    const auto cb = (float) colour.getBlue() * energy;

    for (int y = y0; y <= y1; ++y)
    {
        const auto dy = (float) y - py;
        const auto dy2 = dy * dy;
        auto* rowPtr = data->getLinePointer (y);

        for (int x = x0; x <= x1; ++x)
        {
            const auto dx = (float) x - px;
            const auto d2 = dx * dx + dy2;

            if (d2 >= r2)
                continue;

            const auto falloff = 1.0f - d2 * invR2;
            const auto f = falloff * falloff;
            auto* p = rowPtr + x * stride;

            // JUCE's RGB image stores pixels as blue, green, red.
            p[0] = (juce::uint8) juce::jmin (255, (int) p[0] + (int) (cb * f));
            p[1] = (juce::uint8) juce::jmin (255, (int) p[1] + (int) (cg * f));
            p[2] = (juce::uint8) juce::jmin (255, (int) p[2] + (int) (cr * f));
            p[3] = 255;
        }
    }

}

// =============================================================================
// Construction
// =============================================================================
GrainCloud::GrainCloud (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel)
    : apvts (state), lnf (lookAndFeel), controls (state, lookAndFeel),
      chrome (*this), gridOverlay (*this)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    setTitle ("Grain cloud");
    setDescription ("Drag horizontally to move the focus, vertically to transpose the grains, "
                    "shift-drag to widen the spray, scroll to change density.");
    setTooltip (utf8 ("Grain cloud — drag: focus/pitch · shift-drag: spray · wheel: density · double-click: re-centre"));

    focusParam = apvts.getParameter (pid::focus);
    pitchParam = apvts.getParameter (pid::pitch);
    sprayParam = apvts.getParameter (pid::spray);
    densityParam = apvts.getParameter (pid::density);
    sizeParam = apvts.getParameter (pid::size);
    scanParam = apvts.getParameter (pid::scan);

    scanRibbon = &controls.addRibbon (*this, pid::scan, "SCAN");
    reverseRibbon = &controls.addRibbon (*this, pid::reverse, "REV");

    motes.reserve ((size_t) kMaxMotes);
}

GrainCloud::~GrainCloud() = default;

void GrainCloud::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

void GrainCloud::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.4f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);
    chrome.invalidate();
    gridOverlay.invalidate();
    prewarmNeeded = true;
    layoutAtScale();
    repaint();
}

void GrainCloud::resized()
{
    layoutAtScale();
    controls.setUiScale (uiScale);
    chrome.invalidate();
    gridOverlay.invalidate();
    prewarmNeeded = true;

    if (scanRibbon != nullptr)
        scanRibbon->setBounds (scanRect.toNearestInt());
    if (reverseRibbon != nullptr)
        reverseRibbon->setBounds (reverseRect.toNearestInt());
}

void GrainCloud::layoutAtScale()
{
    const auto s = uiScale;
    auto content = getLocalBounds().toFloat().reduced (9.0f * s);

    headerRect = content.removeFromTop (20.0f * s);
    content.removeFromTop (5.0f * s);

    timelineRect = content.removeFromBottom (64.0f * s);
    content.removeFromBottom (5.0f * s);

    leftAxisRect = content.removeFromLeft (24.0f * s);
    bottomAxisRect = content.removeFromBottom (12.0f * s);
    fieldRect = content.reduced (0.5f * s, 1.0f * s);

    auto t = timelineRect;
    t.removeFromTop (timelineLabelH * s);
    t.removeFromTop (2.0f * s);
    trackRect = t.removeFromTop (12.0f * s);
    t.removeFromTop (5.0f * s);

    const auto ribbonRow = t.removeFromTop (22.0f * s);
    t.removeFromTop (2.0f * s);
    hintRect = t.removeFromTop (9.0f * s);

    scanRect = ribbonRow.withWidth (ribbonRow.getWidth() * 0.5f - 3.0f * s);
    reverseRect = ribbonRow.withTrimmedLeft (ribbonRow.getWidth() * 0.5f + 3.0f * s);
}

float GrainCloud::focusToX (float focus) const
{
    return fieldRect.getX() + juce::jlimit (0.0f, 1.0f, focus) * fieldRect.getWidth();
}

float GrainCloud::pitchToNorm (float semitones) const
{
    return juce::jlimit (0.0f, 1.0f, (semitones + 24.0f) / 48.0f);
}

float GrainCloud::pitchToY (float semitones) const
{
    return fieldRect.getBottom() - pitchToNorm (semitones) * fieldRect.getHeight();
}

// --- parameter helpers -------------------------------------------------------
void GrainCloud::setParamNormalised (juce::RangedAudioParameter* param, float normalised)
{
    if (param != nullptr)
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void GrainCloud::nudgeParam (juce::RangedAudioParameter* param, float deltaNormalised)
{
    if (param == nullptr)
        return;

    beginGesture (param);
    setParamNormalised (param, param->getValue() + deltaNormalised);
    endGesture (param);
}

float GrainCloud::paramNormalised (const juce::RangedAudioParameter* param) const
{
    return param != nullptr ? param->getValue() : 0.0f;
}

float GrainCloud::paramReal (const juce::RangedAudioParameter* param) const
{
    return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
}

void GrainCloud::beginGesture (juce::RangedAudioParameter* param)
{
    if (param == nullptr)
        return;

    if (param == focusParam && ! focusGesture)
    {
        param->beginChangeGesture();
        focusGesture = true;
    }
    else if (param == pitchParam && ! pitchGesture)
    {
        param->beginChangeGesture();
        pitchGesture = true;
    }
    else if (param == sprayParam && ! sprayGesture)
    {
        param->beginChangeGesture();
        sprayGesture = true;
    }
}

void GrainCloud::endGesture (juce::RangedAudioParameter* param)
{
    if (param == nullptr)
        return;

    if (param == focusParam && focusGesture)
    {
        param->endChangeGesture();
        focusGesture = false;
    }
    else if (param == pitchParam && pitchGesture)
    {
        param->endChangeGesture();
        pitchGesture = false;
    }
    else if (param == sprayParam && sprayGesture)
    {
        param->endChangeGesture();
        sprayGesture = false;
    }
}

// =============================================================================
// Animation
// =============================================================================
void GrainCloud::tick (const VisualState& snapshot, double elapsedSeconds)
{
    visual = snapshot;
    haveVisual = true;
    ++frameCounter;

    const auto targetFocus = paramNormalised (focusParam);
    const auto targetSpray = paramNormalised (sprayParam);
    const auto targetPitch = paramReal (pitchParam);

    if (! smoothedInit)
    {
        smoothedInit = true;
        shownFocus = targetFocus;
        shownSpray = targetSpray;
        shownPitch = targetPitch;
        shownBufferFill = visual.bufferFill;
    }
    else
    {
        const auto rate = juce::jlimit (0.10f, 0.45f, (float) (elapsedSeconds * 60.0) * 0.30f);
        shownFocus += (targetFocus - shownFocus) * rate;
        shownSpray += (targetSpray - shownSpray) * rate;
        shownPitch += (targetPitch - shownPitch) * rate;
        shownBufferFill += (visual.bufferFill - shownBufferFill) * 0.10f;
    }

    if (readoutCounter++ % 6 == 0)
        updateReadouts();

    if (phosphor.isReady() && deviceScale > 0.0f)
    {
        const auto timeScale = juce::jlimit (0.4f, 2.5f, (float) (elapsedSeconds * 60.0));

        if (visual.numGrains > 0)
            drawGrains();
        else
            drawIdleDust (timeScale);
    }

    repaint();
}

void GrainCloud::updateReadouts()
{
    focusText = "FOCUS " + percentText (paramNormalised (focusParam));
    sprayText = "SPRAY " + percentText (paramNormalised (sprayParam));

    if (densityParam != nullptr)
        densityText = densityParam->getText (densityParam->getValue(), 24).toUpperCase();
    else
        densityText = utf8 ("—");

    grainRateText = juce::String (visual.grainRate, visual.grainRate < 10.0f ? 2 : 1) + " G/S";
    bufferText = "BUFFER " + percentText (visual.bufferFill);

    headerInfoText = densityText + utf8 ("   ·   ") + juce::String (visual.numGrains) + " GRAINS";
    timelineInfoText = bufferText + "    " + focusText + "    " + sprayText;
}

int GrainCloud::targetMoteCount() const
{
    const auto densityNorm = juce::jlimit (0.0f, 1.0f, paramNormalised (densityParam));
    const auto scaled = 140.0f + 300.0f * std::pow (densityNorm, 0.70f);
    return (int) juce::jlimit (140.0f, 430.0f, scaled);
}

void GrainCloud::spawnMote (Mote& m, bool anywhere)
{
    std::uniform_real_distribution<float> dist (0.0f, 1.0f);
    auto rnd = [&] { return dist (rng); };
    auto gauss = [&] { return (rnd() + rnd() - 1.0f) * 1.5f; };

    const auto sizeNorm = paramNormalised (sizeParam);
    const auto pitchNorm = pitchToNorm (paramReal (pitchParam));

    m.wanderer = anywhere;

    if (anywhere)
    {
        m.x = rnd();
        m.y = rnd();
        m.vx = (rnd() - 0.5f) * 0.0022f;
        m.vy = (rnd() - 0.5f) * 0.0006f;
        m.size = 1.2f + rnd() * 1.4f;
        m.energy = 0.012f + rnd() * 0.014f;
        m.warmth = rnd();
    }
    else
    {
        const auto spreadX = 0.070f + shownSpray * 0.62f;
        const auto spreadY = 0.070f + shownSpray * 0.38f;
        const auto roll = rnd();
        const auto spark = roll < 0.045f;
        const auto core = ! spark && roll < 0.075f;
        const auto bokeh = ! spark && ! core && roll < 0.22f;

        m.x = juce::jlimit (-0.08f, 1.08f, shownFocus + gauss() * spreadX);
        m.y = juce::jlimit (-0.12f, 1.12f, (1.0f - pitchNorm) + gauss() * spreadY);
        m.vx = (rnd() - 0.5f) * 0.0013f;

        if (spark)
        {
            m.vy = -(0.0010f + rnd() * 0.0018f);
            m.size = 2.2f + rnd() * 1.6f;
            m.energy = 0.050f + rnd() * 0.045f;
        }
        else if (core)
        {
            m.vy = (rnd() - 0.5f) * 0.0006f;
            m.size = 2.2f + rnd() * 1.5f;
            m.energy = 0.075f + rnd() * 0.045f;
        }
        else if (bokeh)
        {
            m.vy = (rnd() - 0.5f) * 0.0007f;
            m.size = 5.0f + rnd() * 4.5f;
            m.energy = 0.005f + rnd() * 0.007f;
        }
        else
        {
            m.vy = (rnd() - 0.58f) * 0.0016f;
            m.size = 1.5f + rnd() * 1.8f;
            m.energy = 0.0080f + rnd() * 0.0090f;
        }

        m.warmth = juce::jlimit (0.0f, 1.0f, 0.15f + rnd() * 0.60f + paramReal (pitchParam) / 120.0f);
    }

    m.phase = rnd() * juce::MathConstants<float>::twoPi;
    m.phaseSpeed = 0.015f + rnd() * 0.05f;
    m.sway = 0.3f + rnd() * 1.4f;
    m.size *= 0.65f + 0.85f * sizeNorm;
}

void GrainCloud::advanceMote (Mote& m, float timeScale)
{
    m.phase += m.phaseSpeed * timeScale;
    const auto sway = std::sin (m.phase) * m.sway * 0.00020f;
    const auto breath = std::sin (windPhase + m.y * 3.1f) * 0.00006f;

    // A slow curl around the focus point gives the dust a living, eddying
    // quality instead of straight rising sparks.
    const auto dx = m.x - shownFocus;
    const auto dy = m.y - (1.0f - pitchToNorm (shownPitch));
    const auto distance = std::sqrt (dx * dx + dy * dy) + 0.03f;
    const auto swirl = (m.wanderer ? 0.00007f : 0.00034f) * (1.0f + shownSpray) / distance;

    m.x += (-dy * swirl + m.vx + sway + breath) * timeScale;
    m.y += ( dx * swirl + m.vy) * timeScale;

    if (m.y < -0.10f || m.y > 1.10f || m.x < -0.15f || m.x > 1.15f)
        spawnMote (m, m.wanderer);
}

void GrainCloud::drawIdleDust (float timeScale)
{
    if (! phosphor.isReady())
        return;


    phosphor.fade (kFadeAlpha * juce::jlimit (0.5f, 1.6f, timeScale));

    const auto target = targetMoteCount();
    if ((int) motes.size() < target)
    {
        while ((int) motes.size() < target)
        {
            Mote m;
            spawnMote (m, motes.size() < 60);
            motes.push_back (m);
        }
    }
    else if ((int) motes.size() > target + 32)
    {
        motes.resize ((size_t) target);
    }

    windPhase += 0.004f * timeScale;

    const auto fw = fieldRect.getWidth();
    const auto fh = fieldRect.getHeight();

    // A quiet pool of light under the focus point, so the screen always has an
    // afterglow even before the first ember has drifted.
    {
        const auto glowX = shownFocus * fw;
        const auto glowY = (1.0f - pitchToNorm (shownPitch)) * fh;
        const auto haze = 0.9f + shownSpray * 1.4f;
        phosphor.splat (glowX, glowY, 78.0f * uiScale * haze, ember, 0.0130f);
        phosphor.splat (glowX, glowY, 30.0f * uiScale * haze, gold, 0.0160f);
    }

    for (auto& m : motes)
    {
        advanceMote (m, timeScale);

        const auto px = m.x * fw;
        const auto py = m.y * fh;
        if (px < -12.0f || px > fw + 12.0f || py < -12.0f || py > fh + 12.0f)
            continue;

        const auto flicker = 0.72f + 0.28f * std::sin (m.phase * 1.9f + m.y * 7.0f);
        const auto energy = m.energy * flicker;
        const auto colour = emberRamp (0.22f + 0.60f * m.warmth);

        phosphor.splat (px, py, m.size * 2.8f, colour, energy * 0.38f);
        phosphor.splat (px, py, m.size, colour, energy);

        // Short motion-blur ghost along the velocity vector.
        if (m.size > 1.0f)
        {
            phosphor.splat (px - m.vx * fw * 2.5f,
                            py - m.vy * fh * 2.5f,
                            m.size * 0.95f, colour, energy * 0.50f);
        }

        if (energy > 0.09f)
            phosphor.splat (px, py, m.size * 0.45f, paleEmber, energy * 0.45f);
        else if (m.size > 2.6f)
            phosphor.splat (px, py, m.size * 0.32f, gold, energy * 0.55f);
    }
}

void GrainCloud::prewarm()
{
    if (! phosphor.isReady())
        return;

    phosphor.clear();
    motes.clear();
    windPhase = 0.0f;

    if (! smoothedInit)
    {
        smoothedInit = true;
        shownFocus = paramNormalised (focusParam);
        shownSpray = paramNormalised (sprayParam);
        shownPitch = paramReal (pitchParam);
    }

    prewarmNeeded = false;

    if (visual.numGrains > 0)
        return;   // live grains will paint themselves; no dust to fake

    for (int i = 0; i < kPrewarmFrames; ++i)
        drawIdleDust (1.0f);
}

void GrainCloud::drawGrains()
{
    if (! phosphor.isReady())
        return;

    phosphor.fade (kFadeAlpha);

    const auto fw = fieldRect.getWidth();
    const auto fh = fieldRect.getHeight();
    const auto sizeNorm = paramNormalised (sizeParam);
    const auto num = juce::jlimit (0, VisualState::kMaxGrainViews, visual.numGrains);

    for (int i = 0; i < num; ++i)
    {
        const auto& grain = visual.grains[(size_t) i];
        const auto env = juce::jlimit (0.0f, 1.0f, grain.env);
        const auto x = grain.pos * fw + grain.pan * 6.0f * uiScale;
        const auto y = (1.0f - pitchToNorm (grain.pitch)) * fh;
        const auto radius = (0.9f + 2.6f * env) * (0.75f + 0.6f * sizeNorm);
        const auto colour = pitchColour (grain.pitch);
        const auto brightness = juce::jlimit (0.0f, 1.0f, grain.brightness);
        const auto energy = 0.15f + 0.55f * env;

        phosphor.splat (x, y, radius * 2.3f, colour, energy * 0.24f);
        phosphor.splat (x, y, radius, colour, energy * 0.70f);

        if (brightness > 0.35f)
            phosphor.splat (x, y, radius * 0.45f, paleEmber,
                            energy * 0.30f * (brightness - 0.35f) / 0.65f);
    }
}

// =============================================================================
// Painting
// =============================================================================
void GrainCloud::ensureCaches (float currentDeviceScale)
{
    deviceScale = juce::jlimit (0.5f, 3.0f, currentDeviceScale);

    // Keep the cloud's cached pixels and phosphor buffer bounded on very large
    // editor sizes; the screen is glow, so a slightly lower pixel density is
    // invisible, while the per-frame blit and decay stay fluid.
    const auto logicalArea = (float) juce::jmax (1, getWidth()) * (float) juce::jmax (1, getHeight());
    constexpr float maxCachedPixels = 1'800'000.0f;
    const auto densityCap = std::sqrt (maxCachedPixels / juce::jmax (1.0f, logicalArea));
    const auto cacheScale = juce::jlimit (1.0f, juce::jmin (deviceScale, 2.2f), densityCap);

    chrome.ensure (getWidth(), getHeight(), cacheScale);
    gridOverlay.ensure (getWidth(), getHeight(), cacheScale);

    const auto fw = juce::jmax (1, (int) std::lround (fieldRect.getWidth()));
    const auto fh = juce::jmax (1, (int) std::lround (fieldRect.getHeight()));

    if (phosphor.ensure (fw, fh, juce::jmin (cacheScale, 2.0f)))
        prewarmNeeded = true;
}

void GrainCloud::paint (juce::Graphics& g)
{
    if (uiScale <= 0.0f)
        layoutAtScale();

    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    ensureCaches (scale);

    if (phosphor.isReady() && prewarmNeeded)
        prewarm();

    drawCached (g, chrome.getImage(), getLocalBounds());

    if (phosphor.isReady())
        g.drawImage (phosphor.getImage(), fieldRect, juce::RectanglePlacement::stretchToFit, false);

    drawCached (g, gridOverlay.getImage(), getLocalBounds());

    drawFieldOverlay (g);
    drawTimeline (g);
    drawReadouts (g);
}

// --- cached chrome -----------------------------------------------------------
void GrainCloud::ChromeCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderChrome (g, bounds);
}

void GrainCloud::GridCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderGrid (g, bounds);
}

void GrainCloud::renderChrome (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    drawPanelPlate (g, bounds.reduced (0.5f * s), s, 5.0f * s);
    drawHeader (g, headerRect, s, "02", utf8 ("SINTEL · GRAIN FIELD"));

    // Screen well, sitting in a pool of warm light.
    {
        const auto fr = fieldRect;

        {
            juce::ColourGradient bloom (ember.withAlpha (0.055f), fr.getCentreX(), fr.getCentreY(),
                                        ember.withAlpha (0.0f), fr.getCentreX(),
                                        fr.getCentreY() + fr.getHeight() * 0.85f, true);
            g.setGradientFill (bloom);
            g.fillRoundedRectangle (fr.expanded (16.0f * s), 6.0f * s);
        }

        juce::ColourGradient well (wellTop, fr.getX(), fr.getY(),
                                   wellBottom, fr.getX(), fr.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (fr, 2.0f * s);

        g.setColour (copper.withAlpha (0.28f));
        g.drawRoundedRectangle (fr, 2.0f * s, 0.9f * s);

        g.setColour (lineSoft.withAlpha (0.7f));
        g.drawRoundedRectangle (fr.reduced (1.5f * s), 1.6f * s, 0.7f * s);

        g.setColour (paleEmber.withAlpha (0.045f));
        g.fillRect (fr.getX() + 2.0f * s, fr.getY() + 1.0f * s,
                    fr.getWidth() - 4.0f * s, 0.8f * s);
    }

    // Pitch axis (left of the screen).
    {
        const std::array<int, 5> pitches { 24, 12, 0, -12, -24 };

        for (auto pitch : pitches)
        {
            const auto y = pitchToY ((float) pitch);
            const auto label = (pitch > 0 ? "+" : "") + juce::String (pitch);

            drawMicroText (g, label,
                           { leftAxisRect.getX(), y - 5.0f * s,
                             leftAxisRect.getWidth() - 5.0f * s, 10.0f * s },
                           monoFont (6.0f * fs, juce::Font::plain, 0.06f),
                           dim.withAlpha (0.9f), juce::Justification::centredRight);

            g.setColour (copperDim.withAlpha (0.35f));
            g.fillRect (fieldRect.getX() - 3.0f * s, y - 0.5f * s, 3.0f * s, 0.8f * s);
        }
    }

    // Position ticks under the screen.
    {
        for (int i = 0; i <= 4; ++i)
        {
            const auto t = (float) i / 4.0f;
            const auto x = fieldRect.getX() + t * fieldRect.getWidth();
            const auto label = juce::String (t, 2);

            g.setColour (copperDim.withAlpha (0.35f));
            g.fillRect (x - 0.4f * s, fieldRect.getBottom(), 0.8f * s, 3.0f * s);

            drawMicroText (g, label,
                           { x - 12.0f * s, bottomAxisRect.getY() + 1.0f * s,
                             24.0f * s, bottomAxisRect.getHeight() },
                           monoFont (5.8f * fs, juce::Font::plain, 0.02f),
                           dim.withAlpha (0.85f), juce::Justification::centred);
        }
    }

    // Capture-buffer timeline.
    {
        drawMicroText (g, "CAPTURE BUFFER",
                       { timelineRect.getX(), timelineRect.getY(),
                         timelineRect.getWidth() * 0.5f, timelineLabelH * s },
                       monoFont (6.4f * fs, juce::Font::plain, 0.16f),
                       copper.withAlpha (0.75f));

        const auto tr = trackRect;
        g.setColour (juce::Colour (0xff070a0e));
        g.fillRoundedRectangle (tr, tr.getHeight() * 0.5f);
        g.setColour (lineSoft.withAlpha (0.9f));
        g.drawRoundedRectangle (tr, tr.getHeight() * 0.5f, 1.0f * s);

        for (int i = 0; i <= 10; ++i)
        {
            const auto x = tr.getX() + tr.getWidth() * (float) i / 10.0f;
            g.setColour (copperDim.withAlpha (i % 5 == 0 ? 0.42f : 0.20f));
            g.fillRect (x - 0.3f * s, tr.getY() + tr.getHeight() * 0.5f - 2.0f * s,
                        0.6f * s, 4.0f * s);
        }

        drawMicroText (g, utf8 ("DRAG X FOCUS · Y PITCH · SHIFT SPRAY · WHEEL DENSITY · DOUBLE-CLICK CENTRE"),
                       { hintRect.getX(), hintRect.getY(), hintRect.getWidth(), hintRect.getHeight() },
                       monoFont (5.9f * fs, juce::Font::plain, 0.06f),
                       dim.withAlpha (0.9f));
    }

    // The little "02" is part of drawHeader; add a soft ember marker at the
    // spot where the cloud meets the timeline.
    {
        const auto y = fieldRect.getBottom() + 1.5f * s;
        g.setColour (ember.withAlpha (0.25f));
        g.fillRect (fieldRect.getX(), y, fieldRect.getWidth(), 0.7f * s);
    }
}

void GrainCloud::renderGrid (juce::Graphics& g, juce::Rectangle<float>)
{
    const auto s = uiScale;
    const auto fr = fieldRect.reduced (1.0f * s);

    if (fr.isEmpty())
        return;

    g.saveState();
    g.reduceClipRegion (fr.toNearestInt());

    // Position rules.
    for (int i = 1; i < 4; ++i)
    {
        const auto x = fr.getX() + fr.getWidth() * (float) i * 0.25f;
        g.setColour (line.withAlpha (i == 2 ? 0.10f : 0.055f));
        g.fillRect (x - 0.25f * s, fr.getY(), 0.5f * s, fr.getHeight());
    }

    // Pitch rules, with a slightly brighter centre line.
    for (int pitch = -24; pitch <= 24; pitch += 6)
    {
        if (pitch % 12 != 0 && pitch != 0)
            continue;

        const auto y = pitchToY ((float) pitch);
        g.setColour (line.withAlpha (pitch == 0 ? 0.10f : 0.055f));
        g.fillRect (fr.getX(), y - 0.25f * s, fr.getWidth(), 0.5f * s);
    }

    // Engraved corner brackets inside the screen.
    {
        g.setColour (copperDim.withAlpha (0.42f));
        const auto len = 9.0f * s;
        const auto pen = 0.8f * s;
        const auto tl = fr.getTopLeft() + juce::Point<float> (4.0f * s, 4.0f * s);
        const auto tr = fr.getTopRight() + juce::Point<float> (-4.0f * s, 4.0f * s);
        const auto bl = fr.getBottomLeft() + juce::Point<float> (4.0f * s, -4.0f * s);
        const auto br = fr.getBottomRight() + juce::Point<float> (-4.0f * s, -4.0f * s);

        g.fillRect (tl.x, tl.y, len, pen);
        g.fillRect (tl.x, tl.y, pen, len);
        g.fillRect (tr.x - len, tr.y, len, pen);
        g.fillRect (tr.x - pen, tr.y, pen, len);
        g.fillRect (bl.x, bl.y - pen, len, pen);
        g.fillRect (bl.x, bl.y - len, pen, len);
        g.fillRect (br.x - len, br.y - pen, len, pen);
        g.fillRect (br.x - pen, br.y - len, pen, len);
    }

    // Soft edge fade so drifting embers dissolve instead of being clipped.
    {
        const auto fadeTop = fr.getHeight() * 0.16f;
        const auto fadeSide = fr.getWidth() * 0.10f;

        juce::ColourGradient top (juce::Colours::black.withAlpha (0.55f), fr.getX(), fr.getY(),
                                  juce::Colours::transparentBlack, fr.getX(), fr.getY() + fadeTop, false);
        g.setGradientFill (top);
        g.fillRect (juce::Rectangle<float> (fr.getX(), fr.getY(), fr.getWidth(), fadeTop));

        juce::ColourGradient bottom (juce::Colours::transparentBlack, fr.getX(),
                                     fr.getBottom() - fadeTop,
                                     juce::Colours::black.withAlpha (0.55f), fr.getX(),
                                     fr.getBottom(), false);
        g.setGradientFill (bottom);
        g.fillRect (juce::Rectangle<float> (fr.getX(), fr.getBottom() - fadeTop,
                                            fr.getWidth(), fadeTop));

        juce::ColourGradient left (juce::Colours::black.withAlpha (0.35f), fr.getX(), fr.getY(),
                                   juce::Colours::transparentBlack, fr.getX() + fadeSide, fr.getY(), false);
        g.setGradientFill (left);
        g.fillRect (juce::Rectangle<float> (fr.getX(), fr.getY(), fadeSide, fr.getHeight()));

        juce::ColourGradient right (juce::Colours::transparentBlack, fr.getRight() - fadeSide, fr.getY(),
                                    juce::Colours::black.withAlpha (0.35f), fr.getRight(), fr.getY(), false);
        g.setGradientFill (right);
        g.fillRect (juce::Rectangle<float> (fr.getRight() - fadeSide, fr.getY(),
                                            fadeSide, fr.getHeight()));
    }

    g.restoreState();
}

// --- live overlays -----------------------------------------------------------
void GrainCloud::drawFieldOverlay (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto fr = fieldRect;
    const auto fx = focusToX (shownFocus);
    const auto fy = pitchToY (shownPitch);

    const auto sprayX = (0.05f + shownSpray * 0.42f) * fr.getWidth();
    const auto sprayY = (0.04f + shownSpray * 0.26f) * fr.getHeight();

    g.saveState();
    g.reduceClipRegion (fr.toNearestInt());

    // Spray halo: a soft horizontal band with a vertical falloff, plus a lens
    // of light at the focus.
    {
        constexpr int strips = 16;
        const auto bandTop = fy - sprayY * 0.55f;
        const auto stripH = sprayY * 1.1f / (float) strips;

        for (int i = 0; i < strips; ++i)
        {
            const auto t = ((float) i + 0.5f) / (float) strips;
            const auto edge = 1.0f - std::abs (t * 2.0f - 1.0f);
            const auto alpha = (0.022f + 0.075f * shownSpray) * edge * edge;

            juce::ColourGradient strip (ember.withAlpha (0.0f), fx - sprayX, bandTop,
                                        ember.withAlpha (0.0f), fx + sprayX, bandTop, false);
            strip.addColour (0.5, ember.withAlpha (alpha));
            g.setGradientFill (strip);
            g.fillRect (juce::Rectangle<float> (fx - sprayX, bandTop + (float) i * stripH,
                                                sprayX * 2.0f, stripH + 0.6f * s));
        }

        juce::ColourGradient lens (ember.withAlpha (0.060f + 0.045f * shownSpray), fx, fy,
                                   ember.withAlpha (0.0f), fx, fy + sprayY, true);
        g.setGradientFill (lens);
        g.fillEllipse (fx - sprayY, fy - sprayY, sprayY * 2.0f, sprayY * 2.0f);
    }

    // Soft optical bloom around the focus point (on top of the phosphor).
    {
        const auto bloomRadius = (78.0f + 70.0f * shownSpray) * s;
        juce::ColourGradient bloom (ember.withAlpha (0.10f + 0.05f * shownSpray), fx, fy,
                                    ember.withAlpha (0.0f), fx, fy + bloomRadius, true);
        g.setGradientFill (bloom);
        g.fillEllipse (fx - bloomRadius, fy - bloomRadius,
                       bloomRadius * 2.0f, bloomRadius * 2.0f);
    }

    // Vertical focus rule.
    {
        g.setColour (ember.withAlpha (0.06f));
        g.fillRect (fx - 2.0f * s, fr.getY(), 4.0f * s, fr.getHeight());
        g.setColour (copper.withAlpha (0.34f));
        g.fillRect (fx - 0.5f * s, fr.getY(), 1.0f * s, fr.getHeight());
    }

    // Horizontal pitch rule (dashed, engraved).
    {
        g.setColour (copper.withAlpha (0.24f));
        const auto dash = 3.0f * s;
        const auto gap = 4.0f * s;
        for (float x = fr.getX(); x < fr.getRight(); x += dash + gap)
            g.fillRect (x, fy - 0.4f * s, dash, 0.8f * s);
    }

    // Focus diamond at the top edge and crosshair node.
    {
        juce::Path arrow;
        const auto cx = fx;
        const auto cy = fr.getY() + 1.0f * s;
        arrow.startNewSubPath (cx, cy);
        arrow.lineTo (cx - 3.0f * s, cy - 4.0f * s);
        arrow.lineTo (cx + 3.0f * s, cy - 4.0f * s);
        arrow.closeSubPath();
        g.setColour (paleEmber.withAlpha (0.85f));
        g.fillPath (arrow);

        g.setColour (ember.withAlpha (0.20f));
        g.drawEllipse (fx - 5.0f * s, fy - 5.0f * s, 10.0f * s, 10.0f * s, 1.0f * s);
        drawGlowDot (g, fx, fy, 1.8f * s, ember, 0.75f, s);
    }

    // Mouse crosshair, only while hovering (and not dragging).
    if (hoverField && ! dragging)
    {
        const auto mx = hoverPos.x;
        const auto my = hoverPos.y;
        const auto len = 5.0f * s;
        const auto gap = 2.0f * s;

        g.setColour (muted.withAlpha (0.35f));
        g.fillRect (mx - len - gap, my - 0.4f * s, len, 0.8f * s);
        g.fillRect (mx + gap, my - 0.4f * s, len, 0.8f * s);
        g.fillRect (mx - 0.4f * s, my - len - gap, 0.8f * s, len);
        g.fillRect (mx - 0.4f * s, my + gap, 0.8f * s, len);
        g.drawEllipse (mx - 3.0f * s, my - 3.0f * s, 6.0f * s, 6.0f * s, 0.7f * s);
    }

    g.restoreState();

    // Drag HUD.
    if (dragging && dragHud.isNotEmpty())
    {
        const auto font = monoFont (6.6f * fs, juce::Font::plain, 0.10f);
        const auto width = theme::textWidth (font, dragHud) + 14.0f * s;
        juce::Rectangle<float> hud (fr.getRight() - width - 8.0f * s,
                                    fr.getY() + 7.0f * s, width, 16.0f * s);

        g.setColour (juce::Colour (0xff080b10).withAlpha (0.82f));
        g.fillRoundedRectangle (hud, 2.0f * s);
        g.setColour (copper.withAlpha (0.5f));
        g.drawRoundedRectangle (hud, 2.0f * s, 0.8f * s);
        drawMicroText (g, dragHud, hud, font, paleEmber, juce::Justification::centred);
    }
}

void GrainCloud::drawTimeline (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto tr = trackRect;

    const auto focusX = tr.getX() + juce::jlimit (0.0f, 1.0f, shownFocus) * tr.getWidth();

    // Spray band on the buffer.
    {
        const auto half = (0.015f + shownSpray * 0.24f) * tr.getWidth();
        juce::ColourGradient band (ember.withAlpha (0.0f), focusX - half, tr.getCentreY(),
                                   ember.withAlpha (0.0f), focusX + half, tr.getCentreY(), false);
        band.addColour (0.5, ember.withAlpha (0.07f));
        g.setGradientFill (band);
        g.fillRect (juce::Rectangle<float> (focusX - half, tr.getY() + 1.0f * s,
                                            half * 2.0f, tr.getHeight() - 2.0f * s));
    }

    // Buffer fill.
    const auto fill = juce::jlimit (0.0f, 1.0f, shownBufferFill);
    if (fill > 0.002f)
    {
        const auto fillW = tr.getWidth() * fill;
        juce::Rectangle<float> fillArea (tr.getX(), tr.getY(), fillW, tr.getHeight());

        juce::ColourGradient fillGrad (copper.withAlpha (0.30f), tr.getX(), tr.getY(),
                                       ember.withAlpha (0.50f), tr.getX() + fillW, tr.getY(), false);
        g.setGradientFill (fillGrad);
        g.fillRoundedRectangle (fillArea, tr.getHeight() * 0.5f);

        const auto edgeX = tr.getX() + fillW;
        g.setColour (paleEmber.withAlpha (0.10f));
        g.fillRect (edgeX - 3.0f * s, tr.getY(), 6.0f * s, tr.getHeight());
        g.setColour (paleEmber.withAlpha (0.7f));
        g.fillRect (edgeX - 0.5f * s, tr.getY() - 2.0f * s, 1.0f * s, tr.getHeight() + 4.0f * s);
    }

    // Focus marker.
    {
        g.setColour (paleEmber.withAlpha (0.85f));
        g.fillRect (focusX - 0.7f * s, tr.getY() - 3.0f * s, 1.4f * s, tr.getHeight() + 6.0f * s);
        g.setColour (ember.withAlpha (0.22f));
        g.fillRect (focusX - 2.5f * s, tr.getY() - 3.0f * s, 5.0f * s, tr.getHeight() + 6.0f * s);

        juce::Path marker;
        marker.startNewSubPath (focusX, tr.getY() - 3.0f * s);
        marker.lineTo (focusX - 2.5f * s, tr.getY() - 7.0f * s);
        marker.lineTo (focusX + 2.5f * s, tr.getY() - 7.0f * s);
        marker.closeSubPath();
        g.setColour (paleEmber);
        g.fillPath (marker);
    }

    // Scan direction arrow(s).
    {
        const auto scan = juce::jlimit (-1.0f, 1.0f, paramReal (scanParam));
        const auto strength = juce::jmax (0.25f, std::abs (scan));
        const auto hot = scan >= -0.01f;

        auto drawArrow = [&] (float tipX, float dir, float alpha)
        {
            const auto c = juce::Point<float> (tipX, tr.getCentreY());
            const auto r = 3.2f * s;
            juce::Path arrow;
            arrow.startNewSubPath (c.x + dir * r * 0.9f, c.y - r);
            arrow.lineTo (c.x - dir * r * 0.9f, c.y);
            arrow.lineTo (c.x + dir * r * 0.9f, c.y + r);
            g.setColour (gold.withAlpha (alpha));
            g.strokePath (arrow, juce::PathStrokeType (1.1f * s, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        };

        if (std::abs (scan) > 0.02f)
        {
            drawArrow (hot ? tr.getRight() + 6.0f * s : tr.getX() - 6.0f * s,
                       hot ? 1.0f : -1.0f, 0.45f + 0.45f * strength);
            g.setFont (monoFont (5.8f * fs, juce::Font::plain, 0.12f));
            g.setColour (dim.withAlpha (0.8f));
            g.drawText (hot ? utf8 ("SCAN →") : utf8 ("SCAN ←"),
                        juce::Rectangle<float> (tr.getRight() + 10.0f * s, tr.getY() - 1.0f * s,
                                                56.0f * s, tr.getHeight()),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            drawArrow (tr.getRight() + 6.0f * s, 1.0f, 0.20f);
            drawArrow (tr.getX() - 6.0f * s, -1.0f, 0.20f);
        }
    }
}

void GrainCloud::drawReadouts (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    // Header right: mode chip + grain rate.
    {
        const auto live = visual.numGrains > 0 || visual.grainRate > 0.05f;
        const auto chipArea = headerRect.withTrimmedLeft (headerRect.getWidth() - 58.0f * s)
                                         .withTrimmedRight (4.0f * s)
                                         .reduced (0.0f, 4.0f * s);

        const auto dotColour = live ? mint : copper;
        drawGlowDot (g, chipArea.getX() + 3.0f * s, chipArea.getCentreY(), 1.3f * s,
                     dotColour, live ? 0.9f : 0.5f, s);

        drawMicroText (g, live ? "LIVE" : "IDLE",
                       chipArea.withTrimmedLeft (9.0f * s),
                       monoFont (6.5f * fs, juce::Font::plain, 0.16f),
                       live ? mint.withAlpha (0.9f) : copper.withAlpha (0.85f));

        const auto infoWidth = 210.0f * s;
        drawMicroText (g, headerInfoText,
                       headerRect.withTrimmedLeft (headerRect.getWidth() - infoWidth - 64.0f * s)
                                 .withWidth (infoWidth)
                                 .withTrimmedRight (4.0f * s),
                       monoFont (6.3f * fs, juce::Font::plain, 0.08f),
                       dim.withAlpha (0.95f), juce::Justification::centredRight);
    }

    // Timeline label row right: buffer / focus / spray.
    {
        const auto area = juce::Rectangle<float> (timelineRect.getX() + timelineRect.getWidth() * 0.28f,
                                                  timelineRect.getY(),
                                                  timelineRect.getWidth() * 0.72f,
                                                  timelineLabelH * s);
        drawMicroText (g, timelineInfoText,
                       area, monoFont (6.2f * fs, juce::Font::plain, 0.08f),
                       muted.withAlpha (0.9f), juce::Justification::centredRight);
    }

    // Grain count next to the section title.
    if (visual.numGrains > 0)
    {
        drawMicroText (g, juce::String (visual.numGrains) + " GRAINS",
                       headerRect.withTrimmedLeft (headerRect.getWidth() * 0.42f)
                                 .withWidth (120.0f * s),
                       monoFont (6.2f * fs, juce::Font::plain, 0.12f),
                       copper.withAlpha (0.8f));
    }
}

// =============================================================================
// Gestures
// =============================================================================
void GrainCloud::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.mods.isPopupMenu()
        || (! fieldRect.contains (e.position) && ! trackRect.contains (e.position)))
        return;

    dragging = true;
    dragStartMouse = e.position;
    dragStartFocusNorm = paramNormalised (focusParam);
    dragStartPitchReal = paramReal (pitchParam);
    dragStartSprayNorm = paramNormalised (sprayParam);

    if (trackRect.contains (e.position))
    {
        timelineDrag = true;
        shiftSpray = false;
        beginGesture (focusParam);
        dragHud = "FOCUS " + percentText (dragStartFocusNorm);
    }
    else
    {
        timelineDrag = false;
        shiftSpray = e.mods.isShiftDown();

        if (shiftSpray)
        {
            beginGesture (sprayParam);
            dragHud = "SPRAY " + percentText (dragStartSprayNorm);
        }
        else
        {
            beginGesture (focusParam);
            beginGesture (pitchParam);
            dragHud = "FOCUS " + percentText (dragStartFocusNorm)
                      + utf8 (" · PITCH ") + juce::String (dragStartPitchReal, 1) + " ST";
        }
    }

    repaint();
}

void GrainCloud::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const auto dx = e.position.x - dragStartMouse.x;
    const auto dy = e.position.y - dragStartMouse.y;

    if (timelineDrag)
    {
        const auto norm = dragStartFocusNorm + dx / juce::jmax (1.0f, fieldRect.getWidth() * 0.9f);
        setParamNormalised (focusParam, norm);
        dragHud = "FOCUS " + percentText (paramNormalised (focusParam));
    }
    else if (shiftSpray)
    {
        const auto norm = dragStartSprayNorm
                          + dx / juce::jmax (1.0f, fieldRect.getWidth() * 0.55f);
        setParamNormalised (sprayParam, norm);
        dragHud = "SPRAY " + percentText (paramNormalised (sprayParam));
    }
    else
    {
        const auto focusNorm = dragStartFocusNorm
                               + dx / juce::jmax (1.0f, fieldRect.getWidth() * 0.9f);
        const auto pitch = dragStartPitchReal - dy / juce::jmax (1.0f, fieldRect.getHeight()) * 48.0f;

        setParamNormalised (focusParam, focusNorm);
        setParamNormalised (pitchParam, pitchParam != nullptr
                                             ? pitchParam->convertTo0to1 (pitch)
                                             : 0.5f);

        dragHud = "FOCUS " + percentText (paramNormalised (focusParam))
                  + utf8 (" · PITCH ") + juce::String (paramReal (pitchParam), 1) + " ST";
    }

    repaint();
}

void GrainCloud::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    timelineDrag = false;
    dragHud.clear();

    endGesture (focusParam);
    endGesture (pitchParam);
    endGesture (sprayParam);

    repaint();
}

void GrainCloud::mouseDoubleClick (const juce::MouseEvent&)
{
    if (focusParam != nullptr)
    {
        focusParam->beginChangeGesture();
        focusParam->setValueNotifyingHost (focusParam->getDefaultValue());
        focusParam->endChangeGesture();
    }

    if (pitchParam != nullptr)
    {
        pitchParam->beginChangeGesture();
        pitchParam->setValueNotifyingHost (pitchParam->getDefaultValue());
        pitchParam->endChangeGesture();
    }

    if (sprayParam != nullptr)
    {
        sprayParam->beginChangeGesture();
        sprayParam->setValueNotifyingHost (sprayParam->getDefaultValue());
        sprayParam->endChangeGesture();
    }

    repaint();
}

void GrainCloud::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (densityParam == nullptr)
        return;

    auto delta = wheel.deltaY;
    if (std::abs (delta) < 0.0001f)
        delta = wheel.deltaX;

    if (std::abs (delta) < 0.0001f)
        return;

    densityParam->beginChangeGesture();
    setParamNormalised (densityParam, densityParam->getValue() + delta * 0.75f);
    densityParam->endChangeGesture();

    repaint();
}

void GrainCloud::mouseMove (const juce::MouseEvent& e)
{
    const auto insideField = fieldRect.contains (e.position);
    hoverPos = e.position;

    if (insideField != hoverField)
    {
        hoverField = insideField;
        setMouseCursor (insideField ? juce::MouseCursor::CrosshairCursor
                                    : juce::MouseCursor::NormalCursor);
    }

    if (insideField && ! dragging)
        repaint();
}

void GrainCloud::mouseExit (const juce::MouseEvent&)
{
    if (hoverField || dragging)
    {
        hoverField = false;
        repaint();
    }
}

bool GrainCloud::keyPressed (const juce::KeyPress& key)
{
    const auto fine = key.getModifiers().isShiftDown();

    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        nudgeParam (focusParam, key == juce::KeyPress::leftKey ? -0.02f : 0.02f);
        return true;
    }

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const auto direction = key == juce::KeyPress::upKey ? 1.0f : -1.0f;

        if (fine)
            nudgeParam (sprayParam, direction * 0.02f);
        else
            nudgeParam (pitchParam, direction * (0.5f / 48.0f));

        return true;
    }

    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=')
    {
        nudgeParam (densityParam, 0.02f);
        return true;
    }

    if (key.getTextCharacter() == '-' || key.getTextCharacter() == '_')
    {
        nudgeParam (densityParam, -0.02f);
        return true;
    }

    if (key.getTextCharacter() == ' ')
    {
        if (auto* freezeParam = apvts.getParameter (pid::freeze))
        {
            freezeParam->beginChangeGesture();
            freezeParam->setValueNotifyingHost (freezeParam->getValue() > 0.5f ? 0.0f : 1.0f);
            freezeParam->endChangeGesture();
        }
        return true;
    }

    return false;
}

} // namespace nagloed::ui
