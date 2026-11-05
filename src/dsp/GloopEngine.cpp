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
    return std::max (0, std::min (target, gloop::kNumTargetChoices - 1));
}

inline int clampConcreteTarget (int target) noexcept
{
    return std::max (1, std::min (target, gloop::kNumTargets - 1));
}
} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

GloopEngine::GloopEngine()
    : lanes ((size_t) gloop::kNumLanes),
      slots ((size_t) kQueueSlots)
{
    // The lane buffers and the hand-off queue are large (a full four-track
    // take is ~48 KB per lane), so they live on the heap and are allocated here
    // on the message thread. prepare() and the audio thread only touch them.
}

void GloopEngine::prepare (double sr) noexcept
{
    // Nothing to allocate here: the queue and every gesture buffer were created
    // in the constructor. The sample rate is informational because every glide
    // and fade is driven by the dt the caller passes to processFrame().
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
        lane.centreNorm = 0.0f;
        lane.effectiveAmount = 0.0f;
        lane.rawTrack.fill (0.0f);
        lane.offsetTrack.fill (0.0f);
        lane.centreTrack.fill (0.0f);
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

bool GloopEngine::publish (int lane, const gloop::Gesture& gesture) noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return false;

    if (gesture.count < 0 || gesture.count > gloop::kMaxGestureSamples
        || gesture.numTracks < 0 || gesture.numTracks > gloop::kMaxTracksPerLane)
        return false;

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
    slot.gesture = gesture;

    if (! std::isfinite (slot.gesture.durationSeconds)
        || slot.gesture.durationSeconds < 0.0f)
        slot.gesture.durationSeconds = 0.0f;

    fifo.finishedWrite (1);
    return true;
}

