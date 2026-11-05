#include "EngineDSP.h"

#include <algorithm>
#include <cmath>

#include "core/Parameters.h"

namespace nagloed
{

namespace
{
// Parameter clamping helpers. EngineParams is a plain data blob: a host (or a
// test) can hand us anything, so every value is clamped before it reaches the
// DSP. A non-finite value falls back to the documented default.
inline float fit (float value, float fallback, float lo, float hi) noexcept
{
    if (! (value == value))
        return fallback;
    return dsp::clamp (value, lo, hi);
}

inline float clamp01 (float v) noexcept { return dsp::clamp (v, 0.0f, 1.0f); }

constexpr float kSampleRateReference = 48000.0f;
} // namespace

// =============================================================================
// Smoothed parameter set
// =============================================================================

void EngineDSP::SmoothedParams::prepare (double sr, int intervalSamples) noexcept
{
    const auto init = [sr, intervalSamples] (dsp::Smoothed& s, float tau)
    {
        s.prepare (sr, tau, intervalSamples);
    };

    // Grain scheduler: fast enough to feel immediate, slow enough that a knob
    // jump never produces a click or a density step.
    init (density, 0.20f);
    init (sizeMs, 0.15f);
    init (spray, 0.20f);
    init (scan, 0.40f);
    init (pitch, 0.02f);
    init (spread, 0.10f);
    init (reverse, 0.15f);
    init (jitter, 0.15f);
    init (shape, 0.12f);
    init (width, 0.20f);
    init (feedback, 0.30f);
    init (focus, 0.25f);

    // Space: size and modulation may glide slowly (it is a room, not a switch).
    init (spaceMix, 0.25f);
    init (spaceSize, 0.40f);
    init (spaceDamp, 0.10f);
    init (spaceMod, 0.20f);

    // EAR: cutoff intentionally fast (filters want to be played), the rest is
    // tape-like and takes its time.
    init (driveDb, 0.04f);
    init (bias, 0.05f);
    init (tone, 0.05f);
    init (cutoff, 0.02f);
    init (reso, 0.05f);
    init (drift, 0.30f);
    init (wow, 0.30f);
    init (tape, 0.08f);
    init (lofi, 0.04f);
    init (hiss, 0.10f);
    init (inputDb, 0.05f);
    init (outputDb, 0.05f);
    init (mix, 0.05f);

    init (macroGlow, 0.15f);
    init (macroDrift, 0.15f);
    init (macroGrain, 0.15f);
    init (macroSpace, 0.15f);
    init (macroShimmer, 0.15f);

    init (lfoRate, 0.30f);
    init (lfoAmount, 0.10f);
    init (autoMotion, 0.40f);
}

void EngineDSP::SmoothedParams::setTargets (const EngineParams& p) noexcept
{
    density.setTarget   (fit (p.density, 30.0f, 0.1f, 400.0f));
    sizeMs.setTarget    (fit (p.sizeMs, 80.0f, 5.0f, 500.0f));
    spray.setTarget     (clamp01 (p.spray));
    scan.setTarget      (dsp::clamp (p.scan, -1.0f, 1.0f));
    pitch.setTarget     (fit (p.pitchSemis, 0.0f, -24.0f, 24.0f));
    spread.setTarget    (fit (p.pitchSpread, 0.0f, 0.0f, 24.0f));
    reverse.setTarget   (clamp01 (p.reverse));
    jitter.setTarget    (clamp01 (p.jitter));
    shape.setTarget     (clamp01 (p.shape));
    width.setTarget     (clamp01 (p.width));
    feedback.setTarget  (fit (p.feedback, 0.0f, 0.0f, 0.95f));
    focus.setTarget     (clamp01 (p.focus));

    spaceMix.setTarget  (clamp01 (p.spaceMix));
    spaceSize.setTarget (clamp01 (p.spaceSize));
    spaceDamp.setTarget (clamp01 (p.spaceDamp));
    spaceMod.setTarget  (clamp01 (p.spaceMod));

    driveDb.setTarget   (fit (p.driveDb, 6.0f, 0.0f, 30.0f));
    bias.setTarget      (clamp01 (p.bias));
    tone.setTarget      (clamp01 (p.tone));
    cutoff.setTarget    (fit (p.cutoffHz, 9000.0f, 20.0f, 20000.0f));
    reso.setTarget      (clamp01 (p.reso));
    drift.setTarget     (clamp01 (p.drift));
    wow.setTarget       (clamp01 (p.wow));
    tape.setTarget      (clamp01 (p.tape));
    lofi.setTarget      (clamp01 (p.lofi));
    hiss.setTarget      (clamp01 (p.hiss));
    inputDb.setTarget   (fit (p.inputDb, 0.0f, -24.0f, 24.0f));
    outputDb.setTarget  (fit (p.outputDb, 0.0f, -24.0f, 6.0f));
    mix.setTarget       (clamp01 (p.mix));

    macroGlow.setTarget    (clamp01 (p.macroGlow));
    macroDrift.setTarget   (clamp01 (p.macroDrift));
    macroGrain.setTarget   (clamp01 (p.macroGrain));
    macroSpace.setTarget   (clamp01 (p.macroSpace));
    macroShimmer.setTarget (clamp01 (p.macroShimmer));

    lfoRate.setTarget    (fit (p.lfoRate, 0.15f, 0.01f, 20.0f));
    lfoAmount.setTarget  (dsp::clamp (p.lfoAmount, -1.0f, 1.0f));
    autoMotion.setTarget (clamp01 (p.autoMotion));
}

void EngineDSP::SmoothedParams::snap (const EngineParams& p) noexcept
{
    // Jump straight to the values (used on the first setParams after prepare, so
    // the plugin starts exactly where the preset says instead of ramping in).
    setTargets (p);

    density.snap (density.target);
    sizeMs.snap (sizeMs.target);
    spray.snap (spray.target);
    scan.snap (scan.target);
    pitch.snap (pitch.target);
    spread.snap (spread.target);
    reverse.snap (reverse.target);
    jitter.snap (jitter.target);
    shape.snap (shape.target);
    width.snap (width.target);
    feedback.snap (feedback.target);
    focus.snap (focus.target);

    spaceMix.snap (spaceMix.target);
    spaceSize.snap (spaceSize.target);
    spaceDamp.snap (spaceDamp.target);
    spaceMod.snap (spaceMod.target);

    driveDb.snap (driveDb.target);
    bias.snap (bias.target);
    tone.snap (tone.target);
    cutoff.snap (cutoff.target);
    reso.snap (reso.target);
    drift.snap (drift.target);
    wow.snap (wow.target);
    tape.snap (tape.target);
    lofi.snap (lofi.target);
    hiss.snap (hiss.target);
    inputDb.snap (inputDb.target);
    outputDb.snap (outputDb.target);
    mix.snap (mix.target);

    macroGlow.snap (macroGlow.target);
    macroDrift.snap (macroDrift.target);
    macroGrain.snap (macroGrain.target);
    macroSpace.snap (macroSpace.target);
    macroShimmer.snap (macroShimmer.target);

    lfoRate.snap (lfoRate.target);
    lfoAmount.snap (lfoAmount.target);
    autoMotion.snap (autoMotion.target);
}

void EngineDSP::SmoothedParams::tick() noexcept
{
    density.tick();  sizeMs.tick();   spray.tick();    scan.tick();
    pitch.tick();    spread.tick();   reverse.tick();  jitter.tick();
    shape.tick();    width.tick();    feedback.tick(); focus.tick();

    spaceMix.tick(); spaceSize.tick(); spaceDamp.tick(); spaceMod.tick();

    driveDb.tick();  bias.tick();     tone.tick();     cutoff.tick();
    reso.tick();     drift.tick();    wow.tick();      tape.tick();
    lofi.tick();     hiss.tick();     inputDb.tick();  outputDb.tick();
    mix.tick();

    macroGlow.tick(); macroDrift.tick(); macroGrain.tick();
    macroSpace.tick(); macroShimmer.tick();

    lfoRate.tick(); lfoAmount.tick(); autoMotion.tick();
}

// =============================================================================
// Lifecycle
// =============================================================================

EngineDSP::EngineDSP() = default;
EngineDSP::~EngineDSP() = default;

void EngineDSP::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = (sr > 0.0 ? sr : 48000.0);
    maxBlock   = std::max (1, std::min (maxBlockSize, 8192));
    // The internal graph is always stereo. The host channel count is taken from
    // the buffer at process() time: mono buffers are explicitly folded into the
    // stereo graph on input and downmixed on output, so prepare() has nothing
    // channel-dependent to do.
    (void) numChannels;

