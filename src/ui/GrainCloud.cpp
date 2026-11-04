#include "GrainCloud.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace nagloed::ui
{

using namespace theme;

namespace
{
constexpr int   kMaxMotes           = 460;
constexpr int   kLiveFadeShift      = 5;      // 1/32 per frame
constexpr int   kPrewarmFadeShift   = 5;
constexpr float kMaxPhosphorPixels  = 4.5e6f;
constexpr float kChromeMaxPixels    = 5.0e6f;

juce::String percentText (float normalised)
{
    return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, normalised) * 100.0f)) + "%";
}

float sanitise (float value, float fallback = 0.0f)
{
    return std::isfinite (value) ? value : fallback;
}

// A small radial falloff sprite drawn with its alpha channel as a mask. One
// scaled blit gives a perfectly smooth glow without gradients or banding.
const juce::Image& glowSprite()
{
    static const juce::Image sprite = []
    {
        constexpr int size = 128;
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
        const auto half = (float) size * 0.5f;

        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const auto dx = ((float) x + 0.5f - half) / half;
                const auto dy = ((float) y + 0.5f - half) / half;
                const auto d2 = juce::jmin (1.0f, dx * dx + dy * dy);
                const auto falloff = (1.0f - d2) * (1.0f - d2);
                auto* p = data.getPixelPointer (x, y);
                p[0] = 255; p[1] = 255; p[2] = 255;
                p[3] = (juce::uint8) juce::roundToInt (falloff * 255.0f);
            }
        }

        return image;
    }();

    return sprite;
}
} // namespace

// =============================================================================
// PhosphorField: additive ember buffer with an in-place, bounded decay.
// =============================================================================
bool GrainCloud::PhosphorField::ensure (int logicalWidth, int logicalHeight, float scale)
{
    if (logicalWidth <= 0 || logicalHeight <= 0)
        return false;

    scale = juce::jlimit (0.5f, 3.0f, scale);

    if (buffer.isValid() && logicalWidth == logicalW && logicalHeight == logicalH
        && std::abs (scale - pixelScale) < 0.02f)
        return false;

    logicalW = logicalWidth;
    logicalH = logicalHeight;
    pixelScale = scale;

    const auto physicalW = juce::jmax (1, juce::roundToInt ((float) logicalWidth * scale));
    const auto physicalH = juce::jmax (1, juce::roundToInt ((float) logicalHeight * scale));

    buffer = juce::Image (juce::Image::ARGB, physicalW, physicalH, true);
    buffer.clear (buffer.getBounds(), juce::Colour (0xff05070a));   // brand phosphor base

    pixels = std::make_unique<juce::Image::BitmapData> (buffer, juce::Image::BitmapData::writeOnly);
    active = false;
    alphaBroken = false;
    return true;
}

void GrainCloud::PhosphorField::clear()
{
    if (! buffer.isValid())
        return;

    buffer.clear (buffer.getBounds(), juce::Colour (0xff05070a));
    active = false;
    alphaBroken = false;
}

void GrainCloud::PhosphorField::fade (int shift)
{
    if (! pixels || ! active)
        return;

    const auto step = (juce::uint32) juce::jlimit (2, 6, shift);
    // Keep only the bits that came from the same colour byte, and mask OUT the
    // alpha byte: the phosphor image must stay fully opaque. Decaying alpha
    // breaks JUCE's premultiplied ARGB compositing, which turns faded embers
    // into additive white ghosts (the "black/white stuttering field" bug).
    const auto mask = (juce::uint32) ((((1u << (8u - step)) - 1u) * 0x01010101u) & 0x00FFFFFFu);

    auto* data = pixels.get();
    if (data->pixelStride != 4)
        return;

    const auto x0 = juce::jmax (0, minX);
    const auto x1 = juce::jmin (data->width - 1, maxX);
    const auto y0 = juce::jmax (0, minY);
    const auto y1 = juce::jmin (data->height - 1, maxY);

    for (int y = y0; y <= y1; ++y)
    {
        auto* row = reinterpret_cast<juce::uint32*> (data->getLinePointer (y));

        for (int x = x0; x <= x1; ++x)
        {
            const auto p = row[x];
            // Subtract the high bits of each colour byte (1/16 .. 1/64 decay),
            // leaving the alpha byte untouched. Two ops, no division.
            const auto delta = (p >> step) & mask;
            const auto decayed = p - delta;

            // The invariant must be checked BEFORE forcing alpha, otherwise the
            // QA flag could never trip (the bug this guards against decays alpha).
            if ((decayed >> 24) != 0xFFu)
                alphaBroken = true;

            row[x] = decayed | 0xFF000000u;   // keep the field opaque
        }
    }
}

