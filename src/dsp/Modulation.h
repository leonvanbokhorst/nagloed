#pragma once

#include "DspUtil.h"

// -----------------------------------------------------------------------------
// Modulation sources for NAGLOED.
//
//  * Lfo          : one free-running LFO (sine/tri/saw/square/S&H) used for the
//                   explicit mod.lfo* destinations.
//  * SlowRandom   : filtered random walk used to build the "analog instability"
//                   and mod.auto generative motion.
//  * Modulation   : bundles the three analog drift generators (pitch/cutoff/pan)
//                   and the three hands-off auto generators (focus/spray/pitch).
//
// All of it advances at the engine control rate (every kControlInterval
// samples), which is fast enough for a 20 Hz LFO and cheap enough to leave the
// per-sample loop focused on the grains.
// -----------------------------------------------------------------------------
namespace nagloed::dsp
{

// Slow, smoothed random walk in [-1, 1]. Every `periodSeconds` a new random
// target is picked; a one-pole filter glides towards it so the output is a
// continuous, organic wobble rather than a sequence of steps.
class SlowRandom
{
public:
    void prepare (Rng* randomSource, double sr, float periodSeconds, float smoothSeconds,
                  int intervalSamples) noexcept
    {
        rng = randomSource;
        const float srF = (float) std::max (sr, 1.0);
        periodSamples = std::max (1, (int) std::lround (std::max (periodSeconds, 0.01f) * srF));
        countdown     = periodSamples;

        const float samples = (float) std::max (intervalSamples, 1);
        const float tau     = std::max (smoothSeconds, 1.0e-4f) * srF;
        coeff = clamp (1.0f - std::exp (-samples / tau), 0.0f, 1.0f);
    }

    void reset() noexcept
    {
        value    = 0.0f;
        target   = 0.0f;
        countdown = periodSamples;
    }

    inline float tick() noexcept
    {
        if (rng == nullptr)
            return 0.0f;

        if (--countdown <= 0)
        {
            target    = rng->bipolar();
            countdown = periodSamples;
        }

        value += (target - value) * coeff;
        return value;
    }

private:
    Rng*  rng           = nullptr;
    float value         = 0.0f;
    float target        = 0.0f;
    float coeff         = 0.0f;
    int   countdown     = 1;
    int   periodSamples = 1;
};

// One free-running LFO. Rate is passed in on every tick so it can be
// parameter-smoothed upstream (no zipper if the host automates mod.lfoRate).
class Lfo
{
public:
    void prepare (double sr, int intervalSamples) noexcept
    {
        sampleRate = std::max (sr, 1.0);
        interval   = std::max (intervalSamples, 1);
        reset();
    }

    void reset() noexcept
    {
        phase   = 0.0;
        lastOut = 0.0f;
        rng.seed (0x51ed270bu);
        hold = rng.bipolar();   // S&H must be alive from the first sample
    }

    // Returns -1..1. `shape` uses the order of lfoShapeNames():
    // 0 sine, 1 triangle, 2 saw up, 3 saw down, 4 square, 5 sample & hold.
    inline float tick (float rateHz, int shape) noexcept
    {
        const float rate = clamp (rateHz, 0.01f, 20.0f);
        phase += (double) rate * (double) interval / sampleRate;

        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            hold = rng.bipolar();   // new S&H value once per cycle
        }

        const float t = (float) phase;

        switch (shape)
        {
            case 1:  lastOut = 1.0f - 4.0f * std::abs (t - 0.5f); break;  // triangle
            case 2:  lastOut = 2.0f * t - 1.0f;                    break;  // saw up
            case 3:  lastOut = 1.0f - 2.0f * t;                    break;  // saw down
            case 4:  lastOut = t < 0.5f ? 1.0f : -1.0f;            break;  // square
            case 5:  lastOut = hold;                                break;  // S&H
            default: lastOut = sine (t);                            break;  // sine
        }

        return lastOut;
    }


private:
    Rng    rng;
    double sampleRate = 48000.0;
    double phase      = 0.0;
    int    interval   = 16;
    float  hold       = 0.0f;
    float  lastOut    = 0.0f;
};