    dsp::prepareSharedTables();

    grainEngine.prepare (sampleRate);
    reverb.prepare (sampleRate);
    analogChain.prepare (sampleRate, maxBlock);
    modulation.prepare (sampleRate, kControlInterval);
    gloop.prepare (sampleRate);
    smooth.prepare (sampleRate, kControlInterval);

    inputL.assign ((size_t) maxBlock, 0.0f);
    inputR.assign ((size_t) maxBlock, 0.0f);
    earL.assign ((size_t) maxBlock, 0.0f);
    earR.assign ((size_t) maxBlock, 0.0f);

    // One control group per control frame plus one for a frame that spans the
    // block boundary.
    controlGroups.assign ((size_t) (maxBlock / kControlInterval + 2), dsp::AnalogChain::Group {});

    prepared = true;
    reset();
}

void EngineDSP::reset()
{
    if (! prepared)
        return;

    grainEngine.reset();
    reverb.reset();
    analogChain.reset();
    modulation.reset();
    gloop.reset();

    sampleCounter = 0;

    std::fill (inputL.begin(), inputL.end(), 0.0f);
    std::fill (inputR.begin(), inputR.end(), 0.0f);
    std::fill (earL.begin(), earL.end(), 0.0f);
    std::fill (earR.begin(), earR.end(), 0.0f);

    visualInL = visualInR = visualOutL = visualOutR = 0.0f;
    visualInPeak = visualOutPeak = 0.0f;
    visualFocus = 0.5f;
    visualSpray = 0.25f;
    visualLfo = 0.0f;
    visualCutoff = 9000.0f;
    visualDensity = 30.0f;
    visualDriveDb = 6.0f;
    visualLofi = 0.0f;
    visualFrame = 0;

    accumInL = accumInR = accumOutL = accumOutR = 0.0;
    accumSamples = 0;

    // The engine starts from the documented EngineParams defaults; the first
    // setParams() snaps the smoothers straight to the caller's values.
    smooth.snap (EngineParams {});
    needsSnap = true;

    updateControlFrame();
}