void GrainCloud::PhosphorField::splat (float cx, float cy, float radius,
                                       juce::Colour colour, float energy)
{
    if (energy <= 0.0005f || ! pixels || radius <= 0.0f)
        return;

    auto* data = pixels.get();
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

    // Per-splat colour ceiling. Pixels hit repeatedly by a grain in consecutive
    // frames keep adding, so a loose ceiling lets red/green clip to 255 and the
    // ember turns into a hard pale disc. Cap close to the ember colour itself:
    // bright cores stay warm, never white.
    const auto capR = juce::jmin (255, juce::roundToInt ((float) colour.getRed()   * 1.15f + 10.0f));
    const auto capG = juce::jmin (255, juce::roundToInt ((float) colour.getGreen() * 1.15f + 10.0f));
    const auto capB = juce::jmin (255, juce::roundToInt ((float) colour.getBlue()  * 1.15f + 10.0f));

    for (int y = y0; y <= y1; ++y)
    {
        const auto dy = (float) y - py;
        const auto dy2 = dy * dy;
        auto* row = data->getLinePointer (y) + x0 * stride;

        for (int x = x0; x <= x1; ++x, row += stride)
        {
            const auto dx = (float) x - px;
            const auto d2 = dx * dx + dy2;

            if (d2 >= r2)
                continue;

            const auto falloff = 1.0f - d2 * invR2;
            const auto f = falloff * falloff;

            // JUCE ARGB stores pixels as blue, green, red, alpha; keep the
            // screen fully opaque so the ember colour is what glows.
            row[0] = (juce::uint8) juce::jmin (capB, (int) row[0] + (int) (cb * f));
            row[1] = (juce::uint8) juce::jmin (capG, (int) row[1] + (int) (cg * f));
            row[2] = (juce::uint8) juce::jmin (capR, (int) row[2] + (int) (cr * f));
            row[3] = 255;
        }
    }

    if (! active)
    {
        minX = x0; minY = y0; maxX = x1; maxY = y1;
        active = true;
    }
    else
    {
        minX = juce::jmin (minX, x0);
        minY = juce::jmin (minY, y0);
        maxX = juce::jmax (maxX, x1);
        maxY = juce::jmax (maxY, y1);
    }
}

// =============================================================================
// Construction
// =============================================================================
GrainCloud::GrainCloud (juce::AudioProcessorValueTreeState& state, AfterglowLookAndFeel& lookAndFeel)
    : apvts (state), lnf (lookAndFeel), controls (state, lookAndFeel),
      chrome (*this), fieldOverlay (*this)
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

    screen = std::make_unique<Screen> (*this);
    addAndMakeVisible (*screen);

    motes.reserve ((size_t) kMaxMotes);

    setUiScale (1.0f);
}

GrainCloud::~GrainCloud() = default;

void GrainCloud::collectParameterIds (juce::StringArray& ids) const
{
    controls.collectParameterIds (ids);
}

void GrainCloud::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.4f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f && uiScale > 0.0f)
        return;

    uiScale = newScale;
    controls.setUiScale (uiScale);

    const auto fs = fontScaleFor (uiScale);
    headerFont   = monoFont (7.0f * fs, juce::Font::plain, 0.10f);
    timelineFont = monoFont (7.0f * fs, juce::Font::plain, 0.06f);
    chipFont     = monoFont (7.2f * fs, juce::Font::plain, 0.16f);
    hudFont      = monoFont (7.6f * fs, juce::Font::plain, 0.08f);
    axisFont     = monoFont (6.9f * fs, juce::Font::plain, 0.06f);
    hintFont     = monoFont (6.8f * fs, juce::Font::plain, 0.06f);

    chrome.invalidate();
    fieldOverlay.invalidate();
    layoutAtScale();
    repaint();
}

