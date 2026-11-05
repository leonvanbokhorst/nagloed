#include "GloopDspTests.h"

#include "AllocCounter.h"
#include "TestHarness.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "core/EngineParams.h"
#include "core/Gloop.h"
#include "dsp/Engine.h"
#include "dsp/GloopEngine.h"

// =============================================================================
// Gloop DSP tests.
//
// Two levels are exercised on purpose:
//   * GloopEngine directly, where phase/fades/glides can be inspected exactly;
//   * the real Engine/EngineDSP, where the lanes meet the modulation matrix and
//     the live-centre mapping of the parameters.
//
// Everything here is deterministic: the lanes are driven by explicit control
// frames, and the engine's own RNGs are seeded from constants.
// =============================================================================

namespace
{

namespace nagl = nagloed;

constexpr double kSampleRate = 48000.0;
constexpr double kControlDt  = 16.0 / 48000.0;   // one EngineDSP control frame

using nagl_test::check;
using nagl_test::checkInRange;
using nagl_test::checkNear;
using nagl_test::info;

using nagl::gloop::Mode;
using nagl::gloop::Target;

std::string fmt (const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start (args, format);
    std::vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);
    return std::string (buffer);
}

int asInt (Target target) { return (int) target; }
int asInt (Mode mode)     { return (int) mode; }

// EngineParams with the generative motion switched off, so "focus" reads back
// as the manual value plus Gloop and nothing else.
nagloed::EngineParams gloopParams()
{
    nagloed::EngineParams p;
    p.autoMotion  = 0.0f;
    p.drift       = 0.0f;
    p.macroDrift  = 0.0f;
    p.lfoAmount   = 0.0f;
    p.lfoDest     = 0;
    p.macroGlow   = 0.0f;
    p.macroSpace  = 0.0f;
    p.macroGrain  = 0.5f;
    p.macroShimmer = 0.0f;
    return p;
}

void setLane (nagloed::EngineParams& p, int lane, Target target, float amount,
              float speed, Mode mode)
{
    auto& l = p.gloopLanes[(size_t) lane];
    l.target = asInt (target);
    l.amount = amount;
    l.speed  = speed;
    l.mode   = asInt (mode);
}

std::vector<float> sineGesture (int count, float amplitude)
{
    std::vector<float> g ((size_t) count, 0.0f);

    for (int i = 0; i < count; ++i)
        g[(size_t) i] = amplitude
                        * (float) std::sin (6.283185307179586 * (double) i / (double) count);

    return g;
}

std::vector<float> constantGesture (int count, float value)
{
    return std::vector<float> ((size_t) count, value);
}

std::vector<float> rampGesture (int count, float from, float to)
{
    std::vector<float> g ((size_t) count, from);

    for (int i = 0; i < count; ++i)
    {
        const float t = count > 1 ? (float) i / (float) (count - 1) : 0.0f;
        g[(size_t) i] = from + (to - from) * t;
    }

    return g;
}

// A GloopEngine plus its parameter snapshot, driven frame by frame.
struct Harness
{
    nagloed::dsp::GloopEngine gloop;
    nagloed::EngineParams params = gloopParams();

    Harness()
    {
        gloop.prepare (kSampleRate);
        gloop.reset();
    }

    bool publish (int lane, const std::vector<float>& take, float durationSeconds)
    {
        nagloed::gloop::Gesture gesture;
        gesture.count = (int) take.size();
        gesture.durationSeconds = durationSeconds;
        gesture.numTracks = take.size() >= 2 ? 1 : 0;

        if (! take.empty())
        {
            int target = params.gloopLanes[(size_t) lane].target;

            if (target <= asInt (Target::Off) || target >= nagl::gloop::kNumTargets)
                target = asInt (Target::Focus);

            gesture.tracks[0].target = target;
            std::copy (take.begin(), take.end(), gesture.tracks[0].offsets.begin());
        }

        return gloop.publish (lane, gesture);
    }

    void begin() { gloop.beginBlock (params); }

    void run (int frames, double dt = kControlDt)
    {
        begin();
        for (int i = 0; i < frames; ++i)
            gloop.processFrame (dt);
    }

    nagloed::gloop::LaneVisual visual (int lane) const
    {
        nagloed::gloop::Visual v;
        gloop.copyVisual (v, 0);
        return v.lanes[(size_t) lane];
    }
};

// --- engine-level helpers ----------------------------------------------------

std::vector<float> renderEngine (nagloed::Engine& engine, int blocks, int blockSize,
                                 bool& finite)
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);
    std::vector<float> samples;
    samples.reserve ((size_t) blocks * (size_t) blockSize * 2u);

    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            float* data = buffer.getWritePointer (ch);

            for (int i = 0; i < blockSize; ++i)
            {
                const auto n = (float) (b * blockSize + i);
                data[i] = 0.05f * std::sin (0.01f * n + 0.3f * (float) ch);
            }
        }

        engine.process (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < blockSize; ++i)
            {
                if (! std::isfinite (data[i]))
                    finite = false;

                samples.push_back (data[i]);
            }
        }
    }

    return samples;
}

// =============================================================================
// 1. Silence rules: no take, no motion, no lane.
// =============================================================================

