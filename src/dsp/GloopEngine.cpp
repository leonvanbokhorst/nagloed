#include "GloopEngine.h"

#include <algorithm>
#include <cmath>

#include "core/Parameters.h"

namespace nagloed::dsp
{

namespace
{
// EngineParams is a plain data blob: a host, a preset or a test can hand us
// anything, so every value crossing into DSP state is clamped or rejected here.
inline double clampSeconds (double v, double lo, double hi) noexcept
{
    if (! (v == v) || ! std::isfinite (v))
        return lo;
    return std::max (lo, std::min (hi, v));
}

inline float sanitiseOffset (float v) noexcept
{
    if (! (v == v))
        return 0.0f;
    return dsp::clamp (v, -1.0f, 1.0f);
}

inline int clampMode (int mode) noexcept
{
    return std::max (0, std::min (mode, (int) gloop::Mode::Count - 1));
}

inline int clampTarget (int target) noexcept
{
    return std::max (0, std::min (target, gloop::kNumTargets - 1));
}
} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

void GloopEngine::prepare (double sr) noexcept
{
    // Nothing to allocate: the queue and every gesture buffer are members. The
    // sample rate is only informational because every glide and fade is driven
    // by the dt the caller passes to processFrame().
    sampleRate = (sr > 0.0 && std::isfinite (sr)) ? sr : 48000.0;
    lastDtSeconds = 0.0;
    lastSmoothing = -1.0f;   // force a coefficient refresh on the next frame
}

void GloopEngine::reset() noexcept
{
    // A reset rewinds playback but must not throw recorded takes away: the
    // processor republishes after prepare(), and the UI's READY state depends on
    // the take still being installed.
    for (auto& lane : lanes)
    {
        lane.phase = 0.0;
        lane.offset = 0.0f;
        lane.rawOffset = 0.0f;
        lane.effectiveAmount = 0.0f;
        lane.amountScale = lane.amountScaleTarget = 1.0f;
        lane.randomSpeed = lane.randomSpeedTarget = 1.0f;
        lane.speedInitialised = false;      // re-snap to the parameter speed
        lane.fadeGain = lane.hasGesture ? 1.0f : 0.0f;
        refreshActualMode (lane);
    }

    offsetByTarget.fill (0.0f);

    // No spurious Randomize on the first block after a reset: the epoch that is
    // already in settings counts as seen.
    lastRandomize = settings.gloopRandomize;

    lastDtSeconds = 0.0;
    lastSmoothing = -1.0f;
}

// =============================================================================
// Message thread -> audio thread hand-off
// =============================================================================

bool GloopEngine::publish (int lane, const float* offsets, int count,
                           float durationSeconds) noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return false;

    if (count < 0 || count > gloop::kMaxGestureSamples)
        return false;

    if (count > 0 && offsets == nullptr)
        return false;

    if (! std::isfinite (durationSeconds) || durationSeconds < 0.0f)
        durationSeconds = 0.0f;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 <= 0)
        return false;   // full: the caller keeps the take and retries

    // AbstractFifo reports the writable run as up to two regions; write into the
    // one that actually has room so a burst (e.g. a state-restore republish of
    // all eight lanes) never fails spuriously when the free space wraps.
    const int index = size1 > 0 ? start1 : start2;

    auto& slot = slots[(size_t) index];
    slot.lane = lane;
    slot.count = count;
    slot.durationSeconds = durationSeconds;

    if (count > 0)
        std::copy (offsets, offsets + count, slot.offsets.begin());

    fifo.finishedWrite (1);
    return true;
}

void GloopEngine::drainGestures() noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);

    const int ready = size1 + size2;
    if (ready <= 0)
        return;

    for (int i = 0; i < size1; ++i)
        installFromSlot (slots[(size_t) (start1 + i)]);

    for (int i = 0; i < size2; ++i)
        installFromSlot (slots[(size_t) (start2 + i)]);

    fifo.finishedRead (ready);
}

