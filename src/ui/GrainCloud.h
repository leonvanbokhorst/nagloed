#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <random>
#include <vector>

#include "AfterglowLookAndFeel.h"
#include "ParamControls.h"
#include "Theme.h"
#include "core/Contracts.h"
#include "core/ParamIDs.h"

namespace nagloed::ui
{

// -----------------------------------------------------------------------------
// The grain cloud. This is the heart of the instrument's face:
//
//  * a phosphor screen (persistence buffer) that accumulates embers additively
//    and decays slowly, so moving grains draw their own afterglow trails;
//  * a live capture-buffer timeline underneath with focus marker, spray halo,
//    buffer fill and scan direction;
//  * direct-manipulation gestures: drag = focus/pitch, shift-drag = spray,
//    wheel = density, double-click = re-centre, arrows = fine knobs;
//  * when the engine has no grains to show (early boot, or DSP not running) a
//    deterministic dust field drifts around the focus point so the frame is
//    never empty or broken.
//
// All drawing is procedural and cached; the audio thread is only ever touched
// through VisualSource snapshots published by the editor.
// -----------------------------------------------------------------------------
class GrainCloud final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    GrainCloud (juce::AudioProcessorValueTreeState& apvts, AfterglowLookAndFeel& lnf);
    ~GrainCloud() override;

    void setUiScale (float newScale);
    float getUiScale() const noexcept { return uiScale; }

    /** Current visual frame. Called from the editor's 60 Hz timer. */
    void tick (const VisualState& snapshot, double elapsedSeconds);

    void collectParameterIds (juce::StringArray& ids) const;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // --- phosphor persistence screen ---------------------------------------
    class PhosphorField
    {
    public:
        bool ensure (int logicalWidth, int logicalHeight, float scale);
        bool isReady() const noexcept { return buffer.isValid(); }
        void clear();
        void fade (float amount);

        /** Additive ember splat; coordinates and radius are logical pixels. */
        void splat (float cx, float cy, float radius, juce::Colour colour, float energy);

        const juce::Image& getImage() const noexcept { return buffer; }
        float getScale() const noexcept { return pixelScale; }

    private:
        juce::Image buffer;
        std::unique_ptr<juce::Image::BitmapData> pixels;
        int logicalW = 0, logicalH = 0;
        float pixelScale = 1.0f;
    };

    struct Mote
    {
        float x = 0.5f, y = 0.5f;      // normalised inside the field
        float vx = 0.0f, vy = 0.0f;    // normalised per frame
        float phase = 0.0f;
        float phaseSpeed = 0.0f;
        float sway = 0.0f;
        float size = 1.0f;
        float energy = 0.1f;
        float warmth = 0.5f;
        bool wanderer = false;
    };

    class ChromeCache final : public theme::CachedBackground
    {
    public:
        explicit ChromeCache (GrainCloud& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        GrainCloud& owner;
    };

    class GridCache final : public theme::CachedBackground
    {
    public:
        explicit GridCache (GrainCloud& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        GrainCloud& owner;
    };

    void layoutAtScale();
    void ensureCaches (float currentDeviceScale);
    void prewarm();
    void spawnMote (Mote&, bool anywhere);
    void drawIdleDust (float timeScale);
    void simulateDustFrames (int frames);
    void advanceMote (Mote&, float timeScale);
    int targetMoteCount() const;
    void drawGrains();
    void updateReadouts();
    void renderChrome (juce::Graphics&, juce::Rectangle<float>);
    void renderGrid (juce::Graphics&, juce::Rectangle<float>);
    void drawFieldOverlay (juce::Graphics&);
    void drawTimeline (juce::Graphics&);
    void drawReadouts (juce::Graphics&);

    void setParamNormalised (juce::RangedAudioParameter*, float normalised);
    void nudgeParam (juce::RangedAudioParameter*, float deltaNormalised);
    float paramNormalised (const juce::RangedAudioParameter*) const;
    float paramReal (const juce::RangedAudioParameter*) const;
    float pitchToNorm (float semitones) const;
    float pitchToY (float semitones) const;
    float focusToX (float focus) const;

    void beginGesture (juce::RangedAudioParameter*);
    void endGesture (juce::RangedAudioParameter*);

    juce::AudioProcessorValueTreeState& apvts;
    AfterglowLookAndFeel& lnf;
    ParamControlSet controls;
    juce::Slider* scanRibbon = nullptr;
    juce::Slider* reverseRibbon = nullptr;

    juce::RangedAudioParameter* focusParam = nullptr;
    juce::RangedAudioParameter* pitchParam = nullptr;
    juce::RangedAudioParameter* sprayParam = nullptr;
    juce::RangedAudioParameter* densityParam = nullptr;
    juce::RangedAudioParameter* sizeParam = nullptr;
    juce::RangedAudioParameter* scanParam = nullptr;

    float uiScale = 1.0f;
    float deviceScale = 0.0f;

    // Logical layout (relative to the component).
    juce::Rectangle<float> headerRect, fieldRect, leftAxisRect, bottomAxisRect,
                           timelineRect, trackRect, scanRect, reverseRect, hintRect;
    float timelineLabelH = 10.0f;

    ChromeCache chrome;
    GridCache gridOverlay;
    PhosphorField phosphor;

    VisualState visual;
    bool haveVisual = false;
    bool prewarmNeeded = true;

    // Smoothed display values.
    float shownFocus = 0.5f, shownSpray = 0.25f, shownPitch = 0.0f;
    float shownBufferFill = 0.0f;
    bool smoothedInit = false;

    std::vector<Mote> motes;
    std::mt19937 rng { 0x4e41474cu };
    float windPhase = 0.0f;

    // Cached readout strings (regenerated at ~10 Hz, never per frame).
    juce::String focusText, sprayText, densityText, grainRateText, bufferText;
    juce::String headerInfoText, timelineInfoText;
    int readoutCounter = 0;

    // Gesture state.
    bool dragging = false;
    bool shiftSpray = false;
    bool timelineDrag = false;
    bool focusGesture = false;
    bool pitchGesture = false;
    bool sprayGesture = false;
    juce::Point<float> dragStartMouse;
    juce::Point<float> hoverPos;
    float dragStartFocusNorm = 0.5f;
    float dragStartPitchReal = 0.0f;
    float dragStartSprayNorm = 0.25f;
    bool hoverField = false;
    juce::String dragHud;

    int frameCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainCloud)
};

} // namespace nagloed::ui