void testGloopSilence()
{
    info ("Gloop: silence rules");

    {   // IDLE with an installed take: READY for the UI, zero influence.
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Idle);
        const auto take = sineGesture (100, 0.8f);
        check (h.publish (0, take, 1.0f), "idle: a take can be installed");
        h.run (600);

        const auto v = h.visual (0);
        check (h.gloop.laneHasGesture (0), "idle: the take stays installed (READY)");
        check (h.gloop.laneMode (0) == asInt (Mode::Idle), "idle: the lane reports Idle");
        check (h.gloop.laneOffset (0) == 0.0f, "idle: zero effective offset");
        check (h.gloop.offsetFor (Target::Focus) == 0.0f, "idle: zero target offset");
        check (v.hasGesture && v.offset == 0.0f && v.amount == 0.0f,
               "idle: the visual reports a take but no influence");
        check (v.phase == 0.0f, "idle: the playhead does not advance");
    }

    {   // Target Off: reports target and mode, contributes nothing.
        Harness h;
        setLane (h.params, 0, Target::Off, 1.0f, 1.0f, Mode::Loop);
        const auto take = sineGesture (100, 0.8f);
        check (h.publish (0, take, 1.0f), "off target: a take can be installed");
        h.run (300);

        const auto v = h.visual (0);
        check (h.gloop.offsetFor (Target::Focus) == 0.0f, "off target: no contribution");
        check (h.gloop.offsetFor (Target::Off) == 0.0f, "off target: Off itself has no offset");
        check (v.target == asInt (Target::Off), "off target: the target is still reported");
        check (v.mode == asInt (Mode::Loop), "off target: the mode is still reported");
        check (v.amount == 0.0f && v.offset == 0.0f,
               "off target: zero effective amount and offset");
    }

    {   // Amount 0: a silent lane even while playing.
        Harness h;
        setLane (h.params, 0, Target::Focus, 0.0f, 1.0f, Mode::Loop);
        const auto take = sineGesture (100, 0.8f);
        check (h.publish (0, take, 1.0f), "amount 0: a take can be installed");
        h.run (300);

        check (h.gloop.laneOffset (0) == 0.0f, "amount 0: zero offset");
        check (h.visual (0).amount == 0.0f, "amount 0: zero effective amount");
        check (h.gloop.lanePhase (0) > 0.0f, "amount 0: the playhead still runs");
    }

    {   // A motionless take is valid and contributes exactly nothing.
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
        const auto take = constantGesture (100, 0.0f);
        check (h.publish (0, take, 1.0f), "motionless: a motionless take is accepted");
        h.run (300);

        check (h.gloop.laneOffset (0) == 0.0f, "motionless: no invented motion");
        check (h.gloop.offsetFor (Target::Focus) == 0.0f,
               "motionless: the target offset is exactly zero");
        check (h.gloop.lanePhase (0) > 0.0f, "motionless: the lane still runs");
    }

    {   // An empty publish clears the lane instead of leaving stale data.
        Harness h;
        const float single = 0.5f;
        check (h.gloop.publish (0, nullptr, 0, 0.0f), "empty: an empty publish is accepted");
        check (h.gloop.publish (1, &single, 1, 0.0f), "empty: a one-sample publish is accepted");
        h.run (64);
        check (! h.gloop.laneHasGesture (0), "empty: the lane holds no take");
        check (! h.gloop.laneHasGesture (1), "empty: one sample is not a take");
    }
}

// =============================================================================
// 2. A take plays, and amount scales it linearly.
// =============================================================================

void testGloopPlaybackAndAmount()
{
    info ("Gloop: playback and amount");

    const auto take = sineGesture (200, 0.5f);

    const auto peakFor = [&take] (float amount)
    {
        Harness h;
        setLane (h.params, 0, Target::Focus, amount, 1.0f, Mode::Loop);
        h.publish (0, take, 2.0f);
        h.begin();

        float peak = 0.0f;
        for (int i = 0; i < 3000; ++i)   // 1 s at the control rate
        {
            h.gloop.processFrame (kControlDt);
            peak = std::max (peak, std::abs (h.gloop.laneOffset (0)));
        }

        return peak;
    };

    const float full = peakFor (1.0f);
    const float half = peakFor (0.5f);

    info (fmt ("gloop: peak offset amount 1.0 = %.3f, amount 0.5 = %.3f",
               (double) full, (double) half));

    check (full > 0.40f && full <= 0.51f, "playback: a recorded take moves the offset");
    check (half > 0.20f && half <= 0.26f, "playback: amount 0.5 halves the movement");
    checkNear ((double) half, 0.5 * (double) full, 0.02,
               "playback: amount scales the offset linearly");

    {   // The same take, verified through the public visual snapshot.
        Harness h;
        setLane (h.params, 0, Target::Focus, 0.5f, 1.0f, Mode::Loop);
        h.publish (0, take, 2.0f);
        h.run (1200);

        const auto v = h.visual (0);
        checkNear ((double) v.amount, 0.5, 1.0e-4, "playback: visual amount is the effective amount");
        checkNear ((double) v.offset, (double) h.gloop.laneOffset (0), 1.0e-6,
                   "playback: visual offset is the effective offset");
        checkNear ((double) v.durationSeconds, 2.0, 1.0e-4,
                   "playback: visual reports the take duration");
    }
}

