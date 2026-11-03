#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// The one LookAndFeel used by everything in the editor. Knobs are painted
// entirely procedurally: engraved tick rings, a dark machined body, an ember
// arc that glows, and an engraved monospaced readout that replaces the label
// while the control is hovered or dragged.
// -----------------------------------------------------------------------------
class AfterglowLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AfterglowLookAndFeel();
    ~AfterglowLookAndFeel() override = default;

    /** Editor scale (1.0 at 1180x680). Rebuilds the cached micro-fonts. */
    void setUiScale (float newScale);
    float getUiScale() const noexcept { return uiScale; }

    // --- sliders ------------------------------------------------------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // --- buttons ------------------------------------------------------------
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    // --- combo boxes and menus ----------------------------------------------
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    // --- tooltips -----------------------------------------------------------
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    void rebuildFonts();

    float uiScale = 1.0f;
    juce::Font knobLabelFont { juce::FontOptions() }, knobValueFont { juce::FontOptions() };
    juce::Font comboCaptionFont { juce::FontOptions() }, comboValueFont { juce::FontOptions() };
    juce::Font buttonFont { juce::FontOptions() }, menuFont { juce::FontOptions() };
    juce::Font tooltipFont { juce::FontOptions() }, labelFont { juce::FontOptions() };
    juce::Font tickFont { juce::FontOptions() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterglowLookAndFeel)
};

} // namespace nagloed::ui
