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
// The phosphor screen is its own child component so the 60 Hz repaint touches
// only the field: panel chrome, header and timeline are redrawn at a lower rate.
// The audio thread is only ever touched through VisualSource snapshots.
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

        /** Decay the screen. shift 4 = 1/16 per frame, 5 = 1/32, 6 = 1/64.
            Only the region that has seen a splat is touched. */
        void fade (int shift);

        /** Additive ember splat; coordinates and radius are logical pixels. */
        void splat (float cx, float cy, float radius, juce::Colour colour, float energy);

        const juce::Image& getImage() const noexcept { return buffer; }
        float getScale() const noexcept { return pixelScale; }

    private:
        juce::Image buffer;
        std::unique_ptr<juce::Image::BitmapData> pixels;
        int logicalW = 0, logicalH = 0;
        int minX = 0, minY = 0, maxX = 0, maxY = 0;   // active pixel bounds
        bool active = false;
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

    class FieldOverlayCache final : public theme::CachedBackground
    {
    public:
        explicit FieldOverlayCache (GrainCloud& o) : owner (o) {}
    protected:
        void render (juce::Graphics&, juce::Rectangle<float>) override;
    private:
        GrainCloud& owner;
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

    class Screen final : public juce::Component
    {
    public:
        explicit Screen (GrainCloud& o) : owner (o)
        {
            setOpaque (true);
            setInterceptsMouseClicks (false, false);
        }

        void paint (juce::Graphics&) override;

    private:
        GrainCloud& owner;
    };

    void layoutAtScale();
    void ensureChromeCache (float scale);
    void renderFieldOverlay (juce::Graphics&, juce::Rectangle<float>);
    void ensureSimulationBuffers();
    void simulateWarmupFrames (int frames);
    void renderChrome (juce::Graphics&, juce::Rectangle<float>);

    void prewarmNow();
    void spawnMote (Mote&, bool anywhere);
    void advanceMote (Mote&, float timeScale);
    int  targetMoteCount() const;
    void drawDust (float timeScale, float energyScale);
    void drawGrains();
    void drawGhostGrains();
    void updateReadouts();
    void drawScreenForeground (juce::Graphics&, juce::Rectangle<float>);

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

    float uiScale = 0.0f;   // 0 means fonts/layout not prepared yet
    float deviceScale = 0.0f;

    // Logical layout (relative to the component).
    juce::Rectangle<float> headerRect, fieldRect, leftAxisRect, bottomAxisRect,
                           timelineRect, trackRect, scanRect, reverseRect, hintRect;
    float timelineLabelH = 10.0f;

    ChromeCache chrome;
    FieldOverlayCache fieldOverlay;
    PhosphorField phosphor;
    std::unique_ptr<Screen> screen;

    VisualState visual;
    bool prewarmPending = true;
    int  warmupFramesLeft = 0;

    // Smoothed display values.
    float shownFocus = 0.5f, shownSpray = 0.25f, shownPitch = 0.0f;
    float shownBufferFill = 0.0f;
    bool smoothedInit = false;
    float dustActivity = 0.0f;   // 0 = idle dust, 1 = grains dominate

    std::vector<Mote> motes;
    std::mt19937 rng { 0x4e41474cu };
    float windPhase = 0.0f;

    // Cached readout strings (regenerated at ~10 Hz, never per frame).
    juce::String focusText, sprayText, densityText, bufferText;
    juce::String headerInfoText, timelineInfoText;
    int readoutCounter = 0;

    // Cached fonts (rebuilt on scale change; no font construction in paint()).
    juce::Font headerFont { juce::FontOptions() }, timelineFont { juce::FontOptions() };
    juce::Font chipFont { juce::FontOptions() }, hudFont { juce::FontOptions() };
    juce::Font axisFont { juce::FontOptions() }, hintFont { juce::FontOptions() };

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


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainCloud)
};

} // namespace nagloed::ui