void EngineDSP::setParams (const EngineParams& p) noexcept
{
    params = p;

    if (! prepared)
        return;

    if (needsSnap)
    {
        smooth.snap (p);
        needsSnap = false;
    }
    else
    {
        smooth.setTargets (p);
    }
}

bool EngineDSP::publishGloopGesture (int lane, const gloop::Gesture& gesture) noexcept
{
    // Message thread only. The GloopEngine queue is preallocated, so this copies
    // and publishes without allocating; a full queue is reported, never waited on.
    return gloop.publish (lane, gesture);
}

bool EngineDSP::publishGloopGesture (int lane, const float* offsets, int count,
                                     float durationSeconds) noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes || count < 0
        || count > gloop::kMaxGestureSamples || (count > 0 && offsets == nullptr))
        return false;

    gloop::Gesture gesture;
    gesture.count = count;
    gesture.durationSeconds = durationSeconds;
    gesture.numTracks = count >= 2 ? 1 : 0;

    if (count > 0)
    {
        int target = params.gloopLanes[(size_t) lane].target;

        if (target <= (int) gloop::Target::Off || target >= gloop::kNumTargets)
            target = (int) gloop::Target::Focus;

        gesture.tracks[0].target = target;
        std::copy (offsets, offsets + count, gesture.tracks[0].offsets.begin());
    }

    return gloop.publish (lane, gesture);
}

// =============================================================================
// Control rate
// =============================================================================

