#include "Theme.h"

#include <array>

namespace nagloed::ui::theme
{

// -----------------------------------------------------------------------------
// Families. Resolve once, preferring the faces used on the design sketch, with
// sane fallbacks for machines that don't have them.
// -----------------------------------------------------------------------------
namespace
{
juce::String chooseFamily (std::initializer_list<const char*> candidates,
                           const juce::String& fallback)
{
    static const auto installed = juce::Font::findAllTypefaceNames();

    for (auto* candidate : candidates)
    {
        const juce::String name (candidate);
        if (installed.contains (name))
            return name;
    }

    // Try a forgiving prefix match (e.g. "Avenir Next Condensed Demi Bold").
    for (auto* candidate : candidates)
    {
        const juce::String name (candidate);
        for (const auto& available : installed)
            if (available.startsWithIgnoreCase (name))
                return available;
    }

    return fallback;
}

std::vector<juce::String> parentFallbacks (const juce::String& family)
{
    // FontOptions fallbacks kick in for missing glyphs; the family list here is
    // deliberately short and safe.
    std::vector<juce::String> list;
    if (family != juce::Font::getDefaultSansSerifFontName())
        list.push_back (juce::Font::getDefaultSansSerifFontName());
    return list;
}
} // namespace

const juce::String& displayFamily()
{
    static const juce::String name = chooseFamily (
        { "Avenir Next Condensed", "Avenir Next", "Helvetica Neue", "Helvetica" },
        juce::Font::getDefaultSansSerifFontName());
    return name;
}

const juce::String& bodyFamily()
{
    static const juce::String name = chooseFamily (
        { "Avenir Next", "Helvetica Neue", "Helvetica" },
        juce::Font::getDefaultSansSerifFontName());
    return name;
}

const juce::String& monoFamily()
{
    static const juce::String name = chooseFamily (
        { "Menlo", "SF Mono", "Monaco", "Courier New" },
        juce::Font::getDefaultMonospacedFontName());
    return name;
}

namespace
{
juce::Font makeFont (const juce::String& family, float height, int style, float kerning)
{
    auto options = juce::FontOptions()
                       .withName (family)
                       .withFallbacks (parentFallbacks (family))
                       .withFallbackEnabled (true)
                       .withKerningFactor (kerning)
                       .withHeight (juce::jmax (1.0f, height));

    if ((style & juce::Font::bold) != 0)
        options = options.withStyleFlags (juce::Font::bold);
    if ((style & juce::Font::italic) != 0)
        options = options.withStyleFlags (juce::Font::italic);

    return juce::Font (options);
}
} // namespace

juce::Font displayFont (float height, int style, float kerning)
{
    return makeFont (displayFamily(), height, style, kerning);
}

juce::Font bodyFont (float height, int style, float kerning)
{
    return makeFont (bodyFamily(), height, style, kerning);
}

juce::Font monoFont (float height, int style, float kerning)
{
    return makeFont (monoFamily(), height, style, kerning);
}

float textWidth (const juce::Font& font, const juce::String& textToMeasure)
{
    return juce::GlyphArrangement::getStringWidth (font, textToMeasure);
}

// -----------------------------------------------------------------------------
float scaleFor (juce::Rectangle<int> bounds) noexcept
{
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return 1.0f;

    const auto sx = (float) bounds.getWidth() / designWidth;
    const auto sy = (float) bounds.getHeight() / designHeight;
    return juce::jlimit (minUiScale, maxUiScale, juce::jmin (sx, sy));
}

float fontScaleFor (float uiScale) noexcept
{
    // Never shrink type below its design size: at 960x560 on a 1x screen the
    // micro-labels stay legible instead of melting into the plate.
    return juce::jlimit (1.0f, 2.15f, uiScale);
}

// -----------------------------------------------------------------------------
void CachedBackground::ensure (int logicalWidth, int logicalHeight, float scale,
                               float maxPixels)
{
    if (logicalWidth <= 0 || logicalHeight <= 0)
        return;

    scale = juce::jmax (0.5f, scale);

    if (maxPixels > 0.0f)
    {
        const auto logicalPixels = (float) logicalWidth * (float) logicalHeight;
        if (logicalPixels * scale * scale > maxPixels)
            scale = juce::jmax (0.5f, std::sqrt (maxPixels / juce::jmax (1.0f, logicalPixels)));
    }

    if (logicalWidth == logicalW && logicalHeight == logicalH
        && std::abs (scale - renderedScale) < 0.01f && image.isValid())
        return;

    logicalW = logicalWidth;
    logicalH = logicalHeight;
    renderedScale = scale;

    const auto physicalW = juce::jmax (1, juce::roundToInt ((float) logicalWidth * scale));
    const auto physicalH = juce::jmax (1, juce::roundToInt ((float) logicalHeight * scale));

    image = juce::Image (juce::Image::ARGB, physicalW, physicalH, true);
    image.clear (image.getBounds());

    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale ((float) physicalW / (float) logicalWidth,
                                                  (float) physicalH / (float) logicalHeight));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    render (g, juce::Rectangle<float> (0.0f, 0.0f,
                                       (float) logicalWidth, (float) logicalHeight));
}