// =============================================================================
// 3. Speed: playback rate, and smooth changes without a phase restart.
// =============================================================================

void testGloopSpeed()
{
    info ("Gloop: speed");

    const auto phaseAfter = [] (float speed, int frames)
    {
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, speed, Mode::Loop);
        const auto take = constantGesture (100, 0.5f);
        h.publish (0, take, 1.0f);
        h.run (frames);
        return (double) h.gloop.lanePhase (0);
    };

    // 750 control frames = 0.25 s of a 1 s take (period 0.955 s after the seam).
    const double slow = phaseAfter (0.25f, 750);
    const double fast = phaseAfter (2.0f, 750);

    info (fmt ("gloop: phase after 0.25 s at speed 0.25 = %.4f, at speed 2 = %.4f",
               slow, fast));

    check (slow > 0.05, "speed: 0.25 still moves the playhead");
    check (fast > 0.50, "speed: 2.0 moves the playhead fast");
    checkNear (fast / slow, 8.0, 0.1, "speed: 2.0 advances eight times faster than 0.25");

    {   // A speed change mid-play glides: no restart, no phase jump.
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
        const auto take = constantGesture (100, 0.5f);
        h.publish (0, take, 1.0f);
        h.run (300);

        const double before = (double) h.gloop.lanePhase (0);
        check (before > 0.05, "speed change: the playhead had advanced before the change");

        h.params.gloopLanes[0].speed = 2.0f;
        h.begin();

        double previous = before;
        double largestStep = 0.0;
        bool   restarted = false;

        for (int i = 0; i < 2400; ++i)   // 0.8 s at the control rate
        {
            h.gloop.processFrame (kControlDt);
            const double phase = (double) h.gloop.lanePhase (0);

            if (phase < previous)
            {
                const bool wrapped = previous > 0.9 && phase < 0.1;

                if (! wrapped)
                    restarted = true;
            }
            else
            {
                largestStep = std::max (largestStep, phase - previous);
            }

            previous = phase;
        }

        check (! restarted, "speed change: the playhead never jumps back or restarts");
        check (largestStep < 0.01, "speed change: the playhead advances smoothly");
        check (h.gloop.lanePlaybackRate (0) > 1.8f,
               "speed change: the speed glides up to the new value");
    }
}

// =============================================================================
// 4. FADE: monotonic 2.5 s withdraw, then Idle until asked again.
// =============================================================================

void testGloopFade()
{
    info ("Gloop: fade");

    Harness h;
    setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
    const auto take = constantGesture (100, 0.4f);
    h.publish (0, take, 1.0f);
    h.run (600);

    checkNear ((double) h.gloop.laneOffset (0), 0.4, 0.01,
               "fade: the take is playing at full influence");
    checkNear ((double) h.gloop.laneEffectiveAmount (0), 1.0, 1.0e-4,
               "fade: effective amount starts at 1");

    h.params.gloopLanes[0].mode = asInt (Mode::Fade);
    h.begin();

    bool  monotonic = true;
    bool  sawFade = false;
    float previous = h.gloop.laneEffectiveAmount (0);
    int   framesToIdle = -1;

    for (int i = 0; i < 3200; ++i)
    {
        const float dt = 0.001f;
        h.gloop.processFrame (dt);

        const float amount = h.gloop.laneEffectiveAmount (0);
        if (amount > previous + 1.0e-6f)
            monotonic = false;
        previous = amount;

        if (h.gloop.laneMode (0) == asInt (Mode::Fade))
            sawFade = true;

        if (h.gloop.laneMode (0) == asInt (Mode::Idle))
        {
            framesToIdle = i + 1;
            break;
        }
    }

    info (fmt ("gloop: fade reached Idle after %.3f s (monotonic %d)",
               (double) framesToIdle * 0.001, monotonic ? 1 : 0));

    check (sawFade, "fade: the lane reports Fade while withdrawing");
    check (monotonic, "fade: the effective amount decreases monotonically");
    check (framesToIdle > 2400 && framesToIdle <= 2600,
           "fade: the withdraw takes about 2.5 s");
    check (h.gloop.laneMode (0) == asInt (Mode::Idle), "fade: the lane ends in Idle");
    check (h.gloop.laneFadeGain (0) == 0.0f, "fade: the gain reaches zero");
    check (h.gloop.laneEffectiveAmount (0) == 0.0f, "fade: the effective amount reaches zero");
    check (h.gloop.laneHasGesture (0), "fade: the take is kept for a later LOOP");

    {   // The request still says Fade: a completed fade must not restart.
        h.begin();

        for (int i = 0; i < 1000; ++i)
            h.gloop.processFrame (0.001);

        check (h.gloop.laneMode (0) == asInt (Mode::Idle),
               "fade: a completed fade does not restart while requested as Fade");
        check (h.gloop.laneEffectiveAmount (0) == 0.0f,
               "fade: a completed fade stays silent");
    }

    {   // Asking for LOOP again glides back in instead of jumping.
        h.params.gloopLanes[0].mode = asInt (Mode::Loop);
        h.begin();

        float largestStep = 0.0f;
        float previousAmount = h.gloop.laneEffectiveAmount (0);

        for (int i = 0; i < 200; ++i)
        {
            h.gloop.processFrame (0.001);
            const float amount = h.gloop.laneEffectiveAmount (0);
            largestStep = std::max (largestStep, std::abs (amount - previousAmount));
            previousAmount = amount;
        }

        check (largestStep < 0.01f, "fade: returning to Loop glides, it does not jump");
        check (h.gloop.laneEffectiveAmount (0) > 0.01f,
               "fade: returning to Loop restores influence");
    }
}