void EngineDSP::updateControlFrame() noexcept
{
    smooth.tick();

    // Gloop advances at the same control rate as the rest of the matrix: one
    // frame is exactly kControlInterval samples, so a lane's phase, fades and
    // glides depend on seconds rather than on the host block size.
    gloop.processFrame ((double) kControlInterval / sampleRate);

    const auto& p = params;

    // Gloop is applied to the *raw* smoothed knob value (the "live centre" the
    // UI ring starts from) before the macros and the LFO add their contribution.
    // That keeps the ring and the audible centre in agreement, and it means an
    // active lane can never clamp a macro-raised value back into the knob's
    // range. gloopApplyOffset() returns its input untouched for a zero offset,
    // so all-idle lanes stay bit-identical to the pre-Gloop engine.
    const auto gloopOffset = [this] (gloop::Target target) noexcept
    {
        return gloop.offsetFor (target);
    };

    const auto gloopKnob = [&gloopOffset] (gloop::Target target, float knobValue) noexcept
    {
        return gloopApplyOffset ((int) target, knobValue, gloopOffset (target));
    };

    // --- macros --------------------------------------------------------------
    // Mappings (all additive on top of the manual parameters):
    //   macroGrain   : density x 2^(+/-1.2 oct), spray +/-, width +/-, spread
    //                  +/-6 st — turns the cloud from sparse/dry to dense/wide.
    //   macroShimmer : probability that a grain jumps +1 octave (harp-like
    //                  glints) plus extra space mix/mod.
    //   macroGlow    : drive +8 dB, cutoff down 1.2 oct, hiss +0.35, tone darker
    //                  — the "warm old tape" macro.
    //   macroDrift   : ear drift + wow + tape + extra pitch instability.
    //   macroSpace   : space mix +0.35, size +0.25, mod +0.30.
    const float macroGrain   = smooth.macroGrain.value();
    const float macroShimmer = smooth.macroShimmer.value();
    const float macroGlow    = smooth.macroGlow.value();
    const float macroDrift   = smooth.macroDrift.value();
    const float macroSpace   = smooth.macroSpace.value();

    // --- modulation sources --------------------------------------------------
    const float driftAmount = clamp01 (gloopKnob (gloop::Target::Drift, smooth.drift.value())
                                           + 0.8f * macroDrift);
    const float autoAmount  = clamp01 (smooth.autoMotion.value());
    modulation.update (smooth.lfoRate.value(), p.lfoShape, driftAmount, autoAmount);

    const auto& mod = modulation.output();
    visualLfo = mod.lfo;

    // --- LFO destinations ----------------------------------------------------
    const int   lfoDest   = (p.lfoDest >= 0 && p.lfoDest <= 6) ? p.lfoDest : 0;
    const float lfoAmount = dsp::clamp (smooth.lfoAmount.value(), -1.0f, 1.0f);
    const float lfo       = mod.lfo;

    float focusLfo      = 0.0f;
    float pitchLfoSemis = 0.0f;
    float cutoffLfoOct  = 0.0f;
    float panLfo        = 0.0f;
    float spaceLfo      = 0.0f;
    float densityLfoMul = 1.0f;

    switch (lfoDest)
    {
        case 1: focusLfo      = lfoAmount * lfo * 0.60f; break;              // Focus
        case 2: pitchLfoSemis = lfoAmount * lfo * 12.0f; break;              // Pitch
        case 3: cutoffLfoOct  = lfoAmount * lfo * 4.0f;  break;              // Cutoff
        case 4: panLfo        = lfoAmount * lfo;         break;              // Pan
        case 5: spaceLfo      = lfoAmount * lfo * 0.40f; break;              // Space
        case 6: densityLfoMul = std::pow (2.0f, lfoAmount * lfo * 1.5f); break; // Density
        default: break;                                                      // Off
    }

    // --- Sintel controls -----------------------------------------------------
    const float grainOffset = (macroGrain - 0.5f) * 2.0f;   // -1..1, 0 at centre

    const float density = dsp::clamp (gloopKnob (gloop::Target::Density, smooth.density.value())
                                          * std::pow (2.0f, grainOffset * 1.2f)
                                          * densityLfoMul,
                                      0.1f, 400.0f);
    const float grainSeconds = dsp::clamp (gloopKnob (gloop::Target::Grain,
                                                      smooth.sizeMs.value()),
                                           5.0f, 500.0f)
                               * 0.001f;
    const float spray   = clamp01 (gloopKnob (gloop::Target::Spray, smooth.spray.value())
                                       + 0.30f * mod.sprayAuto + 0.25f * grainOffset);
    const float focus   = clamp01 (gloopKnob (gloop::Target::Focus, smooth.focus.value())
                                       + 0.30f * mod.focusAuto + focusLfo);
    const float width   = clamp01 (gloopKnob (gloop::Target::Width, smooth.width.value())
                                       + 0.35f * grainOffset);
    const float spread  = dsp::clamp (gloopKnob (gloop::Target::Spread, smooth.spread.value())
                                          + 6.0f * grainOffset,
                                      0.0f, 24.0f);
    const float shape   = clamp01 (gloopKnob (gloop::Target::Shape, smooth.shape.value()));
    const float scan    = dsp::clamp (smooth.scan.value(), -1.0f, 1.0f);
    const float pitch   = dsp::clamp (gloopKnob (gloop::Target::Pitch, smooth.pitch.value())
                                          + mod.pitchAutoSemis + mod.pitchDriftSemis
                                          + pitchLfoSemis,
                                      -48.0f, 48.0f);
    const float feedback = dsp::clamp (gloopKnob (gloop::Target::Feedback,
                                                  smooth.feedback.value()),
                                       0.0f, 0.95f);

    // Feedback stability, part 1: reading the buffer faster than real time
    // multiplies the energy that the feedback loop sees, so upward pitch scales
    // the feedback down (1/sqrt(pitchRatio)).
    const float maxUpPitch = dsp::clamp (pitch + spread * 0.5f
                                             + (macroShimmer > 0.02f ? 12.0f : 0.0f),
                                         -48.0f, 48.0f);
    const float feedbackPitchScale = maxUpPitch <= 0.0f
                                         ? 1.0f
                                         : std::pow (2.0f, -0.5f * maxUpPitch / 12.0f);

    // Automatic level compensation for the cloud: keep the summed level roughly
    // constant as density/size change. The soft limiter downstream handles the
    // residual peaks.
    const float activeEstimate = std::max (density * grainSeconds, 0.6f);
    const float grainGain = dsp::clamp (1.6f / std::sqrt (activeEstimate), 0.05f, 1.3f);

    // Feedback stability, part 2: when grains are correlated (same pitch, no
    // spray/scatter) their windows add up, so the summed bus can amplify the
    // capture buffer by roughly (overlap * grain gain * average window value),
    // folded to mono by the equal-power pan (~0.71). Scaling the regeneration
    // by the inverse of that worst case keeps the loop gain below unity at every
    // density and every window shape, so the cloud decays once the source
    // stops — while sparse/dense clouds still receive a musical amount.
    const float shapePos = shape * 2.0f;
    const float windowMean = shapePos <= 1.0f
                                 ? 0.50f + 0.25f * shapePos          // Hann -> Tukey
                                 : 0.75f - 0.45f * (shapePos - 1.0f); // Tukey -> expodec
    const float worstCaseLoopGain = activeEstimate * grainGain * windowMean * 0.80f;
    const float feedbackLoopScale = dsp::clamp (1.0f / (worstCaseLoopGain * 1.10f),
                                                0.02f, 1.0f);

    grainControls.densityHz     = density;
    grainControls.grainSeconds  = grainSeconds;
    grainControls.spray         = spray;
    grainControls.scan          = scan;
    grainControls.pitchSemis    = pitch;
    grainControls.pitchSpread   = spread;
    grainControls.reverse       = clamp01 (smooth.reverse.value());
    grainControls.jitter        = clamp01 (smooth.jitter.value());
    grainControls.shape         = shape;
    grainControls.width         = width;
    grainControls.feedback      = feedback;
    grainControls.focus         = focus;
    grainControls.panOffset     = dsp::clamp (panLfo + mod.panDrift, -1.0f, 1.0f);
    grainControls.shimmer       = clamp01 (macroShimmer);
    grainControls.gain          = grainGain;
    grainControls.feedbackScale = feedbackPitchScale * feedbackLoopScale;
    effectiveFeedback = feedback * grainControls.feedbackScale;
    grainControls.freeze        = p.freeze;
    grainControls.tuneMode      = (p.tuneMode > 0 && p.tuneMode < dsp::kNumTuneModes) ? p.tuneMode : 0;
    grainControls.sourceMode    = (p.sourceMode >= 0 && p.sourceMode <= 2) ? p.sourceMode : 0;

    visualFocus   = focus;
    visualSpray   = spray;
    visualDensity = density;

    // --- EAR: cutoff / resonance --------------------------------------------
    const float cutoff = dsp::clamp (gloopKnob (gloop::Target::Cutoff, smooth.cutoff.value())
                                         * std::pow (2.0f, -1.2f * macroGlow
                                                              + mod.cutoffDriftOct
                                                              + cutoffLfoOct),
                                     20.0f, 20000.0f);

    // --- Ruimte --------------------------------------------------------------
    reverbControls.size = clamp01 (smooth.spaceSize.value() + 0.25f * macroSpace);
    reverbControls.damp = clamp01 (smooth.spaceDamp.value());
    reverbControls.mod  = clamp01 (smooth.spaceMod.value() + 0.30f * macroSpace
                                       + 0.15f * macroShimmer);

    currentSpaceMix = clamp01 (gloopKnob (gloop::Target::Space, smooth.spaceMix.value())
                                   + 0.35f * macroSpace + 0.12f * macroShimmer + spaceLfo);

    reverb.update (reverbControls);

    // --- EAR: full character chain ------------------------------------------
    const float srScale   = (float) (sampleRate / (double) kSampleRateReference);
    const float osRate    = (float) sampleRate * 2.0f;

    const float driveDb   = dsp::clamp (gloopKnob (gloop::Target::Drive, smooth.driveDb.value())
                                            + 8.0f * macroGlow,
                                        0.0f, 36.0f);
    const float hissEff   = clamp01 (smooth.hiss.value() + 0.35f * macroGlow);
    const float toneEff   = clamp01 (gloopKnob (gloop::Target::Tone, smooth.tone.value())
                                         - 0.20f * macroGlow);
    const float reso      = clamp01 (gloopKnob (gloop::Target::Reso, smooth.reso.value()));
    const float wowEff    = clamp01 (gloopKnob (gloop::Target::Wow, smooth.wow.value())
                                         + 0.5f * macroDrift);
    const float tapeEff   = clamp01 (gloopKnob (gloop::Target::Tape, smooth.tape.value())
                                         + 0.4f * macroDrift);
    const float lofiEff   = clamp01 (smooth.lofi.value());

    earControls.inputGain = dsp::gainFromDb (fit (smooth.inputDb.value(), 0.0f, -24.0f, 24.0f));
    earControls.driveGain = dsp::gainFromDb (driveDb);

    // Tone tilt: d in -0.75..+0.75, low band gain (1-d), high band (1+d), so the
    // mid level stays put while the balance tips dark or bright.
    const float tilt = (toneEff - 0.5f) * 1.5f;
    earControls.toneLow  = 1.0f - tilt;
    earControls.toneHigh = 1.0f + tilt;

    earControls.bias = clamp01 (smooth.bias.value());

    // 24 dB ZDF ladder cutoff. The 1.35 factor compensates the -12 dB point of
    // a 4-pole cascade so the knob tracks the perceived cutoff; the bilinear
    // prewarp (tan) keeps the filter tuning accurate up to the top of the range.
    const float ladderCutoff = dsp::clamp (cutoff * 1.35f, 20.0f, osRate * 0.45f);
    earControls.ladderG = dsp::clamp (std::tan (dsp::kPi * ladderCutoff / osRate), 1.0e-4f, 8.0f);

    // k = 4 is the self-oscillation threshold of the 4-pole ladder; the tanh in
    // the loop makes that oscillation bounded rather than explosive.
    earControls.ladderK = reso * 4.0f;

    // Tape transport: even at wow = 0 a couple of samples of movement keeps the
    // character alive without sounding out of tune.
    earControls.wowDepth     = (2.0f + 26.0f * wowEff) * srScale;
    earControls.flutterDepth = (0.5f + 2.8f * wowEff) * srScale;
    earControls.driftDepth   = mod.wowDriftSamples * srScale;

    const float tapeDrive = 1.0f + 5.0f * tapeEff;
    earControls.tapeDrive = tapeDrive;
    earControls.tapeComp  = 1.0f / tapeDrive;

    earControls.lofiActive = lofiEff > 0.01f;
    earControls.lofiHold   = (1.0f + 44.0f * lofiEff) * srScale;
    earControls.lofiLevels = std::pow (2.0f, 16.0f - 12.0f * lofiEff);
    earControls.hissActive = hissEff > 0.001f;
    earControls.hissGain   = hissEff * 0.02f;
    earControls.outputGain = dsp::gainFromDb (fit (smooth.outputDb.value(), 0.0f, -24.0f, 6.0f));
    earControls.mix        = clamp01 (smooth.mix.value());

    visualCutoff  = cutoff;
    visualDriveDb = driveDb;
    visualLofi    = lofiEff;
    grainControls.brightness = dsp::logNorm (cutoff, 20.0f, 20000.0f);
}