void GloopEngine::installFromSlot (const Slot& slot) noexcept
{
    if (slot.lane < 0 || slot.lane >= gloop::kNumLanes)
        return;

    auto& lane = lanes[(size_t) slot.lane];

    if (slot.count < kMinSamples)
    {
        // Empty take: the lane stays valid but has nothing to play.
        lane.gesture.count = 0;
        lane.gesture.durationSeconds = 0.0f;
        lane.gestureDuration = 0.0;
        lane.seamSeconds = 0.0;
        lane.loopPeriod = 0.0;
        lane.hasGesture = false;
        lane.phase = 0.0;
        refreshActualMode (lane);
        return;
    }

    const int count = std::min (slot.count, gloop::kMaxGestureSamples);

    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float value = sanitiseOffset (slot.offsets[(size_t) i]);
        lane.gesture.offsets[(size_t) i] = value;
        peak = std::max (peak, std::abs (value));
    }

    lane.gesture.count = count;

    const double fallbackDuration = (double) count / (double) gloop::kGestureSampleRate;
    double duration = slot.durationSeconds > 0.0f ? (double) slot.durationSeconds
                                                  : fallbackDuration;
    duration = clampSeconds (duration, 1.0e-3, 600.0);

    // Crossfade the last seam into the first seam: the loop period shrinks by the
    // seam length, which is exactly what keeps the wrap continuous (see readLane).
    const double seam = std::min ((double) kSeamSeconds, duration * 0.25);

    lane.gesture.durationSeconds = (float) duration;
    lane.gestureDuration = duration;
    lane.seamSeconds = seam;
    lane.loopPeriod = duration - seam;

    lane.hasGesture = true;
    lane.phase = 0.0;          // a new take starts at its own beginning
    lane.fadeGain = 1.0f;      // and enters at full influence; no fade-in on REC

    (void) peak;               // a motionless take is valid and simply yields 0
    refreshActualMode (lane);
}

// =============================================================================
// Control rate
// =============================================================================

void GloopEngine::beginBlock (const EngineParams& params) noexcept
{
    // Install first: a take published just before this block must be playable in
    // it, so a LOOP request in the same block never waits a block for its data.
    drainGestures();

    settings = params;

    if (params.gloopRandomize != lastRandomize)
    {
        lastRandomize = params.gloopRandomize;
        randomiseTargets (lastRandomize);
    }

    for (int i = 0; i < gloop::kNumLanes; ++i)
    {
        auto& lane = lanes[(size_t) i];
        const auto& lp = params.gloopLanes[(size_t) i];

        lane.target = clampTarget (lp.target);
        lane.amountParam = dsp::clamp (lp.amount, 0.0f, 1.0f);
        lane.speedParam = dsp::clamp (lp.speed, gloop::kMinSpeed, gloop::kMaxSpeed);

        if (! lane.speedInitialised)
        {
            // The first block snaps; later changes glide so an automated speed
            // knob can never restart or jump the playhead.
            lane.speedSmoothed = lane.speedParam;
            lane.speedInitialised = true;
        }

        const int requested = clampMode (lp.mode);
        if (requested != lane.lastRequested)
        {
            lane.lastRequested = requested;

            // Entering REC throws the previous take away. The processor starts a
            // fresh capture at that moment; if the new hand-off is briefly
            // pending (its message-thread tick has not fired yet), LOOP must not
            // fall back to replaying the old take.
            if (requested == (int) gloop::Mode::Record)
                clearLaneForRecording (lane);

            refreshActualMode (lane);
        }

        lane.centreNorm = lane.target > (int) gloop::Target::Off
                              ? gloopTargetNorm (params, lane.target)
                              : 0.0f;
    }
}