void GrainCloud::resized()
{
    layoutAtScale();

    controls.setUiScale (uiScale);
    chrome.invalidate();
    fieldOverlay.invalidate();
    prewarmPending = true;

    if (scanRibbon != nullptr)
        scanRibbon->setBounds (scanRect.toNearestInt());
    if (reverseRibbon != nullptr)
        reverseRibbon->setBounds (reverseRect.toNearestInt());

    if (screen != nullptr)
        screen->setBounds (fieldRect.toNearestInt());

    // Prepare the persistence buffer now rather than in paint(). Offscreen
    // renders (preview/hero) warm fully here so the very first snapshot is
    // complete; a visible editor warms up incrementally in tick().
    ensureSimulationBuffers();

    if (! isShowing())
    {
        simulateWarmupFrames (warmupFramesLeft);
        warmupFramesLeft = 0;
    }
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
// Simulation
// =============================================================================
void GrainCloud::ensureSimulationBuffers()
{
    const auto fw = juce::jmax (1, (int) std::lround (fieldRect.getWidth()));
    const auto fh = juce::jmax (1, (int) std::lround (fieldRect.getHeight()));

    auto targetScale = deviceScale;

    if (targetScale <= 0.0f)
    {
        if (auto* peer = getPeer())
            targetScale = (float) peer->getPlatformScaleFactor();
        else
            targetScale = 2.0f;   // offscreen preview snapshot scale
    }

    const auto pixels = (float) fw * (float) fh;
    const auto maxPixelsScale = std::sqrt (kMaxPhosphorPixels / juce::jmax (1.0f, pixels));
    const auto scale = juce::jlimit (0.5f, 3.0f, juce::jmin (targetScale, maxPixelsScale));

    if (phosphor.ensure (fw, fh, scale))
    {
        warmupFramesLeft = 0;
        prewarmPending = true;
    }

    if (prewarmPending)
    {
        const auto physicalPixels = (float) phosphor.getImage().getWidth()
                                    * (float) phosphor.getImage().getHeight();
        const auto frames = juce::jlimit (32, 96,
                                          juce::roundToInt (96.0f * (1.2e6f / juce::jmax (1.0f, physicalPixels))));
        warmupFramesLeft = juce::jmax (warmupFramesLeft, frames);
        prewarmPending = false;
    }
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
            m.energy = 0.070f + rnd() * 0.050f;
        }
        else if (core)
        {
            m.vy = (rnd() - 0.5f) * 0.0006f;
            m.size = 2.2f + rnd() * 1.5f;
            m.energy = 0.100f + rnd() * 0.060f;
        }
        else if (bokeh)
        {
            m.vy = (rnd() - 0.5f) * 0.0007f;
            m.size = 5.0f + rnd() * 4.5f;
            m.energy = 0.008f + rnd() * 0.010f;
        }
        else
        {
            m.vy = (rnd() - 0.58f) * 0.0016f;
            m.size = 1.5f + rnd() * 1.8f;
            m.energy = 0.012f + rnd() * 0.012f;
        }

        m.warmth = juce::jlimit (0.0f, 1.0f, 0.15f + rnd() * 0.60f + paramReal (pitchParam) / 120.0f);
    }

    m.phase = rnd() * juce::MathConstants<float>::twoPi;
    m.phaseSpeed = 0.015f + rnd() * 0.05f;
    m.sway = 0.3f + rnd() * 1.4f;
    m.size *= 0.65f + 0.85f * sizeNorm;
}

int GrainCloud::targetMoteCount() const
{
    const auto densityNorm = juce::jlimit (0.0f, 1.0f, paramNormalised (densityParam));
    auto scaled = 120.0f + 280.0f * std::pow (densityNorm, 0.70f);

    if (phosphor.isReady())
    {
        const auto pixels = (float) phosphor.getImage().getWidth()
                            * (float) phosphor.getImage().getHeight();
        if (pixels > 2.5e6f)
            scaled *= 0.95f;
    }

    return (int) juce::jlimit (100.0f, (float) kMaxMotes, scaled);
}

void GrainCloud::drawDust (float timeScale, float energyScale)
{
    const auto target = targetMoteCount();

    if ((int) motes.size() < target)
    {
        while ((int) motes.size() < target)
        {
            Mote m;
            spawnMote (m, motes.size() < 52);
            motes.push_back (m);
        }
    }
    else if ((int) motes.size() > target + 40)
    {
        motes.resize ((size_t) target);
    }

    windPhase += 0.004f * timeScale;

    const auto fw = fieldRect.getWidth();
    const auto fh = fieldRect.getHeight();
    const auto energy = juce::jlimit (0.15f, 1.0f, energyScale);

    for (auto& m : motes)
    {
        advanceMote (m, timeScale);

        const auto px = m.x * fw;
        const auto py = m.y * fh;
        if (px < -12.0f || px > fw + 12.0f || py < -12.0f || py > fh + 12.0f)
            continue;

        const auto flicker = 0.72f + 0.28f * std::sin (m.phase * 1.9f + m.y * 7.0f);
        const auto e = m.energy * flicker * energy;
        const auto colour = emberRamp (0.22f + 0.60f * m.warmth);

        phosphor.splat (px, py, m.size * 2.8f, colour, e * 0.30f);
        phosphor.splat (px, py, m.size, colour, e * 0.72f);

        if (m.size > 1.0f)
        {
            phosphor.splat (px - m.vx * fw * 2.5f,
                            py - m.vy * fh * 2.5f,
                            m.size * 0.95f, colour, e * 0.34f);
        }

        if (e > 0.09f)
            phosphor.splat (px, py, m.size * 0.45f, gold, e * 0.30f);
    }

}

void GrainCloud::drawGrains()
{
    const auto fw = fieldRect.getWidth();
    const auto fh = fieldRect.getHeight();
    const auto sizeNorm = paramNormalised (sizeParam);
    const auto num = juce::jlimit (0, VisualState::kMaxGrainViews, visual.numGrains);

    for (int i = 0; i < num; ++i)
    {
        const auto& grain = visual.grains[(size_t) i];

        const auto pos = juce::jlimit (0.0f, 1.0f, sanitise (grain.pos, 0.5f));
        const auto pan = juce::jlimit (-1.0f, 1.0f, sanitise (grain.pan));
        const auto pitch = juce::jlimit (-24.0f, 24.0f, sanitise (grain.pitch));
        const auto env = juce::jlimit (0.0f, 1.0f, sanitise (grain.env));
        const auto brightness = juce::jlimit (0.0f, 1.0f, sanitise (grain.brightness, 0.5f));

        const auto x = pos * fw + pan * 6.0f * uiScale;
        const auto y = (1.0f - pitchToNorm (pitch)) * fh;
        const auto radius = (0.9f + 2.6f * env) * (0.75f + 0.6f * sizeNorm);
        const auto colour = pitchColour (pitch);
        const auto energy = 0.15f + 0.55f * env;

        phosphor.splat (x, y, radius * 2.3f, colour, energy * 0.20f);
        phosphor.splat (x, y, radius, colour, energy * 0.55f);
        phosphor.splat (x, y, radius * 0.55f, gold, energy * 0.28f);

        if (brightness > 0.35f)
            phosphor.splat (x, y, radius * 0.40f, gold,
                            energy * 0.18f * (brightness - 0.35f) / 0.65f);
    }
}

