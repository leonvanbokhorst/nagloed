#include "AnalogChain.h"

namespace nagloed::dsp
{

namespace
{
inline int nextPowerOfTwo (int v) noexcept
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

// Tone tilt: a fixed low/high split around ~750 Hz. Two gains (toneLow,
// toneHigh) are crossfaded upstream, so the tilt stays a clean first-order
// shelf instead of a resonant filter.
constexpr float kToneSplitHz = 750.0f;

// Tape transport: base delay of ~1 ms so wow/flutter always has room to move
// the read head both forwards and backwards. Depths are supplied in samples by
// EngineDSP (scaled with the sample rate).
constexpr float kTapeBaseSeconds = 0.001f;
constexpr int   kTapeMaxExtraSamples = 160;

// Hiss is coloured (lowpassed) so it sounds like tape rather than white noise.
constexpr float kHissCutoffHz = 7000.0f;

// Wow/flutter rates: classic tape wow is sub-1 Hz, flutter is a few Hz.
constexpr float kWowHz     = 0.7f;
constexpr float kFlutterHz = 6.3f;

// DC blocker corner: protects the output from any residual saturation offset
// without eating the low end.
constexpr float kDcCutoffHz = 8.0f;
} // namespace

AnalogChain::AnalogChain() = default;

void AnalogChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = (sr > 0.0 ? sr : 48000.0);
    maxBlock   = std::max (1, maxBlockSize);

    // --- scratch buffers -----------------------------------------------------
    osInput.setSize (2, maxBlock, false, true, true);
    osOutput.setSize (2, maxBlock, false, true, true);

    // --- oversampled nonlinear stages ---------------------------------------
    oversampler.initProcessing ((size_t) maxBlock);
    oversamplerLatency = (int) std::lround (oversampler.getLatencyInSamples());

    // --- filters -------------------------------------------------------------
    // The tone tilt is applied inside the oversampled nonlinear loop (between
    // saturation and the ladder, per the signal-chain design), so its split
    // filter runs at 2x the host rate.
    toneLpL.setCutoff (kToneSplitHz, sampleRate * 2.0);
    toneLpR.setCutoff (kToneSplitHz, sampleRate * 2.0);
    hissLpL.setCutoff (kHissCutoffHz, sampleRate);
    hissLpR.setCutoff (kHissCutoffHz, sampleRate);
    dcL.setCutoff (kDcCutoffHz, sampleRate);
    dcR.setCutoff (kDcCutoffHz, sampleRate);

    // --- tape transport ------------------------------------------------------
    tapeBaseDelay = std::max (16.0f, kTapeBaseSeconds * (float) sampleRate);
    {
        const int length = nextPowerOfTwo ((int) tapeBaseDelay + kTapeMaxExtraSamples + 8);
        tapeL.assign ((size_t) length, 0.0f);
        tapeR.assign ((size_t) length, 0.0f);
        tapeMask  = (std::uint32_t) (length - 1);
        tapeWrite = 0;
    }

    // --- latency and dry-path compensation ----------------------------------
    // The wet path is delayed by the oversampler plus the tape transport. The
    // reported latency uses the mean tape delay (the wow/flutter modulation
    // averages out), while the dry path below tracks the instantaneous delay so
    // the blend never combs.
    latency = oversamplerLatency + (int) std::lround (tapeBaseDelay);
    {
        const int length = nextPowerOfTwo (oversamplerLatency + (int) tapeL.size() + 8);
        dryDelayL.assign ((size_t) length, 0.0f);
        dryDelayR.assign ((size_t) length, 0.0f);
        dryMask  = (std::uint32_t) (length - 1);
        dryWrite = 0;
    }

    wowInc     = (float) (kWowHz / sampleRate);
    flutterInc = (float) (kFlutterHz / sampleRate);

    reset();
}