// =============================================================================
// 5. Seam: a take whose start and end disagree still wraps cleanly.
// =============================================================================

void testGloopSeam()
{
    info ("Gloop: seam crossfade");

    Harness h;
    h.params.gloopSmoothing = 0.0f;   // 8 ms: the crossfade has to do the work
    setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);

    // A ramp from 0 to 0.8: without a seam crossfade the wrap would step by the
    // whole 0.8 in one control frame.
    const auto take = rampGesture (100, 0.0f, 0.8f);
    h.publish (0, take, 1.0f);
    h.begin();

    float previous = h.gloop.laneOffset (0);
    float largestStep = 0.0f;
    float lo = previous, hi = previous;

    for (int i = 0; i < 12000; ++i)   // 4 s: several wraps of a 1 s take
    {
        h.gloop.processFrame (kControlDt);
        const float offset = h.gloop.laneOffset (0);
        largestStep = std::max (largestStep, std::abs (offset - previous));
        previous = offset;
        lo = std::min (lo, offset);
        hi = std::max (hi, offset);
    }

    info (fmt ("gloop: seam largest per-frame step %.5f over range %.3f",
               (double) largestStep, (double) (hi - lo)));

    check (hi - lo > 0.3f, "seam: the take still plays across the loop");
    check (largestStep < 0.02f,
           "seam: a discontinuous take wraps without a large per-frame jump");
}

// =============================================================================
// 6. Live centre: one offset, two knob positions (through the real engine).
// =============================================================================