void GrainCloud::drawGhostGrains()
{
    // Only used for offscreen previews: gives the still frame a short trail
    // without pretending to know the engine's history.
    const auto fw = fieldRect.getWidth();
    const auto fh = fieldRect.getHeight();
    const auto num = juce::jlimit (0, 96, visual.numGrains);

    for (int i = 0; i < num; ++i)
    {
        const auto& grain = visual.grains[(size_t) i];
        const auto pos = juce::jlimit (0.0f, 1.0f, sanitise (grain.pos, 0.5f));
        const auto pan = juce::jlimit (-1.0f, 1.0f, sanitise (grain.pan));
        const auto pitch = juce::jlimit (-24.0f, 24.0f, sanitise (grain.pitch));
        const auto env = juce::jlimit (0.0f, 1.0f, sanitise (grain.env));
        const auto colour = pitchColour (pitch);

        for (int g = 0; g < 6; ++g)
        {
            const auto a = (float) (i * 7 + g * 13) * 1.7f;
            const auto offsetX = std::sin (a) * 5.0f * uiScale;
            const auto offsetY = std::cos (a * 1.3f) * 3.5f * uiScale;
            const auto e = (0.25f + 0.5f * env) * (1.0f - (float) g / 7.0f);

            phosphor.splat (pos * fw + pan * 6.0f * uiScale + offsetX,
                            (1.0f - pitchToNorm (pitch)) * fh + offsetY,
                            (1.4f + 2.2f * env) * uiScale, colour, e * 0.26f);
        }
    }
}

void GrainCloud::simulateWarmupFrames (int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        phosphor.fade (kPrewarmFadeShift);
        drawDust (1.0f, 1.0f * (1.0f - 0.4f * dustActivity));

        if (visual.numGrains > 0 && ! isShowing())
            drawGhostGrains();
    }
}

void GrainCloud::resetForDeterministicPreview()
{
    rng.seed (0x4e41474cu);
    motes.clear();
    phosphor.clear();
    warmupFramesLeft = 0;
    smoothedInit = false;
    dustActivity = 0.0f;
    windPhase = 0.0f;
    shownFocus = 0.5f;
    shownSpray = 0.25f;
    shownPitch = 0.0f;
    shownBufferFill = 0.0f;
    headerInfoText.clear();
    timelineInfoText.clear();
    repaint();
}

void GrainCloud::tick (const VisualState& snapshot, double elapsedSeconds)
{
    visual = snapshot;

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

    const auto targetActivity = juce::jlimit (0.0f, 1.0f, (float) visual.numGrains / 40.0f);
    dustActivity += (targetActivity - dustActivity) * 0.08f;

    ensureSimulationBuffers();

    if (warmupFramesLeft > 0)
    {
        if (isShowing())
        {
            const auto chunk = juce::jmin (8, warmupFramesLeft);
            simulateWarmupFrames (chunk);
            warmupFramesLeft -= chunk;
        }
        else
        {
            // Offscreen render (preview/hero): warm fully so the still frame is
            // complete before the first snapshot.
            simulateWarmupFrames (warmupFramesLeft);
            warmupFramesLeft = 0;
        }
    }

    if (phosphor.isReady())
    {
        const auto timeScale = juce::jlimit (0.4f, 2.5f, (float) (elapsedSeconds * 60.0));

        phosphor.fade (kLiveFadeShift);
        drawDust (timeScale, 1.0f - 0.55f * dustActivity);

        if (visual.numGrains > 0)
            drawGrains();
    }

    if (screen != nullptr)
        screen->repaint();

    if (readoutCounter++ % 6 == 0)
    {
        const auto oldHeader = headerInfoText;
        const auto oldTimeline = timelineInfoText;
        updateReadouts();

        if (headerInfoText != oldHeader)
            repaint (headerRect.toNearestInt().expanded (2));
        if (timelineInfoText != oldTimeline)
            repaint (timelineRect.toNearestInt().expanded (2));
    }

}

