#include "AfterglowLookAndFeel.h"
#include "Theme.h"

#include <cmath>

namespace nagloed::ui
{

using namespace theme;

namespace
{
float scaleOf (const juce::Component& c)
{
    return juce::jlimit (0.5f, 3.0f,
                         (float) (double) c.getProperties().getWithDefault ("uiScale", 1.0));
}

juce::String captionOf (const juce::Component& c)
{
    const auto caption = c.getProperties().getWithDefault ("caption", juce::String()).toString();
    return caption.isNotEmpty() ? caption : c.getName().toUpperCase();
}

juce::Colour accentOf (const juce::Component& c)
{
    const auto argb = (juce::uint32) (int) c.getProperties().getWithDefault ("accentArgb", 0);
    return argb != 0 ? juce::Colour (argb) : theme::ember;
}

juce::String roleOf (const juce::Button& b)
{
    return b.getProperties().getWithDefault ("role", juce::String()).toString();
}

juce::String glyphOf (const juce::Button& b)
{
    return b.getProperties().getWithDefault ("glyph", juce::String()).toString();
}

juce::Point<float> polar (float cx, float cy, float radius, float angle)
{
    return { cx + std::sin (angle) * radius, cy - std::cos (angle) * radius };
}

bool isBipolar (const juce::Slider& slider)
{
    return slider.getMinimum() < -0.0001 && slider.getMaximum() > 0.0001;
}

void addArc (juce::Path& path, float cx, float cy, float radius,
             float from, float to)
{
    if (std::abs (to - from) < 0.0005f)
        return;
    path.addCentredArc (cx, cy, radius, radius, 0.0f,
                        juce::jmin (from, to), juce::jmax (from, to), true);
}

juce::Path glyphPath (const juce::String& glyph, juce::Rectangle<float> area, float scale)
{
    juce::Path path;
    const auto c = area.getCentre();
    const auto s = juce::jmax (0.6f, scale);
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.22f;

    if (glyph == "prev")
    {
        path.startNewSubPath (c.x + r * 0.55f, c.y - r);
        path.lineTo (c.x - r * 0.55f, c.y);
        path.lineTo (c.x + r * 0.55f, c.y + r);
    }
    else if (glyph == "next")
    {
        path.startNewSubPath (c.x - r * 0.55f, c.y - r);
        path.lineTo (c.x + r * 0.55f, c.y);
        path.lineTo (c.x - r * 0.55f, c.y + r);
    }
    else if (glyph == "open")
    {
        path.startNewSubPath (c.x - r, c.y - r * 0.45f);
        path.lineTo (c.x, c.y + r * 0.55f);
        path.lineTo (c.x + r, c.y - r * 0.45f);
    }
    else if (glyph == "close")
    {
        path.startNewSubPath (c.x - r, c.y + r * 0.45f);
        path.lineTo (c.x, c.y - r * 0.55f);
        path.lineTo (c.x + r, c.y + r * 0.45f);
    }

    juce::ignoreUnused (s);
    return path;
}
} // namespace

// =============================================================================
AfterglowLookAndFeel::AfterglowLookAndFeel()
{
    // Menu / popup colours.
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0c1017));
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::ember.withAlpha (0.16f));
    setColour (juce::PopupMenu::highlightedTextColourId, theme::paleEmber);
    setColour (juce::PopupMenu::headerTextColourId, theme::muted);

    // Combo boxes.
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, theme::ember);
    setColour (juce::ComboBox::buttonColourId, juce::Colours::transparentBlack);

    // Labels.
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);

    // Sliders (no text boxes, but keep them themed for safety).
    setColour (juce::Slider::thumbColourId, theme::paleEmber);
    setColour (juce::Slider::trackColourId, theme::ember);
    setColour (juce::Slider::rotarySliderFillColourId, theme::ember);
    setColour (juce::Slider::rotarySliderOutlineColourId, theme::lineSoft);
    setColour (juce::Slider::textBoxTextColourId, theme::paleEmber);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Buttons.
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, theme::muted);
    setColour (juce::TextButton::textColourOnId, theme::paleEmber);

    // Tooltips.
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0d1119));
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::line.withAlpha (0.9f));

    rebuildFonts();
}

