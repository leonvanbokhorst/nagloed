#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// -----------------------------------------------------------------------------
// Shared, audio-thread-safe building blocks for the NAGLOED engine.
//
// Everything in this header is allocation-free, lock-free and deterministic.
// It deliberately avoids JUCE so the grain/saturation maths stays readable and
// testable on its own.
//
// House rules:
//   * no allocation, locks, exceptions or logging anywhere below;
//   * every state update flushes denormals and clamps to a sane range so that
//     no parameter combination can push the engine into NaN/Inf territory;
//   * where a cheap approximation is used for tone (tanh, window shapes) the
//     comment explains the musical intent, not just the maths.
// -----------------------------------------------------------------------------
namespace nagloed::dsp
{

inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

// =============================================================================
// Basic maths helpers
// =============================================================================

inline float clamp (float v, float lo, float hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float gainFromDb (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

// Normalised log position of a frequency inside [lo, hi] (0..1), used for the
// visual "brightness"/"cutoff" readouts. Returns 0 for degenerate input.
inline float logNorm (float value, float lo, float hi) noexcept
{
    const float v  = std::max (value, 1.0e-3f);
    const float lo2 = std::max (lo, 1.0e-3f);
    const float hi2 = std::max (hi, lo2 * 1.0001f);
    return clamp (std::log (v / lo2) / std::log (hi2 / lo2), 0.0f, 1.0f);
}

// Bounded, CPU-friendly tanh approximation (Padé 3/2). It is *deliberately* a
// little softer than the true tanh: the tiny extra curvature is part of the
// analog character of the EAR chain and of the feedback safety net. Input is
// clamped so the result is always inside [-1, 1].
inline float fastTanh (float x) noexcept
{
    x = clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Soft ceiling for the granular capture buffer. Transparent up to +/-1 and
// asymptotically bounded to +/-~1.9, which is what guarantees that the grain
// feedback loop can never run away no matter which parameters the user dials.
inline float softBoundCapture (float x) noexcept
{
    if (x > 1.0f)
        return 1.0f + 0.9f * fastTanh (x - 1.0f);
    if (x < -1.0f)
        return -1.0f - 0.9f * fastTanh (-x - 1.0f);
    return x;
}

// Soft ceiling for the summed grain bus before it reaches Ruimte and the grain
// feedback tap. Unity up to +/-2.5, saturating towards +/-4.5, so a cloud of
// hundreds of (partly correlated) grains stays loud but never explodes.
inline float softClipGrain (float x) noexcept
{
    if (x > 2.5f)
        return 2.5f + 2.0f * fastTanh ((x - 2.5f) * 0.5f);
    if (x < -2.5f)
        return -2.5f - 2.0f * fastTanh ((-x - 2.5f) * 0.5f);
    return x;
}

// Final output safety stage: gentle analog-ish saturation plus a hard ceiling
// at +/-2 (+6 dBFS). At normal levels it behaves like a wire; when pushed the
// curve rounds off, which is far more forgiving than a hard digital clip.
inline float softLimitOutput (float x) noexcept
{
    // 0.65 scales the curve so normal operating levels remain almost linear.
    const float y = fastTanh (x * 0.65f) * (1.0f / 0.65f);
    return clamp (y, -2.0f, 2.0f);
}

// Clears sub-audible (denormal) values on state variables. FTZ is active on
// most hosts, but tests render the engine standalone so we defend in depth.
inline float flushDenormal (float x) noexcept
{
    return std::abs (x) < 1.0e-20f ? 0.0f : x;
}

// Rejects NaN/Inf coming from the host (some hosts hand you garbage after a
// transport glitch) and clamps absurdly hot input before it reaches the capture
// buffer.
inline float sanitiseInput (float x) noexcept
{
    return std::abs (x) < 1.0e6f ? x : 0.0f;
}

// =============================================================================
// Deterministic RNG
//
// xorshift32: fast, allocation-free and good enough for scatter/pan/jitter.
// Each engine section owns its own instance so the sections stay decoupled.
// =============================================================================
class Rng
{
public:
    void seed (std::uint32_t s) noexcept { state = s == 0u ? 0x9e3779b9u : s; }

    inline std::uint32_t nextUInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform in [0, 1).
    inline float uniform() noexcept
    {
        return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    // Uniform in [-1, 1).
    inline float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

    inline float range (float lo, float hi) noexcept { return lo + uniform() * (hi - lo); }

    // True with probability p.
    inline bool chance (float p) noexcept { return uniform() < p; }

private:
    std::uint32_t state = 0x9e3779b9u;
};

// =============================================================================
// Parameter smoothing
//
// One-pole smoother advanced at control rate. Every automatable parameter goes
// through one of these, which is what keeps knob moves and host automation
// completely zipper-free.
// =============================================================================
struct Smoothed
{
    float current = 0.0f;
    float target  = 0.0f;
    float coeff   = 0.0f;

    // tauSeconds is the 63% settling time; intervalSamples is how often tick()
    // is called (the engine's control rate).
    void prepare (double sr, float tauSeconds, int intervalSamples) noexcept
    {
        const float samples = (float) std::max (intervalSamples, 1);
        const float tau     = std::max (tauSeconds, 1.0e-4f) * (float) std::max (sr, 1.0);
        coeff = clamp (1.0f - std::exp (-samples / tau), 0.0f, 1.0f);
    }

    inline void setTarget (float t) noexcept { target = t; }
    inline void tick() noexcept { current += (target - current) * coeff; }
    inline void snap (float v) noexcept { current = target = v; }

    inline float value() const noexcept { return current; }
};

// =============================================================================
// Filters used throughout the character/space sections
// =============================================================================

struct OnePole
{
    float state = 0.0f;
    float coeff = 0.0f;

    void setCoeff (float c) noexcept { coeff = clamp (c, 0.0f, 1.0f); }

    void setCutoff (float hz, double sr) noexcept
    {
        const float nyquist = (float) sr * 0.5f;
        const float f       = clamp (hz, 0.5f, nyquist * 0.49f);
        coeff = clamp (1.0f - std::exp (-kTwoPi * f / (float) sr), 0.0f, 1.0f);
    }

    inline float process (float x) noexcept
    {
        state = flushDenormal (state + coeff * (x - state));
        return state;
    }

    void reset (float v = 0.0f) noexcept { state = flushDenormal (v); }
};

struct DcBlocker
{
    float x1 = 0.0f;
    float y1 = 0.0f;
    float r  = 0.9995f;

    void setCutoff (float hz, double sr) noexcept
    {
        // One-pole highpass (r close to 1 => very low corner).
        r = clamp (1.0f - kTwoPi * hz / (float) sr, 0.0f, 0.99995f);
    }

    inline float process (float x) noexcept
    {
        const float y = flushDenormal (x - x1 + r * y1);
        x1 = x;
        y1 = y;
        return y;
    }

    void reset() noexcept { x1 = y1 = 0.0f; }
};

// =============================================================================
// Sine lookup
//
// A single shared 2048-point table is enough for LFOs and delay-line
// modulation: interpolation keeps it smooth and it is much cheaper than a
// libm call per voice. The table is built in EngineDSP::prepare() (message
// thread) and only read afterwards.
// =============================================================================
inline constexpr int kSineTableSize = 2048;

inline std::array<float, kSineTableSize + 1>& sineTable() noexcept
{
    static std::array<float, kSineTableSize + 1> table = []()
    {
        std::array<float, kSineTableSize + 1> t {};
        for (int i = 0; i <= kSineTableSize; ++i)
            t[(size_t) i] = std::sin (kTwoPi * (float) i / (float) kSineTableSize);
        return t;
    }();

    return table;
}

// Call once from prepare() so the table is never built on the audio thread.
inline void prepareSharedTables() noexcept
{
    (void) sineTable();
}

// sin(2*pi*phase) for any real phase (in cycles).
inline float sine (float phase) noexcept
{
    phase -= std::floor (phase);
    const float index = phase * (float) kSineTableSize;
    const int   i0    = (int) index;
    const float frac  = index - (float) i0;
    const auto& t     = sineTable();
    const float a     = t[(size_t) i0];
    return a + (t[(size_t) i0 + 1] - a) * frac;
}

// =============================================================================
// Grain windows
//
// Three envelope families, morphed continuously by grain.shape:
//   shape 0.00 .. 0.50 : Hann  -> Tukey(50%)   (soft, classic granular)
//   shape 0.50 .. 1.00 : Tukey -> expodec      (percussive, plucky)
//
// The expodec curve (fast smooth attack, exponential decay) is the "Sintel"
// fingerprint: grains become small sparks rather than smooth pads. All three
// windows are exactly zero at both ends, which is what makes grain boundaries
// click-free even when the source is discontinuous.
// =============================================================================
class WindowMorph
{
public:
    static constexpr int kTableSize = 2048;

    // Message thread only (prepare()).
    void prepare() noexcept
    {
        for (int i = 0; i <= kTableSize; ++i)
        {
            const float p = (float) i / (float) kTableSize;

            tables_[0][(size_t) i] = hann (p);
            tables_[1][(size_t) i] = tukey (p, 0.5f);
            tables_[2][(size_t) i] = expodec (p);
        }

        normalise (tables_[1]);
        normalise (tables_[2]);
    }

    // phase and shape are both 0..1.
    inline float value (float phase, float shape) const noexcept
    {
        phase = clamp (phase, 0.0f, 1.0f);

        const float index = phase * (float) kTableSize;
        int         i0    = (int) index;
        if (i0 >= kTableSize)
            i0 = kTableSize - 1;
        const float frac = index - (float) i0;

        // Shape 0..1 maps onto the two-segment table morph 0..2.
        const float pos     = clamp (shape, 0.0f, 1.0f) * 2.0f;
        const int   tableA  = pos >= 2.0f ? 1 : (int) pos;
        const int   tableB  = tableA + 1;
        const float morph   = pos - (float) tableA;

        const float a0 = tables_[(size_t) tableA][(size_t) i0];
        const float a1 = tables_[(size_t) tableA][(size_t) i0 + 1];
        const float b0 = tables_[(size_t) tableB][(size_t) i0];
        const float b1 = tables_[(size_t) tableB][(size_t) i0 + 1];

        const float wa = a0 + (a1 - a0) * frac;
        const float wb = b0 + (b1 - b0) * frac;
        return wa + (wb - wa) * morph;
    }

private:
    static float hann (float p) noexcept
    {
        return 0.5f - 0.5f * std::cos (kTwoPi * p);
    }

    // Cosine-tapered rectangular window with a flat middle (alpha = taper span).
    static float tukey (float p, float alpha) noexcept
    {
        if (p <= 0.0f || p >= 1.0f)
            return 0.0f;

        const float edge = alpha * 0.5f;
        if (p < edge)
            return 0.5f - 0.5f * std::cos (kPi * p / edge);
        if (p > 1.0f - edge)
            return 0.5f - 0.5f * std::cos (kPi * (1.0f - p) / edge);
        return 1.0f;
    }

    // "Exponential decay" grain: immediate but smooth attack, exponential
    // decay, forced back to zero at the tail. Sounds like a struck spark.
    static float expodec (float p) noexcept
    {
        if (p <= 0.0f || p >= 1.0f)
            return 0.0f;

        const float attack = 1.0f - std::exp (-14.0f * p);
        const float decay  = std::exp (-3.0f * p);
        const float tail   = 1.0f - p;
        return attack * decay * tail;
    }

    static void normalise (std::array<float, kTableSize + 1>& t) noexcept
    {
        float peak = 0.0f;
        for (float v : t)
            peak = std::max (peak, std::abs (v));

        if (peak > 1.0e-6f)
            for (float& v : t)
                v /= peak;

        t[0] = 0.0f;
        t[(size_t) kTableSize] = 0.0f;
    }

    std::array<std::array<float, kTableSize + 1>, 3> tables_ {};
};

// =============================================================================
// Tuning
//
// grain.tune quantises the *summed* grain pitch (manual pitch + per-grain
// spread + shimmer/drift) onto a scale. The engine remains a microtonal
// instrument in "Free" mode; the other modes let the cloud lock to a key.
// Scale degrees are relative to the engine root (C = 0 semitones).
// =============================================================================
enum TuneMode
{
    tuneFree = 0,
    tuneChromatic,
    tuneMinor,
    tuneMajor,
    tuneDorian,
    tunePhrygian,
    tunePentatonic,
    tuneWholeTone,
    kNumTuneModes
};

inline int scaleDegrees (int mode, const int*& degrees) noexcept
{
    // Dorian: minor with a raised 6th. Phrygian: flat 2nd. Pentatonic: the
    // five-note scale that never sounds wrong on a granular cloud. Whole-tone:
    // maximum ambiguity, maximum shimmer.
    static constexpr int chromatic[]  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr int minor[]      { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int major[]      { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int dorian[]     { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr int phrygian[]   { 0, 1, 3, 5, 7, 8, 10 };
    static constexpr int pentatonic[] { 0, 2, 4, 7, 9 };
    static constexpr int wholeTone[]  { 0, 2, 4, 6, 8, 10 };

    switch (mode)
    {
        case tuneMinor:      degrees = minor;      return 7;
        case tuneMajor:      degrees = major;      return 7;
        case tuneDorian:     degrees = dorian;     return 7;
        case tunePhrygian:   degrees = phrygian;   return 7;
        case tunePentatonic: degrees = pentatonic; return 5;
        case tuneWholeTone:  degrees = wholeTone;  return 6;
        default:             degrees = chromatic;  return 12;
    }
}

// The grain engine clamps its summed pitch to these bounds. The quantiser uses
// the same limits so scale folding cannot produce an octave error at the edge.
inline constexpr float kMinGrainSemitones = -48.0f;
inline constexpr float kMaxGrainSemitones =  48.0f;

// Nearest pitch in the selected scale. The requested pitch is clamped to the
// grain range first, then the whole +/-4 octave span is searched so that the
// boundaries (-48 / +48 st) quantise correctly instead of folding into the
// wrong octave.
inline float quantisePitch (int mode, float semitones) noexcept
{
    if (mode <= tuneFree || mode >= kNumTuneModes)
        return semitones;

    const int* degrees = nullptr;
    const int  count   = scaleDegrees (mode, degrees);
    if (degrees == nullptr || count <= 0)
        return semitones;

    const float pitch = clamp (semitones, kMinGrainSemitones, kMaxGrainSemitones);

    float best     = pitch;
    float bestDist = 1.0e9f;

    // Searches every scale degree in -4..+4 octaves, which covers the full
    // clamped +/-48 semitone range (-48 ... +59) with margin.
    for (int octave = -4; octave <= 4; ++octave)
    {
        for (int d = 0; d < count; ++d)
        {
            const float candidate = (float) (degrees[d] + 12 * octave);
            const float dist      = std::abs (candidate - pitch);
            if (dist < bestDist)
            {
                bestDist = dist;
                best     = candidate;
            }
        }
    }

    return best;
}

// Equal-power stereo pan. pan = -1 hard left, +1 hard right.
inline void panGains (float pan, float& leftGain, float& rightGain) noexcept
{
    const float angle = (clamp (pan, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
    leftGain  = std::cos (angle);
    rightGain = std::sin (angle);
}

} // namespace nagloed::dsp