// =============================================================================
// Audio processing
// =============================================================================

void EngineDSP::process (juce::AudioBuffer<float>& io, const juce::MidiBuffer& midi) noexcept
{
    (void) midi;

    if (! prepared)
    {
        io.clear();
        return;
    }

    // Install any Gloop take published by the message thread *before* the first
    // control frame of this block, so a take that arrives together with a LOOP
    // request is audible in this block instead of the next one.
    gloop.beginBlock (params);

    const int numSamples = io.getNumSamples();
    const int numCh      = io.getNumChannels();
    if (numSamples <= 0 || numCh <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    // Defensive: the host may hand us more than two channels even though the
    // engine only speaks stereo. Never leave uninitialised audio behind.
    for (int ch = 2; ch < numCh; ++ch)
        io.clear (ch, 0, numSamples);

    const bool mono = (numCh < 2);
    float* ioL = io.getWritePointer (0);
    float* ioR = mono ? ioL : io.getWritePointer (1);

    accumInL = accumInR = accumOutL = accumOutR = 0.0;
    accumSamples = 0;
    visualInPeak = 0.0f;
    visualOutPeak = 0.0f;
    preLimiterPeak = 0.0f;
    nonFiniteOutput = false;

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min (maxBlock, numSamples - offset);
        processChunk (ioL + offset, ioR + offset, chunk, mono);
        offset += chunk;
    }

    if (accumSamples > 0)
    {
        const double norm = 1.0 / (double) accumSamples;
        visualInL  = (float) std::sqrt (accumInL * norm);
        visualInR  = (float) std::sqrt (accumInR * norm);
        visualOutL = (float) std::sqrt (accumOutL * norm);
        visualOutR = (float) std::sqrt (accumOutR * norm);
    }

    ++visualFrame;
}

