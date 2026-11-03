#include "DspTests.h"
#include "AllocCounter.h"
#include "TestHarness.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "core/EngineParams.h"
#include "dsp/AnalogChain.h"
#include "dsp/DspUtil.h"
#include "dsp/Engine.h"
#include "dsp/EngineDSP.h"

// =============================================================================
// Helpers
// =============================================================================

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

using nagl_test::check;
using nagl_test::checkInRange;
using nagl_test::checkNear;
using nagl_test::info;

std::string fmt (const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start (args, format);
    std::vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);
    return std::string (buffer);
}

// A deterministic test RNG (no std::random dependency, identical on every run).
struct TestRng
{
    std::uint32_t state = 0x12345678u;

    std::uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float range (float lo, float hi) noexcept { return lo + uniform() * (hi - lo); }
    int   intRange (int lo, int hi) noexcept { return lo + (int) (next() % (std::uint32_t) (hi - lo + 1)); }
};

struct Stats
{
    double rms    = 0.0;
    double rmsL   = 0.0;
    double rmsR   = 0.0;
    float  peak   = 0.0f;
    bool   finite = true;
};

template <typename Processor, typename Generator>
Stats render (Processor& engine, long long totalSamples, int blockSize, int channels,
              Generator generator)
{
    Stats stats;
    juce::MidiBuffer midi;

    double energy = 0.0, energyL = 0.0, energyR = 0.0;
    long long position = 0;

    while (position < totalSamples)
    {
        const int n = (int) std::min<long long> (blockSize, totalSamples - position);

        juce::AudioBuffer<float> buffer (channels, n);
        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                data[i] = generator (ch, position + i);
        }

        engine.process (buffer, midi);

        for (int ch = 0; ch < channels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float v = data[i];

                if (! std::isfinite (v))
                    stats.finite = false;

                stats.peak = std::max (stats.peak, std::abs (v));
                energy += (double) v * (double) v;

                if (ch == 0)
                    energyL += (double) v * (double) v;
                else if (ch == 1)
                    energyR += (double) v * (double) v;
            }
        }

        position += n;
    }

    const double sampleCount = (double) std::max<long long> (1, totalSamples);
    stats.rms  = std::sqrt (energy / (sampleCount * (double) std::max (1, channels)));
    stats.rmsL = std::sqrt (energyL / sampleCount);
    stats.rmsR = std::sqrt (energyR / sampleCount);
    return stats;
}

// Renders through EngineDSP while checking the *pre-safety-limiter* signal on
// every block. "Finite"/"peak" assertions on this data cannot be satisfied by
// the engine's final output clamp hiding a diverging graph.
template <typename Generator>
Stats renderProbed (nagloed::EngineDSP& engine, long long totalSamples, int blockSize, int channels,
                    Generator generator, float& maxPreLimiterPeak, bool& anyNonFinite)
{
    Stats stats;
    juce::MidiBuffer midi;

    double energy = 0.0;
    long long position = 0;

    while (position < totalSamples)
    {
        const int n = (int) std::min<long long> (blockSize, totalSamples - position);

        juce::AudioBuffer<float> buffer (channels, n);
        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                data[i] = generator (ch, position + i);
        }

        engine.process (buffer, midi);

        if (engine.hadNonFiniteOutput())
            anyNonFinite = true;

        maxPreLimiterPeak = std::max (maxPreLimiterPeak, engine.getPreLimiterPeak());

        for (int ch = 0; ch < channels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float v = data[i];
                if (! std::isfinite (v))
                    stats.finite = false;

                stats.peak = std::max (stats.peak, std::abs (v));
                energy += (double) v * (double) v;
            }
        }

        position += n;
    }

    stats.rms = std::sqrt (energy / ((double) std::max<long long> (1, totalSamples)
                                     * (double) std::max (1, channels)));
    return stats;
}

auto silence = [] (int, long long) { return 0.0f; };

auto sine220 = [] (int, long long i)
{
    return 0.20f * (float) std::sin (kTwoPi * 220.0 * (double) i / kSampleRate);
};

auto sineStereo = [] (int ch, long long i)
{
    const double f = (ch == 0 ? 220.0 : 330.0);
    return 0.18f * (float) std::sin (kTwoPi * f * (double) i / kSampleRate);
};

// A clean starting point: the grain engine alone, no space, no colour. Tests
// then opt into the sections they exercise. This keeps silence/finiteness tests
// independent of whether tape hiss or analog drift is audible.
nagloed::EngineParams cleanParams()
{
    nagloed::EngineParams p;
    p.density      = 40.0f;
    p.sizeMs       = 100.0f;
    p.spray        = 0.35f;
    p.focus        = 0.6f;
    p.spaceMix     = 0.0f;
    p.spaceSize    = 0.0f;
    p.spaceDamp    = 0.0f;
    p.spaceMod     = 0.0f;
    p.driveDb      = 0.0f;
    p.bias         = 0.0f;
    p.tone         = 0.5f;
    p.cutoffHz     = 20000.0f;
    p.reso         = 0.0f;
    p.drift        = 0.0f;
    p.wow          = 0.0f;
    p.tape         = 0.0f;
    p.lofi         = 0.0f;
    p.hiss         = 0.0f;
    p.inputDb      = 0.0f;
    p.outputDb     = 0.0f;
    p.mix          = 1.0f;
    p.macroGlow    = 0.0f;
    p.macroDrift   = 0.0f;
    p.macroGrain   = 0.5f;    // neutral
    p.macroSpace   = 0.0f;
    p.macroShimmer = 0.0f;
    p.lfoAmount    = 0.0f;
    p.autoMotion   = 0.0f;
    return p;
}

// A "musical default" setup used for the long render and the CPU benchmark.
nagloed::EngineParams musicalParams()
{
    nagloed::EngineParams p;
    p.density      = 80.0f;
    p.sizeMs       = 180.0f;
    p.spray        = 0.45f;
    p.scan         = 0.1f;
    p.pitchSemis   = 0.0f;
    p.pitchSpread  = 3.0f;
    p.reverse      = 0.2f;
    p.jitter       = 0.35f;
    p.shape        = 0.25f;
    p.width        = 0.7f;
    p.feedback     = 0.25f;
    p.focus        = 0.55f;
    p.tuneMode     = nagloed::dsp::tuneMinor;
    p.spaceMix     = 0.35f;
    p.spaceSize    = 0.6f;
    p.spaceDamp    = 0.4f;
    p.spaceMod     = 0.3f;
    p.driveDb      = 6.0f;
    p.bias         = 0.15f;
    p.tone         = 0.5f;
    p.cutoffHz     = 9000.0f;
    p.reso         = 0.2f;
    p.drift        = 0.3f;
    p.wow          = 0.25f;
    p.tape         = 0.35f;
    p.lofi         = 0.05f;
    p.hiss         = 0.06f;
    p.outputDb     = 0.0f;
    p.mix          = 0.8f;
    p.macroGlow    = 0.3f;
    p.macroDrift   = 0.3f;
    p.macroGrain   = 0.5f;
    p.macroSpace   = 0.4f;
    p.macroShimmer = 0.2f;
    p.lfoRate      = 0.15f;
    p.lfoShape     = 0;
    p.lfoDest      = 1;      // Focus
    p.lfoAmount    = 0.2f;
    p.autoMotion   = 0.15f;
    return p;
}