void GrainCloud::updateReadouts()
{
    focusText = "FOCUS " + percentText (paramNormalised (focusParam));
    sprayText = "SPRAY " + percentText (paramNormalised (sprayParam));

    if (densityParam != nullptr)
        densityText = densityParam->getText (densityParam->getValue(), 24).toUpperCase();
    else
        densityText = utf8 ("—");

    bufferText = "BUFFER " + percentText (visual.bufferFill);

    headerInfoText = densityText + utf8 ("   ·   ")
                     + juce::String (visual.numGrains)
                     + (visual.numGrains == 1 ? " GRAIN" : " GRAINS");
    timelineInfoText = bufferText + "    " + focusText + "    " + sprayText;
}

// =============================================================================
// Painting: parent (chrome + timeline readouts) and the 60 Hz screen child.
// =============================================================================
void GrainCloud::ChromeCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderChrome (g, bounds);
}

void GrainCloud::FieldOverlayCache::render (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    owner.renderFieldOverlay (g, bounds);
}

void GrainCloud::Screen::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    if (! owner.phosphor.isReady())
    {
        g.fillAll (juce::Colour (0xff05070a));
        return;
    }

    // Draw the phosphor at its native pixel density: the local transform maps
    // image pixels to logical pixels so the outer device transform finishes the
    // job 1:1, avoiding CoreGraphics resampling of a multi-megapixel image.
    const auto imageScale = juce::jmax (0.5f, owner.phosphor.getScale());
    const auto drawIdentity = [&] (const juce::Image& image)
    {
        if (! image.isValid())
            return;

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f / imageScale));
        g.drawImageAt (image, 0, 0);
        g.restoreState();
    };

    drawIdentity (owner.phosphor.getImage());


    owner.fieldOverlay.ensure (juce::roundToInt (b.getWidth()),
                               juce::roundToInt (b.getHeight()),
                               imageScale, kMaxPhosphorPixels);
    drawIdentity (owner.fieldOverlay.getImage());


    owner.drawScreenForeground (g, b);

}

void GrainCloud::renderFieldOverlay (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;
    const auto fw = bounds.getWidth();
    const auto fh = bounds.getHeight();

    // Engraved grid.
    for (int i = 1; i < 4; ++i)
    {
        const auto x = fw * (float) i * 0.25f;
        g.setColour (line.withAlpha (i == 2 ? 0.16f : 0.09f));
        g.fillRect (x - 0.25f * s, 0.0f, 0.5f * s, fh);
    }

    for (int pitch = -24; pitch <= 24; pitch += 12)
    {
        const auto y = (1.0f - pitchToNorm ((float) pitch)) * fh;
        g.setColour (line.withAlpha (pitch == 0 ? 0.18f : 0.10f));
        g.fillRect (0.0f, y - 0.25f * s, fw, 0.5f * s);
    }

    // Corner brackets.
    {
        g.setColour (line.withAlpha (0.95f));
        const auto len = 9.0f * s;
        const auto pen = 0.8f * s;
        g.fillRect (4.0f * s, 4.0f * s, len, pen);
        g.fillRect (4.0f * s, 4.0f * s, pen, len);
        g.fillRect (fw - 4.0f * s - len, 4.0f * s, len, pen);
        g.fillRect (fw - 4.0f * s - pen, 4.0f * s, pen, len);
        g.fillRect (4.0f * s, fh - 4.0f * s - pen, len, pen);
        g.fillRect (4.0f * s, fh - 4.0f * s - len, pen, len);
        g.fillRect (fw - 4.0f * s - len, fh - 4.0f * s - pen, len, pen);
        g.fillRect (fw - 4.0f * s - pen, fh - 4.0f * s - len, pen, len);
    }

    // Edge fade so embers dissolve instead of being clipped.
    {
        constexpr int bands = 6;
        const auto fadeTop = fh * 0.15f;
        const auto fadeSide = fw * 0.09f;
        const auto bandH = fadeTop / (float) bands;
        const auto bandW = fadeSide / (float) bands;

        for (int i = 0; i < bands; ++i)
        {
            const auto t = (float) i / (float) bands;
            const auto alpha = 0.34f * (1.0f - t);
            g.setColour (juce::Colours::black.withAlpha (alpha));
            g.fillRect (0.0f, t * fadeTop, fw, bandH + 1.0f);
            g.fillRect (0.0f, fh - (t + 1.0f) * bandH, fw, bandH + 1.0f);
            g.fillRect (t * fadeSide, 0.0f, bandW + 1.0f, fh);
            g.fillRect (fw - (t + 1.0f) * bandW, 0.0f, bandW + 1.0f, fh);
        }
    }
}