void EngineDSP::processChunk (float* ioL, float* ioR, int numSamples, bool mono) noexcept
{
    // -------------------------------------------------------------------------
    // 1. Copy the input into the dry buffer (and meter it).
    // -------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = dsp::sanitiseInput (ioL[i]);
        const float r = mono ? l : dsp::sanitiseInput (ioR[i]);

        inputL[(size_t) i] = l;
        inputR[(size_t) i] = r;

        accumInL += (double) l * (double) l;
        accumInR += (double) r * (double) r;
        visualInPeak = std::max (visualInPeak, std::max (std::abs (l), std::abs (r)));
    }

    // -------------------------------------------------------------------------
    // 2. Sintel + Ruimte, one sample at a time, with control-rate frames.
    // -------------------------------------------------------------------------
    int groupCount = 1;
    controlGroups[0].start = 0;
    controlGroups[0].controls = earControls;

    for (int i = 0; i < numSamples; ++i)
    {
        // Absolute sample counter (not block-relative) so the control rate and
        // therefore the whole audio result are independent of the block size.
        if ((sampleCounter % (std::uint64_t) kControlInterval) == 0)
        {
            updateControlFrame();

            if (i == 0)
            {
                controlGroups[0].controls = earControls;
            }
            else if (groupCount < (int) controlGroups.size())
            {
                controlGroups[(size_t) groupCount].start = i;
                controlGroups[(size_t) groupCount].controls = earControls;
                ++groupCount;
            }
        }

        const auto grain = grainEngine.process (inputL[(size_t) i], inputR[(size_t) i], grainControls);
        const auto wet   = reverb.process (grain.left, grain.right);

        earL[(size_t) i] = grain.left + (wet.left - grain.left) * currentSpaceMix;
        earR[(size_t) i] = grain.right + (wet.right - grain.right) * currentSpaceMix;

        ++sampleCounter;
    }

    // -------------------------------------------------------------------------
    // 3. EAR: analog character, output limiter and the final dry/wet blend.
    // -------------------------------------------------------------------------
    analogChain.process (earL.data(), earR.data(),
                         inputL.data(), inputR.data(),
                         numSamples,
                         controlGroups.data(), groupCount);

    // -------------------------------------------------------------------------
    // 4. Write back (mono downmix when the host bus is mono) and meter.
    // -------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        float l = earL[(size_t) i];
        float r = earR[(size_t) i];

        // QA probe: record the signal *before* the final safety stage. Tests
        // assert on this, so "finite/loudness" checks cannot be satisfied by
        // the clamp hiding a diverging engine.
        const auto probe = [this] (float v) noexcept
        {
            if (v == v && std::abs (v) < 1.0e12f)
                preLimiterPeak = std::max (preLimiterPeak, std::abs (v));
            else
                nonFiniteOutput = true;
        };
        probe (l);
        probe (r);

        // Final safety stage: a soft analog-style limiter on the *post-mix*
        // signal (so the dry side is limited too), then a hard +/-6 dBFS
        // guarantee, and NaN/Inf neutralisation as the last line of defence.
        l = (l == l) ? dsp::softLimitOutput (l) : 0.0f;
        r = (r == r) ? dsp::softLimitOutput (r) : 0.0f;
        l = dsp::clamp (l, -2.0f, 2.0f);
        r = dsp::clamp (r, -2.0f, 2.0f);

        if (mono)
        {
            const float m = 0.5f * (l + r);
            ioL[i] = m;
            accumOutL += (double) m * (double) m;
            accumOutR += (double) m * (double) m;
            visualOutPeak = std::max (visualOutPeak, std::abs (m));
        }
        else
        {
            ioL[i] = l;
            ioR[i] = r;
            accumOutL += (double) l * (double) l;
            accumOutR += (double) r * (double) r;
            visualOutPeak = std::max (visualOutPeak, std::max (std::abs (l), std::abs (r)));
        }

        ++accumSamples;
    }
}