void CachedBackground::invalidate()
{
    image = juce::Image();
    logicalW = 0;
    logicalH = 0;
    renderedScale = 0.0f;
}

void drawCached (juce::Graphics& g, const juce::Image& image, juce::Rectangle<int> bounds)
{
    if (image.isValid() && ! bounds.isEmpty())
        g.drawImage (image, bounds.toFloat());
}

// -----------------------------------------------------------------------------
const juce::Image& noiseTile()
{
    static const juce::Image tile = []
    {
        constexpr int size = 128;
        juce::Image result (juce::Image::ARGB, size, size, true);
        juce::Random random (0x4e41474c);

        juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                data.setPixelColour (x, y,
                                     juce::Colours::white.withAlpha (random.nextFloat() * 0.55f));

        return result;
    }();

    return tile;
}

void drawAtmosphere (juce::Graphics& g, juce::Rectangle<float> r, float scale, float strength)
{
    if (r.isEmpty() || strength <= 0.0f)
        return;

    const auto s = juce::jmax (0.5f, scale);

    // Very quiet grain. Tiling a cached tile is far cheaper than per-pixel work.
    {
        static constexpr int tileSize = 128;
        g.setOpacity (0.030f * strength);
        for (float y = r.getY(); y < r.getBottom(); y += tileSize)
            for (float x = r.getX(); x < r.getRight(); x += tileSize)
                g.drawImageAt (noiseTile(), juce::roundToInt (x), juce::roundToInt (y));
        g.setOpacity (1.0f);
    }

    // Scanlines: one dark hairline every 2.5 logical pixels.
    {
        g.setColour (juce::Colours::black.withAlpha (0.055f * strength));
        const auto step = juce::jmax (2.0f, 2.5f * s);
        for (float y = r.getY(); y < r.getBottom(); y += step)
            g.fillRect (r.getX(), y, r.getWidth(), juce::jmax (0.6f, 0.8f * s));
    }

    // Warm bloom from the top-left lamp.
    {
        const auto cx = r.getX() + r.getWidth() * 0.18f;
        const auto cy = r.getY() - r.getHeight() * 0.10f;
        const auto radius = r.getWidth() * 0.85f;
        juce::ColourGradient bloom (ember.withAlpha (0.028f * strength), cx, cy,
                                    ember.withAlpha (0.0f), cx, cy + radius, true);
        g.setGradientFill (bloom);
        g.fillRect (r);
    }

    // Vignette.
    {
        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY() * 0.92f;
        juce::ColourGradient vignette (juce::Colours::transparentBlack, cx, cy,
                                       juce::Colours::black.withAlpha (0.42f * strength),
                                       cx, cy + r.getHeight() * 0.78f, true);
        g.setGradientFill (vignette);
        g.fillRect (r);
    }
}