// =============================================================================
// Tests
// =============================================================================

void testSilenceInSilenceOut()
{
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = cleanParams();
    p.density = 60.0f;
    engine.setParams (p);

    const auto stats = render (engine, 48000, 512, 2, silence);

    info (fmt ("silence: peak %.9f", (double) stats.peak));
    check (stats.finite, "silence in -> silence out: output is finite");
    check (stats.peak < 1.0e-6f, "silence in -> silence out: peak is effectively zero");
}

void testSelfGeneratingSources()
{
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto noise = cleanParams();
    noise.sourceMode = 1;   // Noise
    noise.density = 50.0f;
    noise.sizeMs = 140.0f;
    engine.setParams (noise);

    const auto noiseStats = render (engine, 96000, 512, 2, silence);

    nagloed::VisualState v;
    engine.getVisual (v);

    info (fmt ("noise source: rms %.4f, grains %d, bufferFill %.3f",
               noiseStats.rms, v.numGrains, (double) v.bufferFill));
    check (noiseStats.finite, "noise source: output is finite");
    check (noiseStats.rms > 1.0e-3, "noise source generates material with a silent input");
    check (noiseStats.peak <= 2.0f, "noise source: output stays within the safety ceiling");
    check (v.bufferFill > 0.0f, "noise source fills the capture buffer");

    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto drone = cleanParams();
    drone.sourceMode = 2;   // Drone
    drone.density = 80.0f;
    drone.sizeMs = 100.0f;
    drone.pitchSemis = 12.0f;   // the cluster (and therefore the visual pitch) is up an octave
    engine.setParams (drone);

    const auto droneStats = render (engine, 96000, 512, 2, silence);
    check (droneStats.finite, "drone source: output is finite");
    check (droneStats.rms > 1.0e-3, "drone source generates material with a silent input");
    check (droneStats.peak <= 2.0f, "drone source: output stays within the safety ceiling");
    info (fmt ("drone source: rms %.4f", droneStats.rms));

    nagloed::VisualState droneVisual;
    engine.getVisual (droneVisual);
    double pitchSum = 0.0;
    int pitchCount = 0;
    for (int g = 0; g < droneVisual.numGrains; ++g)
    {
        pitchSum += droneVisual.grains[(size_t) g].pitch;
        ++pitchCount;
    }
    const double meanPitch = pitchCount > 0 ? pitchSum / pitchCount : 0.0;
    check (pitchCount > 0, "drone source: grain views are visible");
    check (std::abs (meanPitch - 12.0) < 1.0,
           fmt ("drone source: grain views include the cluster pitch (mean %.2f st)", meanPitch));
}

void testFreezeKeepsGrainsAlive()
{
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = cleanParams();
    p.density = 90.0f;
    p.sizeMs = 150.0f;
    p.spray = 0.4f;
    p.focus = 0.5f;
    engine.setParams (p);

    // Capture 2 seconds of tone.
    render (engine, 96000, 512, 2, sine220);

    nagloed::VisualState before;
    engine.getVisual (before);

    // Freeze and stop feeding input: the grains must keep playing the captured
    // material, and the capture must stop growing.
    auto frozen = p;
    frozen.freeze = true;
    engine.setParams (frozen);

    const auto stats = render (engine, 96000, 512, 2, silence);

    nagloed::VisualState after;
    engine.getVisual (after);

    info (fmt ("freeze: rms %.4f, fill before %.4f after %.4f",
               stats.rms, (double) before.bufferFill, (double) after.bufferFill));

    check (stats.finite, "freeze: output is finite");
    check (stats.rms > 1.0e-3, "freeze: grains keep playing the captured material");
    check (after.bufferFill >= before.bufferFill - 1.0e-6f,
           "freeze: capture buffer does not shrink");
    check (after.bufferFill <= before.bufferFill + 1.0e-4f,
           "freeze: capture stops growing while frozen");
}

void testDensityMatchesGrainRate()
{
    struct Case { float density; float tolerance; };
    const Case cases[] = { { 60.0f, 12.0f }, { 150.0f, 30.0f } };

    for (const auto& c : cases)
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();

        auto p = cleanParams();
        p.density = c.density;
        p.jitter = 0.0f;         // exact scheduling for a precise rate check
        p.sizeMs = 90.0f;
        engine.setParams (p);

        render (engine, 192000, 512, 2, sine220);   // 4 s

        nagloed::VisualState v;
        engine.getVisual (v);

        info (fmt ("density %.1f g/s -> measured grainRate %.2f g/s", c.density, (double) v.grainRate));
        checkNear ((double) v.grainRate, (double) c.density, (double) c.tolerance,
                   fmt ("density %.0f matches measured grain rate", (double) c.density));
    }
}