void testGloopLiveCentre()
{
    info ("Gloop: live centre");

    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = gloopParams();
    p.focus = 0.2f;
    setLane (p, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
    engine.setParams (p);

    const auto take = constantGesture (200, 0.2f);
    check (engine.publishGloopGesture (0, take.data(), (int) take.size(), 2.0f),
           "live centre: the take is published to the engine");

    bool finite = true;
    renderEngine (engine, 100, 512, finite);   // > 1 s: the offset smoother settles

    nagloed::VisualState v;
    engine.getVisual (v);
    info (fmt ("gloop: focus knob 0.20 -> engine focus %.3f", (double) v.focus));

    check (finite, "live centre: the output stays finite");
    checkNear ((double) v.focus, 0.4, 0.02, "live centre: the offset orbits the 0.20 centre");

    // The APVTS value is never written: only the engine's local control changes.
    checkNear ((double) p.focus, 0.2, 1.0e-6, "live centre: the parameter value is untouched");

    p.focus = 0.7f;
    engine.setParams (p);
    renderEngine (engine, 100, 512, finite);
    engine.getVisual (v);

    info (fmt ("gloop: focus knob 0.70 -> engine focus %.3f", (double) v.focus));

    checkNear ((double) v.focus, 0.9, 0.02,
               "live centre: the same offset orbits a different centre");
    check (v.gloop.lanes[0].hasGesture, "live centre: the lane visual reports the take");
    check (v.frame > 0, "live centre: the visual frame counter advances");
}

// =============================================================================
// 6b. Gloop orbits the knob, before the macros add their contribution.
// =============================================================================

void testGloopKnobCentre()
{
    info ("Gloop: Gloop is centred on the knob, not on the macro result");

    // Glow adds +8 dB of Drive while the Drive knob itself only goes to 30 dB.
    // A lane at -0.2 must move the 0 dB knob (and stay at 0 dB), not drag the
    // macro-raised 8 dB value down by 20% of the knob range.
    nagloed::Engine engine;
    engine.prepare (kSampleRate, 512, 2);
    engine.reset();

    auto p = gloopParams();
    p.macroGlow = 1.0f;
    p.driveDb = 0.0f;
    p.gloopSmoothing = 0.0f;   // 8 ms: settle within the render
    setLane (p, 0, Target::Drive, 1.0f, 1.0f, Mode::Loop);
    engine.setParams (p);

    const auto take = constantGesture (100, -0.2f);
    check (engine.publishGloopGesture (0, take.data(), (int) take.size(), 1.0f),
           "knob centre: the drive take is published");

    bool finite = true;
    renderEngine (engine, 40, 512, finite);

    nagloed::VisualState v;
    engine.getVisual (v);

    // Gloop(0 dB knob, -0.2) -> 0 dB, then Glow adds 8 dB -> 8/30 of the drive meter.
    info (fmt ("gloop: drive knob 0 with Glow, lane -0.2 -> drive meter %.4f "
               "(knob-centred 0.2667, macro-centred 0.0667)",
               (double) v.drive));

    check (finite, "knob centre: the output stays finite");
    checkNear ((double) v.drive, 8.0 / 30.0, 0.005,
               "knob centre: a lane cannot pull the macro-raised drive below its knob");
}

// =============================================================================
// 7. Two lanes on one target: they sum, bounded and smooth.
// =============================================================================

void testGloopSharedTarget()
{
    info ("Gloop: two lanes, one target");

    {   // One lane below the limiter knee is exactly linear.
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
        const auto take = constantGesture (100, 0.6f);
        h.publish (0, take, 1.0f);
        h.run (1200);

        checkNear ((double) h.gloop.offsetFor (Target::Focus), 0.6, 0.01,
                   "shared target: a single lane is linear below the knee");
    }

    {   // Two lanes sum, then the smooth limiter bounds the result.
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
        setLane (h.params, 1, Target::Focus, 1.0f, 1.3f, Mode::Loop);
        const auto takeA = constantGesture (100, 0.9f);
        const auto takeB = sineGesture (100, 0.9f);
        h.publish (0, takeA, 1.0f);
        h.publish (1, takeB, 1.0f);
        h.begin();

        float previous = h.gloop.offsetFor (Target::Focus);
        float largestStep = 0.0f;
        float peak = 0.0f;

        for (int i = 0; i < 2400; ++i)
        {
            h.gloop.processFrame (kControlDt);
            const float sum = h.gloop.offsetFor (Target::Focus);

            // The first 600 frames are the lanes' own attack (the ~40 ms offset
            // smoother); smoothness is measured once the sum is moving.
            if (i > 600)
                largestStep = std::max (largestStep, std::abs (sum - previous));

            previous = sum;
            peak = std::max (peak, std::abs (sum));
        }

        info (fmt ("gloop: two lanes (constant + sine) -> bounded offset %.3f, "
                   "largest step %.5f",
                   (double) peak, (double) largestStep));

        check (peak > 0.75f, "shared target: two lanes add up");
        check (peak < 1.25f, "shared target: the sum is bounded, never clipped");
        check (largestStep < 0.01f, "shared target: the summed offset is smooth");
    }

    {   // The full engine keeps the parameter inside its own range.
        nagloed::Engine engine;
        engine.prepare (kSampleRate, 512, 2);
        engine.reset();

        auto p = gloopParams();
        p.focus = 0.5f;
        setLane (p, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
        setLane (p, 1, Target::Focus, 1.0f, 1.7f, Mode::Loop);
        engine.setParams (p);

        const auto take = constantGesture (100, 0.9f);
        check (engine.publishGloopGesture (0, take.data(), (int) take.size(), 1.0f),
               "shared target: lane 0 take published");
        check (engine.publishGloopGesture (1, take.data(), (int) take.size(), 1.0f),
               "shared target: lane 1 take published");

        bool finite = true;
        renderEngine (engine, 100, 512, finite);

        nagloed::VisualState v;
        engine.getVisual (v);

        info (fmt ("gloop: two lanes at focus 0.5 -> engine focus %.3f", (double) v.focus));

        check (finite, "shared target: the engine stays finite");
        checkInRange ((double) v.focus, 0.0, 1.0,
                      "shared target: focus respects the parameter range");
        check (v.focus > 0.95f, "shared target: both lanes are audible on one knob");
    }
}

// =============================================================================
// 8. Randomize: only playing lanes, only amount/speed, always gradual.
// =============================================================================

void testGloopRandomize()
{
    info ("Gloop: randomize");

    Harness h;
    setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);   // playing
    setLane (h.params, 1, Target::Cutoff, 0.5f, 1.0f, Mode::Idle);  // READY, not playing

    const auto takeA = sineGesture (200, 0.5f);
    const auto takeB = sineGesture (200, 0.4f);
    h.publish (0, takeA, 2.0f);
    h.publish (1, takeB, 2.0f);
    h.run (600);

    const int   targetBefore = h.visual (0).target;
    const float idleScaleBefore = h.gloop.laneAmountScale (1);
    const float idleRateBefore = h.gloop.lanePlaybackRate (1);
    const double idlePhaseBefore = (double) h.gloop.lanePhase (1);

    float largestAmountStep = 0.0f;
    float largestRateStep = 0.0f;
    float largestScaleChange = 0.0f;

    for (std::uint32_t epoch = 1; epoch <= 4; ++epoch)
    {
        h.params.gloopRandomize = epoch;
        h.begin();

        const float epochStartScale = h.gloop.laneAmountScale (0);
        float previousAmount = h.gloop.laneEffectiveAmount (0);
        float previousRate = h.gloop.lanePlaybackRate (0);

        for (int i = 0; i < 6000; ++i)   // 2 s per epoch
        {
            h.gloop.processFrame (kControlDt);
            const float amount = h.gloop.laneEffectiveAmount (0);
            const float rate = h.gloop.lanePlaybackRate (0);

            largestAmountStep = std::max (largestAmountStep, std::abs (amount - previousAmount));
            largestRateStep = std::max (largestRateStep, std::abs (rate - previousRate));

            previousAmount = amount;
            previousRate = rate;
        }

        // How far one press moved the lane's amount over the whole epoch.
        largestScaleChange = std::max (largestScaleChange,
                                       std::abs (h.gloop.laneAmountScale (0) - epochStartScale));
    }

    info (fmt ("gloop: randomize max per-frame amount step %.5f, rate step %.5f, "
               "largest scale change %.3f",
               (double) largestAmountStep, (double) largestRateStep,
               (double) largestScaleChange));

    check (largestAmountStep < 0.01f, "randomize: the amount moves gradually");
    check (largestRateStep < 0.02f, "randomize: the speed moves gradually");
    check (largestScaleChange > 0.01f, "randomize: a playing lane is re-aimed");

    // Idle lanes, and the lane's target, are untouched.
    check (h.gloop.laneAmountScale (1) == idleScaleBefore,
           "randomize: an idle lane keeps its amount scale");
    check (h.gloop.lanePlaybackRate (1) == idleRateBefore,
           "randomize: an idle lane keeps its speed");
    check ((double) h.gloop.lanePhase (1) == idlePhaseBefore,
           "randomize: an idle lane does not start playing");
    check (h.visual (0).target == targetBefore, "randomize: the lane target never changes");
    check (h.gloop.laneMode (1) == asInt (Mode::Idle), "randomize: the idle lane stays idle");
}

// =============================================================================
// 9. The SPSC hand-off: preallocated, retryable, lossless.
// =============================================================================

void testGloopQueue()
{
    info ("Gloop: gesture hand-off");

    Harness h;
    const auto take = constantGesture (50, 0.5f);

    int accepted = 0;
    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
        if (h.gloop.publish (lane, take.data(), (int) take.size(), 0.5f))
            ++accepted;

    check (accepted == nagl::gloop::kNumLanes,
           "queue: a burst of eight takes is accepted without a retry");

    check (! h.gloop.publish (0, take.data(), (int) take.size(), 0.5f),
           "queue: a full queue is reported instead of silently dropping the take");

    h.begin();   // the audio thread drains at the start of the block

    bool allInstalled = true;
    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
        allInstalled = allInstalled && h.gloop.laneHasGesture (lane);

    check (allInstalled, "queue: every published take is installed by the next block");
    check (h.gloop.publish (0, take.data(), (int) take.size(), 0.5f),
           "queue: the drained queue accepts a retry");

    check (! h.gloop.publish (-1, take.data(), (int) take.size(), 0.5f),
           "queue: an invalid lane is rejected");
    check (! h.gloop.publish (0, nullptr, 10, 0.5f),
           "queue: a null take is rejected");
    check (! h.gloop.publish (0, take.data(), nagl::gloop::kMaxGestureSamples + 1, 1.0f),
           "queue: an oversized take is rejected");
}

// =============================================================================
// 9b. reset()/prepare(): rewind playback, never lose a take.
// =============================================================================

void testGloopReset()
{
    info ("Gloop: reset keeps recorded takes");

    Harness h;
    setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);
    const auto take = sineGesture (200, 0.5f);
    h.publish (0, take, 2.0f);
    h.run (600);

    check (h.gloop.lanePhase (0) > 0.0f, "reset: the playhead had advanced");

    h.gloop.reset();

    check (h.gloop.laneHasGesture (0), "reset: the recorded take survives");
    check (h.gloop.laneMode (0) == asInt (Mode::Loop),
           "reset: the lane returns to its requested mode");
    check (h.gloop.lanePhase (0) == 0.0f, "reset: the playhead is rewound");
    check (h.gloop.laneOffset (0) == 0.0f, "reset: no residual offset");

    // A host flush must not leave a half-finished fade or glide behind.
    h.gloop.prepare (kSampleRate);
    check (h.gloop.laneHasGesture (0), "prepare: the recorded take survives");

    h.run (1200);   // the processor republishes after prepare(); the lane plays on
    check (h.gloop.laneOffset (0) != 0.0f, "reset: the lane plays again after the rewind");
    check (h.gloop.laneMode (0) == asInt (Mode::Loop), "reset: the lane keeps playing");
}

