#include "RuimteReverb.h"

namespace nagloed::dsp
{

namespace
{
// Incommensurate delay ratios: no two lines line up, so the echo density grows
// quickly and the tail never "rings" on a single pitch.
constexpr float kLineRatio[RuimteReverb::kLines] = {
    1.00f, 1.17f, 1.37f, 1.59f, 1.83f, 2.09f, 2.37f, 2.67f
};

// Four slowly drifting modulation rates per side (Hz). Very slow: this is a
// space that breathes, not a chorus.
constexpr float kModRate[RuimteReverb::kLines] = {
    0.11f, 0.13f, 0.17f, 0.19f, 0.23f, 0.29f, 0.31f, 0.37f
};

constexpr float kInverseSqrt8 = 0.3535533905932738f;   // 1 / sqrt(8)

// Delay range: 8 ms (tight room) to 80 ms base (cathedral-ish). Ratios above
// stretch individual lines up to ~213 ms while keeping the density high.
constexpr float kBaseDelayMinSeconds = 0.008f;
constexpr float kBaseDelayMaxSeconds = 0.080f;
constexpr float kMaxModDepthSamples  = 32.0f;
constexpr float kMaxLoopGain         = 0.97f;

inline int nextPowerOfTwo (int v) noexcept
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}
} // namespace

void RuimteReverb::prepare (double sr)
{
    sampleRate = (sr > 0.0 ? sr : 48000.0);

    for (int i = 0; i < kLines; ++i)
    {
        // Longest possible line: 80 ms * 2.67 + modulation headroom.
        const int longest = (int) std::lround (kBaseDelayMaxSeconds * kLineRatio[i] * sampleRate)
                          + (int) kMaxModDepthSamples + 64;
        lineLengths[(size_t) i] = nextPowerOfTwo (std::max (longest, 128));
        masks[(size_t) i]       = (std::uint32_t) (lineLengths[(size_t) i] - 1);
        lines[(size_t) i].assign ((size_t) lineLengths[(size_t) i], 0.0f);

        modulationInc[(size_t) i] = (float) (kModRate[i] / sampleRate);
    }

    reset();
}

void RuimteReverb::reset() noexcept
{
    for (int i = 0; i < kLines; ++i)
    {
        std::fill (lines[(size_t) i].begin(), lines[(size_t) i].end(), 0.0f);
        dampState[(size_t) i] = 0.0f;

        // Stagger the modulation phases so the eight lines never move together.
        modulationPhase[(size_t) i] = (float) (i * 0.125 + 0.03);

        // Start current delays at a musical mid-size so the first block after
        // prepare() already has a sensible tail instead of a click.
        targetDelay[(size_t) i]  = kBaseDelayMinSeconds * kLineRatio[i] * (float) sampleRate;
        currentDelay[(size_t) i] = targetDelay[(size_t) i];
    }

    writePos = 0;
    loopGain = 0.85f;
    dampingCoeff = 0.4f;
    modDepthSamples = 0.0f;
    injectionGain = 0.2f;
}

void RuimteReverb::update (const ReverbControls& controls) noexcept
{
    const float size = clamp (controls.size, 0.0f, 1.0f);
    const float damp = clamp (controls.damp, 0.0f, 1.0f);
    const float mod  = clamp (controls.mod, 0.0f, 1.0f);

    // --- delay lengths -------------------------------------------------------
    const float baseSeconds = kBaseDelayMinSeconds
                            + (kBaseDelayMaxSeconds - kBaseDelayMinSeconds) * size;

    for (int i = 0; i < kLines; ++i)
    {
        const float computed = baseSeconds * kLineRatio[i] * (float) sampleRate;
        targetDelay[(size_t) i] = clamp (computed, 8.0f, (float) (lineLengths[(size_t) i] - 16));

        // Glide towards the target: when the Size knob is automated the delay
        // lines tape-style pitch-bend instead of clicking. The modulation adds
        // the final smoothing touch.
        currentDelay[(size_t) i] += (targetDelay[(size_t) i] - currentDelay[(size_t) i]) * 0.15f;
    }

    // --- loop gain from the desired RT60 -------------------------------------
    // RT60 spans ~0.35 s (small room) to ~11 s (distant hall). The average
    // loop delay scales with size, so the gain is derived from it.
    const float rt60 = 0.35f * std::pow (32.0f, size);
    const float avgDelaySeconds = baseSeconds * 1.7f;
    loopGain = clamp (std::pow (10.0f, -3.0f * avgDelaySeconds / rt60), 0.55f, kMaxLoopGain);

    // --- damping -------------------------------------------------------------
    // Damp = 0 is an open, bright space; damp = 1 is a dark, tape-like tail.
    const float dampCutoff = 800.0f * std::pow (20.0f, 1.0f - damp);
    dampingCoeff = clamp (1.0f - std::exp (-kTwoPi * dampCutoff / (float) sampleRate), 0.02f, 0.995f);

    // --- modulation depth ----------------------------------------------------
    modDepthSamples = clamp (mod * kMaxModDepthSamples, 0.0f, kMaxModDepthSamples);

    // --- injection compensation ---------------------------------------------
    // Scale the input by sqrt(1 - g^2): a lossy network needs less injection to
    // reach the same steady-state level, which keeps the wet level roughly
    // constant as Size moves. Clamped so silence-extreme sizes stay sane.
    injectionGain = 0.5f * std::sqrt (std::max (1.0e-4f, 1.0f - loopGain * loopGain));
}