void testTuningQuantisation()
{
    using namespace nagloed::dsp;

    // Chromatic rounds to the nearest semitone.
    checkNear (quantisePitch (tuneChromatic, 1.4f), 1.0, 1.0e-4, "chromatic tuning rounds to semitones");
    checkNear (quantisePitch (tuneChromatic, -0.6f), -1.0, 1.0e-4, "chromatic tuning rounds negatives");

    // Major scale: 1.4 is nearer to 2 than to 0.
    checkNear (quantisePitch (tuneMajor, 1.4f), 2.0, 1.0e-4, "major tuning quantises 1.4 st to 2 st");
    checkNear (quantisePitch (tuneMajor, 3.1f), 4.0, 1.0e-4, "major tuning quantises 3.1 st to 4 st");
    checkNear (quantisePitch (tuneMajor, -1.0f), -1.0, 1.0e-4, "major tuning quantises -1 st to -1 st");

    // Pentatonic has no 3 or 4 semitones above the root? It has 4; 3.5 -> 4.
    checkNear (quantisePitch (tunePentatonic, 3.4f), 4.0, 1.0e-4, "pentatonic tuning quantises 3.4 st to 4 st");
    checkNear (quantisePitch (tunePentatonic, 6.0f), 7.0, 1.0e-4, "pentatonic tuning quantises 6 st to 7 st");

    // Whole-tone: only even semitones (ties fall to the lower degree).
    checkNear (quantisePitch (tuneWholeTone, 3.1f), 4.0, 1.0e-4, "whole-tone tuning quantises 3.1 st to 4 st");
    checkNear (quantisePitch (tuneWholeTone, 0.9f), 0.0, 1.0e-4, "whole-tone tuning quantises 0.9 st to 0 st");

    // Free mode is a pass-through.
    checkNear (quantisePitch (tuneFree, 1.37f), 1.37, 1.0e-6, "free tuning leaves pitch untouched");

    // Boundaries: the grain range is +/-4 octaves, and the quantiser must cover
    // it instead of folding the edges into the wrong octave.
    checkNear (quantisePitch (tuneChromatic, -48.0f), -48.0, 1.0e-4,
               "quantiser keeps the -48 st boundary");
    checkNear (quantisePitch (tuneChromatic, 48.0f), 48.0, 1.0e-4,
               "quantiser keeps the +48 st boundary");
    checkNear (quantisePitch (tuneChromatic, -55.0f), -48.0, 1.0e-4,
               "quantiser clamps pitches below -48 st before folding");
    checkNear (quantisePitch (tuneChromatic, 55.0f), 48.0, 1.0e-4,
               "quantiser clamps pitches above +48 st before folding");
    checkNear (quantisePitch (tuneMajor, -47.5f), -48.0, 1.0e-4,
               "major tuning folds near the bottom boundary");
    checkNear (quantisePitch (tuneMajor, 47.5f), 47.0, 1.0e-4,
               "major tuning folds near the top boundary (B is in the scale)");

    // The visualiser must report the quantised pitch for every live grain.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = cleanParams();
    p.density = 120.0f;
    p.sizeMs = 120.0f;
    p.pitchSemis = 1.4f;
    p.tuneMode = tuneMajor;
    engine.setParams (p);

    juce::MidiBuffer midi;
    bool allQuantised = true;
    bool sawGrain = false;

    const int block = 512;
    const int blocks = 192000 / block;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buffer (2, block);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buffer.setSample (ch, i, 0.2f * (float) std::sin (kTwoPi * 220.0 * (b * block + i) / kSampleRate));

        engine.process (buffer, midi);

        nagloed::VisualState v;
        engine.getVisual (v);

        for (int g = 0; g < v.numGrains; ++g)
        {
            sawGrain = true;
            const float pitch = v.grains[(size_t) g].pitch;
            const int pc = ((int) std::llround (pitch) % 12 + 12) % 12;
            const int degrees[] = { 0, 2, 4, 5, 7, 9, 11 };
            bool inScale = false;
            for (int d : degrees)
                if (pc == d && std::abs (pitch - std::round (pitch)) < 1.0e-2f)
                    inScale = true;
            if (! inScale)
                allQuantised = false;
        }
    }

    check (sawGrain, "tuning: grains are visible to the UI");
    check (allQuantised, "tuning: every visible grain pitch is quantised to the major scale");
}

void testFeedbackStability()
{
    // Case A: sparse cloud -> the overlap compensation is 1.0, so the capture
    // path really receives the full 0.95 feedback. This is the configuration the
    // old test never reached (it used +24 st, which scaled feedback to ~0.25).
    {
        nagloed::EngineDSP engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();

        auto p = cleanParams();
        p.density = 1.0f;
        p.sizeMs = 500.0f;
        p.feedback = 0.95f;
        p.jitter = 0.0f;
        p.spray = 0.0f;
        p.pitchSemis = 0.0f;
        p.sourceMode = 1;      // noise bed: keeps feeding the loop
        engine.setParams (p);

        float maxPre = 0.0f;
        bool nonFinite = false;
        const auto stats = renderProbed (engine, 288000, 512, 2, sineStereo, maxPre, nonFinite);

        const float effective = engine.getEffectiveFeedback();
        info (fmt ("feedback 0.95 (sparse, full path): effective %.3f, pre-limiter peak %.3f, out rms %.4f",
                   (double) effective, (double) maxPre, stats.rms));

        check (effective > 0.90f, "feedback stress: sparse cloud reaches the full 0.95 feedback");
        check (! nonFinite, "feedback stress: pre-limiter signal never becomes NaN/Inf");
        check (maxPre <= 4.0f, "feedback stress: pre-limiter peak stays bounded");
        check (stats.finite, "feedback stress: output is finite");
        check (stats.peak <= 2.0f, "feedback stress: output stays below +6 dBFS");
    }

    // Case B: hostile chain — dense cloud, self-oscillating ladder, full space,
    // LoFi, maximum drive/drift. Must stay finite and bounded.
    {
        nagloed::EngineDSP engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();

        auto p = cleanParams();
        p.density = 180.0f;
        p.sizeMs = 400.0f;
        p.feedback = 0.95f;
        p.pitchSemis = 0.0f;
        p.pitchSpread = 6.0f;
        p.reverse = 0.5f;
        p.jitter = 0.0f;
        p.spray = 0.0f;
        p.shape = 1.0f;
        p.sourceMode = 1;      // noise bed keeps the loop fed
        p.driveDb = 24.0f;
        p.bias = 0.8f;
        p.reso = 0.95f;
        p.cutoffHz = 4000.0f;
        p.spaceMix = 1.0f;
        p.spaceSize = 1.0f;
        p.spaceDamp = 0.0f;
        p.spaceMod = 1.0f;
        p.lofi = 0.7f;
        p.tape = 1.0f;
        p.wow = 1.0f;
        p.drift = 1.0f;
        p.mix = 1.0f;
        p.macroGlow = 1.0f;
        p.macroDrift = 1.0f;
        p.macroSpace = 1.0f;
        p.macroShimmer = 1.0f;
        p.autoMotion = 1.0f;
        p.lfoAmount = 1.0f;
        p.lfoDest = 2;         // pitch
        engine.setParams (p);

        float maxPre = 0.0f;
        bool nonFinite = false;
        const auto stats = renderProbed (engine, 288000, 512, 2, sineStereo, maxPre, nonFinite);

        info (fmt ("feedback stress (hostile): effective %.3f, pre-limiter peak %.3f, rms %.4f, peak %.3f",
                   (double) engine.getEffectiveFeedback(), (double) maxPre,
                   stats.rms, (double) stats.peak));

        check (! nonFinite, "hostile stress: pre-limiter signal never becomes NaN/Inf");
        check (maxPre <= 4.0f, "hostile stress: pre-limiter peak stays bounded");
        check (stats.finite, "hostile stress: output is finite");
        check (stats.peak <= 2.0f, "hostile stress: output stays below +6 dBFS");
        check (stats.rms < 2.0, "hostile stress: no runaway energy");
    }
}