bool GloopEngine::publish (int lane, const float* offsets, int count,
                           float durationSeconds) noexcept
{
    // Compatibility helper for QA harnesses: one offset track using the lane's
    // current target. The processor always uses the full-gesture overload.
    if (lane < 0 || lane >= gloop::kNumLanes)
        return false;

    if (count < 0 || count > gloop::kMaxGestureSamples)
        return false;

    if (count > 0 && offsets == nullptr)
        return false;

    gloop::Gesture gesture;
    gesture.count = count;
    gesture.durationSeconds = durationSeconds;
    gesture.numTracks = count >= 2 ? 1 : 0;

    if (count > 0)
    {
        int target = clampTarget (settings.gloopLanes[(size_t) lane].target);

        if (target <= (int) gloop::Target::Off || target >= gloop::kNumTargets)
            target = (int) gloop::Target::Focus;

        gesture.tracks[0].target = target;
        std::copy (offsets, offsets + count, gesture.tracks[0].offsets.begin());
    }

    return publish (lane, gesture);
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
    const auto& incoming = slot.gesture;

    if (incoming.count < kMinSamples || incoming.numTracks <= 0)
    {
        // Empty take: the lane stays valid but has nothing to play.
        lane.gesture.count = 0;
        lane.gesture.durationSeconds = 0.0f;
        lane.gesture.numTracks = 0;
        lane.gestureDuration = 0.0;
        lane.seamSeconds = 0.0;
        lane.loopPeriod = 0.0;
        lane.hasGesture = false;
        lane.phase = 0.0;
        lane.fadeGain = 0.0f;
        lane.rawTrack.fill (0.0f);
        lane.offsetTrack.fill (0.0f);
        lane.centreTrack.fill (0.0f);
        lane.rawOffset = lane.offset = lane.centreNorm = 0.0f;
        lane.effectiveAmount = 0.0f;
        refreshActualMode (lane);
        return;
    }

    const int count = std::min (incoming.count, gloop::kMaxGestureSamples);
    const int numTracks = std::min (incoming.numTracks, gloop::kMaxTracksPerLane);

    lane.gesture.count = count;
    lane.gesture.numTracks = numTracks;

    for (int t = 0; t < numTracks; ++t)
    {
        const auto& source = incoming.tracks[(size_t) t];
        auto& destination = lane.gesture.tracks[(size_t) t];

        // The processor should never send an invalid target, but a corrupt
        // state blob must not be able to write outside the target table.
        destination.target = clampConcreteTarget (source.target);

        for (int i = 0; i < count; ++i)
            destination.offsets[(size_t) i] = sanitiseOffset (source.offsets[(size_t) i]);
    }

    const double fallbackDuration = (double) count / (double) gloop::kGestureSampleRate;
    double duration = incoming.durationSeconds > 0.0f
                          ? (double) incoming.durationSeconds
                          : fallbackDuration;
    duration = clampSeconds (duration, 1.0e-3, 600.0);

    // Crossfade the last seam into the first seam: the loop period shrinks by the
    // seam length, which is exactly what keeps the wrap continuous (see readTrack).
    const double seam = std::min ((double) kSeamSeconds, duration * 0.25);

    lane.gesture.durationSeconds = (float) duration;
    lane.gestureDuration = duration;
    lane.seamSeconds = seam;
    lane.loopPeriod = duration - seam;

    lane.hasGesture = true;
    lane.phase = 0.0;          // a new take starts at its own beginning
    lane.fadeGain = 1.0f;      // and enters at full influence; no fade-in on REC
    lane.rawTrack.fill (0.0f);
    lane.offsetTrack.fill (0.0f);
    lane.centreTrack.fill (0.0f);
    lane.rawOffset = lane.offset = lane.centreNorm = 0.0f;

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

        // Each recorded track lives around its own knob's live centre; a lane
        // selected as Multi still has concrete per-track targets.
        for (int t = 0; t < gloop::kMaxTracksPerLane; ++t)
        {
            if (t >= lane.gesture.numTracks)
            {
                lane.centreTrack[(size_t) t] = 0.0f;
                continue;
            }

            const int target = lane.gesture.tracks[(size_t) t].target;
            lane.centreTrack[(size_t) t] =
                target > (int) gloop::Target::Off && target < gloop::kNumTargets
                    ? gloopTargetNorm (params, target)
                    : 0.0f;
        }

        lane.centreNorm = lane.centreTrack[0];
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
        // One period per lane, no shared clock: all tracks of a lane share this
        // playhead, while the lanes drift apart on purpose.
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

        // A concrete target gates the gesture's tracks: an Off lane is silent
        // even if it still holds a take, and a single-target lane cannot leak a
        // track aimed at another knob. Multi (kAutoTarget) plays all tracks.
        const bool targetAllowsTracks = lane.target > (int) gloop::Target::Off
                                        && (lane.target == gloop::kAutoTarget
                                            || lane.gesture.numTracks > 0);

        lane.effectiveAmount = (playing && targetAllowsTracks)
                                   ? dsp::clamp (lane.amountParam * lane.amountScale
                                                     * lane.fadeGain,
                                                 0.0f, 1.0f)
                                   : 0.0f;

        // --- per-track offsets ------------------------------------------------
        for (int t = 0; t < gloop::kMaxTracksPerLane; ++t)
        {
            const bool hasTrack = t < lane.gesture.numTracks;
            const auto& track = lane.gesture.tracks[(size_t) t];
            const bool targetMatches = lane.target == gloop::kAutoTarget
                                       || (track.target == lane.target);
            const bool contributing = playing && targetAllowsTracks && hasTrack
                                      && targetMatches
                                      && track.target > (int) gloop::Target::Off
                                      && track.target < gloop::kNumTargets;

            float raw = 0.0f;

            if (contributing)
                raw = dsp::clamp (readTrack (lane, track) * lane.effectiveAmount,
                                  -1.0f, 1.0f);

            float& offset = lane.offsetTrack[(size_t) t];
            offset += (raw - offset) * offsetCoeff;
            offset = dsp::flushDenormal (offset);
            offset = dsp::clamp (offset, -1.0f, 1.0f);

            // An idle/empty track withdraws through the smoother and then sits at
            // hard zero, so it can never leave residual modulation behind.
            if (raw == 0.0f && std::abs (offset) < 1.0e-6f)
                offset = 0.0f;

            lane.rawTrack[(size_t) t] = raw;

            if (contributing && offset != 0.0f)
                sums[(size_t) track.target] += offset;
        }

        // The legacy single-target/QA projection is the first track.
        lane.rawOffset = lane.rawTrack[0];
        lane.offset = lane.offsetTrack[0];
        lane.centreNorm = lane.centreTrack[0];
    }

    // Several lanes (and several tracks) may orbit the same knob. They sum, then
    // go through a limiter that is exactly linear below kOffsetLinear and
    // smoothly bounded above it: no last-writer-wins, no hard clipping.
    for (int t = 0; t < gloop::kNumTargets; ++t)
        offsetByTarget[(size_t) t] = softLimitOffset (sums[(size_t) t]);
}

