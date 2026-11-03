#include "NagloedEditor.h"

namespace nagloed::ui
{

NagloedEditor::NagloedEditor (juce::AudioProcessor& processor,
                              juce::AudioProcessorValueTreeState& state,
                              const VisualSource& visualSource,
                              PresetAccess& presetAccess)
    : juce::AudioProcessorEditor (&processor),
      apvts (state),
      visual (visualSource),
      presets (presetAccess)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (960, 560, 2400, 1500);
    setSize (1180, 680);
    startTimerHz (30);
}

void NagloedEditor::timerCallback()
{
    visual.copyVisual (snapshot);
    if (snapshot.frame != lastFrame)
    {
        lastFrame = snapshot.frame;
        repaint();
    }
}

void NagloedEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0e14));

    // Scaffold: ember field placeholder using the live visual snapshot.
    const auto area = getLocalBounds().toFloat();
    auto cloud = area.reduced (area.getWidth() * 0.08f, area.getHeight() * 0.18f);

    g.setColour (juce::Colour (0xffff6b1a).withAlpha (0.08f));
    g.fillRoundedRectangle (cloud, 12.0f);

    const int n = juce::jmin (snapshot.numGrains, VisualState::kMaxGrainViews);
    for (int i = 0; i < n; ++i)
    {
        const auto& grain = snapshot.grains[(size_t) i];
        const float x = cloud.getX() + grain.pos * cloud.getWidth();
        const float y = cloud.getBottom() - grain.progress * cloud.getHeight();
        const float radius = 1.0f + grain.env * 5.0f;
        g.setColour (juce::Colour (0xffffb347).withAlpha (0.15f + 0.6f * grain.env));
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
    }

    g.setColour (juce::Colour (0xffffe9d6));
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("NAGLOED", getLocalBounds().removeFromTop (72).reduced (28, 0),
                juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText ("granular afterglow  //  scaffold UI", getLocalBounds().removeFromTop (96).reduced (28, 0),
                juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (kVersion) + "   presets: " + juce::String (presets.getNumPresets()),
                getLocalBounds().removeFromBottom (28).reduced (28, 0),
                juce::Justification::bottomLeft);
}

void NagloedEditor::resized() {}

} // namespace nagloed::ui