void testMonoAndStereo()
{
    // The engine graph is stereo; mono buffers are explicitly folded in/out.
    // 1) stereo buffer
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();
        engine.setParams (musicalParams());

        const auto stats = render (engine, 96000, 512, 2, sineStereo);
        info (fmt ("stereo: rmsL %.4f rmsR %.4f peak %.3f latency %d",
                   stats.rmsL, stats.rmsR, (double) stats.peak, engine.getLatencySamples()));
        check (stats.finite, "stereo: output is finite");
        check (stats.peak <= 2.0f, "stereo: output stays below +6 dBFS");
        check (stats.rmsL > 1.0e-4 && stats.rmsR > 1.0e-4, "stereo: both channels carry audio");
        check (engine.getLatencySamples() > 0 && engine.getLatencySamples() < 256,
               "stereo: reported latency includes the tape transport and is sane");
    }

    // 2) mono buffer: mono input is folded into the stereo graph
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();
        engine.setParams (musicalParams());

        const auto stats = render (engine, 96000, 512, 1, sine220);
        info (fmt ("mono buffer: rms %.4f peak %.3f", stats.rms, (double) stats.peak));
        check (stats.finite, "mono buffer: output is finite");
        check (stats.peak <= 2.0f, "mono buffer: output stays below +6 dBFS");
        check (stats.rms > 1.0e-4, "mono buffer: audio is produced");
    }

    // 3) mono output is exactly the explicit 0.5*(L+R) fold of the stereo run
    {
        auto params = musicalParams();
        params.mix = 0.8f;

        nagloed::Engine stereoEngine;
        stereoEngine.prepare (kSampleRate, 512, 2);
        stereoEngine.reset();
        stereoEngine.setParams (params);

        nagloed::Engine monoEngine;
        monoEngine.prepare (kSampleRate, 512, 2);
        monoEngine.reset();
        monoEngine.setParams (params);

        juce::MidiBuffer midi;
        constexpr int block = 256;
        float maxDifference = 0.0f;
        bool finite = true;

        for (int b = 0; b < 128; ++b)
        {
            juce::AudioBuffer<float> stereoBuffer (2, block);
            juce::AudioBuffer<float> monoBuffer (1, block);
            for (int i = 0; i < block; ++i)
            {
                const float s = 0.2f * (float) std::sin (kTwoPi * 220.0 * (double) (b * block + i) / kSampleRate);
                stereoBuffer.setSample (0, i, s);
                stereoBuffer.setSample (1, i, s);
                monoBuffer.setSample (0, i, s);
            }

            stereoEngine.process (stereoBuffer, midi);
            monoEngine.process (monoBuffer, midi);

            for (int i = 0; i < block; ++i)
            {
                const float l = stereoBuffer.getSample (0, i);
                const float r = stereoBuffer.getSample (1, i);
                const float mono = monoBuffer.getSample (0, i);
                if (! std::isfinite (mono) || ! std::isfinite (l) || ! std::isfinite (r))
                    finite = false;
                maxDifference = std::max (maxDifference, std::abs (mono - 0.5f * (l + r)));
            }
        }

        info (fmt ("mono downmix: max |mono - 0.5*(L+R)| = %.3e", (double) maxDifference));
        check (finite, "mono downmix: all outputs finite");
        check (maxDifference < 1.0e-6f, "mono downmix equals the explicit stereo fold");
    }

    // 4) defensive: engine prepared for 1 channel but handed a stereo buffer
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 1);
        engine.reset();
        engine.setParams (musicalParams());

        const auto stats = render (engine, 96000, 512, 2, sineStereo);
        check (stats.finite, "channel mismatch: output is finite");
        check (stats.peak <= 2.0f, "channel mismatch: output stays below +6 dBFS");
    }
}

void testBlockSizeInvariance()
{
    const int blockSizes[] = { 128, 512, 4096 };
    const long long totalSamples = 491520;   // 10.24 s: divisible by 128, 512, 4096
    double rmsResults[3] = {};

    auto p = musicalParams();
    p.lofi = 0.0f;       // keep the comparison free of sample-and-hold aliasing
    p.hiss = 0.0f;

    // Prepare once for the largest block and vary only the host block size, so
    // this really tests the control-rate/chunking logic rather than re-preparing
    // different engines.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 4096, 2);

    for (int i = 0; i < 3; ++i)
    {
        engine.reset();
        engine.setParams (p);

        const auto stats = render (engine, totalSamples, blockSizes[i], 2, sineStereo);
        check (stats.finite, fmt ("block size %d: output is finite", blockSizes[i]));
        rmsResults[i] = stats.rms;
        info (fmt ("block size %d: rms %.6f", blockSizes[i], stats.rms));
    }

    const double minRms = std::min ({ rmsResults[0], rmsResults[1], rmsResults[2] });
    const double maxRms = std::max ({ rmsResults[0], rmsResults[1], rmsResults[2] });

    check (minRms > 1.0e-4, "block size invariance: audio is present in all runs");
    check (maxRms <= minRms * 1.02,
           fmt ("block size invariance: 128/512/4096 rms within 2%% (%.6f vs %.6f)", minRms, maxRms));

    // The host may hand us a block larger than prepare()'s maximum: the engine
    // must chunk internally and produce the same result.
    nagloed::Engine chunked;
    chunked.prepare (kSampleRate, 128, 2);
    chunked.reset();
    chunked.setParams (p);

    const auto chunkedStats = render (chunked, totalSamples, 4096, 2, sineStereo);
    info (fmt ("oversized 4096 block with 128-sample prepare: rms %.6f", chunkedStats.rms));
    check (chunkedStats.finite, "oversized block: output is finite");
    check (std::abs (chunkedStats.rms - rmsResults[0]) <= rmsResults[0] * 1.0e-4,
           "oversized block: internal chunking matches the 128-sample result");
}

