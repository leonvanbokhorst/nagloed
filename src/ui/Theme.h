#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// -----------------------------------------------------------------------------
// NAGLOED visual language: a dim analogue laboratory at night. Warm ember light
// on near-black glass, copper rules, engraved monospaced micro-labels.
//
// Everything in this file is pure drawing code: no image assets, no fonts on
// disk. The only shared state is a cached noise tile and the resolved system
// font families, all created once on first use.
// -----------------------------------------------------------------------------

namespace nagloed::ui
{

namespace theme
{

using juce::Colour;

// --- palette ----------------------------------------------------------------
inline const Colour pageTop      { 0xff0b0e14 };
inline const Colour pageBottom   { 0xff06080c };
inline const Colour plateTop     { 0xff141922 };
inline const Colour plateBottom  { 0xff0b0f16 };
inline const Colour wellTop      { 0xff080b10 };
inline const Colour wellBottom   { 0xff05070a };
inline const Colour panelEdge    { 0xff28303e };
inline const Colour panelEdgeHot { 0xff4a3826 };
inline const Colour ember        { 0xffff6b1a };
inline const Colour emberDeep    { 0xff9c3d08 };
inline const Colour gold         { 0xffffb347 };
inline const Colour paleEmber    { 0xffffe9d6 };
inline const Colour mint         { 0xff7ce8c7 };
inline const Colour copper       { 0xffb87333 };
inline const Colour copperDim    { 0xff6f4622 };
inline const Colour text         { 0xffe7e2da };
inline const Colour muted        { 0xff8a8f98 };
inline const Colour dim          { 0xff565d69 };
inline const Colour warning      { 0xffff4d6d };
inline const Colour line         { 0xff2b3240 };
inline const Colour lineSoft     { 0xff1c222d };

// --- geometry ---------------------------------------------------------------
inline constexpr float designWidth  = 1180.0f;
inline constexpr float designHeight = 680.0f;
inline constexpr float minUiScale   = 0.78f;
inline constexpr float maxUiScale   = 2.20f;

/** Uniform scale factor for a given editor size (clamped). */
float scaleFor (juce::Rectangle<int> bounds) noexcept;

/** Fonts may not shrink quite as far as geometry, so they stay legible. */
float fontScaleFor (float uiScale) noexcept;

/** Smallest readable font height on a 1x display. */
inline constexpr float minimumTextHeight = 8.0f;

// --- system fonts (resolved once, with graceful fallbacks) -------------------
const juce::String& displayFamily();
const juce::String& bodyFamily();
const juce::String& monoFamily();

juce::Font displayFont (float height, int style = juce::Font::plain, float kerning = 0.0f);
juce::Font bodyFont    (float height, int style = juce::Font::plain, float kerning = 0.0f);
juce::Font monoFont    (float height, int style = juce::Font::plain, float kerning = 0.0f);

/** Shaped text width (JUCE 9 moved this out of Font). */
float textWidth (const juce::Font& font, const juce::String& textToMeasure);

/** JUCE narrow string literals are Latin-1 by default; UTF-8 must be explicit. */
inline juce::String utf8 (const char* utf8Text) { return juce::String::fromUTF8 (utf8Text); }

// --- cached, scale-aware background painting --------------------------------
/**
    Owns an ARGB image rendered at the current device scale. Subclasses draw in
    *logical* coordinates inside render(); the image is rebuilt automatically
    when the logical size or the physical scale changes, and blitted 1:1 by
    paint() so no per-frame re-layout or font work happens.
*/
class CachedBackground
{
public:
    virtual ~CachedBackground() = default;

    /** Rebuild the cache if needed. Safe to call from paint().
        If maxPixels > 0 the render scale is reduced just enough to keep the
        cached image under that pixel budget.
    */
    void ensure (int logicalWidth, int logicalHeight, float scale,
                 float maxPixels = 0.0f);

    /** Force a rebuild on the next ensure() (e.g. after a UI-scale change). */
    void invalidate();

    const juce::Image& getImage() const noexcept { return image; }

protected:
    virtual void render (juce::Graphics& g, juce::Rectangle<float> bounds) = 0;

private:
    juce::Image image;
    int logicalW = 0;
    int logicalH = 0;
    float renderedScale = 0.0f;
};

/** Blit a cached background to fill the component. */
void drawCached (juce::Graphics& g, const juce::Image& image, juce::Rectangle<int> bounds);

// --- drawing helpers --------------------------------------------------------
void drawPanelPlate (juce::Graphics& g, juce::Rectangle<float> r, float scale,
                     float radius = 4.0f);

void drawHeader (juce::Graphics& g, juce::Rectangle<float> r, float scale,
                 const juce::String& index, const juce::String& title,
                 const juce::String& right = {},
                 Colour accent = muted);

void drawMicroText (juce::Graphics& g, const juce::String& microText,
                    juce::Rectangle<float> r, juce::Font font, Colour colour,
                    juce::Justification justification = juce::Justification::centredLeft,
                    float alpha = 1.0f);

/** Soft additive-style dot: a handful of translucent circles, no gradients. */
void drawGlowDot (juce::Graphics& g, float cx, float cy, float radius, Colour colour,
                  float intensity = 1.0f, float scale = 1.0f);

/** A line with a subtle glow; width in logical pixels. */
void drawGlowLine (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                   Colour colour, float width, float intensity = 1.0f, float scale = 1.0f);

/** Very quiet procedural texture: noise, scanlines and vignette. */
void drawAtmosphere (juce::Graphics& g, juce::Rectangle<float> r, float scale,
                     float strength = 1.0f);

/** Hairline rule that fades out towards the right. */
void drawFadeRule (juce::Graphics& g, juce::Rectangle<float> r, Colour colour,
                   float alpha = 0.35f);

/** 0 = copper .. 0.45 = ember .. 0.75 = gold .. 1 = pale ember. */
Colour emberRamp (float t);

/** Map a pitch in semitones to the cloud colour ramp. */
Colour pitchColour (float semitones);

/** Eight distinguishable Gloop lane colours, warm-biased so a lane ring still
    reads as an ember rather than a rainbow: copper -> ember -> gold -> mint. */
Colour gloopLaneColour (int lane) noexcept;

/** Cool light-blue reserved for Gloop's relative movement around a knob. It is
    deliberately outside the warm ember palette so it reads as "Gloop is moving
    this" rather than as part of the live knob value. */
Colour gloopMotion() noexcept;

/** Cheap cached 128x128 monochrome noise tile (alpha only, drawn by atmosphere). */
const juce::Image& noiseTile();

/** Centre a knob-sized control inside a grid cell so labels hug the knob. */
inline juce::Rectangle<float> fittedKnobBounds (juce::Rectangle<float> cell, float scale) noexcept
{
    const auto h = juce::jmin (cell.getHeight(), 84.0f * scale);
    return cell.withSizeKeepingCentre (cell.getWidth(), h);
}

// --- small numeric helpers --------------------------------------------------
inline float clamp01 (float v) noexcept            { return juce::jlimit (0.0f, 1.0f, v); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

inline juce::Rectangle<float> toFloat (juce::Rectangle<int> r) noexcept
{
    return juce::Rectangle<float> { (float) r.getX(), (float) r.getY(),
                                    (float) r.getWidth(), (float) r.getHeight() };
}

} // namespace theme
} // namespace nagloed::ui