void GloopEngine::processFrame (double dtSeconds) noexcept
{
    // Defensive: a host that hands us a nonsensical dt must not teleport a loop.
    if (! (dtSeconds > 0.0) || ! std::isfinite (dtSeconds))
        dtSeconds = 0.0;
    dtSeconds = std::min (dtSeconds, 0.25);

    if (std::abs (dtSeconds - lastDtSeconds) > 1.0e-12
        || std::abs (settings.gloopSmoothing - lastSmoothing) > 1.0e-6f)
        updateCoefficients (dtSeconds);

    const float fadeSlope = (float) (dtSeconds / (double) kFadeSeconds);

    std::array<float, (size_t) gloop::kNumTargets> sums {};

    for (int i = 0; i < gloop::kNumLanes; ++i)
    {
        auto& lane = lanes[(size_t) i];

        // --- fade in/out ------------------------------------------------------
        // FADE withdraws monotonically; LOOP glides back in at the same slope so
        // returning to a lane never steps the modulation.
        if (lane.actualMode == (int) gloop::Mode::Fade)
        {
            lane.fadeGain -= fadeSlope;

            if (lane.fadeGain <= 0.0f)
            {
                lane.fadeGain = 0.0f;
                lane.actualMode = (int) gloop::Mode::Idle;   // visual reports Idle
            }
        }
        else if (lane.actualMode == (int) gloop::Mode::Loop)
        {
            lane.fadeGain = std::min (1.0f, lane.fadeGain + fadeSlope);
        }

        // --- slow glides (randomise, speed parameter) ------------------------
        lane.amountScale += (lane.amountScaleTarget - lane.amountScale) * randomCoeff;
        lane.randomSpeed += (lane.randomSpeedTarget - lane.randomSpeed) * randomCoeff;
        lane.speedSmoothed += (lane.speedParam - lane.speedSmoothed) * speedCoeff;

        const bool playing = lane.hasGesture
                             && (lane.actualMode == (int) gloop::Mode::Loop
                                 || lane.actualMode == (int) gloop::Mode::Fade);

        // --- phase ------------------------------------------------------------
        // One period per lane, no shared clock: the lanes drift apart on purpose.
        if (playing && lane.loopPeriod > 1.0e-6)
        {
            // Randomize may nudge the effective playback time, but the lane's
            // documented speed range (0.25x..2x) remains the hard bound; the
            // user's SPEED knob is never silently exceeded.
            const double rate = std::clamp ((double) lane.speedSmoothed
                                                * (double) lane.randomSpeed,
                                            (double) gloop::kMinSpeed,
                                            (double) gloop::kMaxSpeed);
            lane.phase += (dtSeconds * rate) / lane.loopPeriod;

            if (! std::isfinite (lane.phase))
                lane.phase = 0.0;

            lane.phase -= std::floor (lane.phase);
        }

        // --- offset -----------------------------------------------------------
        const bool contributing = playing && lane.target > (int) gloop::Target::Off;

        if (contributing)
        {
            lane.effectiveAmount = dsp::clamp (lane.amountParam * lane.amountScale
                                                   * lane.fadeGain,
                                               0.0f, 1.0f);
            lane.rawOffset = dsp::clamp (readLane (lane) * lane.effectiveAmount,
                                         -1.0f, 1.0f);
        }
        else
        {
            lane.effectiveAmount = 0.0f;
            lane.rawOffset = 0.0f;
        }

        lane.offset += (lane.rawOffset - lane.offset) * offsetCoeff;
        lane.offset = dsp::flushDenormal (lane.offset);
        lane.offset = dsp::clamp (lane.offset, -1.0f, 1.0f);

        // An idle lane withdraws through the smoother and then sits at hard zero,
        // so an empty lane can never leave a residual modulation behind.
        if (lane.rawOffset == 0.0f && std::abs (lane.offset) < 1.0e-6f)
            lane.offset = 0.0f;

        if (contributing)
            sums[(size_t) lane.target] += lane.offset;
    }

    // Several lanes may orbit the same knob. They sum, then go through a limiter
    // that is exactly linear below kOffsetLinear and smoothly bounded above it:
    // no last-writer-wins, no hard clipping, no fighting.
    for (int t = 0; t < gloop::kNumTargets; ++t)
        offsetByTarget[(size_t) t] = softLimitOffset (sums[(size_t) t]);
}

void GloopEngine::clearLaneForRecording (Lane& lane) noexcept
{
    lane.gesture.count = 0;
    lane.gesture.durationSeconds = 0.0f;
    lane.gestureDuration = 0.0;
    lane.seamSeconds = 0.0;
    lane.loopPeriod = 0.0;
    lane.hasGesture = false;
    lane.phase = 0.0;
    lane.fadeGain = 0.0f;
    lane.effectiveAmount = 0.0f;
    lane.rawOffset = 0.0f;
    lane.offset = 0.0f;

    // A fresh take starts from the user's ribbon, not from a previous Randomize
    // transformation: the AMOUNT knob must represent the effective depth again.
    lane.amountScale = lane.amountScaleTarget = 1.0f;
    lane.randomSpeed = lane.randomSpeedTarget = 1.0f;
}