void testExtremeRandomParameters()
{
    // Drives EngineDSP directly so the assertions run on the *pre-safety-limiter*
    // signal: a diverging graph or a NaN cannot be hidden by the output clamp.
    TestRng rng;
    nagloed::EngineDSP engine;
    engine.prepare (kSampleRate, 512, 2);

    bool allPreFinite  = true;
    bool allOutFinite  = true;
    bool allOutBounded = true;
    float worstPrePeak = 0.0f;

    for (int trial = 0; trial < 32; ++trial)
    {
        engine.reset();

        nagloed::EngineParams p;
        // Values deliberately overshoot the documented ranges to exercise the
        // engine's own clamping as well.
        p.density       = rng.range (-100.0f, 520.0f);
        p.sizeMs        = rng.range (-100.0f, 900.0f);
        p.spray         = rng.range (-0.5f, 1.5f);
        p.scan          = rng.range (-2.0f, 2.0f);
        p.pitchSemis    = rng.range (-40.0f, 40.0f);
        p.pitchSpread   = rng.range (-5.0f, 30.0f);
        p.reverse       = rng.range (-0.5f, 1.5f);
        p.jitter        = rng.range (-0.5f, 1.5f);
        p.shape         = rng.range (-0.5f, 1.5f);
        p.width         = rng.range (-0.5f, 1.5f);
        p.feedback      = rng.range (-0.5f, 1.2f);
        p.focus         = rng.range (-0.5f, 1.5f);
        p.freeze        = rng.uniform() < 0.2f;
        p.tuneMode      = rng.intRange (0, 8);
        p.sourceMode    = rng.intRange (0, 3);
        p.spaceMix      = rng.range (-0.5f, 1.5f);
        p.spaceSize     = rng.range (-0.5f, 1.5f);
        p.spaceDamp     = rng.range (-0.5f, 1.5f);
        p.spaceMod      = rng.range (-0.5f, 1.5f);
        p.driveDb       = rng.range (-10.0f, 45.0f);
        p.bias          = rng.range (-0.5f, 1.5f);
        p.tone          = rng.range (-0.5f, 1.5f);
        p.cutoffHz      = rng.range (-100.0f, 40000.0f);
        p.reso          = rng.range (-0.5f, 1.5f);
        p.drift         = rng.range (-0.5f, 1.5f);
        p.wow           = rng.range (-0.5f, 1.5f);
        p.tape          = rng.range (-0.5f, 1.5f);
        p.lofi          = rng.range (-0.5f, 1.5f);
        p.hiss          = rng.range (-0.5f, 1.5f);
        p.inputDb       = rng.range (-40.0f, 40.0f);
        p.outputDb      = rng.range (-40.0f, 12.0f);
        p.mix           = rng.range (-0.5f, 1.5f);
        p.macroGlow     = rng.range (-0.5f, 1.5f);
        p.macroDrift    = rng.range (-0.5f, 1.5f);
        p.macroGrain    = rng.range (-0.5f, 1.5f);
        p.macroSpace    = rng.range (-0.5f, 1.5f);
        p.macroShimmer  = rng.range (-0.5f, 1.5f);
        p.lfoRate       = rng.range (0.001f, 40.0f);
        p.lfoShape      = rng.intRange (0, 6);
        p.lfoDest       = rng.intRange (0, 8);
        p.lfoAmount     = rng.range (-2.0f, 2.0f);
        p.autoMotion    = rng.range (-0.5f, 1.5f);

        engine.setParams (p);

        float maxPre = 0.0f;
        bool nonFinite = false;
        const auto stats = renderProbed (engine, 24000, 512, 2, sineStereo, maxPre, nonFinite);

        allPreFinite  = allPreFinite && ! nonFinite;
        allOutFinite  = allOutFinite && stats.finite;
        allOutBounded = allOutBounded && stats.peak <= 2.0f;
        worstPrePeak  = std::max (worstPrePeak, maxPre);
    }

    info (fmt ("random extreme parameters: 32 hostile configs, worst pre-limiter peak %.3f",
               (double) worstPrePeak));
    check (allPreFinite, "random extreme parameters: pre-limiter signal never NaN/Inf");
    check (worstPrePeak <= 4.0f, "random extreme parameters: pre-limiter peak stays bounded");
    check (allOutFinite, "random extreme parameters: output always finite");
    check (allOutBounded, "random extreme parameters: output always within +6 dBFS");
}

void testVisualState()
{
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();
    engine.setParams (musicalParams());

    render (engine, 96000, 512, 2, sineStereo);

    nagloed::VisualState v;
    engine.getVisual (v);

    info (fmt ("visual: grains %d, rate %.1f, fill %.3f, lfo %.3f, frame %u",
               v.numGrains, (double) v.grainRate, (double) v.bufferFill,
               (double) v.lfo, (unsigned) v.frame));

    check (v.numGrains > 0, "visual: live grains are reported");
    check (v.numGrains <= nagloed::VisualState::kMaxGrainViews, "visual: grain count fits the view array");
    checkInRange ((double) v.inL, 0.0, 1.0, "visual: input RMS in range");
    checkInRange ((double) v.outL, 0.0, 1.0, "visual: output RMS in range");
    checkInRange ((double) v.outPeak, 0.0, 2.0, "visual: output peak in range");
    checkInRange ((double) v.focus, 0.0, 1.0, "visual: focus in range");
    checkInRange ((double) v.spray, 0.0, 1.0, "visual: spray in range");
    checkInRange ((double) v.lfo, -1.0, 1.0, "visual: lfo in range");
    checkInRange ((double) v.cutoffNorm, 0.0, 1.0, "visual: cutoffNorm in range");
    checkInRange ((double) v.densityNorm, 0.0, 1.0, "visual: densityNorm in range");
    checkInRange ((double) v.bufferFill, 0.0, 1.0, "visual: bufferFill in range");
    checkInRange ((double) v.reduce, 0.0, 1.0, "visual: reduce in range");
    checkInRange ((double) v.drive, 0.0, 1.0, "visual: drive in range");
    check (v.grainRate > 0.0f, "visual: grain rate is measured");
    check (v.bufferFill > 0.0f, "visual: capture buffer is filling");
    check (v.frame > 0, "visual: frame counter advances");

    bool grainsValid = true;
    for (int i = 0; i < v.numGrains; ++i)
    {
        const auto& g = v.grains[(size_t) i];
        grainsValid = grainsValid
                   && g.pos >= 0.0f && g.pos <= 1.0f
                   && g.progress >= 0.0f && g.progress <= 1.0f
                   && g.pan >= -1.0f && g.pan <= 1.0f
                   && g.pitch >= -48.0f && g.pitch <= 48.0f
                   && g.env >= 0.0f && g.env <= 1.0f
                   && g.brightness >= 0.0f && g.brightness <= 1.0f;
    }
    check (grainsValid, "visual: every grain view is in range");
}

void testCpuTiming()
{
    // 64 active grains (80 g/s * 0.8 s) plus the full reverb and character
    // chain at 48 kHz / 128 samples / stereo — the documented performance target.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 128, 2);
    engine.reset();

    auto p = musicalParams();
    p.density = 80.0f;
    p.sizeMs = 800.0f;
    p.spaceMix = 0.4f;
    engine.setParams (p);

    const long long totalSamples = 480000;   // 10 s
    const auto start = std::chrono::steady_clock::now();
    const auto stats = render (engine, totalSamples, 128, 2, sineStereo);
    const auto end = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double> (end - start).count();
    const double audioSeconds = (double) totalSamples / kSampleRate;
    const double percentOfCore = 100.0 * seconds / audioSeconds;

    info (fmt ("CPU: %.1f ms for %.1f s of audio at 128 samples = %.2f%% of one core",
               seconds * 1000.0, audioSeconds, percentOfCore));
    info (fmt ("CPU: 64-grain cloud rms %.4f peak %.3f", stats.rms, (double) stats.peak));

    check (stats.finite, "CPU bench: output is finite");
    check (percentOfCore < 15.0, "CPU bench: below 15% of one core at 48k/128/stereo with 64 grains");
}

void testLongRender()
{
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();
    engine.setParams (musicalParams());

    const long long totalSamples = (long long) (30.0 * kSampleRate);
    const auto stats = render (engine, totalSamples, 512, 2, sineStereo);

    info (fmt ("30 s render: rms %.4f peak %.4f finite %d",
               stats.rms, (double) stats.peak, stats.finite ? 1 : 0));
    check (stats.finite, "30 s render: output is finite");
    check (stats.peak <= 2.0f, "30 s render: output stays below +6 dBFS");
    check (stats.rms > 1.0e-4, "30 s render: audio is produced");
}


