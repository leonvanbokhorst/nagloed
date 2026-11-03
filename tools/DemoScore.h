#pragma once

#include <vector>

// -----------------------------------------------------------------------------
// Demo score: the little "input performance" the render tool feeds to NAGLOED.
//
// OWNERSHIP: the sound-design workstream. You own this file. Keep it
// dependency-free: it must only need <vector> and <cmath>.
//
// renderInput() writes `numSamples` frames of a stereo source starting at
// absolute time t0 (seconds). It is called sequentially, but should be written
// as if stateless (a function of absolute time) so it is easy to reason about.
//
// automation() lists parameter moves in real-world units, e.g.
//   { 6.0f, "grain.density", 80.0f }
// Times outside the score are ignored. Short fades are applied by the renderer.
// -----------------------------------------------------------------------------
namespace demo
{

struct AutomationPoint
{
    float       time;
    const char* parameter;
    float       value;
};

float lengthSeconds();

const std::vector<AutomationPoint>& automation();

void renderInput (float t0, double sampleRate, int numSamples,
                  float* left, float* right);

} // namespace demo