void GrainCloud::renderChrome (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto s = uiScale;

    drawPanelPlate (g, bounds.reduced (0.5f * s), s, 5.0f * s);
    drawHeader (g, headerRect, s, "02", utf8 ("SINTEL · GRAIN FIELD"));

    // Screen well (the Screen child paints the phosphor itself).
    {
        const auto fr = fieldRect;
        juce::ColourGradient well (wellTop, fr.getX(), fr.getY(),
                                   wellBottom, fr.getX(), fr.getBottom(), false);
        g.setGradientFill (well);
        g.fillRoundedRectangle (fr, 2.0f * s);

        g.setColour (gold.withAlpha (0.25f));
        g.drawRoundedRectangle (fr, 2.0f * s, 0.9f * s);
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
                           axisFont, dim.withAlpha (0.95f), juce::Justification::centredRight);

            g.setColour (line.withAlpha (0.9f));
            g.fillRect (fieldRect.getX() - 3.0f * s, y - 0.5f * s, 3.0f * s, 0.8f * s);
        }
    }

    // Position ticks under the screen.
    {
        for (int i = 0; i <= 4; ++i)
        {
            const auto t = (float) i / 4.0f;
            const auto x = fieldRect.getX() + t * fieldRect.getWidth();

            g.setColour (line.withAlpha (0.9f));
            g.fillRect (x - 0.4f * s, fieldRect.getBottom(), 0.8f * s, 3.0f * s);

            drawMicroText (g, juce::String (t, 2),
                           { x - 12.0f * s, bottomAxisRect.getY() + 1.0f * s,
                             24.0f * s, bottomAxisRect.getHeight() },
                           axisFont,
                           dim.withAlpha (0.9f), juce::Justification::centred);
        }
    }

    // Capture-buffer timeline chrome.
    {
        drawMicroText (g, "CAPTURE BUFFER",
                       { timelineRect.getX(), timelineRect.getY(),
                         timelineRect.getWidth() * 0.5f, timelineLabelH * s },
                       hintFont, muted.withAlpha (0.85f));

        const auto tr = trackRect;
        g.setColour (juce::Colour (0xff070a0e));
        g.fillRoundedRectangle (tr, tr.getHeight() * 0.5f);
        g.setColour (lineSoft.withAlpha (0.95f));
        g.drawRoundedRectangle (tr, tr.getHeight() * 0.5f, 1.0f * s);

        for (int i = 0; i <= 10; ++i)
        {
            const auto x = tr.getX() + tr.getWidth() * (float) i / 10.0f;
            g.setColour (line.withAlpha (i % 5 == 0 ? 0.65f : 0.3f));
            g.fillRect (x - 0.3f * s, tr.getY() + tr.getHeight() * 0.5f - 2.0f * s,
                        0.6f * s, 4.0f * s);
        }

        drawMicroText (g, utf8 ("DRAG X FOCUS · Y PITCH · SHIFT SPRAY · WHEEL DENSITY · DOUBLE-CLICK RE-CENTRE"),
                       { hintRect.getX(), hintRect.getY(), hintRect.getWidth(), hintRect.getHeight() },
                       hintFont, dim.withAlpha (0.95f));
    }
}

void GrainCloud::drawScreenForeground (juce::Graphics& g, juce::Rectangle<float> b)
{
    const auto s = uiScale;
    const auto fw = b.getWidth();
    const auto fh = b.getHeight();
    const auto fx = juce::jlimit (0.0f, 1.0f, shownFocus) * fw;
    const auto fy = (1.0f - pitchToNorm (shownPitch)) * fh;

    // Spray halo and focus lens: smooth radial sprites, one blit each.
    {
        const auto& sprite = glowSprite();
        const auto rx = (0.10f + shownSpray * 0.55f) * fw;
        const auto ry = (0.08f + shownSpray * 0.34f) * fh;
        const auto lens = (0.06f + 0.05f * shownSpray) * fh;

        g.setColour (ember.withAlpha (0.20f + 0.25f * shownSpray));
        g.drawImage (sprite, juce::Rectangle<float> (fx - rx, fy - ry, rx * 2.0f, ry * 2.0f),
                     juce::RectanglePlacement::stretchToFit, true);

        g.setColour (gold.withAlpha (0.16f + 0.18f * shownSpray));
        g.drawImage (sprite, juce::Rectangle<float> (fx - lens, fy - lens,
                                                     lens * 2.0f, lens * 2.0f),
                     juce::RectanglePlacement::stretchToFit, true);
    }

    // Focus rule.
    {
        g.setColour (ember.withAlpha (0.10f));
        g.fillRect (fx - 2.0f * s, 0.0f, 4.0f * s, fh);
        g.setColour (gold.withAlpha (0.50f));
        g.fillRect (fx - 0.5f * s, 0.0f, 1.0f * s, fh);
    }

    // Pitch rule, dashed and engraved.
    {
        g.setColour (gold.withAlpha (0.30f));
        const auto dash = 5.0f * s;
        const auto gap = 9.0f * s;
        for (float x = 0.0f; x < fw; x += dash + gap)
            g.fillRect (x, fy - 0.4f * s, dash, 0.8f * s);
    }

    // Focus marker and crosshair node.
    {
        g.setColour (paleEmber.withAlpha (0.9f));
        g.fillEllipse (fx - 2.6f * s, 3.0f * s, 5.2f * s, 3.0f * s);
        g.setColour (ember.withAlpha (0.30f));
        g.drawEllipse (fx - 4.5f * s, fy - 4.5f * s, 9.0f * s, 9.0f * s, 0.9f * s);
        g.setColour (ember);
        g.fillEllipse (fx - 1.8f * s, fy - 1.8f * s, 3.6f * s, 3.6f * s);
        g.setColour (paleEmber.withAlpha (0.9f));
        g.fillEllipse (fx - 0.7f * s, fy - 0.7f * s, 1.4f * s, 1.4f * s);
    }

    // Mouse crosshair.
    if (hoverField && ! dragging)
    {
        const auto mx = hoverPos.x - fieldRect.getX();
        const auto my = hoverPos.y - fieldRect.getY();
        const auto len = 5.0f * s;
        const auto gap = 2.0f * s;

        g.setColour (muted.withAlpha (0.45f));
        g.fillRect (mx - len - gap, my - 0.4f * s, len, 0.8f * s);
        g.fillRect (mx + gap, my - 0.4f * s, len, 0.8f * s);
        g.fillRect (mx - 0.4f * s, my - len - gap, 0.8f * s, len);
        g.fillRect (mx - 0.4f * s, my + gap, 0.8f * s, len);
        g.drawEllipse (mx - 3.0f * s, my - 3.0f * s, 6.0f * s, 6.0f * s, 0.7f * s);
    }

    // Drag HUD.
    if (dragging && dragHud.isNotEmpty())
    {
        const auto width = textWidth (hudFont, dragHud) + 14.0f * s;
        const juce::Rectangle<float> hud (fw - width - 8.0f * s, 7.0f * s,
                                          width, 16.0f * s);

        g.setColour (juce::Colour (0xff080b10).withAlpha (0.85f));
        g.fillRoundedRectangle (hud, 2.0f * s);
        g.setColour (gold.withAlpha (0.55f));
        g.drawRoundedRectangle (hud, 2.0f * s, 0.8f * s);
        g.setFont (hudFont);
        g.setColour (paleEmber);
        g.drawText (dragHud, hud, juce::Justification::centred, false);
    }
}