void AfterglowLookAndFeel::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 3.0f, newScale);

    if (std::abs (newScale - uiScale) < 0.005f)
        return;

    uiScale = newScale;
    rebuildFonts();
}

void AfterglowLookAndFeel::rebuildFonts()
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);

    knobLabelFont   = monoFont (9.6f * fs, juce::Font::plain, 0.11f);
    knobValueFont   = monoFont (10.0f * fs, juce::Font::plain, 0.02f);
    comboCaptionFont = monoFont (8.2f * fs, juce::Font::plain, 0.13f);
    comboValueFont  = monoFont (9.4f * fs, juce::Font::plain, 0.02f);
    buttonFont      = monoFont (10.0f * fs, juce::Font::plain, 0.15f);
    menuFont        = monoFont (10.0f * fs, juce::Font::plain, 0.02f);
    tooltipFont     = monoFont (9.6f * fs, juce::Font::plain, 0.03f);
    labelFont       = monoFont (9.6f * fs, juce::Font::plain, 0.03f);
    tickFont        = monoFont (8.2f * fs, juce::Font::plain, 0.05f);
}

// =============================================================================
// Sliders
// =============================================================================
void AfterglowLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    const auto s = scaleOf (slider);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto hover = slider.isMouseOverOrDragging();
    const auto focused = slider.hasKeyboardFocus (false);
    const auto accent = accentOf (slider);
    const auto bipolar = isBipolar (slider);

    // The knob and its engraved label move together as one vertically centred
    // group, so tall grid cells never leave the label floating far below.
    const auto labelH = 12.0f * s;
    const auto availW = bounds.getWidth() - 16.0f * s;
    const auto availH = bounds.getHeight() - labelH - 6.0f * s;
    const auto diameter = juce::jmax (10.0f * s, juce::jmin (availW, availH));
    const auto groupH = diameter + labelH;
    const auto groupTop = bounds.getY() + juce::jmax (0.0f, (bounds.getHeight() - groupH) * 0.5f);
    const auto radius = diameter * 0.5f;
    const auto cx = bounds.getCentreX();
    const auto cy = groupTop + radius;
    const auto labelTop = groupTop + diameter + 1.0f * s;

    const auto pos = juce::jlimit (0.0f, 1.0f, sliderPos);
    const auto angle = rotaryStartAngle + pos * (rotaryEndAngle - rotaryStartAngle);
    const auto zeroAngle = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

    // --- halo behind the knob ------------------------------------------------
    {
        const auto hot = hover || focused ? 1.0f : 0.0f;
        juce::ColourGradient halo (accent.withAlpha (0.05f + 0.06f * hot), cx, cy,
                                   accent.withAlpha (0.0f), cx, cy + radius * 2.6f, true);
        g.setGradientFill (halo);
        g.fillEllipse (cx - radius * 2.6f, cy - radius * 2.6f,
                       radius * 5.2f, radius * 5.2f);
    }

    // --- engraved tick ring --------------------------------------------------
    {
        const auto trackR = radius + 3.5f * s;
        const auto tickInner = trackR + 1.2f * s;
        const auto tickOuterMajor = trackR + 4.6f * s;
        const auto tickOuterMinor = trackR + 3.1f * s;
        constexpr int numTicks = 37;

        for (int i = 0; i < numTicks; ++i)
        {
            const auto t = (float) i / (float) (numTicks - 1);
            const auto a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const auto major = (i % 4 == 0);
            const auto inRange = bipolar ? (a >= zeroAngle ? a <= angle : a >= angle)
                                         : a <= angle;
            const auto col = inRange ? accent.withAlpha (major ? 0.9f : 0.55f)
                                     : theme::lineSoft.withAlpha (major ? 0.95f : 0.6f);

            g.setColour (col);
            const auto inner = tickInner;
            const auto outer = major ? tickOuterMajor : tickOuterMinor;
            const auto p1 = polar (cx, cy, inner, a);
            const auto p2 = polar (cx, cy, outer, a);
            g.drawLine ({ p1, p2 }, (major ? 1.15f : 0.75f) * s);
        }

        if (bipolar)
        {
            const auto p1 = polar (cx, cy, trackR - 1.5f * s, zeroAngle);
            const auto p2 = polar (cx, cy, trackR + 4.2f * s, zeroAngle);
            g.setColour (theme::paleEmber.withAlpha (0.55f));
            g.drawLine ({ p1, p2 }, 1.0f * s);
        }
    }

    // --- track and glowing active arc ---------------------------------------
    {
        const auto trackR = radius + 3.5f * s;
        const auto stroke = juce::PathStrokeType (1.5f * s, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded);

        juce::Path track;
        addArc (track, cx, cy, trackR, rotaryStartAngle, rotaryEndAngle);
        g.setColour (theme::lineSoft.withAlpha (0.95f));
        g.strokePath (track, stroke);

        juce::Path active;
        addArc (active, cx, cy, trackR, zeroAngle, angle);

        if (! active.isEmpty())
        {
            const auto hot = hover || focused ? 1.0f : 0.85f;
            const auto arcColour = theme::emberRamp (bipolar ? 0.30f + 0.35f * pos
                                                             : 0.25f + 0.55f * pos)
                                       .interpolatedWith (accent, 0.25f);

            g.setColour (arcColour.withAlpha (0.06f * hot));
            g.strokePath (active, juce::PathStrokeType (6.5f * s, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour (arcColour.withAlpha (0.20f * hot));
            g.strokePath (active, juce::PathStrokeType (3.4f * s, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour (arcColour.withAlpha (0.95f));
            g.strokePath (active, juce::PathStrokeType (1.7f * s, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }

    // --- machined knob body --------------------------------------------------
    {
        juce::ColourGradient body (juce::Colour (0xff2c3543), cx - radius * 0.40f, cy - radius * 0.45f,
                                   juce::Colour (0xff080b10), cx + radius * 0.25f, cy + radius * 0.60f,
                                   true);
        body.addColour (0.55, juce::Colour (0xff1a212b));
        g.setGradientFill (body);
        g.fillEllipse (cx - radius, cy - radius, diameter, diameter);

        // Top bevel: cold light. Bottom rim: shadow.
        {
            juce::Path top;
            top.addCentredArc (cx, cy, radius - 0.5f * s, radius - 0.5f * s, 0.0f,
                               juce::MathConstants<float>::pi * 1.5f,
                               juce::MathConstants<float>::pi * 2.5f, true);
            g.setColour (theme::paleEmber.withAlpha (hover ? 0.20f : 0.12f));
            g.strokePath (top, juce::PathStrokeType (0.9f * s));

            juce::Path bottom;
            bottom.addCentredArc (cx, cy, radius - 0.5f * s, radius - 0.5f * s, 0.0f,
                                  juce::MathConstants<float>::pi * 0.5f,
                                  juce::MathConstants<float>::pi * 1.5f, true);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.strokePath (bottom, juce::PathStrokeType (1.1f * s));
        }

        // Knurled rim.
        if (diameter > 22.0f * s)
        {
            const auto rOuter = radius - 0.6f * s;
            const auto rInner = radius - 2.4f * s;
            for (int i = 0; i < 40; ++i)
            {
                const auto a = (float) i / 40.0f * juce::MathConstants<float>::twoPi;
                g.setColour ((i % 2 == 0 ? juce::Colours::black.withAlpha (0.30f)
                                         : theme::paleEmber.withAlpha (0.035f)));
                g.drawLine ({ polar (cx, cy, rInner, a), polar (cx, cy, rOuter, a) },
                            0.8f * s);
            }
        }

        // Pointer with an ember tip.
        {
            const auto hot = hover ? 1.0f : 0.9f;
            const auto inner = polar (cx, cy, radius * 0.18f, angle);
            const auto outer = polar (cx, cy, radius * 0.42f, angle);
            const auto tip = polar (cx, cy, radius * 0.52f, angle);
            const auto pointerColour = theme::emberRamp (0.35f + 0.45f * pos);

            g.setColour (pointerColour.withAlpha (0.14f * hot));
            g.drawLine ({ inner, outer }, 4.6f * s);
            g.setColour (pointerColour.withAlpha (0.40f * hot));
            g.drawLine ({ inner, outer }, 2.6f * s);
            g.setColour (theme::paleEmber.withAlpha (0.95f));
            g.drawLine ({ inner, outer }, 1.5f * s);

            g.setColour (pointerColour.withAlpha (0.9f));
            g.fillEllipse (tip.x - 1.6f * s, tip.y - 1.6f * s, 3.2f * s, 3.2f * s);

            if (hover || focused)
            {
                theme::drawGlowDot (g, tip.x, tip.y, 1.5f * s, pointerColour, 0.8f, s);
            }
        }

        // Centre cap.
        {
            g.setColour (juce::Colour (0xff0d1118));
            g.fillEllipse (cx - radius * 0.20f, cy - radius * 0.20f,
                           radius * 0.40f, radius * 0.40f);
            g.setColour (theme::line.withAlpha (0.9f));
            g.drawEllipse (cx - radius * 0.20f, cy - radius * 0.20f,
                           radius * 0.40f, radius * 0.40f, 0.8f * s);
            g.setColour (theme::ember.withAlpha (0.35f));
            g.fillEllipse (cx - 1.0f * s, cy - 1.0f * s, 2.0f * s, 2.0f * s);
        }
    }

    // --- engraved label / live value ----------------------------------------
    {
        const auto area = juce::Rectangle<float> (bounds.getX(), labelTop,
                                                  bounds.getWidth(), labelH);
        const auto caption = captionOf (slider);

        if (hover || focused)
        {
            auto valueText = slider.getTextFromValue (slider.getValue());
            if (valueText.isEmpty())
                valueText = juce::String (slider.getValue(), 2);

            g.setFont (knobValueFont);
            g.setColour (theme::paleEmber.withAlpha (0.96f));
            g.drawText (valueText, area, juce::Justification::centred, false);
        }
        else
        {
            g.setFont (knobLabelFont);
            g.setColour (theme::muted.withAlpha (0.92f));
            g.drawText (caption, area, juce::Justification::centred, false);
        }
    }

    // --- keyboard focus ring --------------------------------------------------
    if (focused)
    {
        g.setColour (theme::ember.withAlpha (0.28f));
        g.drawEllipse (cx - radius - 6.0f * s, cy - radius - 6.0f * s,
                       (radius + 6.0f * s) * 2.0f, (radius + 6.0f * s) * 2.0f,
                       1.0f * s);
    }

}

void AfterglowLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    const auto s = scaleOf (slider);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto hover = slider.isMouseOverOrDragging();
    const auto bipolar = isBipolar (slider);
    const auto pos = juce::jlimit (0.0f, 1.0f, (float) slider.valueToProportionOfLength (slider.getValue()));
    const auto caption = captionOf (slider);
    auto valueText = slider.getTextFromValue (slider.getValue());
    if (valueText.isEmpty())
        valueText = juce::String (slider.getValue(), 2);

    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto captionW = caption.isNotEmpty() ? theme::textWidth (knobLabelFont, caption) + 7.0f * s
                                               : 0.0f;
    const auto valueW = theme::textWidth (knobValueFont, valueText) + 7.0f * s;
    const auto rail = juce::Rectangle<float> (bounds.getX() + captionW,
                                              bounds.getCentreY() - 1.5f * s,
                                              juce::jmax (8.0f, bounds.getWidth() - captionW - valueW),
                                              3.0f * s);

    // Caption and value.
    if (captionW > 0.0f)
    {
        g.setFont (knobLabelFont);
        g.setColour (theme::muted.withAlpha (0.95f));
        g.drawText (caption, bounds.withWidth (captionW), juce::Justification::centredLeft, false);
    }

    g.setFont (knobValueFont);
    g.setColour (theme::paleEmber.withAlpha (hover ? 0.98f : 0.86f));
    g.drawText (valueText, bounds.withTrimmedLeft (bounds.getWidth() - valueW),
                juce::Justification::centredRight, false);

    // Rail.
    g.setColour (theme::lineSoft.withAlpha (0.9f));
    g.fillRoundedRectangle (rail, rail.getHeight() * 0.5f);

    const auto thumbX = rail.getX() + pos * rail.getWidth();
    const auto zeroX = bipolar ? rail.getCentreX() : rail.getX();
    const auto fillFrom = juce::jmin (zeroX, thumbX);
    const auto fillTo = juce::jmax (zeroX, thumbX);
    if (fillTo - fillFrom > 0.5f)
    {
        juce::ColourGradient fill (theme::emberDeep.withAlpha (0.55f), fillFrom, rail.getCentreY(),
                                   theme::ember, fillTo, rail.getCentreY(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle ({ fillFrom, rail.getY(), fillTo - fillFrom, rail.getHeight() },
                                rail.getHeight() * 0.5f);
        g.setColour (theme::ember.withAlpha (hover ? 0.22f : 0.12f));
        g.fillRoundedRectangle (rail.expanded (0.0f, 2.0f * s), rail.getHeight());
    }

    if (bipolar)
    {
        g.setColour (theme::muted.withAlpha (0.6f));
        g.fillRect (rail.getCentreX() - 0.5f * s, rail.getCentreY() - 3.0f * s,
                    1.0f * s, 6.0f * s);
    }

    // Diamond thumb.
    {
        const auto r = (hover ? 4.3f : 3.6f) * s;
        juce::Path thumb;
        thumb.startNewSubPath (thumbX, rail.getCentreY() - r);
        thumb.lineTo (thumbX + r, rail.getCentreY());
        thumb.lineTo (thumbX, rail.getCentreY() + r);
        thumb.lineTo (thumbX - r, rail.getCentreY());
        thumb.closeSubPath();

        g.setColour (theme::ember.withAlpha (0.16f));
        g.fillEllipse (thumbX - r * 1.7f, rail.getCentreY() - r * 1.7f, r * 3.4f, r * 3.4f);
        g.setColour (theme::paleEmber);
        g.fillPath (thumb);
        g.setColour (theme::ember.withAlpha (0.8f));
        g.strokePath (thumb, juce::PathStrokeType (0.8f * s));
    }

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (theme::ember.withAlpha (0.30f));
        g.drawRoundedRectangle (bounds.reduced (1.0f * s), 2.0f * s, 1.0f * s);
    }
}

// =============================================================================
// Buttons
// =============================================================================
void AfterglowLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool highlighted, bool down)
{
    juce::ignoreUnused (backgroundColour);

    const auto s = scaleOf (button);
    const auto role = roleOf (button);
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f * s);
    const auto radius = 3.0f * s;

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (theme::ember.withAlpha (0.65f));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f * s),
                                3.0f * s, 1.2f * s);
    }

    if (role == "glyph")
    {
        if (highlighted || down)
        {
            const auto centre = bounds.getCentre();
            const auto r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.46f;
            juce::ColourGradient glow (theme::ember.withAlpha (down ? 0.22f : 0.13f), centre.x, centre.y,
                                       theme::ember.withAlpha (0.0f), centre.x, centre.y + r * 1.6f, true);
            g.setGradientFill (glow);
            g.fillEllipse (centre.x - r * 1.6f, centre.y - r * 1.6f, r * 3.2f, r * 3.2f);
        }
        return;
    }

    if (role == "freeze")
    {
        const auto on = button.getToggleState();
        const auto pulse = (float) (0.5 + 0.5 * std::sin (juce::Time::getMillisecondCounterHiRes() * 0.0034));
        const auto hot = (highlighted || down) ? 1.0f : 0.0f;

        if (on)
        {
            juce::ColourGradient glow (theme::ember.withAlpha (0.10f + 0.06f * pulse), bounds.getCentreX(), bounds.getCentreY(),
                                       theme::ember.withAlpha (0.0f), bounds.getCentreX(), bounds.getBottom() + 4.0f * s, true);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (bounds.expanded (2.0f * s), radius * 1.6f);
        }

        juce::ColourGradient base (on ? juce::Colour (0xff3a1507) : juce::Colour (0xff141922),
                                   bounds.getCentreX(), bounds.getY(),
                                   on ? juce::Colour (0xff0d0705) : juce::Colour (0xff080b10),
                                   bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (base);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (on ? theme::gold.withAlpha (0.75f + 0.15f * pulse)
                        : theme::line.withAlpha (0.9f + 0.05f * hot));
        g.drawRoundedRectangle (bounds, radius, 1.0f * s);

        g.setColour (theme::paleEmber.withAlpha (0.05f));
        g.fillRect (bounds.getX() + 5.0f * s, bounds.getY() + 1.0f * s,
                    bounds.getWidth() - 10.0f * s, 0.8f * s);
        return;
    }

    juce::ColourGradient base (theme::plateTop, bounds.getCentreX(), bounds.getY(),
                               theme::plateBottom, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour ((highlighted || down) ? theme::ember.withAlpha (0.55f)
                                       : theme::panelEdge.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds, radius, 1.0f * s);
}

void AfterglowLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                           bool highlighted, bool down)
{
    const auto s = scaleOf (button);
    const auto role = roleOf (button);
    const auto glyph = glyphOf (button);
    const auto bounds = button.getLocalBounds().toFloat();

    if (role == "glyph")
    {
        const auto path = glyphPath (glyph, bounds, s);
        const auto col = (highlighted || down) ? theme::paleEmber
                                               : theme::muted.withAlpha (0.85f);
        if (! path.isEmpty())
        {
            if (highlighted || down)
            {
                g.setColour (theme::ember.withAlpha (0.16f));
                g.strokePath (path, juce::PathStrokeType (3.0f * s, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
            g.setColour (col);
            g.strokePath (path, juce::PathStrokeType (1.15f * s, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        return;
    }

    if (role == "freeze")
    {
        const auto on = button.getToggleState();
        const auto pulse = (float) (0.5 + 0.5 * std::sin (juce::Time::getMillisecondCounterHiRes() * 0.0034));
        const auto indicator = juce::Rectangle<float> (bounds.getX() + 9.0f * s,
                                                       bounds.getCentreY() - 2.5f * s,
                                                       5.0f * s, 5.0f * s);

        if (on)
            theme::drawGlowDot (g, indicator.getCentreX(), indicator.getCentreY(), 1.6f * s,
                                theme::ember, 0.75f + 0.25f * pulse, s);

        g.setColour (on ? theme::ember : theme::dim);
        g.fillEllipse (indicator);

        g.setFont (buttonFont);
        g.setColour (on ? theme::paleEmber : theme::muted.withAlpha (0.9f));
        g.drawText ("FREEZE", bounds.withTrimmedLeft (10.0f * s),
                    juce::Justification::centred, false);

        return;
    }

    g.setFont (buttonFont);
    g.setColour (theme::paleEmber.withAlpha (highlighted ? 1.0f : 0.82f));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
}

void AfterglowLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool highlighted, bool down)
{
    juce::ignoreUnused (highlighted, down);
    const auto s = scaleOf (button);
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f * s);
    const auto on = button.getToggleState();

    g.setColour (on ? theme::ember.withAlpha (0.22f) : theme::lineSoft);
    g.fillRoundedRectangle (bounds, 2.0f * s);
    g.setColour (on ? theme::ember : theme::line.withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, 2.0f * s, 1.0f * s);
    g.setFont (buttonFont);
    g.setColour (on ? theme::paleEmber : theme::muted);
    g.drawText (button.getButtonText(), bounds.reduced (4.0f * s), juce::Justification::centredLeft, false);
}

// =============================================================================
// Combo boxes and menus
// =============================================================================
void AfterglowLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                          bool isButtonDown, int buttonX, int buttonY,
                                          int buttonW, int buttonH, juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto s = scaleOf (box);
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                            .reduced (0.5f * s);
    const auto radius = 2.5f * s;
    const auto hot = box.isMouseOverOrDragging() || box.isPopupActive() || isButtonDown;

    juce::ColourGradient base (theme::wellTop, bounds.getCentreX(), bounds.getY(),
                               theme::wellBottom, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (hot ? theme::ember.withAlpha (0.55f) : theme::line.withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, radius, 1.0f * s);

    g.setColour (theme::paleEmber.withAlpha (0.04f));
    g.fillRect (bounds.getX() + 4.0f * s, bounds.getY() + 1.0f * s,
                bounds.getWidth() - 8.0f * s, 0.7f * s);

    // Caption (e.g. "TUNE").
    const auto caption = box.getProperties().getWithDefault ("caption", juce::String()).toString();
    if (caption.isNotEmpty())
    {
        g.setFont (comboCaptionFont);
        g.setColour (theme::muted.withAlpha (0.95f));
        g.drawText (caption, bounds.withTrimmedLeft (6.0f * s).withTrimmedRight (14.0f * s),
                    juce::Justification::centredLeft, false);
    }

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (theme::ember.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds.expanded (1.0f * s), radius + 1.0f * s, 1.2f * s);
    }

    // Procedural chevron.
    {
        const auto c = juce::Point<float> (bounds.getRight() - 8.0f * s, bounds.getCentreY());
        const auto r = 2.6f * s;
        juce::Path chevron;
        chevron.startNewSubPath (c.x - r, c.y - r * 0.45f);
        chevron.lineTo (c.x, c.y + r * 0.65f);
        chevron.lineTo (c.x + r, c.y - r * 0.45f);
        g.setColour (hot ? theme::gold : theme::ember.withAlpha (0.85f));
        g.strokePath (chevron, juce::PathStrokeType (1.1f * s, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }
}

void AfterglowLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    const auto s = scaleOf (box);
    const auto caption = box.getProperties().getWithDefault ("caption", juce::String()).toString();
    const auto captionW = caption.isEmpty() ? 0.0f
                                            : theme::textWidth (comboCaptionFont, caption) + 8.0f * s;
    const auto left = 6.0f * s + captionW;
    const auto right = 14.0f * s;

    label.setBounds (juce::roundToInt (left), 0,
                     juce::jmax (10, box.getWidth() - juce::roundToInt (left + right)),
                     box.getHeight());
    label.setFont (comboValueFont);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    label.setColour (juce::Label::textColourId, theme::text);
    label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
}

juce::Font AfterglowLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    juce::ignoreUnused (box);
    return comboValueFont;
}

juce::Font AfterglowLookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
{
    juce::ignoreUnused (button, buttonHeight);
    return buttonFont;
}

juce::Font AfterglowLookAndFeel::getPopupMenuFont()
{
    return menuFont;
}

juce::Font AfterglowLookAndFeel::getLabelFont (juce::Label& label)
{
    juce::ignoreUnused (label);
    return labelFont;
}

void AfterglowLookAndFeel::getIdealPopupMenuItemSize (const juce::String& itemText, bool isSeparator,
                                                      int standardMenuItemHeight,
                                                      int& idealWidth, int& idealHeight)
{
    const auto s = juce::jmax (0.5f, uiScale);
    juce::ignoreUnused (s);

    if (isSeparator)
    {
        idealWidth = 40;
        idealHeight = juce::jmax (5, juce::roundToInt (5.0f * uiScale));
        return;
    }

    const auto fontH = menuFont.getHeight();
    idealHeight = juce::jmax (standardMenuItemHeight, juce::roundToInt (fontH + 10.0f * uiScale));
    idealWidth = juce::jmax (juce::roundToInt (fontH * 6.5f),
                             juce::roundToInt (theme::textWidth (menuFont, itemText) + 34.0f * uiScale));
}

void AfterglowLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto s = juce::jmax (0.5f, uiScale);
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    const auto radius = 3.0f * s;

    g.setColour (juce::Colour (0xff0c1017));
    g.fillRoundedRectangle (bounds.reduced (0.5f * s), radius);
    g.setColour (theme::muted.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.5f * s), radius, 1.0f * s);
    g.setColour (theme::paleEmber.withAlpha (0.05f));
    g.fillRect (4.0f * s, 1.0f * s, (float) width - 8.0f * s, 0.8f * s);
}

void AfterglowLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& itemText,
                                              const juce::String& shortcutKeyText,
                                              const juce::Drawable* icon, const juce::Colour* textColour)
{
    juce::ignoreUnused (icon, textColour, shortcutKeyText);

    const auto s = juce::jmax (0.5f, uiScale);
    const auto r = area.toFloat();

    if (isSeparator)
    {
        g.setColour (theme::line.withAlpha (0.7f));
        g.fillRect (r.getX() + 8.0f * s, r.getCentreY(), r.getWidth() - 16.0f * s,
                    juce::jmax (0.7f, 0.8f * s));
        return;
    }

    if (isHighlighted && isActive)
    {
        juce::ColourGradient hl (theme::ember.withAlpha (0.15f), r.getX(), r.getY(),
                                 theme::ember.withAlpha (0.05f), r.getRight(), r.getY(), false);
        g.setGradientFill (hl);
        g.fillRect (r);
        g.setColour (theme::ember.withAlpha (0.75f));
        g.fillRect (r.getX(), r.getY() + 1.0f, 1.6f * s, r.getHeight() - 2.0f);
    }

    auto textArea = r.withTrimmedLeft (12.0f * s).withTrimmedRight (10.0f * s);

    if (isTicked)
    {
        g.setColour (theme::ember);
        g.fillEllipse (r.getX() + 6.0f * s, r.getCentreY() - 1.6f * s, 3.2f * s, 3.2f * s);
        textArea = textArea.withTrimmedLeft (5.0f * s);
    }

    g.setFont (menuFont);
    g.setColour (isActive ? (isHighlighted ? theme::paleEmber : theme::text)
                          : theme::dim);
    g.drawText (itemText, textArea, juce::Justification::centredLeft, false);

    if (hasSubMenu)
    {
        const auto c = juce::Point<float> (r.getRight() - 10.0f * s, r.getCentreY());
        juce::Path arrow;
        arrow.startNewSubPath (c.x - 1.5f * s, c.y - 3.0f * s);
        arrow.lineTo (c.x + 1.8f * s, c.y);
        arrow.lineTo (c.x - 1.5f * s, c.y + 3.0f * s);
        g.setColour (theme::muted);
        g.strokePath (arrow, juce::PathStrokeType (1.0f * s));
    }
}

void AfterglowLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tipText,
                                         int width, int height)
{
    const auto s = juce::jmax (0.5f, uiScale);
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                            .reduced (1.0f * s);

    g.setColour (juce::Colour (0xff0c1017).withAlpha (0.97f));
    g.fillRoundedRectangle (bounds, 3.0f * s);
    g.setColour (theme::line.withAlpha (0.9f));
    g.drawRoundedRectangle (bounds, 3.0f * s, 1.0f * s);
    g.setColour (theme::paleEmber.withAlpha (0.05f));
    g.fillRect (bounds.getX() + 5.0f * s, bounds.getY() + 1.0f * s,
                bounds.getWidth() - 10.0f * s, 0.8f * s);

    g.setFont (tooltipFont);
    g.setColour (theme::text.withAlpha (0.95f));
    g.drawFittedText (tipText, bounds.reduced (7.0f * s, 4.0f * s).toNearestInt(),
                      juce::Justification::centredLeft, 3);
}

} // namespace nagloed::ui