void GloopEngine::clearLaneForRecording (Lane& lane) noexcept
{
    lane.gesture.count = 0;
    lane.gesture.durationSeconds = 0.0f;
    lane.gesture.numTracks = 0;
    lane.gestureDuration = 0.0;
    lane.seamSeconds = 0.0;
    lane.loopPeriod = 0.0;
    lane.hasGesture = false;
    lane.phase = 0.0;
    lane.fadeGain = 0.0f;
    lane.effectiveAmount = 0.0f;
    lane.rawTrack.fill (0.0f);
    lane.offsetTrack.fill (0.0f);
    lane.centreTrack.fill (0.0f);
    lane.rawOffset = lane.offset = lane.centreNorm = 0.0f;

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

        // Amount and speed only: the recorded tracks and target ids stay, phase
        // keeps running, and the new values are reached by a slow glide.
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

float GloopEngine::readTrack (const Lane& lane, const gloop::Track& track) const noexcept
{
    const auto& gesture = lane.gesture;

    if (gesture.count < kMinSamples || ! (lane.loopPeriod > 0.0)
        || ! (lane.seamSeconds > 0.0))
        return 0.0f;

    if (track.target <= (int) gloop::Target::Off || track.target >= gloop::kNumTargets)
        return 0.0f;

    const auto readAt = [&track, &gesture] (double seconds) noexcept
    {
        seconds = clampSeconds (seconds, 0.0, (double) gesture.durationSeconds);
        const double x = seconds * (double) gesture.count
                         / (double) gesture.durationSeconds;

        int i0 = (int) x;
        i0 = std::max (0, std::min (i0, gesture.count - 1));
        const int i1 = std::min (i0 + 1, gesture.count - 1);
        const float frac = (float) (x - (double) i0);

        const float a = track.offsets[(size_t) i0];
        const float b = track.offsets[(size_t) i1];
        return a + (b - a) * frac;
    };

    const double u = lane.phase * lane.loopPeriod;   // seconds into the loop period
    const double bodyPosition = u + lane.seamSeconds;

    const float body = readAt (bodyPosition);

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
        const float head = readAt (bodyPosition - lane.loopPeriod);
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
        v.amount = lane.effectiveAmount;
        v.durationSeconds = lane.gesture.durationSeconds;

        // Show only tracks that the selected target allows. A concrete lane
        // exposes its matching track (normally the only one); Multi exposes all;
        // Off exposes none.
        int visibleTracks = 0;

        for (int t = 0; t < gloop::kMaxTracksPerLane; ++t)
        {
            auto& outTrack = v.tracks[(size_t) t];
            outTrack.target = (int) gloop::Target::Off;
            outTrack.offset = 0.0f;
            outTrack.centreNorm = 0.0f;

            if (t >= lane.gesture.numTracks || lane.target <= (int) gloop::Target::Off)
                continue;

            const auto& track = lane.gesture.tracks[(size_t) t];

            if (lane.target != gloop::kAutoTarget && track.target != lane.target)
                continue;

            outTrack.target = track.target;
            outTrack.offset = lane.offsetTrack[(size_t) t];
            outTrack.centreNorm = lane.centreTrack[(size_t) t];
            ++visibleTracks;
        }

        v.numTracks = visibleTracks;
        v.offset = visibleTracks > 0 ? v.tracks[0].offset : 0.0f;
        v.centreNorm = visibleTracks > 0 ? v.tracks[0].centreNorm : 0.0f;
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
    return (lane >= 0 && lane < gloop::kNumLanes)
               ? lanes[(size_t) lane].offsetTrack[0]
               : 0.0f;
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

int GloopEngine::laneTrackCount (int lane) const noexcept
{
    return (lane >= 0 && lane < gloop::kNumLanes)
               ? lanes[(size_t) lane].gesture.numTracks
               : 0;
}

int GloopEngine::laneTrackTarget (int lane, int track) const noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return (int) gloop::Target::Off;

    const auto& l = lanes[(size_t) lane];

    return track >= 0 && track < l.gesture.numTracks
               ? l.gesture.tracks[(size_t) track].target
               : (int) gloop::Target::Off;
}

float GloopEngine::laneTrackOffset (int lane, int track) const noexcept
{
    if (lane < 0 || lane >= gloop::kNumLanes
        || track < 0 || track >= gloop::kMaxTracksPerLane)
        return 0.0f;

    return lanes[(size_t) lane].offsetTrack[(size_t) track];
}

} // namespace nagloed::dsp
