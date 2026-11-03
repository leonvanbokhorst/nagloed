#include "PresetBrowser.h"

namespace nagloed::ui
{

using namespace theme;

PresetBrowser::PresetBrowser (PresetAccess& presetAccess)
    : presets (presetAccess)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    setTitle ("Preset browser");
    setDescription ("Browse factory presets: previous, next, or click the name for the full list.");

    for (auto* button : { &prevButton, &nextButton })
    {
        button->setOpaque (false);
        button->getProperties().set ("role", "glyph");
        button->getProperties().set ("uiScale", 1.0);
        button->setWantsKeyboardFocus (true);
        addAndMakeVisible (*button);
    }

    prevButton.getProperties().set ("glyph", "prev");
    nextButton.getProperties().set ("glyph", "next");
    prevButton.setTitle ("Previous preset");
    nextButton.setTitle ("Next preset");
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };

    refresh();
}

void PresetBrowser::setUiScale (float newScale)
{
    newScale = juce::jlimit (0.5f, 2.5f, newScale);

    if (std::abs (newScale - uiScale) < 0.004f)
        return;

    uiScale = newScale;
    prevButton.getProperties().set ("uiScale", (double) uiScale);
    nextButton.getProperties().set ("uiScale", (double) uiScale);
    resized();
    repaint();
}

void PresetBrowser::refresh()
{
    const auto count = presets.getNumPresets();
    const auto index = presets.getCurrentPreset();

    if (count == numPresets && index == currentIndex)
        return;

    numPresets = juce::jmax (0, count);
    currentIndex = juce::jlimit (0, juce::jmax (0, numPresets - 1), index);

    if (numPresets == 0)
    {
        presetName = "INIT";
        category = "FACTORY";
        description = utf8 ("No factory presets installed — start from the init patch.");
    }
    else
    {
        const auto info = presets.getPresetInfo (currentIndex);
        presetName = info.name.isNotEmpty() ? info.name : "UNTITLED";
        category = info.category.isNotEmpty() ? info.category : "FACTORY";
        description = info.description;
    }

    setTooltip (description);
    prevButton.setEnabled (numPresets > 1);
    nextButton.setEnabled (numPresets > 1);
    repaint();
}

void PresetBrowser::resized()
{
    const auto s = uiScale;
    auto b = getLocalBounds();
    const auto buttonW = juce::roundToInt (22.0f * s);

    prevButton.setBounds (b.removeFromLeft (buttonW));
    nextButton.setBounds (b.removeFromRight (buttonW));
}

juce::Rectangle<float> PresetBrowser::nameArea() const
{
    auto b = getLocalBounds().toFloat();
    b.removeFromLeft (24.0f * uiScale);
    b.removeFromRight (24.0f * uiScale);
    return b;
}

