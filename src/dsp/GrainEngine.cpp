#include "GrainEngine.h"

namespace nagloed::dsp
{

namespace
{
// Capture window: the brief asks for 12 seconds of stereo history. The actual
// ring buffer is rounded up to a power of two so that wrapping is a mask (fast
// and branch-free) — the usable window stays exactly 12 seconds.
constexpr double kCaptureSeconds = 12.0;
constexpr int    kMinRingLength  = 1 << 12;   // 4096 samples minimum
constexpr int    kMaxRingLength  = 1 << 22;   // ~4.2 M samples (87 s @48k)
constexpr double kMinFillToSpawn = 256.0;     // wait for ~5 ms of material

// Per-grain pitch is clamped to +/-4 octaves; the parameter range is smaller,
// but spread + shimmer can stack, and a runaway rate would only waste CPU.
constexpr float kMinGrainSemis = -48.0f;
constexpr float kMaxGrainSemis =  48.0f;

// Feedback damping: a one-pole lowpass at roughly 5 kHz keeps the regenerating
// cloud warm instead of turning it into a high-frequency howl.
constexpr float kFeedbackDampHz = 5000.0f;

// The drone is a small analog-style cluster: five slightly detuned saws and a
// sub triangle. Cent offsets are deliberately "wrong" (9/5 instead of 7/12) so
// the beating is slow and organic rather than chorus-like.
constexpr float kDroneDetuneCents[5] = { -9.0f, -5.0f, 0.0f, 5.0f, 9.0f };
constexpr float kDronePanL[5] = { 0.85f, 0.54f, 1.00f, 0.65f, 0.33f };
constexpr float kDronePanR[5] = { 0.54f, 0.85f, 1.00f, 0.33f, 0.65f };

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

} // namespace

GrainEngine::GrainEngine() = default;
GrainEngine::~GrainEngine() = default;

int GrainEngine::nextPowerOfTwo (int v) noexcept
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

void GrainEngine::prepare (double sr)
{
    sampleRate = (sr > 0.0 ? sr : 48000.0);

    const int wanted = (int) std::lround (kCaptureSeconds * sampleRate);
    nominalLen = wanted < kMinRingLength ? kMinRingLength
                                         : (wanted > kMaxRingLength ? kMaxRingLength : wanted);
    ringLen  = nextPowerOfTwo (nominalLen);
    ringMask = (std::uint32_t) (ringLen - 1);

    captureL.assign ((size_t) ringLen, 0.0f);
    captureR.assign ((size_t) ringLen, 0.0f);

    windows.prepare();

    noiseLowL.setCutoff (900.0f, sampleRate);
    noiseLowR.setCutoff (900.0f, sampleRate);
    noiseMidL.setCutoff (4500.0f, sampleRate);
    noiseMidR.setCutoff (4500.0f, sampleRate);
    droneToneL.setCutoff (6000.0f, sampleRate);
    droneToneR.setCutoff (6000.0f, sampleRate);

    noiseBreathInc = (float) (0.07 / sampleRate);            // ~0.07 Hz breathing
    feedbackDampCoeff = clamp (1.0f - std::exp (-kTwoPi * kFeedbackDampHz / (float) sampleRate),
                               0.0f, 0.99f);

    reset();
}

void GrainEngine::reset() noexcept
{
    rng.seed (0x6e61676cu);   // "nagl"

    for (size_t i = 0; i < captureL.size(); ++i)
    {
        captureL[i] = 0.0f;
        captureR[i] = 0.0f;
    }

    for (auto& g : grains)
        g = Grain {};

    writePos        = 0.0;
    filled          = 0.0;
    readHead        = 0.0;
    feedbackLpL     = 0.0f;
    feedbackLpR     = 0.0f;
    activeCount     = 0;
    spawnCountdown  = 0.0f;
    rateCounter     = 0;
    rateSpawnCount  = 0;
    grainRateEstimate = 0.0f;

    noiseWhiteL = noiseWhiteR = 0.0f;
    noiseLowL.reset();
    noiseLowR.reset();
    noiseMidL.reset();
    noiseMidR.reset();
    noiseBreathPhase = 0.0f;

    dronePhase.fill (0.0f);
    droneInc.fill (0.0f);
    dronePitch = -999.0f;
    // Stagger the drone phases so it does not start with a click-like in-phase
    // transient when the buffer is first recorded.
    for (size_t i = 0; i < dronePhase.size(); ++i)
        dronePhase[i] = 0.13f * (float) i;

    droneToneL.reset();
    droneToneR.reset();
}

GrainEngine::Stereo GrainEngine::process (float inL, float inR,
                                          const GrainControls& controls) noexcept
{
    // ---------------------------------------------------------------------
    // 1. Material for the capture buffer: the dry input, or a self-generated
    //    texture. The generators keep running even while frozen so that the
    //    drone/noise phase is continuous when the user unfreezes.
    // ---------------------------------------------------------------------
    float sourceL = inL;
    float sourceR = inR;

    if (controls.sourceMode == 1)
        generateNoise (sourceL, sourceR);
    else if (controls.sourceMode == 2)
        generateDrone (sourceL, sourceR, controls);

    // ---------------------------------------------------------------------
    // 2. Render the active grain voices.
    // ---------------------------------------------------------------------
    float sumL = 0.0f;
    float sumR = 0.0f;

    for (int i = 0; i < kMaxGrains; ++i)
    {
        Grain& g = grains[(size_t) i];
        if (! g.active)
            continue;

        const float phase = g.age / g.length;
        if (phase >= 1.0f)
        {
            g.active = false;
            --activeCount;
            continue;
        }

        const float window = windowValue (phase, g.shape);
        const double readPos = g.sourcePos + (double) g.age * g.rate;

        const float sampleL = readInterpolated (captureL.data(), readPos);
        const float sampleR = readInterpolated (captureR.data(), readPos);
        const float amp     = window * g.gain;

        sumL += sampleL * amp * g.panL;
        sumR += sampleR * amp * g.panR;

        g.env = window;
        g.age += 1.0f;
    }

    // The summed bus is softly bounded before it reaches Ruimte or the feedback
    // tap. With up to 256 partially correlated grains this is what keeps the
    // engine musical instead of explosive.
    sumL = softClipGrain (sumL);
    sumR = softClipGrain (sumR);

    // ---------------------------------------------------------------------
    // 3. Capture write (input + damped grain feedback) unless frozen.
    // ---------------------------------------------------------------------
    if (! controls.freeze)
    {
        // Damped feedback tap: the lowpass tames the high end of the regenerating
        // cloud, and feedbackScale (computed from the pitch ratio in EngineDSP)
        // makes sure upward pitch shifts cannot create a runaway loop.
        feedbackLpL = flushDenormal (feedbackLpL + feedbackDampCoeff * (sumL - feedbackLpL));
        feedbackLpR = flushDenormal (feedbackLpR + feedbackDampCoeff * (sumR - feedbackLpR));

        const float feedback = clamp (controls.feedback, 0.0f, 0.95f)
                             * clamp (controls.feedbackScale, 0.0f, 1.0f);
        const float fbMono = 0.5f * (feedbackLpL + feedbackLpR) * feedback;

        sourceL += fbMono;
        sourceR += fbMono;

        writeCapture (sourceL, sourceR, controls);
    }

    // ---------------------------------------------------------------------
    // 4. Advance the read head and schedule the next grain.
    // ---------------------------------------------------------------------
    advanceReadHead (controls);

    spawnCountdown -= 1.0f;
    if (spawnCountdown <= 0.0f)
    {
        if (filled >= kMinFillToSpawn)
            spawnGrain (controls);

        spawnCountdown = nextSpawnInterval (controls);
    }

    // ---------------------------------------------------------------------
    // 5. Grain-rate metering (for the UI): average spawns over ~100 ms.
    // ---------------------------------------------------------------------
    ++rateCounter;
    if (rateCounter >= (int) (sampleRate * 0.1))
    {
        const float instant = (float) rateSpawnCount * (float) sampleRate / (float) rateCounter;
        grainRateEstimate += (instant - grainRateEstimate) * 0.5f;
        rateCounter    = 0;
        rateSpawnCount = 0;
    }

    return { sumL, sumR };
}

// =============================================================================
// Capture
// =============================================================================

void GrainEngine::writeCapture (float srcL, float srcR,
                                const GrainControls&) noexcept
{
    const std::uint32_t index = (std::uint32_t) ((std::uint64_t) writePos & (std::uint64_t) ringMask);

    captureL[(size_t) index] = softBoundCapture (srcL);
    captureR[(size_t) index] = softBoundCapture (srcR);

    writePos += 1.0;
    filled = std::min (writePos, (double) nominalLen);
}

void GrainEngine::advanceReadHead (const GrainControls& controls) noexcept
{
    // readHead is a *relative* position inside the filled capture window. While
    // recording, the window itself follows the write head, so scan = 0 means
    // "stay at the same distance behind the input" (live granular). While
    // frozen, the window is stationary and scan scrubs through the captured
    // material — negative scan moves backwards, exactly as the demo score
    // expects.
    readHead += (double) clamp (controls.scan, -1.0f, 1.0f);

    if (filled > 1.0)
    {
        if (readHead >= filled)
            readHead -= filled;
        else if (readHead < 0.0)
            readHead += filled;
    }
    else
    {
        readHead = 0.0;
    }
}

// =============================================================================
// Grain spawning
// =============================================================================

float GrainEngine::nextSpawnInterval (const GrainControls& controls) noexcept
{
    const float density  = clamp (controls.densityHz, 0.1f, 400.0f);
    const float base     = (float) sampleRate / density;
    const float jitter   = clamp (controls.jitter, 0.0f, 1.0f);

    // Symmetric timing scatter: the mean interval stays at 1/density, so the
    // measured grain rate still matches the density knob (important both for
    // the UI readout and for deterministic tests).
    float interval = base * (1.0f + jitter * 0.9f * rng.bipolar());
    return std::max (interval, 1.0f);
}

int GrainEngine::findVoiceToUse() noexcept
{
    for (int i = 0; i < kMaxGrains; ++i)
        if (! grains[(size_t) i].active)
            return i;

    // Pool exhausted: steal the quietest grain. This only happens at extreme
    // density/length combinations and is far preferable to dropping notes.
    int   best     = 0;
    float bestEnv  = grains[0].env;
    for (int i = 1; i < kMaxGrains; ++i)
    {
        if (grains[(size_t) i].env < bestEnv)
        {
            bestEnv = grains[(size_t) i].env;
            best    = i;
        }
    }
    return best;
}

void GrainEngine::spawnGrain (const GrainControls& controls) noexcept
{
    const double windowLen = filled;
    if (windowLen < 1.0)
        return;

    const int voice = findVoiceToUse();
    Grain& g = grains[(size_t) voice];

    const bool wasActive = g.active;

    // --- pitch ---------------------------------------------------------------
    // Per-grain scatter always applies. In Drone mode the cluster itself is
    // already tuned to the grain pitch (see generateDrone), so the grains only
    // contribute their spread/shimmer instead of transposing a second time.
    float pitch = controls.pitchSpread * rng.bipolar();

    // Shimmer: a subset of grains jump an octave up, which turns a smooth cloud
    // into the glinting, harp-like texture the macro is named after.
    if (controls.shimmer > 0.0f && rng.chance (clamp (controls.shimmer, 0.0f, 1.0f) * 0.55f))
        pitch += 12.0f;

    if (controls.sourceMode != 2)
    {
        pitch += controls.pitchSemis;

        if (controls.tuneMode > tuneFree && controls.tuneMode < kNumTuneModes)
            pitch = quantisePitch (controls.tuneMode, pitch);
    }

    pitch = clamp (pitch, kMinGrainSemis, kMaxGrainSemis);

    const float ratio = std::pow (2.0f, pitch * (1.0f / 12.0f));
    const bool  reverse = rng.chance (clamp (controls.reverse, 0.0f, 1.0f));

    // --- position ------------------------------------------------------------
    // Focus places the birth point inside the captured window (0 = oldest,
    // 1 = newest), spray scatters it, and the read head adds the scan drift.
    //
    // The position is kept at least one grain-length (in source samples) away
    // from both ends of the captured material, so a grain never reads into the
    // not-yet-written part of the ring (which would be silence) no matter how
    // far Focus or Spray are pushed or how fast the grain reads.
    const float grainLengthSamples = std::max (16.0f, controls.grainSeconds * (float) sampleRate);
    const double readMargin = (double) grainLengthSamples * (double) ratio + 32.0;

    double relative = (double) clamp (controls.focus, 0.0f, 1.0f) * windowLen
                    + readHead
                    + (double) (rng.bipolar() * clamp (controls.spray, 0.0f, 1.0f) * 0.5f * (float) windowLen);

    relative = std::fmod (relative, windowLen);
    if (relative < 0.0)
        relative += windowLen;

    if (windowLen > 2.0 * readMargin)
        relative = std::max (readMargin, std::min (relative, windowLen - readMargin));
    else
        relative = windowLen * 0.5;   // capture still filling: stay in the middle

    g.sourcePos = writePos - windowLen + relative;

    g.rate   = reverse ? -(double) ratio : (double) ratio;
    g.pitch  = pitch;
    g.age    = 0.0f;
    g.length = grainLengthSamples;
    g.gain   = std::max (0.0f, controls.gain);
    g.shape  = clamp (controls.shape, 0.0f, 1.0f);
    g.env    = 0.0f;
    g.brightness = clamp (controls.brightness, 0.0f, 1.0f);

    // --- stereo --------------------------------------------------------------
    // Width scatters grain pan; panOffset (LFO/auto) nudges the whole cloud.
    const float pan = clamp (rng.bipolar() * clamp (controls.width, 0.0f, 1.0f)
                                 + controls.panOffset, -1.0f, 1.0f);
    g.pan = pan;
    panGains (pan, g.panL, g.panR);

    g.active = true;

    if (! wasActive)
        ++activeCount;

    ++rateSpawnCount;
}

// =============================================================================
// Source synthesis (self-generating material)
// =============================================================================

void GrainEngine::generateNoise (float& outL, float& outR) noexcept
{
    const float whiteL = rng.bipolar();
    const float whiteR = rng.bipolar();

    const float lowL = noiseLowL.process (whiteL);
    const float lowR = noiseLowR.process (whiteR);
    const float midL = noiseMidL.process (whiteL);
    const float midR = noiseMidR.process (whiteR);

    // Slow amplitude breathing so the noise bed never sounds like a static
    // hiss generator: it swells and fades like wind through a room.
    const float breath = 0.72f + 0.28f * sine (noiseBreathPhase);
    noiseBreathPhase += noiseBreathInc;
    if (noiseBreathPhase >= 1.0f)
        noiseBreathPhase -= 1.0f;

    const float gain = 0.80f * breath;

    outL = (0.75f * lowL + 0.32f * midL + 0.10f * whiteL) * gain;
    outR = (0.75f * lowR + 0.32f * midR + 0.10f * whiteR) * gain;
}

void GrainEngine::generateDrone (float& outL, float& outR, const GrainControls& controls) noexcept
{
    // The cluster is tuned to the requested (and, if enabled, scale-quantised)
    // grain pitch. The granular layer then only adds spread/shimmer on top, so
    // Pitch plays the drone like an instrument instead of transposing it twice.
    const float tunedPitch = (controls.tuneMode > tuneFree && controls.tuneMode < kNumTuneModes)
                                 ? quantisePitch (controls.tuneMode, controls.pitchSemis)
                                 : controls.pitchSemis;

    // Rebuild oscillator increments only when the requested pitch has moved
    // meaningfully: avoids a pow() per sample while still tracking automation.
    if (std::abs (tunedPitch - dronePitch) > 0.01f || dronePitch < -900.0f)
    {
        dronePitch = tunedPitch;
        const float base = 110.0f * std::pow (2.0f, clamp (tunedPitch, -48.0f, 48.0f) / 12.0f);

        for (int i = 0; i < 5; ++i)
        {
            const float freq = base * std::pow (2.0f, kDroneDetuneCents[i] / 1200.0f);
            droneInc[(size_t) i] = clamp (freq / (float) sampleRate, 0.0f, 0.45f);
        }

        // Sub triangle one octave below the cluster.
        droneInc[5] = clamp (base * 0.5f / (float) sampleRate, 0.0f, 0.45f);
    }

    float left  = 0.0f;
    float right = 0.0f;

    // Five polyBLEP saws: band-limited enough that the drone stays clean even
    // when the grains transpose it up an octave.
    for (int i = 0; i < 5; ++i)
    {
        float& phase = dronePhase[(size_t) i];
        const float dt = droneInc[(size_t) i];

        float saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);

        left  += saw * kDronePanL[i];
        right += saw * kDronePanR[i];

        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    // Sub triangle (cheap, phase-distorted sine).
    float& subPhase = dronePhase[5];
    const float sub = 1.0f - 4.0f * std::abs (subPhase - 0.5f);
    subPhase += droneInc[5];
    if (subPhase >= 1.0f)
        subPhase -= 1.0f;

    const float mono = 0.16f;
    left  = left * mono + sub * 0.12f;
    right = right * mono + sub * 0.12f;

    // Gentle 6 kHz lowpass gives the cluster its tape-ish, analog softness.
    outL = droneToneL.process (left);
    outR = droneToneR.process (right);
}

// =============================================================================
// Interpolation
// =============================================================================

float GrainEngine::readInterpolated (const float* buffer, double position) const noexcept
{
    // Catmull-Rom (4-point cubic). Positions are absolute sample indices in the
    // ring; the power-of-two mask makes the wrap-around seamless, which is what
    // keeps grains that read across the buffer boundary click-free.
    const std::int64_t base = (std::int64_t) std::floor (position);
    const float frac = (float) (position - (double) base);

    const std::uint32_t i0 = (std::uint32_t) (base - 1) & ringMask;
    const std::uint32_t i1 = (std::uint32_t) (base)     & ringMask;
    const std::uint32_t i2 = (std::uint32_t) (base + 1) & ringMask;
    const std::uint32_t i3 = (std::uint32_t) (base + 2) & ringMask;

    const float p0 = buffer[i0];
    const float p1 = buffer[i1];
    const float p2 = buffer[i2];
    const float p3 = buffer[i3];

    const float c0 = p1;
    const float c1 = 0.5f * (p2 - p0);
    const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    const float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// =============================================================================
// Visualisation
// =============================================================================

float GrainEngine::windowValue (float phase, float shape) const noexcept
{
    return windows.value (phase, shape);
}

int GrainEngine::copyGrainViews (GrainView* destination, int maxViews) const noexcept
{
    if (destination == nullptr || maxViews <= 0 || filled < 1.0)
        return 0;

    const double windowLen = filled;
    const double windowStart = writePos - windowLen;

    int count = 0;
    for (int i = 0; i < kMaxGrains && count < maxViews; ++i)
    {
        const Grain& g = grains[(size_t) i];
        if (! g.active)
            continue;

        GrainView& view = destination[count++];

        const double current = g.sourcePos + (double) g.age * g.rate;
        double relative = current - windowStart;
        relative = std::fmod (relative, windowLen);
        if (relative < 0.0)
            relative += windowLen;

        view.pos        = (float) (relative / windowLen);
        view.progress   = clamp (g.age / g.length, 0.0f, 1.0f);
        view.pan        = g.pan;
        view.pitch      = g.pitch;
        view.env        = clamp (g.env, 0.0f, 1.0f);
        view.brightness = g.brightness;
    }

    return count;
}

float GrainEngine::bufferFill() const noexcept
{
    if (nominalLen <= 0)
        return 0.0f;

    return clamp ((float) (filled / (double) nominalLen), 0.0f, 1.0f);
}

} // namespace nagloed::dsp