// =============================================================================
// 10. Visual state.
// =============================================================================

void testGloopVisual()
{
    info ("Gloop: visual state");

    Harness h;
    setLane (h.params, 0, Target::Focus, 0.6f, 1.0f, Mode::Loop);
    const auto take = sineGesture (300, 0.4f);
    h.publish (0, take, 3.0f);
    h.run (600);

    nagl::gloop::Visual visual;
    h.gloop.copyVisual (visual, 42);

    const auto& lane = visual.lanes[0];
    check (visual.frame == 42, "visual: the frame counter is echoed");
    check (lane.target == asInt (Target::Focus), "visual: the target is reported");
    check (lane.mode == asInt (Mode::Loop), "visual: the actual mode is reported");
    check (lane.hasGesture, "visual: the installed take is reported");
    checkInRange ((double) lane.phase, 0.0, 1.0, "visual: the phase is inside the loop");
    check (lane.phase > 0.0f, "visual: the phase advances");
    checkNear ((double) lane.amount, 0.6, 1.0e-4, "visual: the effective amount is reported");
    checkInRange ((double) lane.offset, -1.0, 1.0, "visual: the offset is normalised");
    checkNear ((double) lane.centreNorm, 0.5, 1.0e-4,
               "visual: the live knob centre is reported in normalised units");
    checkNear ((double) lane.durationSeconds, 3.0, 1.0e-4,
               "visual: the take duration is reported");

    // A lane that has not been touched keeps the contract defaults.
    check (visual.lanes[1].target == asInt (Target::Off), "visual: unused lanes are Off");
    check (visual.lanes[1].mode == asInt (Mode::Idle), "visual: unused lanes are Idle");
    check (! visual.lanes[1].hasGesture, "visual: unused lanes hold no take");

    {   // Entering REC throws the old take away (the UI then shows no take).
        h.params.gloopLanes[0].mode = asInt (Mode::Record);
        h.begin();
        h.gloop.copyVisual (visual, 43);

        check (! visual.lanes[0].hasGesture, "visual: a recording lane holds no take");
        check (visual.lanes[0].mode == asInt (Mode::Record),
               "visual: a recording lane reports Record");
        check (visual.lanes[0].amount == 0.0f,
               "visual: a recording lane has no effective amount");
        check (visual.frame == 43, "visual: the frame counter follows the block");
    }

    {   // ...and IDLE after a take is the READY state, not an empty lane.
        h.params.gloopLanes[0].mode = asInt (Mode::Idle);
        h.begin();
        h.run (120);
        h.gloop.copyVisual (visual, 44);

        check (visual.lanes[0].mode == asInt (Mode::Idle),
               "visual: the settled lane reports Idle");
        check (visual.lanes[0].amount == 0.0f && visual.lanes[0].offset == 0.0f,
               "visual: an idle lane reports no influence");
    }
}