void GrainCloud::drawTimeline (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto tr = trackRect;
    const auto focusX = tr.getX() + juce::jlimit (0.0f, 1.0f, shownFocus) * tr.getWidth();

    // Spray band on the buffer.
    {
        const auto half = (0.015f + shownSpray * 0.24f) * tr.getWidth();
        juce::ColourGradient band (ember.withAlpha (0.0f), focusX - half, tr.getCentreY(),
                                   ember.withAlpha (0.0f), focusX + half, tr.getCentreY(), false);
        band.addColour (0.5, ember.withAlpha (0.08f));
        g.setGradientFill (band);
        g.fillRect (juce::Rectangle<float> (focusX - half, tr.getY() + 1.0f * s,
                                            half * 2.0f, tr.getHeight() - 2.0f * s));
    }

    // Buffer fill.
    const auto fill = juce::jlimit (0.0f, 1.0f, shownBufferFill);
    if (fill > 0.002f)
    {
        const auto fillW = tr.getWidth() * fill;
        juce::ColourGradient fillGrad (emberDeep.withAlpha (0.55f), tr.getX(), tr.getY(),
                                       ember.withAlpha (0.85f), tr.getX() + fillW, tr.getY(), false);
        g.setGradientFill (fillGrad);
        g.fillRoundedRectangle ({ tr.getX(), tr.getY(), fillW, tr.getHeight() },
                                tr.getHeight() * 0.5f);

        const auto edgeX = tr.getX() + fillW;
        g.setColour (paleEmber.withAlpha (0.12f));
        g.fillRect (edgeX - 3.0f * s, tr.getY(), 6.0f * s, tr.getHeight());
        g.setColour (paleEmber.withAlpha (0.75f));
        g.fillRect (edgeX - 0.5f * s, tr.getY() - 2.0f * s, 1.0f * s, tr.getHeight() + 4.0f * s);
    }

    // Focus marker.
    {
        g.setColour (ember.withAlpha (0.24f));
        g.fillRect (focusX - 2.5f * s, tr.getY() - 3.0f * s, 5.0f * s, tr.getHeight() + 6.0f * s);
        g.setColour (paleEmber.withAlpha (0.9f));
        g.fillRect (focusX - 0.7f * s, tr.getY() - 3.0f * s, 1.4f * s, tr.getHeight() + 6.0f * s);
        g.setColour (paleEmber);
        g.fillEllipse (focusX - 2.0f * s, tr.getY() - 8.0f * s, 4.0f * s, 3.0f * s);
    }

    // Scan direction, kept inside the component.
    {
        const auto scan = juce::jlimit (-1.0f, 1.0f, paramReal (scanParam));
        const auto strength = juce::jmax (0.25f, std::abs (scan));
        const auto positive = scan >= -0.01f;

        auto drawArrow = [&] (float tipX, float dir, float alpha)
        {
            const auto c = juce::Point<float> (tipX, tr.getCentreY());
            const auto r = 3.2f * s;
            const juce::Point<float> a (c.x + dir * r * 0.9f, c.y - r);
            const juce::Point<float> m (c.x - dir * r * 0.9f, c.y);
            const juce::Point<float> z (c.x + dir * r * 0.9f, c.y + r);
            g.setColour (gold.withAlpha (alpha));
            g.drawLine ({ a, m }, 1.1f * s);
            g.drawLine ({ m, z }, 1.1f * s);
        };

        if (std::abs (scan) > 0.02f)
        {
            drawArrow (positive ? tr.getRight() - 5.0f * s : tr.getX() + 5.0f * s,
                       positive ? 1.0f : -1.0f, 0.45f + 0.45f * strength);

            g.setFont (hintFont);
            g.setColour (muted.withAlpha (0.9f));
            g.drawText (positive ? utf8 ("SCAN →") : utf8 ("SCAN ←"),
                        juce::Rectangle<float> (positive ? tr.getRight() - 62.0f * s
                                                         : tr.getX() + 14.0f * s,
                                                tr.getY(), 48.0f * s, tr.getHeight()),
                        juce::Justification::centredRight, false);
        }
        else
        {
            drawArrow (tr.getRight() - 5.0f * s, 1.0f, 0.25f);
            drawArrow (tr.getX() + 5.0f * s, -1.0f, 0.25f);
        }
    }
}