void drawPanelPlate (juce::Graphics& g, juce::Rectangle<float> r, float scale,
                     float radius)
{
    if (r.isEmpty())
        return;

    const auto s = juce::jmax (0.5f, scale);
    auto rr = r.reduced (0.5f * s);

    // Soft drop shadow so the plate sits above the page.
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRoundedRectangle (rr.translated (0.0f, 2.0f * s), radius);

    juce::ColourGradient grad (plateTop, rr.getX(), rr.getY(),
                               plateBottom, rr.getX(), rr.getBottom(), false);
    grad.addColour (0.35, plateTop.interpolatedWith (plateBottom, 0.25f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (rr, radius);

    // Top bevel: a whisper of cold light on the upper edge.
    g.setColour (paleEmber.withAlpha (0.055f));
    g.fillRect (rr.getX() + 7.0f * s, rr.getY() + 1.0f * s,
                rr.getWidth() - 14.0f * s, juce::jmax (0.8f, 0.8f * s));

    g.setColour (panelEdge.withAlpha (0.9f));
    g.drawRoundedRectangle (rr, radius, juce::jmax (0.9f, 0.85f * s));

    // Engraved corner ticks.
    const auto tick = 7.0f * s;
    const auto inset = 5.0f * s;
    const auto pen = juce::jmax (0.9f, 0.9f * s);
    g.setColour (panelEdge.withAlpha (0.75f));

    const auto tl = rr.getTopLeft() + juce::Point<float> (inset, inset);
    g.fillRect (tl.x, tl.y, tick, pen);
    g.fillRect (tl.x, tl.y, pen, tick);

    const auto br = rr.getBottomRight() - juce::Point<float> (inset, inset);
    g.fillRect (br.x - tick, br.y - pen, tick, pen);
    g.fillRect (br.x - pen, br.y - tick, pen, tick);
}

void drawHeader (juce::Graphics& g, juce::Rectangle<float> r, float scale,
                 const juce::String& index, const juce::String& title,
                 const juce::String& right, Colour accent)
{
    const auto s = juce::jmax (0.5f, scale);
    const auto fs = fontScaleFor (scale);
    auto area = r;

    if (index.isNotEmpty())
    {
        drawMicroText (g, index, area.removeFromLeft (16.0f * s), monoFont (8.6f * fs, juce::Font::plain, 0.13f),
                       muted.withAlpha (0.95f));
    }

    // Tiny ember tick after the index.
    g.setColour (ember.withAlpha (0.55f));
    g.fillRect (area.getX() - 2.0f * s, area.getCentreY() - 1.0f * s,
                6.0f * s, juce::jmax (0.8f, 1.0f * s));

    area.removeFromLeft (8.0f * s);

    const auto titleFont = monoFont (9.4f * fs, juce::Font::plain, 0.16f);
    const auto titleWidth = juce::jmin (area.getWidth() - 6.0f * s,
                                        textWidth (titleFont, title) + 10.0f * s);

    drawMicroText (g, title, area.removeFromLeft (titleWidth), titleFont, muted.withAlpha (0.95f));

    if (right.isNotEmpty())
        drawMicroText (g, right, area, monoFont (8.2f * fs, juce::Font::plain, 0.11f),
                       dim.withAlpha (0.95f), juce::Justification::centredRight);

    // Hairline under the header.
    const auto y = r.getBottom() - 1.0f * s;
    drawFadeRule (g, { r.getX(), y, r.getWidth(), juce::jmax (0.8f, 0.8f * s) },
                  accent, 0.30f);
}

void drawMicroText (juce::Graphics& g, const juce::String& microText,
                    juce::Rectangle<float> r, juce::Font font, Colour colour,
                    juce::Justification justification, float alpha)
{
    if (microText.isEmpty() || r.isEmpty())
        return;

    g.setFont (font);
    g.setColour (colour.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
    g.drawText (microText, r, justification, false);
}

void drawGlowDot (juce::Graphics& g, float cx, float cy, float radius, Colour colour,
                  float intensity, float scale)
{
    if (radius <= 0.0f)
        return;

    const auto s = juce::jmax (0.5f, scale);
    const auto w = juce::jmax (0.8f, 1.0f * s);
    const auto total = juce::jmax (2.0f, radius * 4.2f);
    juce::ignoreUnused (w);

    for (int i = 4; i >= 1; --i)
    {
        const auto t = (float) i / 4.0f;
        const auto r = radius * (1.0f + 3.0f * t);
        g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, intensity * (0.055f / t))));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, intensity * 0.9f)));
    g.fillEllipse (cx - radius * 0.55f, cy - radius * 0.55f, radius * 1.1f, radius * 1.1f);

    g.setColour (paleEmber.withAlpha (juce::jlimit (0.0f, 1.0f, intensity * 0.85f)));
    g.fillEllipse (cx - radius * 0.18f, cy - radius * 0.18f, radius * 0.36f, radius * 0.36f);

    juce::ignoreUnused (total);
}

void drawGlowLine (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                   Colour colour, float width, float intensity, float scale)
{
    const auto s = juce::jmax (0.5f, scale);
    const auto w = juce::jmax (0.8f, width * s);
    const auto i = juce::jlimit (0.0f, 1.0f, intensity);

    g.setColour (colour.withAlpha (i * 0.10f));
    g.drawLine ({ a, b }, w * 3.0f);
    g.setColour (colour.withAlpha (i * 0.30f));
    g.drawLine ({ a, b }, w * 1.7f);
    g.setColour (colour.withAlpha (i));
    g.drawLine ({ a, b }, w);
}

void drawFadeRule (juce::Graphics& g, juce::Rectangle<float> r, Colour colour, float alpha)
{
    if (r.isEmpty())
        return;

    juce::ColourGradient grad (colour.withMultipliedAlpha (alpha), r.getX(), r.getY(),
                               colour.withAlpha (0.0f), r.getRight(), r.getY(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
}

Colour emberRamp (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);

    if (t < 0.35f)
        return emberDeep.interpolatedWith (ember, t / 0.35f);
    if (t < 0.68f)
        return ember.interpolatedWith (gold, (t - 0.35f) / 0.33f);
    if (t < 0.88f)
        return gold.interpolatedWith (paleEmber, (t - 0.68f) / 0.20f);

    return paleEmber.interpolatedWith (juce::Colour (0xfffff6ec), (t - 0.88f) / 0.12f);
}

Colour pitchColour (float semitones)
{
    const auto t = juce::jlimit (0.0f, 1.0f, (semitones + 24.0f) / 48.0f);
    return emberRamp (0.12f + 0.88f * t);
}

} // namespace nagloed::ui::theme
