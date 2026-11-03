// SCAFFOLD demo performance — replaced by the sound-design workstream.
#include "DemoScore.h"
#include <cmath>

namespace demo
{
namespace
{
constexpr float kTwoPi = 6.28318530718f;

// Warm, slightly detuned additive voice (stands in for an analog synth).
float voice (float phase, float detune)
{
    float s = 0.0f;
    for (int k = 1; k <= 10; ++k)
        s += std::sin (phase * k * (1.0f + detune * 0.0015f)) / (float) k;
    return s;
}

float pluckEnvelope (float age, float decay)
{
    return age < 0.0f ? 0.0f : std::exp (-age * decay) * (1.0f - std::exp (-age * 300.0f));
}
} // namespace

float lengthSeconds() { return 78.0f; }

const std::vector<AutomationPoint>& automation()
{
    static const std::vector<AutomationPoint> points {
        { 0.0f,  "ear.mix",        0.2f  },
        { 4.0f,  "grain.density",  18.0f },
        { 8.0f,  "grain.spray",    0.45f },
        { 12.0f, "space.mix",      0.6f  },
        { 16.0f, "grain.density",  60.0f },
        { 20.0f, "grain.pitch",    12.0f },
        { 24.0f, "ear.drive",      12.0f },
        { 28.0f, "grain.size",     240.0f},
        { 32.0f, "grain.freeze",   1.0f  },
        { 36.0f, "grain.scan",     -0.4f },
        { 40.0f, "grain.spray",    0.8f  },
        { 46.0f, "grain.pitch",    -12.0f},
        { 50.0f, "ear.cutoff",     1800.0f },
        { 54.0f, "space.mod",      0.7f  },
        { 58.0f, "grain.feedback", 0.35f },
        { 62.0f, "grain.freeze",   0.0f  },
        { 64.0f, "ear.drive",      4.0f  },
        { 68.0f, "grain.pitch",    0.0f  },
        { 70.0f, "ear.mix",        0.0f  },
    };
    return points;
}

void renderInput (float t0, double sr, int numSamples, float* left, float* right)
{
    constexpr float bpm = 82.0f;
    constexpr float a2  = 110.0f;
    static const int scaleSteps[] = { 0, 7, 12, 3, 10, 5, 12, 7, 0, 15, 7, 12 };
    const float stepLen = 60.0f / bpm * 0.5f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float t = t0 + (float) (i / sr);
        const int step = (int) (t / stepLen);
        const float age = t - (float) step * stepLen;
        const float semi = (float) scaleSteps[step % 12];
        const float freq = a2 * std::pow (2.0f, semi / 12.0f);
        const float phase = t * freq * kTwoPi;

        const float env = pluckEnvelope (age, 3.2f);
        const float sig = voice (phase, -6.0f) * env * 0.22f;

        // Slow pad underneath.
        const float pad = 0.05f * std::sin (t * a2 * kTwoPi * 0.5f)
                        + 0.035f * std::sin (t * a2 * 1.5f * kTwoPi + 0.4f);
        const float swell = 0.5f + 0.5f * std::sin (t * 0.11f);

        left[i]  = sig + pad * swell;
        right[i] = sig * 0.92f + voice (phase, 7.0f) * env * 0.05f + pad * swell * 0.85f;
    }
}

} // namespace demo