// =============================================================================
// 11. Idle Gloop must be bit-transparent.
// =============================================================================

void testGloopIdleIsTransparent()
{
    info ("Gloop: idle lanes do not change the audio");

    const auto params = gloopParams();

    auto idleParams = params;
    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
    {
        const auto target = (Target) (asInt (Target::Focus) + (lane % 15));
        setLane (idleParams, lane, target, 0.8f, 0.5f + 0.15f * (float) lane, Mode::Idle);
    }

    nagloed::Engine reference;
    reference.prepare (kSampleRate, 512, 2);
    reference.reset();
    reference.setParams (params);

    nagloed::Engine withTakes;
    withTakes.prepare (kSampleRate, 512, 2);
    withTakes.reset();

    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
    {
        const auto take = sineGesture (120, 0.7f);
        withTakes.publishGloopGesture (lane, take.data(), (int) take.size(), 1.2f);
    }

    withTakes.setParams (idleParams);

    bool finiteA = true, finiteB = true;
    const auto a = renderEngine (reference, 40, 512, finiteA);
    const auto b = renderEngine (withTakes, 40, 512, finiteB);

    bool identical = a.size() == b.size();
    for (size_t i = 0; identical && i < a.size(); ++i)
        identical = a[i] == b[i];

    nagl::VisualState v;
    withTakes.getVisual (v);
    check (v.gloop.lanes[0].hasGesture, "idle transparency: the takes were installed");

    check (finiteA && finiteB, "idle transparency: both renders are finite");
    check (identical, "idle transparency: eight idle lanes leave the audio bit-identical");
}

// =============================================================================
// 12. Realtime contract: no allocation on the audio thread.
// =============================================================================

void testGloopNoAllocation()
{
    info ("Gloop: audio-thread allocation");

    nagloed::Engine engine;
    engine.prepare (kSampleRate, 128, 2);
    engine.reset();

    auto p = gloopParams();

    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
    {
        setLane (p, lane, (Target) (asInt (Target::Focus) + lane), 0.6f, 1.2f, Mode::Loop);
        const auto take = sineGesture (150, 0.5f);
        engine.publishGloopGesture (lane, take.data(), (int) take.size(), 1.5f);
    }

    engine.setParams (p);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, 128);

    const auto fill = [&buffer] (int block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
                buffer.setSample (ch, i, 0.05f * std::sin (0.02f * (float) (block * 128 + i)));
    };

    // Warm up: any lazy initialisation happens before tracking starts.
    for (int b = 0; b < 8; ++b)
    {
        fill (b);
        engine.process (buffer, midi);
    }

    nagl_test::setAllocationTracking (true);
    nagl_test::resetAllocationCount();

    for (int b = 0; b < 512; ++b)
    {
        fill (b);

        // Exercise fades, mode changes and Randomize on the audio thread too.
        if (b == 64)
        {
            for (int lane = 1; lane < nagl::gloop::kNumLanes; ++lane)
                p.gloopLanes[(size_t) lane].mode = asInt (Mode::Fade);
            engine.setParams (p);
        }
        if (b == 128)
            engine.setParams (p);
        if (b == 192)
        {
            p.gloopSmoothing = 0.8f;
            engine.setParams (p);
        }
        if (b == 256)
        {
            p.gloopRandomize = 3u;
            engine.setParams (p);
        }
        if (b == 320)
        {
            p.gloopSmoothing = 0.0f;
            engine.setParams (p);
        }

        engine.process (buffer, midi);

        nagl::VisualState visual;
        engine.getVisual (visual);
    }

    nagl_test::setAllocationTracking (false);
    const long long allocations = nagl_test::allocationCount();

    info (fmt ("gloop audio-thread allocations: %lld", allocations));
    check (allocations == 0, "gloop: process/setParams/getVisual never allocate");
}

