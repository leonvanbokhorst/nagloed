// -----------------------------------------------------------------------------
// NAGLOED demo performance: 84 s at 80 BPM (28 bars, 3 s per bar).
//
// The source is a small analog-flavoured trio, all a pure function of absolute
// time so the render is bit-for-bit repeatable and needs no state between calls:
//   * a plucked minor arpeggio with tape-style detune and slow pitch drift,
//   * a slow choir-ish pad following Am - F - C - G,
//   * a field-recording bed (wind, sub rumble, tape air, distant chirps).
//
// automation() is the performance: it opens dry, freezes a chord, shimmers it
// up an octave, dusts it with spray and jitter, dives into the internal Drone
// source, opens the space, and finally returns and fades. All five macros move.
// -----------------------------------------------------------------------------
#include "DemoScore.h"
#include <cmath>
#include <cstdint>

namespace demo
{
namespace
{
constexpr float kTwoPi = 6.28318530718f;
constexpr float kLength = 84.0f;
// Source trim: the raw performance peaks around -11 dBFS, and the demo feeds
// roughly -6 dBFS into the EAR stage (hot tape, never clipping the converter).
constexpr float kSourceGain = 1.80f;

// --- Deterministic, stateless noise -----------------------------------------
inline std::uint32_t hash32 (std::uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline float hashUnit (std::uint32_t n)
{
    return (float) (hash32 (n) >> 8) * (1.0f / 16777215.0f);
}

inline float hashBi (std::uint32_t n) { return 2.0f * hashUnit (n) - 1.0f; }

inline float smoothstep (float a, float b, float x)
{
    const float u = std::fmin (1.0f, std::fmax (0.0f, (x - a) / (b - a)));
    return u * u * (3.0f - 2.0f * u);
}

// Smooth 1D value noise: band-limited wind, no filter state required.
inline float valueNoise (float x, std::uint32_t seed)
{
    const float fl = std::floor (x);
    const auto i = (std::int32_t) fl;
    const float f = x - fl;
    const float u = f * f * (3.0f - 2.0f * f);
    const float a = hashBi ((std::uint32_t) i * 0x9e3779b1U + seed);
    const float b = hashBi ((std::uint32_t) (i + 1) * 0x9e3779b1U + seed);
    return a + (b - a) * u;
}

// --- Source material --------------------------------------------------------
constexpr float kBpm = 80.0f;
constexpr float kBeat = 60.0f / kBpm;      // 0.75 s
constexpr float kStep = kBeat * 0.5f;      // 0.375 s, eighth notes
constexpr float kBar = kBeat * 4.0f;       // 3.0 s
constexpr float kChordLen = kBar * 4.0f;   // 12 s

// Am - F - C - G as semitone offsets from A2 (110 Hz).
constexpr int kChords[4][4] = {
    { 0, 3, 7, 12 },     // Am
    { -4, 0, 3, 8 },     // F
    { 3, 7, 10, 15 },    // C
    { -2, 2, 5, 10 },    // G
};

// Finger-picked contour through the four chord tones (+ octave on the 5th pick).
constexpr int kArp[8] = { 0, 2, 1, 3, 4, 3, 1, 2 };

inline float semisToHz (int semis)
{
    return 110.0f * std::pow (2.0f, (float) semis / 12.0f);
}

// Slightly detuned additive pluck: analog saw-ish, two oscillators + sub.
inline float pluckVoice (float phase, float phase2)
{
    float v = std::sin (phase) + 0.55f * std::sin (2.0f * phase)
            + 0.35f * std::sin (3.0f * phase) + 0.22f * std::sin (4.0f * phase)
            + 0.15f * std::sin (5.0f * phase);
    v += 0.60f * (std::sin (phase2) + 0.40f * std::sin (2.0f * phase2)
                + 0.25f * std::sin (3.0f * phase2));
    v += 0.35f * std::sin (0.5f * phase);        // sub
    return v;
}

inline float pluckEnvelope (float age)
{
    if (age < 0.0f)
        return 0.0f;
    const float attack = 1.0f - std::exp (-age * 260.0f);
    return attack * std::exp (-age * 4.2f);
}

inline void addPluck (float t, float& outL, float& outR)
{
    const int stepIndex = (int) std::floor (t / kStep);
    const float age = t - (float) stepIndex * kStep;

    // A few percent of rests let the phrase breathe without going silent.
    if (hashUnit ((std::uint32_t) stepIndex * 104729U + 5U) > 0.92f)
        return;

    const int chord = (int) std::floor (t / kChordLen) % 4;
    const int pick = kArp[(unsigned) stepIndex % 8U];
    const int semis = kChords[chord][pick % 4] + (pick / 4) * 12;
    const float freq = semisToHz (semis);

    // Slow analog pitch drift plus per-pluck tuning scatter.
    const float cents = 4.0f * std::sin (kTwoPi * 0.07f * t)
                      + 2.0f * std::sin (kTwoPi * 0.13f * t + 1.7f)
                      + 2.0f * hashBi ((std::uint32_t) stepIndex * 7919U + 13U);
    const float ratio = 1.0f + cents * 0.0005776f;   // ~cents/1200
    const float phase = kTwoPi * freq * ratio * t;
    const float phase2 = phase + 0.30f + kTwoPi * freq * 0.0000173f * t; // +3 cents

    float vel = 0.62f + 0.38f * hashUnit ((std::uint32_t) stepIndex * 6151U + 3U);
    if ((stepIndex % 8) == 0)
        vel *= 1.18f;

    const float sig = pluckVoice (phase, phase2) * pluckEnvelope (age) * vel * 0.092f;

    // Alternate pick direction and scatter the position for a live feel.
    const float pan = 0.55f * hashBi ((std::uint32_t) stepIndex * 31U + 7U)
                    + ((stepIndex % 2) == 0 ? -0.18f : 0.18f);
    outL += sig * std::sqrt (0.5f * (1.0f - pan));
    outR += sig * std::sqrt (0.5f * (1.0f + pan));
}

inline void addPad (float t, float& outL, float& outR)
{
    const int block = (int) std::floor (t / kChordLen);
    const float age = t - (float) block * kChordLen;
    const int cur = block % 4;
    const int prev = (block + 3) % 4;

    const float gCur = smoothstep (0.0f, 1.2f, age);
    const float gPrev = 1.0f - gCur;
    const float swell = 0.72f + 0.28f * std::sin (kTwoPi * t / 17.0f + (float) cur);

    auto addChord = [&] (int chord, float gain)
    {
        if (gain < 1.0e-4f)
            return;
        for (int v = 0; v < 4; ++v)
        {
            const int semis = kChords[chord][v] + 12;   // pad register
            const float cents = 5.0f * hashBi ((std::uint32_t) (v * 131 + chord * 17))
                              + 3.0f * std::sin (kTwoPi * 0.09f * t + (float) v);
            const float freq = semisToHz (semis) * (1.0f + cents * 0.0005776f);
            const float vib = 0.55f * std::sin (kTwoPi * 5.1f * t + (float) v * 1.7f);
            const float phase = kTwoPi * freq * t + vib;

            const float voice = std::sin (phase)
                              + 0.35f * std::sin (2.0f * phase)
                              + 0.18f * std::sin (3.0f * phase);
            const float sig = voice * gain * 0.024f * swell;

            const float pan = 0.35f * hashBi ((std::uint32_t) (v * 977 + chord * 53));
            outL += sig * std::sqrt (0.5f * (1.0f - pan));
            outR += sig * std::sqrt (0.5f * (1.0f + pan));
        }
    };

    addChord (cur, gCur);
    addChord (prev, gPrev);
}

inline void addFieldBed (float t, std::int64_t n, float& outL, float& outR)
{
    // Wind: two bands of smooth value noise, stereo-decorrelated.
    const float windEnv = 0.35f + 0.65f * (0.5f + 0.5f * std::sin (kTwoPi * t / 23.0f + 0.8f));
    const float windL = 0.7f * valueNoise (t * 0.14f, 101U)
                      + 0.3f * valueNoise (t * 0.53f, 211U);
    const float windR = 0.7f * valueNoise (t * 0.14f + 7.3f, 307U)
                      + 0.3f * valueNoise (t * 0.53f + 3.1f, 401U);
    outL += windL * windEnv * 0.050f;
    outR += windR * windEnv * 0.045f;

    // Sub rumble: slow detuned sines, barely audible but it moves the room.
    const float rumble = 0.022f * std::sin (kTwoPi * 44.0f * t
                                            + 0.7f * std::sin (kTwoPi * 0.037f * t))
                       + 0.013f * std::sin (kTwoPi * 65.7f * t + 1.3f)
                       + 0.008f * std::sin (kTwoPi * 87.3f * t + 0.4f);
    outL += rumble;
    outR += rumble * 0.92f;

    // Tape air: per-sample white noise, a whisper of it.
    const float airEnv = 0.6f + 0.4f * std::sin (kTwoPi * t / 13.0f + 2.1f);
    outL += 0.0035f * airEnv * hashBi ((std::uint32_t) (n * 2 + 1));
    outR += 0.0035f * airEnv * hashBi ((std::uint32_t) (n * 2 + 2));

    // Distant metal chirps in the field, one at a time.
    constexpr float chirpTimes[] = { 9.25f, 22.65f, 37.85f, 61.35f, 74.80f };
    for (int c = 0; c < 5; ++c)
    {
        const float dt = t - chirpTimes[c];
        if (dt < 0.0f || dt > 0.6f)
            continue;
        const float env = 0.020f * std::exp (-dt * 7.0f) * (1.0f - std::exp (-dt * 180.0f));
        const float phase = kTwoPi * (1800.0f * dt + (1200.0f / 8.0f) * (1.0f - std::cos (8.0f * dt)));
        const float sig = env * (std::sin (phase) + 0.30f * std::sin (2.0f * phase));
        const float pan = (c % 2 == 0) ? -0.45f : 0.45f;
        outL += sig * std::sqrt (0.5f * (1.0f - pan));
        outR += sig * std::sqrt (0.5f * (1.0f + pan));
    }
}

// Head and tail fades so the file opens and closes cleanly.
inline float masterFade (float t)
{
    const float in = smoothstep (0.0f, 2.5f, t);
    const float out = smoothstep (0.0f, 3.4f, kLength - t);
    return in * out;
}
} // namespace

float lengthSeconds() { return kLength; }

const std::vector<AutomationPoint>& automation()
{
    // The performance arc, in real-world units. Comments name the section.
    static const std::vector<AutomationPoint> points {
        // --- 0 s: open dry, tap in ---------------------------------------
        { 0.0f,  "ear.mix",         0.0f  },
        { 0.0f,  "ear.output",      6.0f  },

        // ear.input is the demo's section gain: the dry intro stays gentle, the
        // grain sections are pushed hot into the EAR saturation, the drone dive
        // drops back for weight, and the return lifts again before the fade.
        { 0.0f,  "ear.input",       20.0f },
        { 9.0f,  "ear.input",       22.0f },
        { 19.5f, "ear.input",       24.0f },
        { 30.0f, "ear.input",       24.0f },
        { 44.0f, "ear.input",       24.0f },
        { 45.0f, "ear.input",       13.0f },
        { 56.0f, "ear.input",       13.0f },
        { 57.0f, "ear.input",       11.0f },
        { 66.0f, "ear.input",       11.0f },
        { 69.0f, "ear.input",       18.0f },
        { 71.0f, "ear.input",       20.0f },
        { 75.0f, "ear.input",       18.0f },
        { 79.0f, "ear.input",       16.0f },
        { 0.0f,  "space.mix",       0.15f },
        { 0.0f,  "macro.glow",      0.15f },
        { 0.0f,  "macro.space",     0.20f },
        { 0.0f,  "macro.shimmer",   0.20f },
        { 1.5f,  "ear.mix",         0.10f },
        { 3.0f,  "grain.density",   6.0f  },
        { 3.0f,  "grain.size",      120.0f},
        { 4.5f,  "ear.mix",         0.20f },
        { 6.0f,  "macro.glow",      0.30f },
        { 7.5f,  "grain.density",   12.0f },
        { 7.5f,  "grain.spray",     0.18f },
        { 7.5f,  "macro.grain",     0.40f },
        { 9.0f,  "ear.mix",         0.30f },
        { 9.0f,  "ear.drive",       5.0f  },
        { 10.5f, "grain.density",   18.0f },
        { 10.5f, "grain.jitter",    0.30f },
        { 12.0f, "macro.glow",      0.40f },
        { 12.0f, "space.mix",       0.25f },
        { 13.5f, "grain.density",   22.0f },
        { 13.5f, "grain.size",      160.0f},

        // --- 15 s: freeze the chord --------------------------------------
        { 15.0f, "grain.freeze",    1.0f  },
        { 15.0f, "grain.scan",      0.0f  },
        { 15.0f, "macro.glow",      0.55f },
        { 16.5f, "grain.density",   26.0f },
        { 16.5f, "grain.spray",     0.28f },
        { 18.0f, "grain.size",      240.0f},
        { 18.0f, "macro.space",     0.35f },

        // --- 19.5 s: shimmer up an octave --------------------------------
        { 19.5f, "grain.pitch",     4.0f  },
        { 19.5f, "macro.shimmer",   0.25f },
        { 21.0f, "grain.pitch",     7.0f  },
        { 21.0f, "grain.spread",    1.5f  },
        { 22.5f, "macro.shimmer",   0.45f },
        { 22.5f, "space.mix",       0.40f },
        { 24.0f, "grain.pitch",     12.0f },
        { 24.0f, "grain.density",   30.0f },
        { 25.5f, "macro.shimmer",   0.65f },
        { 25.5f, "grain.spray",     0.35f },
        { 27.0f, "macro.space",     0.50f },
        { 27.0f, "space.size",      0.70f },
        { 28.5f, "grain.pitch",     7.0f  },

        // --- 30 s: release the freeze, start dusting ---------------------
        { 30.0f, "grain.freeze",    0.0f  },
        { 30.0f, "grain.scan",      0.20f },
        { 30.0f, "macro.shimmer",   0.45f },
        { 31.5f, "grain.jitter",    0.50f },
        { 31.5f, "grain.reverse",   0.25f },
        { 33.0f, "grain.density",   40.0f },
        { 33.0f, "grain.spray",     0.50f },
        { 33.0f, "macro.grain",     0.60f },
        { 33.0f, "mod.lfoRate",     2.5f  },
        { 33.0f, "mod.lfoDest",     6.0f  },
        { 33.0f, "mod.lfoAmount",   0.25f },
        { 34.5f, "grain.jitter",    0.65f },
        { 34.5f, "macro.drift",     0.45f },

        // --- 36 s: rhythmic grain stutter --------------------------------
        { 36.0f, "grain.pitch",     5.0f  },
        { 36.0f, "grain.reverse",   0.50f },
        { 36.0f, "ear.lofi",        0.25f },
        { 37.5f, "grain.density",   55.0f },
        { 37.5f, "grain.spray",     0.65f },
        { 39.0f, "mod.lfoRate",     5.0f  },
        { 39.0f, "mod.lfoAmount",   0.40f },
        { 40.5f, "grain.jitter",    0.80f },
        { 40.5f, "grain.spray",     0.75f },
        { 40.5f, "macro.grain",     0.70f },
        { 42.0f, "grain.pitch",     0.0f  },
        { 42.0f, "grain.density",   45.0f },
        { 43.5f, "ear.lofi",        0.10f },
        { 43.5f, "mod.lfoRate",     3.0f  },
        { 43.5f, "mod.lfoDest",     2.0f  },

        // --- 45 s: dive to the internal drone ----------------------------
        { 45.0f, "grain.source",    2.0f  },
        { 45.0f, "grain.pitch",     -12.0f},
        { 45.0f, "grain.density",   10.0f },
        { 45.0f, "grain.size",      320.0f},
        { 45.0f, "ear.mix",         1.0f  },
        { 45.0f, "space.mix",       0.50f },
        { 45.0f, "macro.grain",     0.40f },
        { 45.0f, "mod.lfoRate",     0.15f },
        { 45.0f, "mod.lfoDest",     3.0f  },
        { 45.0f, "mod.lfoAmount",   0.20f },
        { 46.5f, "grain.feedback",  0.35f },
        { 46.5f, "ear.drive",       8.0f  },
        { 48.0f, "grain.feedback",  0.50f },
        { 48.0f, "macro.space",     0.60f },
        { 49.5f, "grain.pitch",     -7.0f },
        { 49.5f, "grain.scan",      0.05f },
        { 49.5f, "ear.cutoff",      1800.0f },
        { 51.0f, "macro.drift",     0.60f },
        { 51.0f, "grain.spray",     0.35f },
        { 52.5f, "ear.drive",       10.0f },
        { 52.5f, "grain.size",      360.0f},
        { 54.0f, "grain.pitch",     -5.0f },
        { 54.0f, "macro.glow",      0.60f },
        { 55.5f, "space.mix",       0.65f },
        { 55.5f, "space.size",      0.85f },

        // --- 57 s: open the space ----------------------------------------
        { 57.0f, "space.mix",       0.90f },
        { 57.0f, "space.size",      0.95f },
        { 57.0f, "space.damp",      0.20f },
        { 57.0f, "macro.space",     0.90f },
        { 58.5f, "space.mod",       0.60f },
        { 58.5f, "macro.drift",     0.70f },
        { 60.0f, "grain.pitch",     0.0f  },
        { 60.0f, "grain.density",   14.0f },
        { 60.0f, "ear.cutoff",      3200.0f },
        { 61.5f, "macro.shimmer",   0.50f },
        { 61.5f, "grain.pitch",     7.0f  },
        { 63.0f, "space.mix",       0.85f },
        { 63.0f, "space.size",      0.90f },
        { 64.5f, "grain.size",      260.0f},
        { 64.5f, "macro.space",     0.80f },
        { 66.0f, "grain.pitch",     12.0f },
        { 66.0f, "macro.shimmer",   0.60f },
        { 67.5f, "grain.density",   18.0f },
        { 67.5f, "grain.spray",     0.30f },

        // --- 69 s: return to input / dry, fade home ----------------------
        { 69.0f, "grain.source",    0.0f  },
        { 69.0f, "grain.freeze",    0.0f  },
        { 69.0f, "grain.pitch",     0.0f  },
        { 69.0f, "ear.mix",         0.75f },
        { 69.0f, "space.mix",       0.60f },
        { 69.0f, "macro.glow",      0.50f },
        { 70.5f, "grain.density",   20.0f },
        { 70.5f, "grain.spray",     0.25f },
        { 70.5f, "ear.drive",       6.0f  },
        { 72.0f, "grain.spray",     0.30f },
        { 72.0f, "macro.grain",     0.35f },
        { 72.0f, "macro.space",     0.50f },
        { 73.5f, "grain.density",   12.0f },
        { 73.5f, "grain.size",      180.0f},
        { 73.5f, "macro.drift",     0.40f },
        { 75.0f, "grain.density",   6.0f  },
        { 75.0f, "grain.size",      220.0f},
        { 75.0f, "ear.mix",         0.55f },
        { 75.0f, "space.mix",       0.40f },
        { 76.5f, "ear.mix",         0.35f },
        { 76.5f, "macro.glow",      0.35f },
        { 76.5f, "macro.shimmer",   0.30f },
        { 78.0f, "grain.density",   3.0f  },
        { 78.0f, "grain.spray",     0.20f },
        { 78.0f, "space.mix",       0.28f },
        { 79.5f, "ear.mix",         0.20f },
        { 79.5f, "macro.space",     0.25f },
        { 80.5f, "ear.output",      0.0f  },
        { 80.5f, "ear.input",       -6.0f },
        { 81.5f, "ear.output",      -8.0f },
        { 81.5f, "ear.input",       -12.0f},
        { 82.5f, "ear.output",      -17.0f},
        { 82.5f, "ear.input",       -18.0f},
        { 83.2f, "ear.mix",         0.0f  },
        { 83.5f, "ear.output",      -24.0f},
        { 83.5f, "ear.input",       -24.0f},
    };
    return points;
}

void renderInput (float t0, double sampleRate, int numSamples,
                  float* left, float* right)
{
    const std::int64_t base = (std::int64_t) std::llround ((double) t0 * sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = t0 + (float) ((double) i / sampleRate);
        float l = 0.0f, r = 0.0f;

        addPluck (t, l, r);
        addPad (t, l, r);
        addFieldBed (t, base + i, l, r);

        const float fade = masterFade (t) * kSourceGain;
        left[i]  = std::fmax (-0.95f, std::fmin (0.95f, l * fade));
        right[i] = std::fmax (-0.95f, std::fmin (0.95f, r * fade));
    }
}

} // namespace demo