// Everything the modulation section contributes to a control frame.
struct ModulationOutput
{
    float lfo              = 0.0f;   // -1..1
    float pitchDriftSemis  = 0.0f;   // analog instability, +/- a few cents
    float cutoffDriftOct   = 0.0f;   // analog instability on the ladder cutoff
    float panDrift         = 0.0f;   // analog instability on grain pan
    float wowDriftSamples  = 0.0f;   // extra tape delay wobble
    float focusAuto        = 0.0f;   // -1..1 * amount (mod.auto)
    float sprayAuto        = 0.0f;   // -1..1 * amount (mod.auto)
    float pitchAutoSemis   = 0.0f;   // semitones * amount (mod.auto)
};

// Bundles the LFO, the analog drift and the generative auto motion.
class Modulation
{
public:
    void prepare (double sr, int intervalSamples) noexcept
    {
        lfo.prepare (sr, intervalSamples);
        driftRng.seed (0x1a2b3c4du);
        autoRng.seed (0x7f4a2c11u);

        // Three drift voices at very different geological speeds: the slow one
        // is the "room breathing", the faster ones are capacitor/tape wobble.
        driftA.prepare (&driftRng, sr, 0.70f, 0.22f, intervalSamples);
        driftB.prepare (&driftRng, sr, 2.30f, 0.55f, intervalSamples);
        driftC.prepare (&driftRng, sr, 5.70f, 1.10f, intervalSamples);

        // Hands-off motion, slower still so a cloud evolves over tens of
        // seconds instead of wobbling obviously.
        autoFocus.prepare (&autoRng, sr, 7.10f, 2.40f, intervalSamples);
        autoSpray.prepare (&autoRng, sr, 4.30f, 1.60f, intervalSamples);
        autoPitch.prepare (&autoRng, sr, 11.30f, 3.20f, intervalSamples);

        reset();
    }

    void reset() noexcept
    {
        lfo.reset();
        driftA.reset();
        driftB.reset();
        driftC.reset();
        autoFocus.reset();
        autoSpray.reset();
        autoPitch.reset();
        out = ModulationOutput {};
    }

    // lfoRateHz  : mod.lfoRate (smoothed)
    // lfoShape   : EngineParams::lfoShape
    // driftAmount: 0..1, combines ear.drift + macro.drift
    // autoAmount : 0..1, mod.auto
    void update (float lfoRateHz, int lfoShape, float driftAmount, float autoAmount) noexcept
    {
        out.lfo = lfo.tick (lfoRateHz, lfoShape);

        const float dA = driftA.tick();
        const float dB = driftB.tick();
        const float dC = driftC.tick();
        const float drift = 0.55f * dA + 0.30f * dB + 0.15f * dC;

        // A small floor means nothing in NAGLOED is ever perfectly static, even
        // with ear.drift and macro.drift at zero: a quarter-tone of life.
        const float scale = 0.04f + 0.96f * clamp (driftAmount, 0.0f, 1.0f);

        out.pitchDriftSemis = drift * scale * 0.10f;   // up to ~10 cents
        out.cutoffDriftOct  = drift * scale * 0.10f;   // up to ~1 semitone of cutoff
        out.panDrift        = drift * scale * 0.08f;
        out.wowDriftSamples = drift * scale * 2.5f;    // extra tape flutter, samples

        const float autoAmt = clamp (autoAmount, 0.0f, 1.0f);
        out.focusAuto      = autoFocus.tick() * 0.30f * autoAmt;
        out.sprayAuto      = autoSpray.tick() * 0.30f * autoAmt;
        out.pitchAutoSemis = autoPitch.tick() * 2.5f * autoAmt;
    }

    const ModulationOutput& output() const noexcept { return out; }

private:
    Lfo        lfo;
    Rng        driftRng;
    Rng        autoRng;
    SlowRandom driftA, driftB, driftC;
    SlowRandom autoFocus, autoSpray, autoPitch;
    ModulationOutput out {};
};

} // namespace nagloed::dsp