void GrainCloud::drawReadouts (juce::Graphics& g)
{
    const auto s = uiScale;

    // Header right: mode chip + density + grain count.
    {
        const auto live = visual.numGrains > 0 || visual.grainRate > 0.05f;
        const auto chipArea = headerRect.withTrimmedLeft (headerRect.getWidth() - 58.0f * s)
                                         .withTrimmedRight (4.0f * s)
                                         .reduced (0.0f, 4.0f * s);

        const auto dotColour = live ? gold : muted;
        g.setColour (dotColour.withAlpha (live ? 0.9f : 0.55f));
        g.fillEllipse (chipArea.getX(), chipArea.getCentreY() - 1.3f * s,
                       2.6f * s, 2.6f * s);

        drawMicroText (g, live ? "LIVE" : "IDLE",
                       chipArea.withTrimmedLeft (8.0f * s),
                       chipFont,
                       live ? gold.withAlpha (0.95f) : muted.withAlpha (0.9f));

        const auto infoWidth = 210.0f * s;
        drawMicroText (g, headerInfoText,
                       headerRect.withTrimmedLeft (headerRect.getWidth() - infoWidth - 64.0f * s)
                                 .withWidth (infoWidth)
                                 .withTrimmedRight (4.0f * s),
                       headerFont,
                       dim.withAlpha (0.95f), juce::Justification::centredRight);
    }

    // Timeline label row right: buffer / focus / spray.
    {
        const auto area = juce::Rectangle<float> (timelineRect.getX() + timelineRect.getWidth() * 0.28f,
                                                  timelineRect.getY(),
                                                  timelineRect.getWidth() * 0.72f,
                                                  timelineLabelH * s);
        drawMicroText (g, timelineInfoText, area, timelineFont,
                       muted.withAlpha (0.9f), juce::Justification::centredRight);
    }
}

// =============================================================================
void GrainCloud::ensureChromeCache (float scale)
{
    chrome.ensure (getWidth(), getHeight(),
                   juce::jlimit (1.0f, 2.0f, scale),
                   kChromeMaxPixels);

    if (screen != nullptr)
    {
        const auto b = screen->getBounds();
        fieldOverlay.ensure (b.getWidth(), b.getHeight(),
                             juce::jlimit (1.0f, 3.0f, scale),
                             kMaxPhosphorPixels);
    }
}

void GrainCloud::paint (juce::Graphics& g)
{

    if (uiScale <= 0.0f)
        layoutAtScale();

    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    deviceScale = juce::jlimit (0.5f, 3.0f, scale);
    ensureChromeCache (deviceScale);

    // The opaque Screen child covers the field; skip the parent work entirely
    // for dirty regions that it will paint over.
    const auto clip = g.getClipBounds();
    if (screen != nullptr && screen->getBounds().contains (clip))
        return;

    drawCached (g, chrome.getImage(), getLocalBounds());
    drawTimeline (g);
    drawReadouts (g);

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

    if (screen != nullptr)
        screen->repaint();
    repaint (timelineRect.toNearestInt());
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

    repaint (timelineRect.toNearestInt());
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

    if (screen != nullptr)
        screen->repaint();
    repaint (timelineRect.toNearestInt());
}

void GrainCloud::mouseDoubleClick (const juce::MouseEvent&)
{
    for (auto* param : { focusParam, pitchParam, sprayParam })
    {
        if (param == nullptr)
            continue;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getDefaultValue());
        param->endChangeGesture();
    }

    if (screen != nullptr)
        screen->repaint();
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

    if (insideField && ! dragging && screen != nullptr)
        screen->repaint();
}

void GrainCloud::mouseExit (const juce::MouseEvent&)
{
    if (hoverField || dragging)
    {
        hoverField = false;

        if (screen != nullptr)
            screen->repaint();
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