// =============================================================================
// Visualisation and latency
// =============================================================================

void EngineDSP::getVisual (VisualState& out) const noexcept
{
    out = VisualState {};

    if (! prepared)
        return;

    out.numGrains = grainEngine.copyGrainViews (out.grains.data(), VisualState::kMaxGrainViews);

    out.inL    = visualInL;
    out.inR    = visualInR;
    out.outL   = visualOutL;
    out.outR   = visualOutR;
    out.inPeak = visualInPeak;
    out.outPeak = visualOutPeak;

    out.focus       = visualFocus;
    out.spray       = visualSpray;
    out.lfo         = visualLfo;
    out.cutoffNorm  = dsp::logNorm (visualCutoff, 20.0f, 20000.0f);
    out.densityNorm = dsp::clamp (visualDensity / 200.0f, 0.0f, 1.0f);
    out.grainRate   = grainEngine.grainRate();
    out.bufferFill  = grainEngine.bufferFill();
    out.reduce      = visualLofi;
    out.drive       = dsp::clamp (visualDriveDb / 30.0f, 0.0f, 1.0f);

    // Gloop lanes: playhead, actual mode and the effective offset this frame, so
    // a target knob can draw its moving ring without ever reading the APVTS.
    gloop.copyVisual (out.gloop, visualFrame);

    // No note input in NAGLOED: it is an effect/instrument that never sees MIDI.
    out.mpePressure = 0.0f;
    out.activeNotes = 0;
    out.frame       = visualFrame;
}

int EngineDSP::getLatencySamples() const noexcept
{
    return analogChain.latencySamples();
}

} // namespace nagloed