// =============================================================================
// 13. Bounded CPU: eight lanes are cheap (no per-sample work in Gloop).
// =============================================================================

// =============================================================================
// 16. Multi-parameter tracks: one lane timebase, several knobs.
// =============================================================================

void testGloopMultiTrack()
{
    info ("Gloop: multi-parameter tracks");

    {   // Multi lane: two tracks share one phase and move two knobs.
        Harness h;
        auto& lp = h.params.gloopLanes[0];
        lp.target = nagl::gloop::kAutoTarget;
        lp.amount = 1.0f;
        lp.speed = 1.0f;
        lp.mode = asInt (Mode::Loop);

        nagl::gloop::Gesture gesture;
        gesture.count = 200;
        gesture.durationSeconds = 2.0f;
        gesture.numTracks = 2;
        gesture.tracks[0].target = asInt (Target::Focus);
        gesture.tracks[1].target = asInt (Target::Spray);

        for (int i = 0; i < gesture.count; ++i)
        {
            gesture.tracks[0].offsets[(size_t) i] = 0.5f;
            gesture.tracks[1].offsets[(size_t) i] = 0.3f;
        }

        check (h.gloop.publish (0, gesture), "multi: a two-track take is accepted");
        h.run (600);

        check (h.gloop.laneTrackCount (0) == 2, "multi: both tracks are installed");
        check (h.gloop.laneTrackTarget (0, 0) == asInt (Target::Focus),
               "multi: first track target is Focus");
        check (h.gloop.laneTrackTarget (0, 1) == asInt (Target::Spray),
               "multi: second track target is Spray");
        check (h.gloop.offsetFor (Target::Focus) > 0.3f,
               "multi: Focus track contributes");
        check (h.gloop.offsetFor (Target::Spray) > 0.2f,
               "multi: Spray track contributes");

        const auto v = h.visual (0);
        check (v.numTracks == 2, "multi: the visual exposes both tracks");
        check (v.tracks[0].target == asInt (Target::Focus)
                   && v.tracks[1].target == asInt (Target::Spray),
               "multi: the visual carries the concrete track targets");
    }

    {   // A concrete single-target lane must not replay a mismatched track
        // (this protects old sessions and a target changed in the host).
        Harness h;
        setLane (h.params, 0, Target::Focus, 1.0f, 1.0f, Mode::Loop);

        nagl::gloop::Gesture gesture;
        gesture.count = 100;
        gesture.durationSeconds = 1.0f;
        gesture.numTracks = 1;
        gesture.tracks[0].target = asInt (Target::Spray);

        for (int i = 0; i < gesture.count; ++i)
            gesture.tracks[0].offsets[(size_t) i] = 0.5f;

        check (h.gloop.publish (0, gesture), "multi: a mismatched take is accepted");
        h.run (300);

        check (h.gloop.offsetFor (Target::Focus) == 0.0f,
               "multi: a concrete Focus lane ignores a Spray track");
        check (h.gloop.offsetFor (Target::Spray) == 0.0f,
               "multi: a concrete lane does not leak the other target");
    }
}

void testGloopCpu()
{
    info ("Gloop: CPU with all eight lanes playing");

    nagloed::Engine engine;
    engine.prepare (kSampleRate, 128, 2);
    engine.reset();

    auto p = gloopParams();
    p.density = 80.0f;
    p.sizeMs = 800.0f;
    p.spaceMix = 0.4f;

    const Target targets[8] = { Target::Focus, Target::Pitch, Target::Cutoff, Target::Space,
                                Target::Density, Target::Spray, Target::Width, Target::Shape };

    for (int lane = 0; lane < nagl::gloop::kNumLanes; ++lane)
    {
        setLane (p, lane, targets[(size_t) lane], 0.7f, 0.5f + 0.2f * (float) lane,
                 Mode::Loop);
        const auto take = sineGesture (200, 0.6f);
        engine.publishGloopGesture (lane, take.data(), (int) take.size(), 2.0f);
    }

    engine.setParams (p);

    const long long totalSamples = 480000;   // 10 s
    const int blockSize = 128;

    const auto start = std::chrono::steady_clock::now();
    bool finite = true;
    renderEngine (engine, (int) (totalSamples / blockSize), blockSize, finite);
    const auto end = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double> (end - start).count();
    const double audioSeconds = (double) totalSamples / kSampleRate;
    const double percentOfCore = 100.0 * seconds / audioSeconds;

    info (fmt ("gloop CPU: %.1f ms for %.1f s of audio at 128 samples = %.2f%% of one core",
               seconds * 1000.0, audioSeconds, percentOfCore));

    check (finite, "CPU: eight playing lanes produce finite audio");
    check (percentOfCore < 20.0, "CPU: eight playing lanes stay well inside the budget");
}

} // namespace

void runGloopDspTests()
{
    testGloopSilence();
    testGloopPlaybackAndAmount();
    testGloopSpeed();
    testGloopFade();
    testGloopSeam();
    testGloopLiveCentre();
    testGloopKnobCentre();
    testGloopSharedTarget();
    testGloopRandomize();
    testGloopQueue();
    testGloopReset();
    testGloopVisual();
    testGloopIdleIsTransparent();
    testGloopMultiTrack();
    testGloopNoAllocation();
    testGloopCpu();
}
