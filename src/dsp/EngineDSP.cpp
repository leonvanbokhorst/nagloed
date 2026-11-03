// -----------------------------------------------------------------------------
// SCAFFOLD implementation: transparent pass-through with metering.
// The DSP workstream replaces the body of process()/getVisual() with the real
// Sintel grain engine, Ruimte FDN and EAR analog chain.
// -----------------------------------------------------------------------------
#include "EngineDSP.h"

namespace nagloed
{

EngineDSP::EngineDSP() = default;
EngineDSP::~EngineDSP() = default;

void EngineDSP::prepare (double sr, int maxBlock, int numChannels)
{
    sampleRate = sr;
    blockSize  = juce::jmax (1, maxBlock);
    channels   = juce::jmax (1, numChannels);
    reset();
}

void EngineDSP::reset()
{
    visualInL = visualInR = visualOutL = visualOutR = 0.0f;
    bufferFill = 0.0f;
    frameCounter = 0;
}

void EngineDSP::setParams (const EngineParams& p) noexcept
{
    params = p;
    inputGain = p.inputGain();
}

void EngineDSP::process (juce::AudioBuffer<float>& io, const juce::MidiBuffer&) noexcept
{
    const int numSamples = io.getNumSamples();
    if (numSamples <= 0 || io.getNumChannels() == 0)
        return;

    float sumL = 0.0f, sumR = 0.0f;

    for (int ch = 0; ch < io.getNumChannels(); ++ch)
    {
        auto* data = io.getWritePointer (ch);
        const float g = inputGain * params.outputGain();
        for (int i = 0; i < numSamples; ++i)
            data[i] *= g;

        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            if (ch == 0) sumL += s * s; else if (ch == 1) sumR += s * s;
        }
    }

    const float norm = 1.0f / juce::jmax (1, numSamples);
    visualInL = visualOutL = std::sqrt (sumL * norm);
    visualInR = visualOutR = std::sqrt (sumR * norm);

    bufferFill = juce::jmin (1.0f, bufferFill + numSamples / (float) (sampleRate * 4.0));
    frameCounter += (juce::uint32) numSamples;
}

void EngineDSP::getVisual (VisualState& out) const noexcept
{
    out = VisualState();
    out.inL = out.outL = visualInL;
    out.inR = out.outR = visualInR;
    out.inPeak = out.outPeak = juce::jmax (visualInL, visualInR);
    out.focus = params.focus;
    out.spray = params.spray;
    out.bufferFill = bufferFill;
    out.densityNorm = juce::jlimit (0.0f, 1.0f, params.densityHz() / 200.0f);
    out.cutoffNorm = juce::jlimit (0.0f, 1.0f,
                                   std::log (juce::jmax (20.0f, params.cutoffHz) / 20.0f)
                                       / std::log (20000.0f / 20.0f));
    out.drive = juce::jlimit (0.0f, 1.0f, params.driveDb / 30.0f);
    out.reduce = params.lofi;
    out.frame = frameCounter;
}

int EngineDSP::getLatencySamples() const noexcept { return 0; }

} // namespace nagloed
