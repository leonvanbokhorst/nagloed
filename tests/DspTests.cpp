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

template <typename Generator>
Stats render (nagloed::Engine& engine, long long totalSamples, int blockSize, int channels,
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
    drone.density = 50.0f;
    drone.sizeMs = 140.0f;
    engine.setParams (drone);

    const auto droneStats = render (engine, 96000, 512, 2, silence);
    check (droneStats.finite, "drone source: output is finite");
    check (droneStats.rms > 1.0e-3, "drone source generates material with a silent input");
    check (droneStats.peak <= 2.0f, "drone source: output stays within the safety ceiling");
    info (fmt ("drone source: rms %.4f", droneStats.rms));
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
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    // Deliberately hostile: maximum feedback, maximum upward pitch, dense cloud,
    // self-oscillating filter, full space, LoFi and heavy saturation.
    auto p = cleanParams();
    p.density = 180.0f;
    p.sizeMs = 400.0f;
    p.feedback = 0.95f;
    p.pitchSemis = 24.0f;
    p.pitchSpread = 6.0f;
    p.reverse = 0.5f;
    p.jitter = 0.6f;
    p.sourceMode = 1;          // noise bed keeps the loop fed
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
    p.feedback = 0.95f;
    p.autoMotion = 1.0f;
    p.lfoAmount = 1.0f;
    p.lfoDest = 2;             // pitch
    engine.setParams (p);

    const auto stats = render (engine, 288000, 512, 2, sineStereo);   // 6 s

    info (fmt ("feedback stress: rms %.4f, peak %.4f", stats.rms, (double) stats.peak));
    check (stats.finite, "feedback stress: output is finite");
    check (stats.peak <= 2.0f, "feedback stress: output stays below +6 dBFS");
    check (stats.rms < 2.0, "feedback stress: no runaway energy");
}

void testMonoAndStereo()
{
    // 1) stereo engine, stereo buffer
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();
        engine.setParams (musicalParams());

        const auto stats = render (engine, 96000, 512, 2, sineStereo);
        info (fmt ("stereo: rmsL %.4f rmsR %.4f peak %.3f", stats.rmsL, stats.rmsR, (double) stats.peak));
        check (stats.finite, "stereo: output is finite");
        check (stats.peak <= 2.0f, "stereo: output stays below +6 dBFS");
        check (stats.rmsL > 1.0e-4 && stats.rmsR > 1.0e-4, "stereo: both channels carry audio");
        check (engine.getLatencySamples() >= 0 && engine.getLatencySamples() < 64,
               "stereo: reported latency is sane");
    }

    // 2) stereo engine, mono buffer (host can hand us mono in stereo mode)
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

    // 3) mono engine, stereo buffer (defensive: host mismatch)
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 1);
        engine.reset();
        engine.setParams (musicalParams());

        const auto stats = render (engine, 96000, 512, 2, sineStereo);
        check (stats.finite, "mono engine + stereo buffer: output is finite");
        check (stats.peak <= 2.0f, "mono engine + stereo buffer: output stays below +6 dBFS");
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

    for (int i = 0; i < 3; ++i)
    {
        nagloed::Engine engine;
        engine.prepare (kSampleRate, blockSizes[i], 2);
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
}

void testExtremeRandomParameters()
{
    TestRng rng;
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);

    bool allFinite = true;
    bool allBounded = true;

    for (int trial = 0; trial < 24; ++trial)
    {
        engine.reset();

        nagloed::EngineParams p;
        p.density       = rng.range (0.5f, 200.0f);
        p.sizeMs        = rng.range (5.0f, 500.0f);
        p.spray         = rng.range (0.0f, 1.0f);
        p.scan          = rng.range (-1.0f, 1.0f);
        p.pitchSemis    = rng.range (-24.0f, 24.0f);
        p.pitchSpread   = rng.range (0.0f, 24.0f);
        p.reverse       = rng.range (0.0f, 1.0f);
        p.jitter        = rng.range (0.0f, 1.0f);
        p.shape         = rng.range (0.0f, 1.0f);
        p.width         = rng.range (0.0f, 1.0f);
        p.feedback      = rng.range (0.0f, 0.95f);
        p.focus         = rng.range (0.0f, 1.0f);
        p.freeze        = rng.uniform() < 0.2f;
        p.tuneMode      = rng.intRange (0, 7);
        p.sourceMode    = rng.intRange (0, 2);
        p.spaceMix      = rng.range (0.0f, 1.0f);
        p.spaceSize     = rng.range (0.0f, 1.0f);
        p.spaceDamp     = rng.range (0.0f, 1.0f);
        p.spaceMod      = rng.range (0.0f, 1.0f);
        p.driveDb       = rng.range (0.0f, 30.0f);
        p.bias          = rng.range (0.0f, 1.0f);
        p.tone          = rng.range (0.0f, 1.0f);
        p.cutoffHz      = rng.range (20.0f, 20000.0f);
        p.reso          = rng.range (0.0f, 1.0f);
        p.drift         = rng.range (0.0f, 1.0f);
        p.wow           = rng.range (0.0f, 1.0f);
        p.tape          = rng.range (0.0f, 1.0f);
        p.lofi          = rng.range (0.0f, 1.0f);
        p.hiss          = rng.range (0.0f, 1.0f);
        p.inputDb       = rng.range (-24.0f, 24.0f);
        p.outputDb      = rng.range (-24.0f, 6.0f);
        p.mix           = rng.range (0.0f, 1.0f);
        p.macroGlow     = rng.range (0.0f, 1.0f);
        p.macroDrift    = rng.range (0.0f, 1.0f);
        p.macroGrain    = rng.range (0.0f, 1.0f);
        p.macroSpace    = rng.range (0.0f, 1.0f);
        p.macroShimmer  = rng.range (0.0f, 1.0f);
        p.lfoRate       = rng.range (0.01f, 20.0f);
        p.lfoShape      = rng.intRange (0, 5);
        p.lfoDest       = rng.intRange (0, 6);
        p.lfoAmount     = rng.range (-1.0f, 1.0f);
        p.autoMotion    = rng.range (0.0f, 1.0f);

        engine.setParams (p);

        const auto stats = render (engine, 24000, 512, 2, sineStereo);
        allFinite  = allFinite && stats.finite;
        allBounded = allBounded && stats.peak <= 2.0f;
    }

    check (allFinite, "random extreme parameters: output always finite");
    check (allBounded, "random extreme parameters: output always within +6 dBFS");
    info ("random extreme parameters: 24 hostile configurations rendered");
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

    for (int b = 0; b < 128; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
                buffer.setSample (ch, i, 0.1f * std::sin (0.02f * (float) (b * 128 + i)));

        engine.process (buffer, midi);
        engine.setParams (p);

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
    testMonoAndStereo();
    testBlockSizeInvariance();
    testExtremeRandomParameters();
    testVisualState();
    testNoAllocationOnAudioThread();
    testCpuTiming();
    testLongRender();
}