void GloopEngine::refreshActualMode (Lane& lane) noexcept
{
    switch ((gloop::Mode) lane.lastRequested)
    {
        case gloop::Mode::Record:
            lane.actualMode = (int) gloop::Mode::Record;
            return;

        case gloop::Mode::Loop:
            lane.actualMode = lane.hasGesture ? (int) gloop::Mode::Loop
                                              : (int) gloop::Mode::Idle;
            return;

        case gloop::Mode::Fade:
            // A completed fade stays completed while the request still says Fade:
            // the processor settles the request to Idle on its next tick, and
            // until then nothing may restart the fade.
            if (lane.hasGesture && lane.fadeGain > 0.0f)
            {
                lane.actualMode = (int) gloop::Mode::Fade;
                return;
            }

            lane.fadeGain = 0.0f;
            lane.actualMode = (int) gloop::Mode::Idle;
            return;

        case gloop::Mode::Idle:
        case gloop::Mode::Count:
            break;
    }

    lane.actualMode = (int) gloop::Mode::Idle;
}

void GloopEngine::randomiseTargets (std::uint32_t epoch) noexcept
{
    // Deterministic per epoch: the same take and the same press always produce
    // the same transformation, which makes the behaviour testable.
    random.seed (0x6c6f6f70u ^ (epoch * 2654435761u));

    for (auto& lane : lanes)
    {
        // Only lanes that are already moving are re-aimed. IDLE (READY) and REC
        // lanes keep their settings, so pressing Randomize while recording can
        // never disturb a take that is being captured.
        if (! lane.hasGesture)
            continue;

        if (lane.actualMode != (int) gloop::Mode::Loop
            && lane.actualMode != (int) gloop::Mode::Fade)
            continue;

        // Amount and speed only: target stays, phase keeps running, and the new
        // values are reached by a slow glide, never by a jump.
        lane.amountScaleTarget = random.range (0.35f, 1.0f);
        lane.randomSpeedTarget = random.range (0.60f, 1.40f);
    }
}

void GloopEngine::updateCoefficients (double dtSeconds) noexcept
{
    lastDtSeconds = dtSeconds;
    lastSmoothing = dsp::clamp (settings.gloopSmoothing, 0.0f, 1.0f);

    const auto tau = [dtSeconds] (float seconds) noexcept
    {
        const double value = std::max ((double) seconds, 1.0e-4);
        return dtSeconds > 0.0 ? dsp::clamp ((float) (1.0 - std::exp (-dtSeconds / value)),
                                             0.0f, 1.0f)
                               : 0.0f;
    };

    // 0 -> 8 ms, 0.35 -> ~40 ms, 1 -> 260 ms.
    const float s = lastSmoothing;
    const float offsetTau = kSmoothMinSeconds
                            + s * s * (kSmoothMaxSeconds - kSmoothMinSeconds);

    offsetCoeff = tau (offsetTau);
    speedCoeff  = tau (kSpeedGlideSeconds);
    randomCoeff = tau (kRandomGlideSeconds);
}

// =============================================================================
// Gesture reading
// =============================================================================

float GloopEngine::sampleAt (const gloop::Gesture& gesture, double duration,
                             double seconds) noexcept
{
    if (gesture.count < kMinSamples || ! (duration > 0.0))
        return 0.0f;

    seconds = clampSeconds (seconds, 0.0, duration);

    // The recorder stored `count` samples at 100 Hz; duration/count is the
    // spacing, so a take keeps its musical length even if duration came from a
    // different clock than the nominal 10 ms grid.
    const double x = seconds * (double) gesture.count / duration;

    int i0 = (int) x;
    if (i0 < 0)
        i0 = 0;
    if (i0 > gesture.count - 1)
        i0 = gesture.count - 1;

    const int i1 = std::min (i0 + 1, gesture.count - 1);
    const float frac = (float) (x - (double) i0);

    const float a = gesture.offsets[(size_t) i0];
    const float b = gesture.offsets[(size_t) i1];
    return a + (b - a) * frac;
}