void testWindowMorph()
{
    nagloed::dsp::WindowMorph window;
    window.prepare();

    // Every window shape must be exactly zero at both ends: that is what keeps
    // grain boundaries click-free.
    for (float shape : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        check (std::abs (window.value (0.0f, shape)) < 1.0e-6f,
               fmt ("window shape %.2f is zero at phase 0", (double) shape));
        check (std::abs (window.value (1.0f, shape)) < 1.0e-6f,
               fmt ("window shape %.2f is zero at phase 1", (double) shape));
    }

    // Hann peaks in the middle, expodec peaks early and decays.
    checkNear ((double) window.value (0.5f, 0.0f), 1.0, 1.0e-3, "Hann window peaks at unity in the centre");
    check (window.value (0.1f, 1.0f) > window.value (0.8f, 1.0f),
           "expodec window decays from attack to tail");
    info (fmt ("window morph: hann(0.5)=%.3f expodec(0.1)=%.3f expodec(0.8)=%.3f",
               (double) window.value (0.5f, 0.0f),
               (double) window.value (0.1f, 1.0f),
               (double) window.value (0.8f, 1.0f)));
}

void testSpaceReverbTail()
{
    auto base = cleanParams();
    base.density = 60.0f;
    base.sizeMs = 80.0f;
    base.spray = 0.0f;      // no scatter, so the dry grains die quickly
    base.focus = 0.9f;

    auto dry = base;
    dry.spaceMix = 0.0f;

    auto wet = base;
    wet.spaceMix = 1.0f;
    wet.spaceSize = 0.95f;
    wet.spaceDamp = 0.2f;
    wet.spaceMod = 0.3f;

    const auto measure = [&] (const nagloed::EngineParams& p, Stats& early, Stats& late)
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();
        engine.setParams (p);

        render (engine, 48000, 512, 2, sine220);            // 1 s of excitation
        early = render (engine, 24000, 512, 2, silence);    // 0.5 s tail
        render (engine, 72000, 512, 2, silence);            // 1.5 s more silence
        late = render (engine, 24000, 512, 2, silence);     // measure 2.0-2.5 s
    };

    Stats dryEarly, dryLate, wetEarly, wetLate;
    measure (dry, dryEarly, dryLate);
    measure (wet, wetEarly, wetLate);

    info (fmt ("reverb tail: dry early %.5f late %.5f | wet early %.5f late %.5f",
               dryEarly.rms, dryLate.rms, wetEarly.rms, wetLate.rms));

    check (wetEarly.finite && wetLate.finite, "reverb tail: output is finite");
    check (wetLate.rms > dryLate.rms * 3.0,
           "reverb tail: Ruimte contributes a decaying tail after the source stops");
    check (wetLate.rms > 1.0e-4, "reverb tail: the tail is still audible ~2.5 s after the source");
    check (wetLate.rms < wetEarly.rms,
           "reverb tail: the space decays over time rather than growing");
}

void testLfoAndAutomationSmearing()
{
    // A fast LFO on the ladder cutoff must not produce clicks or non-finite
    // output: filter coefficients are recomputed at control rate from smoothed
    // values.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = musicalParams();
    p.lfoDest = 3;      // Cutoff
    p.lfoAmount = 1.0f;
    p.lfoRate = 8.0f;
    p.lfoShape = 4;     // square: the harshest possible modulation
    p.reso = 0.8f;
    engine.setParams (p);

    const auto stats = render (engine, 96000, 512, 2, sineStereo);
    info (fmt ("fast square LFO on cutoff: rms %.4f peak %.3f", stats.rms, (double) stats.peak));
    check (stats.finite, "fast LFO: output is finite");
    check (stats.peak <= 2.0f, "fast LFO: output stays below +6 dBFS");
}

void testStereoWidth()
{
    // Panning grains across the stereo field must decorrelate the two channels.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = musicalParams();
    p.width = 1.0f;
    p.spaceMix = 0.0f;
    p.mix = 1.0f;
    engine.setParams (p);

    render (engine, 96000, 512, 2, sine220);

    // Measure the final block's channel difference.
    juce::AudioBuffer<float> buffer (2, 2048);
    juce::MidiBuffer midi;
    float differenceEnergy = 0.0f;
    float totalEnergy = 0.0f;
    for (int i = 0; i < 2048; ++i)
    {
        const float s = 0.2f * (float) std::sin (kTwoPi * 220.0 * (96000 + i) / kSampleRate);
        buffer.setSample (0, i, s);
        buffer.setSample (1, i, s);
    }
    engine.process (buffer, midi);
    for (int i = 0; i < 2048; ++i)
    {
        const float l = buffer.getSample (0, i);
        const float r = buffer.getSample (1, i);
        differenceEnergy += (l - r) * (l - r);
        totalEnergy += l * l + r * r;
    }
    const float ratio = std::sqrt (differenceEnergy / std::max (1.0e-9f, totalEnergy));
    info (fmt ("stereo width: L/R difference ratio %.3f", (double) ratio));
    check (ratio > 0.1f, "stereo width: wide setting produces a genuinely stereo image");
}


void testNoAllocationOnAudioThread()
{
    // The realtime contract: prepare() allocates, nothing else does. We count
    // global allocations while hammering process()/setParams()/getVisual().
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 128, 2);
    engine.reset();

    auto p = musicalParams();
    engine.setParams (p);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, 128);

    // Warm up so any (unrelated) lazy initialisation happens before tracking.
    for (int b = 0; b < 8; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
                buffer.setSample (ch, i, 0.1f);
        engine.process (buffer, midi);
    }

    nagl_test::setAllocationTracking (true);
    nagl_test::resetAllocationCount();

    // Cover automation, discrete-parameter switches, freeze toggles and source
    // switches, not just a static run. (The counter wraps the global C++
    // operator new; JUCE's own heap paths go through it as well. Direct libc
    // malloc calls are not intercepted — the engine does not use any.)
    auto frozen = p;
    frozen.freeze = true;

    auto tuned = p;
    tuned.tuneMode = 5;

    for (int b = 0; b < 512; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
                buffer.setSample (ch, i, 0.1f * std::sin (0.02f * (float) (b * 128 + i)));

        if (b == 64)  { engine.setParams (frozen);      }
        if (b == 128) { engine.setParams (p);           }
        if (b == 192) { auto q = p; q.sourceMode = 1; engine.setParams (q); }
        if (b == 256) { engine.setParams (tuned);       }
        if (b == 320) { auto q = p; q.freeze = true; q.density = 200.0f; engine.setParams (q); }
        if (b == 448) { engine.setParams (p);           }

        engine.process (buffer, midi);

        nagloed::VisualState visual;
        engine.getVisual (visual);
    }

    nagl_test::setAllocationTracking (false);
    const long long allocations = nagl_test::allocationCount();

    info (fmt ("audio-thread allocations: %lld", allocations));
    check (allocations == 0, "process/setParams/getVisual perform no heap allocation");
}