void AnalogChain::reset() noexcept
{
    osInput.clear();
    osOutput.clear();

    std::fill (dryDelayL.begin(), dryDelayL.end(), 0.0f);
    std::fill (dryDelayR.begin(), dryDelayR.end(), 0.0f);
    dryWrite = 0;

    std::fill (tapeL.begin(), tapeL.end(), 0.0f);
    std::fill (tapeR.begin(), tapeR.end(), 0.0f);
    tapeWrite = 0;

    oversampler.reset();

    wowPhase     = 0.0f;
    flutterPhase = 0.0f;

    toneLpL.reset();
    toneLpR.reset();
    ladderL.reset();
    ladderR.reset();

    lofiCounter = 0.0f;
    lofiHeldL = lofiHeldR = 0.0f;
    quantLevels = 65536.0f;

    hissRng.seed (0x48495353u);   // "HISS"
    hissLpL.reset();
    hissLpR.reset();

    dcL.reset();
    dcR.reset();
}

float AnalogChain::processLadder (LadderState& state, float x, float g, float k) noexcept
{
    // Zero-delay-feedback (TPT) 4-pole transistor ladder. Solving the feedback
    // loop algebraically instead of using a one-sample delay is what gives the
    // filter its analog phase response: at k = 4 the loop gain reaches unity at
    // the cutoff and the filter self-oscillates; the tanh limit on the driven
    // input keeps that oscillation bounded and musical.
    //
    // g is the prewarped tan(pi * fc / fs); G is the TPT one-pole coefficient.
    const float G  = g / (1.0f + g);
    const float G2 = G * G;
    const float G3 = G2 * G;
    const float G4 = G3 * G;
    const float oneMinusG = 1.0f - G;

    // Contribution of the four integrator states to the 4th stage output.
    const float S = G3 * oneMinusG * state.s1
                  + G2 * oneMinusG * state.s2
                  + G  * oneMinusG * state.s3
                  +        oneMinusG * state.s4;

    float input = (x - k * S) / (1.0f + k * G4);

    // Soft saturation inside the loop: bounds self-oscillation and gives the
    // ladder its classic driven, singing character.
    input = fastTanh (input);

    const float y1 = G * input + oneMinusG * state.s1;
    const float y2 = G * y1    + oneMinusG * state.s2;
    const float y3 = G * y2    + oneMinusG * state.s3;
    const float y4 = G * y3    + oneMinusG * state.s4;

    // TPT integrator update.
    state.s1 = flushDenormal (2.0f * y1 - state.s1);
    state.s2 = flushDenormal (2.0f * y2 - state.s2);
    state.s3 = flushDenormal (2.0f * y3 - state.s3);
    state.s4 = flushDenormal (2.0f * y4 - state.s4);

    return y4;
}