void PresetBrowser::paint (juce::Graphics& g)
{
    const auto s = uiScale;
    const auto fs = fontScaleFor (s);
    const auto area = nameArea();

    if (area.isEmpty())
        return;

    // Invisible hit plate, brightening under the mouse.
    const auto hover = area.contains (mousePosition);
    if (hover)
    {
        juce::ColourGradient glow (ember.withAlpha (0.05f), area.getCentreX(), area.getY(),
                                   ember.withAlpha (0.0f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (area.reduced (2.0f * s, 0.0f), 2.0f * s);
    }

    // Preset name.
    {
        const auto nameFont = displayFont (17.5f * fs, juce::Font::bold, 0.02f);
        const auto indexWidth = 42.0f * s;
        auto nameRect = area.withTrimmedRight (indexWidth).withHeight (area.getHeight() * 0.56f);

        g.setFont (nameFont);
        g.setColour (paleEmber.withAlpha (hover ? 1.0f : 0.92f));
        g.drawText (presetName, nameRect, juce::Justification::centredLeft, true);

        const auto indexText = (numPresets > 0)
                                   ? juce::String (currentIndex + 1).paddedLeft ('0', 2)
                                         + "/" + juce::String (numPresets).paddedLeft ('0', 2)
                                   : "--/--";

        drawMicroText (g, indexText,
                       area.withTrimmedLeft (area.getWidth() - indexWidth)
                           .withHeight (area.getHeight() * 0.56f),
                       monoFont (8.0f * fs, juce::Font::plain, 0.09f),
                       muted.withAlpha (0.85f), juce::Justification::centredRight);
    }

    // Category chip + description.
    {
        const auto lineY = area.getY() + area.getHeight() * 0.58f;
        const auto lineH = area.getHeight() * 0.40f;

        const auto chipFont = monoFont (7.8f * fs, juce::Font::plain, 0.12f);
        const auto chipW = textWidth (chipFont, category) + 9.0f * s;
        juce::Rectangle<float> chip (area.getX(), lineY + (lineH - 10.0f * s) * 0.5f,
                                     chipW, 10.0f * s);

        g.setColour (lineSoft.withAlpha (0.55f));
        g.fillRoundedRectangle (chip, 1.5f * s);
        g.setColour (line.withAlpha (0.95f));
        g.drawRoundedRectangle (chip, 1.5f * s, 0.7f * s);
        drawMicroText (g, category, chip, chipFont, muted.withAlpha (0.95f),
                       juce::Justification::centred);

        auto descRect = juce::Rectangle<float> (chip.getRight() + 8.0f * s, lineY,
                                                area.getRight() - chip.getRight() - 10.0f * s,
                                                lineH);

        g.setFont (monoFont (8.0f * fs, juce::Font::plain, 0.03f));
        g.setColour (muted.withAlpha (0.88f));
        g.drawText (description.isNotEmpty() ? description : utf8 ("—"), descRect,
                    juce::Justification::centredLeft, true);
    }

    // Hover hint underline.
    if (hover)
    {
        drawFadeRule (g, { area.getX(), area.getBottom() - 1.0f * s,
                           area.getWidth(), 0.8f * s },
                      ember, 0.35f);
    }
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (nameArea().contains (e.position))
    {
        grabKeyboardFocus();
        showMenu();
    }
}

void PresetBrowser::mouseMove (const juce::MouseEvent& e)
{
    if (mousePosition != e.position)
    {
        mousePosition = e.position;
        repaint();
    }
}

void PresetBrowser::mouseExit (const juce::MouseEvent&)
{
    if (mousePosition.x > -1000.0f)
    {
        mousePosition = { -10000.0f, -10000.0f };
        repaint();
    }
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)
    {
        step (-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        step (1);
        return true;
    }

    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        showMenu();
        return true;
    }

    return false;
}

void PresetBrowser::load (int index)
{
    if (numPresets <= 0)
        return;

    presets.loadPreset (juce::jlimit (0, numPresets - 1, index));
    currentIndex = -1;   // force refresh
    refresh();
    repaint();
}

void PresetBrowser::step (int delta)
{
    if (numPresets <= 0)
        return;

    const auto base = juce::jmax (0, presets.getCurrentPreset());
    load ((base + delta + numPresets) % numPresets);
}

void PresetBrowser::showMenu()
{
    if (numPresets <= 0)
        return;

    juce::PopupMenu menu;
    juce::String lastCategory;

    for (int i = 0; i < numPresets; ++i)
    {
        const auto info = presets.getPresetInfo (i);
        const auto categoryName = info.category.isNotEmpty() ? info.category.toUpperCase()
                                                             : juce::String ("FACTORY");

        if (categoryName != lastCategory)
        {
            menu.addSectionHeader (categoryName);
            lastCategory = categoryName;
        }

        menu.addItem (i + 1, info.name, true, i == currentIndex);
    }

    const juce::Component::SafePointer<PresetBrowser> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withMinimumWidth (juce::jmax (160, getWidth() / 2)),
                        [safeThis] (int result)
                        {
                            if (result > 0)
                                if (auto* browser = safeThis.getComponent())
                                    browser->load (result - 1);
                        });
}

} // namespace nagloed::ui