void testLadderSelfOscillation()
{
    // Direct AnalogChain test: a single impulse into a maximum-resonance ladder
    // must keep ringing indefinitely (self-oscillation) without ever running
    // away, thanks to the tanh in the feedback loop.
    nagloed::dsp::AnalogChain chain;
    chain.prepare (kSampleRate, 256);
    chain.reset();

    nagloed::dsp::AnalogChain::Controls c;
    c.inputGain = 1.0f;
    c.driveGain = 1.0f;
    c.toneLow = 1.0f;
    c.toneHigh = 1.0f;
    c.bias = 0.0f;
    c.ladderG = 0.0327f;    // ~1 kHz at the 96 kHz oversampled rate
    c.ladderK = 4.0f;       // self-oscillation threshold
    c.wowDepth = c.flutterDepth = c.driftDepth = 0.0f;
    c.tapeDrive = c.tapeComp = 1.0f;
    c.lofiHold = 1.0f;
    c.lofiLevels = 65536.0f;
    c.hissGain = 0.0f;
    c.outputGain = 1.0f;
    c.mix = 1.0f;
    c.lofiActive = false;
    c.hissActive = false;

    const nagloed::dsp::AnalogChain::Group group { 0, c };

    constexpr int block = 256;
    std::vector<float> wetL (block), wetR (block), dryL (block, 0.0f), dryR (block, 0.0f);

    double lateEnergy = 0.0;
    int lateSamples = 0;
    bool finite = true;

    const int totalBlocks = (int) (kSampleRate * 2.0) / block;
    for (int b = 0; b < totalBlocks; ++b)
    {
        std::fill (wetL.begin(), wetL.end(), 0.0f);
        std::fill (wetR.begin(), wetR.end(), 0.0f);
        if (b == 0)
        {
            wetL[0] = 0.5f;
            wetR[0] = 0.5f;
        }

        chain.process (wetL.data(), wetR.data(), dryL.data(), dryR.data(), block, &group, 1);

        for (int i = 0; i < block; ++i)
        {
            if (! std::isfinite (wetL[i]))
                finite = false;

            if (b * block + i > (int) kSampleRate)
            {
                lateEnergy += (double) wetL[i] * (double) wetL[i];
                ++lateSamples;
            }
        }
    }

    const double lateRms = std::sqrt (lateEnergy / (double) std::max (1, lateSamples));
    info (fmt ("ladder self-oscillation: late rms %.5f", lateRms));
    check (finite, "ladder: self-oscillation output is finite");
    check (lateRms > 1.0e-4, "ladder: resonance reaches stable self-oscillation at maximum reso");
    check (lateRms < 2.0, "ladder: self-oscillation is amplitude-limited");
}


void testFocusMapping()
{
    // Focus must map linearly onto the readable part of the capture window:
    // 0 = oldest, 0.5 = middle, 1 = newest (previously focus 1.0 wrapped to the
    // oldest edge because the position was fmod-wrapped over the whole ring).
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = cleanParams();
    p.density = 200.0f;
    p.sizeMs = 50.0f;
    p.spray = 0.0f;
    p.scan = 0.0f;
    p.jitter = 0.0f;
    p.width = 0.0f;
    p.pitchSemis = 0.0f;
    p.focus = 0.5f;
    engine.setParams (p);

    // Fill the 12 s capture window.
    render (engine, (long long) (13.0 * kSampleRate), 512, 2, sine220);

    const auto measureMeanPosition = [&] (float focus)
    {
        auto q = p;
        q.focus = focus;
        engine.setParams (q);

        render (engine, 96000, 512, 2, sine220);   // settle: smoothing + old grains

        double sum = 0.0;
        int count = 0;
        juce::MidiBuffer midi;
        constexpr int block = 512;

        for (int b = 0; b < 16; ++b)
        {
            juce::AudioBuffer<float> buffer (2, block);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buffer.setSample (ch, i, 0.2f * (float) std::sin (
                        kTwoPi * 220.0 * (double) (b * block + i) / kSampleRate));

            engine.process (buffer, midi);

            nagloed::VisualState v;
            engine.getVisual (v);
            for (int g = 0; g < v.numGrains; ++g)
            {
                sum += v.grains[(size_t) g].pos;
                ++count;
            }
        }

        return count > 0 ? sum / (double) count : -1.0;
    };

    const double mean0  = measureMeanPosition (0.0f);
    const double mean05 = measureMeanPosition (0.5f);
    const double mean1  = measureMeanPosition (1.0f);

    info (fmt ("focus mean grain position: 0 -> %.3f, 0.5 -> %.3f, 1 -> %.3f",
               mean0, mean05, mean1));

    check (mean0 >= 0.0 && mean05 >= 0.0 && mean1 >= 0.0, "focus: grain views are visible at 0/0.5/1");
    check (mean0 < 0.05, "focus 0 selects the oldest captured material");
    check (std::abs (mean05 - 0.5) < 0.05, "focus 0.5 selects the middle of the window");
    check (mean1 > 0.95, "focus 1 selects the newest captured material");
    check (mean0 < mean05 && mean05 < mean1, "focus mapping is monotonic");
}

void testFeedbackDecay()
{
    // High feedback with zero decorrelators is the worst case for the loop: all
    // grains are correlated, so their windows add up. The engine compensates the
    // regeneration by the overlap gain, so with the source silent the cloud must
    // decay at every density. A sparse cloud (overlap <= 1) still gets the full
    // 0.95 into the capture path.
    // shape 0.5 = Tukey, whose plateau has the highest average window value
    // (worst case for correlated overlap); shape 1 = expodec.
    struct Case { float density; float sizeMs; float shape; const char* label; bool compensated; };
    const Case cases[] = {
        {   1.0f, 500.0f, 1.0f, "sparse (full 0.95)"     , false },
        {  30.0f,  80.0f, 0.5f, "default overlap, Tukey" , true  },
        {  30.0f,  80.0f, 1.0f, "default overlap, expodec", false },
        { 120.0f, 300.0f, 0.5f, "dense, Tukey"           , true  },
        { 200.0f, 500.0f, 0.5f, "maximum overlap, Tukey" , true  },
    };

    const auto prime = [] (int, long long i)
    {
        // Coherent low-frequency tone (worst case for a feedback loop) plus a
        // little broadband content.
        return 0.4f * (float) std::sin (kTwoPi * 55.0 * (double) i / kSampleRate)
             + 0.05f * (float) std::sin (kTwoPi * 1234.0 * (double) i / kSampleRate);
    };

    for (const auto& c : cases)
    {
        nagloed::EngineDSP engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();

        auto p = cleanParams();
        p.density     = c.density;
        p.sizeMs      = c.sizeMs;
        p.feedback    = 0.95f;
        p.pitchSemis  = 0.0f;
        p.pitchSpread = 0.0f;
        p.reverse     = 0.0f;
        p.jitter      = 0.0f;
        p.spray       = 0.0f;
        p.width       = 0.0f;
        p.shape       = c.shape;
        p.focus       = 1.0f;    // read the freshest (regenerated) material
        p.mix         = 1.0f;
        engine.setParams (p);

        render (engine, 96000, 512, 2, prime);   // 2 s prime

        float maxPre = 0.0f;
        bool nonFinite = false;

        const auto early = renderProbed (engine, 48000, 512, 2, silence, maxPre, nonFinite);   // 0-1 s
        const auto mid   = renderProbed (engine, 48000, 512, 2, silence, maxPre, nonFinite);   // 1-2 s
        renderProbed (engine, 96000, 512, 2, silence, maxPre, nonFinite);                      // 2-4 s skip
        const auto late  = renderProbed (engine, 96000, 512, 2, silence, maxPre, nonFinite);   // 4-6 s

        info (fmt ("feedback decay [%s]: effective %.3f, early %.5f mid %.5f late %.5f, pre-limiter peak %.3f",
                   c.label, (double) engine.getEffectiveFeedback(),
                   early.rms, mid.rms, late.rms, (double) maxPre));

        check (! nonFinite, fmt ("feedback decay [%s]: pre-limiter signal never NaN/Inf", c.label));
        check (maxPre <= 4.0f, fmt ("feedback decay [%s]: pre-limiter peak stays bounded", c.label));
        check (mid.rms < early.rms,
               fmt ("feedback decay [%s]: cloud decays in the 1-2 s window", c.label));
        check (late.rms < mid.rms,
               fmt ("feedback decay [%s]: cloud keeps decaying towards 6 s", c.label));
        check (late.rms < early.rms * 0.5,
               fmt ("feedback decay [%s]: cloud has decayed by at least 6 dB after 4 s", c.label));

        if (c.compensated)
            check (engine.getEffectiveFeedback() < 0.90f,
                   fmt ("feedback decay [%s]: overlapping cloud is regeneration-compensated", c.label));
        else
            check (engine.getEffectiveFeedback() > 0.90f,
                   fmt ("feedback decay [%s]: sparse cloud receives the full 0.95 feedback", c.label));
    }
}