void RuimteReverb::hadamard8 (float* v) noexcept
{
    // In-place fast Walsh-Hadamard transform (unnormalised). Applied as an
    // orthogonal mixing matrix after scaling by 1/sqrt(8).
    float a0 = v[0] + v[1], a1 = v[0] - v[1], a2 = v[2] + v[3], a3 = v[2] - v[3];
    float a4 = v[4] + v[5], a5 = v[4] - v[5], a6 = v[6] + v[7], a7 = v[6] - v[7];

    const float b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3;
    const float b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;

    v[0] = b0 + b4;
    v[1] = b1 + b5;
    v[2] = b2 + b6;
    v[3] = b3 + b7;
    v[4] = b0 - b4;
    v[5] = b1 - b5;
    v[6] = b2 - b6;
    v[7] = b3 - b7;
}

float RuimteReverb::readLine (int line, double delaySamples) const noexcept
{
    const double readPos = (double) writePos - delaySamples;

    const std::int64_t base = (std::int64_t) std::floor (readPos);
    const float frac = (float) (readPos - (double) base);

    const std::uint32_t mask = masks[(size_t) line];
    const std::uint32_t i0 = (std::uint32_t) base & mask;
    const std::uint32_t i1 = (std::uint32_t) (base + 1) & mask;

    const auto& buffer = lines[(size_t) line];
    const float a = buffer[i0];
    const float b = buffer[i1];
    return a + (b - a) * frac;
}

RuimteReverb::Stereo RuimteReverb::process (float inL, float inR) noexcept
{
    // --- read & damp ---------------------------------------------------------
    float mixed[kLines];
    float wet[kLines];

    const bool modulate = modDepthSamples > 0.001f;

    for (int i = 0; i < kLines; ++i)
    {
        float delay = currentDelay[(size_t) i];

        if (modulate)
        {
            delay += modDepthSamples * sine (modulationPhase[(size_t) i]);

            modulationPhase[(size_t) i] += modulationInc[(size_t) i];
            if (modulationPhase[(size_t) i] >= 1.0f)
                modulationPhase[(size_t) i] -= 1.0f;
        }

        const float read = readLine (i, (double) delay);

        // One-pole damping inside the loop: high frequencies die faster than
        // lows, which is what makes a hall sound like a hall.
        dampState[(size_t) i] = flushDenormal (dampState[(size_t) i]
                                               + dampingCoeff * (read - dampState[(size_t) i]));

        wet[(size_t) i]   = read;
        mixed[(size_t) i] = dampState[(size_t) i];
    }

    // --- orthogonal mixing ---------------------------------------------------
    hadamard8 (mixed);
    for (float& v : mixed)
        v *= kInverseSqrt8;

    // --- feedback + injection ------------------------------------------------
    // Two decorrelated stereo injection patterns: even lines carry the left
    // input, odd lines the right, with alternating polarity so the matrix gets
    // an uncorrelated excitation.
    for (int i = 0; i < kLines; ++i)
    {
        const float source = ((i & 1) == 0 ? inL : inR);
        const float sign   = ((i & 2) == 0) ? 1.0f : -1.0f;
        const float injected = source * injectionGain * sign;

        float value = loopGain * mixed[(size_t) i] + injected;

        // Finite/range guard: a single bad sample would otherwise circulate in
        // the network for the whole tail time.
        value = (value == value) ? clamp (value, -8.0f, 8.0f) : 0.0f;

        const std::uint32_t index = (std::uint32_t) writePos & masks[(size_t) i];
        lines[(size_t) i][(size_t) index] = flushDenormal (value);
    }

    ++writePos;

    // --- stereo taps ---------------------------------------------------------
    // Four lines feed each side with alternating signs; the result is wide but
    // mono-compatible because both sides come from the same diffuse field.
    const float outL = (wet[0] + wet[2] - wet[5] - wet[7]) * 0.65f;
    const float outR = (wet[1] + wet[3] - wet[4] - wet[6]) * 0.65f;

    return { outL, outR };
}

} // namespace nagloed::dsp