float GloopEngine::readLane (const Lane& lane) const noexcept
{
    const auto& gesture = lane.gesture;

    if (gesture.count < kMinSamples || ! (lane.loopPeriod > 0.0)
        || ! (lane.seamSeconds > 0.0))
        return 0.0f;

    const double u = lane.phase * lane.loopPeriod;   // seconds into the loop period
    const double bodyPosition = u + lane.seamSeconds;

    const float body = sampleAt (gesture, lane.gestureDuration, bodyPosition);

    // Seam crossfade. The body plays [seam, duration]; over the last `seam`
    // seconds the source tail fades out while the head [0, seam] fades in.
    // Because the loop period is duration - seam, the next cycle already starts
    // at f(seam) — exactly where the crossfade ended — so the wrap is continuous
    // even when the recorded start and end values disagree.
    const double crossfadeStart = lane.loopPeriod - lane.seamSeconds;

    if (u > crossfadeStart)
    {
        const double w = dsp::clamp ((float) ((u - crossfadeStart) / lane.seamSeconds),
                                     0.0f, 1.0f);
        const float head = sampleAt (gesture, lane.gestureDuration,
                                     bodyPosition - lane.loopPeriod);
        return body + (head - body) * (float) w;
    }

    return body;
}

float GloopEngine::softLimitOffset (float offset) noexcept
{
    const float magnitude = std::abs (offset);

    if (magnitude <= kOffsetLinear)
        return offset;

    // C1-continuous knee: unity slope at kOffsetLinear, asymptote kOffsetCeiling.
    const float headroom = kOffsetCeiling - kOffsetLinear;
    const float bounded = kOffsetLinear
                          + headroom * dsp::fastTanh ((magnitude - kOffsetLinear)
                                                      / headroom);
    return offset < 0.0f ? -bounded : bounded;
}

// =============================================================================
// Queries
// =============================================================================

float GloopEngine::offsetFor (gloop::Target target) const noexcept
{
    const int index = (int) target;

    if (index <= (int) gloop::Target::Off || index >= gloop::kNumTargets)
        return 0.0f;

    return offsetByTarget[(size_t) index];
}

void GloopEngine::copyVisual (gloop::Visual& out, std::uint32_t frame) const noexcept
{
    for (int i = 0; i < gloop::kNumLanes; ++i)
    {
        const auto& lane = lanes[(size_t) i];
        auto& v = out.lanes[(size_t) i];

        v.target = lane.target;
        v.mode = lane.actualMode;
        v.hasGesture = lane.hasGesture;
        v.phase = (float) lane.phase;
        v.offset = lane.offset;
        v.amount = lane.effectiveAmount;
        v.centreNorm = lane.centreNorm;
        v.durationSeconds = lane.gesture.durationSeconds;
    }

    out.frame = frame;
}

int GloopEngine::laneMode (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes) ? lanes[(size_t) lane].actualMode
                                                  : (int) gloop::Mode::Idle;
}

bool GloopEngine::laneHasGesture (int lane) const noexcept
{
    return lane >= 0 && lane < gloop::kNumLanes && lanes[(size_t) lane].hasGesture;
}

float GloopEngine::lanePhase (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes) ? (float) lanes[(size_t) lane].phase
                                                  : 0.0f;
}

float GloopEngine::laneOffset (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes) ? lanes[(size_t) lane].offset : 0.0f;
}

float GloopEngine::laneEffectiveAmount (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes)
               ? lanes[(size_t) lane].effectiveAmount
               : 0.0f;
}

float GloopEngine::laneAmountScale (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes) ? lanes[(size_t) lane].amountScale
                                                  : 1.0f;
}

float GloopEngine::lanePlaybackRate (int lane) const noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return 1.0f;

    const auto& l = lanes[(size_t) lane];
    return std::clamp (l.speedSmoothed * l.randomSpeed,
                       gloop::kMinSpeed, gloop::kMaxSpeed);
}

float GloopEngine::laneFadeGain (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes) ? lanes[(size_t) lane].fadeGain : 0.0f;
}

} // namespace nagloed::dsp