void testDryWetAlignment()
{
    // A 50/50 mix must be phase-coherent: the dry path has to be delayed by the
    // full wet latency (oversampler + tape base delay + wow/flutter), otherwise
    // the blend comb-filters. Static slow sine, mix = 0.5 -> level unchanged.
    nagloed::dsp::AnalogChain chain;
    chain.prepare (kSampleRate, 512);
    chain.reset();

    nagloed::dsp::AnalogChain::Controls c;
    c.inputGain  = 1.0f;
    c.driveGain  = 1.0f;
    c.toneLow    = 1.0f;
    c.toneHigh   = 1.0f;
    c.bias       = 0.0f;

    const float osRate = (float) kSampleRate * 2.0f;
    c.ladderG = std::tan (nagloed::dsp::kPi * 20000.0f / osRate);   // wide open
    c.ladderK = 0.0f;

    // Same transport modulation the engine applies (a couple of samples of wow
    // even at wow = 0, plus flutter/drift).
    c.wowDepth     = 2.0f;
    c.flutterDepth = 0.5f;
    c.driftDepth   = 0.5f;
    c.tapeDrive    = 1.0f;
    c.tapeComp     = 1.0f;
    c.lofiHold     = 1.0f;
    c.lofiLevels   = 65536.0f;
    c.hissGain     = 0.0f;
    c.outputGain   = 1.0f;
    c.mix          = 0.5f;
    c.lofiActive   = false;
    c.hissActive   = false;

    const nagloed::dsp::AnalogChain::Group group { 0, c };

    constexpr int block = 512;
    std::vector<float> wetL (block), wetR (block), dryL (block), dryR (block);

    double outEnergy = 0.0, inEnergy = 0.0;
    int measuredSamples = 0;
    long long index = 0;

    const int totalBlocks = (int) (kSampleRate * 3.0) / block;
    for (int b = 0; b < totalBlocks; ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = 0.25f * (float) std::sin (kTwoPi * 220.0 * (double) (index + i) / kSampleRate);
            wetL[i] = wetR[i] = s;
            dryL[i] = dryR[i] = s;
        }

        chain.process (wetL.data(), wetR.data(), dryL.data(), dryR.data(), block, &group, 1);

        if (b * block > (int) (kSampleRate * 2.0))   // skip the delay-line fill
        {
            for (int i = 0; i < block; ++i)
            {
                outEnergy += (double) wetL[i] * wetL[i] + (double) wetR[i] * wetR[i];
                inEnergy  += (double) dryL[i] * dryL[i] + (double) dryR[i] * dryR[i];
            }
            measuredSamples += 2 * block;
        }

        index += block;
    }

    const double outRms = std::sqrt (outEnergy / (double) std::max (1, measuredSamples));
    const double inRms  = std::sqrt (inEnergy  / (double) std::max (1, measuredSamples));
    const double gainDb = 20.0 * std::log10 (outRms / std::max (1.0e-12, inRms));

    info (fmt ("dry/wet alignment: reported latency %d samples, 50/50 gain %.3f dB",
               chain.latencySamples(), gainDb));

    check (chain.latencySamples() >= 32,
           "dry/wet: reported latency includes the tape transport");
    check (std::abs (gainDb) < 0.5,
           "dry/wet: 50/50 blend stays phase-coherent (within 0.5 dB)");
}


void testOutputSafetyStage()
{
    // The final limiter must be a *soft* stage (not a hard clip) and the QA
    // probe must expose the signal before it, so tests can distinguish a hot
    // signal from the safety net that tames it.
    nagloed::EngineDSP engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = cleanParams();
    p.mix = 0.0f;          // fully dry: exercises the limiter on the raw input path
    engine.setParams (p);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, 512);

    float outputPeak = 0.0f;
    float probePeak  = 0.0f;
    bool outputFinite = true;

    for (int b = 0; b < 16; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 1.5f * (float) std::sin (
                    kTwoPi * 220.0 * (double) (b * 512 + i) / kSampleRate));

        engine.process (buffer, midi);

        probePeak = std::max (probePeak, engine.getPreLimiterPeak());

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buffer.getSample (ch, i);
                if (! std::isfinite (v))
                    outputFinite = false;
                outputPeak = std::max (outputPeak, std::abs (v));
            }
    }

    info (fmt ("output safety: pre-limiter peak %.3f, post-limiter peak %.3f", (double) probePeak, (double) outputPeak));

    check (probePeak > 1.4f, "safety: probe exposes the pre-limiter peak (hot dry signal)");
    check (outputPeak < 1.35f, "safety: output is soft-limited rather than hard-clipped");
    check (outputPeak > 1.0f, "safety: soft limiter still passes a hot signal");
    check (outputFinite, "safety: output is finite");
}

} // namespace

// =============================================================================
// Entry point called from test_main.cpp
// =============================================================================

void runDspTests()
{
    std::printf ("\nDSP tests\n");

    testSilenceInSilenceOut();
    testSelfGeneratingSources();
    testFreezeKeepsGrainsAlive();
    testDensityMatchesGrainRate();
    testTuningQuantisation();
    testWindowMorph();
    testLadderSelfOscillation();
    testSpaceReverbTail();
    testLfoAndAutomationSmearing();
    testStereoWidth();
    testFeedbackStability();
    testFeedbackDecay();
    testFocusMapping();
    testDryWetAlignment();
    testMonoAndStereo();
    testBlockSizeInvariance();
    testExtremeRandomParameters();
    testOutputSafetyStage();
    testVisualState();
    testNoAllocationOnAudioThread();
    testCpuTiming();
    testLongRender();
}