void AnalogChain::process (float* wetL, float* wetR,
                           const float* dryL, const float* dryR,
                           int numSamples,
                           const Group* groups, int numGroups) noexcept
{
    if (numSamples <= 0 || wetL == nullptr || wetR == nullptr)
        return;

    numSamples = std::min (numSamples, maxBlock);
    if (groups == nullptr || numGroups <= 0)
        return;

    // -------------------------------------------------------------------------
    // 1. Pre stage: input gain + drive gain -> oversampler input.
    // -------------------------------------------------------------------------
    float* osInL = osInput.getWritePointer (0);
    float* osInR = osInput.getWritePointer (1);

    for (int gi = 0; gi < numGroups; ++gi)
    {
        const int s0 = (gi == 0) ? 0 : std::max (0, std::min (groups[gi].start, numSamples));
        const int s1 = (gi + 1 < numGroups) ? std::max (s0, std::min (groups[gi + 1].start, numSamples))
                                           : numSamples;
        const Controls& c = groups[gi].controls;

        for (int i = s0; i < s1; ++i)
        {
            // Input gain + drive gain: both are linear pre-gains into the
            // oversampled asymmetric saturator.
            const float gain = c.inputGain * c.driveGain;
            osInL[i] = sanitiseInput (wetL[i]) * gain;
            osInR[i] = sanitiseInput (wetR[i]) * gain;
        }
    }

    // -------------------------------------------------------------------------
    // 2. 2x oversampled asymmetric saturation + resonant ladder lowpass.
    // -------------------------------------------------------------------------
    const auto inputBlock = juce::dsp::AudioBlock<const float> (osInput)
                                .getSubBlock (0, (size_t) numSamples);

    auto upBlock = oversampler.processSamplesUp (inputBlock);

    float* upL = upBlock.getChannelPointer (0);
    float* upR = upBlock.getChannelPointer (1);
    const int osSamples = (int) upBlock.getNumSamples();

    for (int gi = 0; gi < numGroups; ++gi)
    {
        const int s0 = (gi == 0) ? 0 : std::max (0, std::min (groups[gi].start * 2, osSamples));
        const int s1 = (gi + 1 < numGroups) ? std::max (s0, std::min (groups[gi + 1].start * 2, osSamples))
                                           : osSamples;
        const Controls& c = groups[gi].controls;

        // Asymmetric bias -> even (2nd) harmonics. The bias offset is removed
        // again so the stage does not inject DC into the ladder.
        const float bias      = clamp (c.bias, 0.0f, 1.0f) * 1.2f;
        const float biasShift = fastTanh (bias);
        const float ladderG   = clamp (c.ladderG, 0.0f, 8.0f);   // prewarped tan(pi*fc/fs)
        const float ladderK   = clamp (c.ladderK, 0.0f, 4.0f);

        for (int i = s0; i < s1; ++i)
        {
            // Asymmetric saturation -> tone tilt -> 24 dB ladder (the EAR
            // chain order documented in AnalogChain.h and the PR description).
            const float satL = fastTanh (upL[i] + bias) - biasShift;
            const float satR = fastTanh (upR[i] + bias) - biasShift;

            const float lowL = toneLpL.process (satL);
            const float lowR = toneLpR.process (satR);
            const float tonedL = lowL * c.toneLow + (satL - lowL) * c.toneHigh;
            const float tonedR = lowR * c.toneLow + (satR - lowR) * c.toneHigh;

            upL[i] = processLadder (ladderL, tonedL, ladderG, ladderK);
            upR[i] = processLadder (ladderR, tonedR, ladderG, ladderK);
        }
    }

    auto downBlock = juce::dsp::AudioBlock<float> (osOutput)
                         .getSubBlock (0, (size_t) numSamples);

    oversampler.processSamplesDown (downBlock);

    const float* downL = osOutput.getReadPointer (0);
    const float* downR = osOutput.getReadPointer (1);

    // -------------------------------------------------------------------------
    // 3. Post stage: tape (wow/flutter + saturation), LoFi, hiss, DC block,
    //    output limiter and the final dry/wet blend.
    // -------------------------------------------------------------------------
    const float tapeMaxDelay = (float) (tapeL.size() - 2);

    for (int gi = 0; gi < numGroups; ++gi)
    {
        const int s0 = (gi == 0) ? 0 : std::max (0, std::min (groups[gi].start, numSamples));
        const int s1 = (gi + 1 < numGroups) ? std::max (s0, std::min (groups[gi + 1].start, numSamples))
                                           : numSamples;
        const Controls& c = groups[gi].controls;

        const float mix       = clamp (c.mix, 0.0f, 1.0f);
        const float outputGain = c.outputGain;
        const float tapeDrive  = std::max (0.01f, c.tapeDrive);
        const float tapeComp   = c.tapeComp;
        const float lofiHold   = std::max (1.0f, c.lofiHold);
        const float lofiLevels = std::max (2.0f, c.lofiLevels);

        for (int i = s0; i < s1; ++i)
        {
            float l = downL[i];
            float r = downR[i];

            // --- wow & flutter ---------------------------------------------
            wowPhase += wowInc;
            if (wowPhase >= 1.0f)
                wowPhase -= 1.0f;

            flutterPhase += flutterInc;
            if (flutterPhase >= 1.0f)
                flutterPhase -= 1.0f;

            float delay = tapeBaseDelay
                        + c.wowDepth * sine (wowPhase)
                        + c.flutterDepth * sine (flutterPhase)
                        + c.driftDepth;

            delay = clamp (delay, 2.0f, tapeMaxDelay);

            tapeWrite = (tapeWrite + 1) & tapeMask;
            tapeL[(size_t) tapeWrite] = flushDenormal (l);
            tapeR[(size_t) tapeWrite] = flushDenormal (r);

            const float readPos = (float) tapeWrite - delay;
            const int   base    = (int) std::floor (readPos);
            const float frac    = readPos - (float) base;

            const std::uint32_t i0 = (std::uint32_t) base & tapeMask;
            const std::uint32_t i1 = (std::uint32_t) (base + 1) & tapeMask;

            l = tapeL[(size_t) i0] + (tapeL[(size_t) i1] - tapeL[(size_t) i0]) * frac;
            r = tapeR[(size_t) i0] + (tapeR[(size_t) i1] - tapeR[(size_t) i0]) * frac;

            // --- tape saturation (compensated so low levels stay unity) ----
            l = fastTanh (l * tapeDrive) * tapeComp;
            r = fastTanh (r * tapeDrive) * tapeComp;

            // --- LoFi: sample-rate + bit reduction -------------------------
            if (c.lofiActive)
            {
                lofiCounter -= 1.0f;
                if (lofiCounter <= 0.0f)
                {
                    lofiCounter = lofiHold;
                    quantLevels = lofiLevels;
                    lofiHeldL = std::floor (l * quantLevels + 0.5f) / quantLevels;
                    lofiHeldR = std::floor (r * quantLevels + 0.5f) / quantLevels;
                }
                l = lofiHeldL;
                r = lofiHeldR;
            }

            // --- tape hiss -------------------------------------------------
            if (c.hissActive)
            {
                l += hissLpL.process (hissRng.bipolar()) * c.hissGain;
                r += hissLpR.process (hissRng.bipolar()) * c.hissGain;
            }

            // --- DC block, output gain, soft ceiling -----------------------
            l = dcL.process (l);
            r = dcR.process (r);

            l = softLimitOutput (l * outputGain);
            r = softLimitOutput (r * outputGain);

            // --- dry/wet blend at the end of the chain ---------------------
            // The dry signal is delayed by the oversampler latency plus the
            // *same* instantaneous tape delay (including wow/flutter/drift) the
            // wet path just went through. This keeps a 50/50 blend perfectly
            // phase-coherent; delaying by a fixed integer only would comb-filter
            // against the ~48-sample tape base delay.
            dryWrite = (dryWrite + 1) & dryMask;
            dryDelayL[(size_t) dryWrite] = flushDenormal (dryL[i]);
            dryDelayR[(size_t) dryWrite] = flushDenormal (dryR[i]);

            const float dryDelayTotal = (float) oversamplerLatency + delay;
            const float dryReadPos    = (float) dryWrite - dryDelayTotal;
            const int   dryBase       = (int) std::floor (dryReadPos);
            const float dryFrac       = dryReadPos - (float) dryBase;

            const std::uint32_t d0 = (std::uint32_t) dryBase & dryMask;
            const std::uint32_t d1 = (std::uint32_t) (dryBase + 1) & dryMask;

            const float dryLAligned = dryDelayL[(size_t) d0]
                                    + (dryDelayL[(size_t) d1] - dryDelayL[(size_t) d0]) * dryFrac;
            const float dryRAligned = dryDelayR[(size_t) d0]
                                    + (dryDelayR[(size_t) d1] - dryDelayR[(size_t) d0]) * dryFrac;

            wetL[i] = dryLAligned + (l - dryLAligned) * mix;
            wetR[i] = dryRAligned + (r - dryRAligned) * mix;
        }
    }
}

} // namespace nagloed::dsp
