#include "PluginProcessor.h"
#include "Presets.h"
#include "ui/EditorFactory.h"

namespace nagloed
{

NagloedProcessor::NagloedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NAGLOED", createParameterLayout())
{
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        gloopRequestedMode[(size_t) lane].store ((int) gloop::Mode::Idle,
                                                 std::memory_order_relaxed);
        gloopActualMode[(size_t) lane].store ((int) gloop::Mode::Idle,
                                              std::memory_order_relaxed);
    }

    // The recorder must keep working while the editor is closed: a lane starts
    // REC from the GUI, but the gesture is captured here, on the processor.
    startTimerHz (100);

    VisualState init;
    visualChannel.publish (init);
}

NagloedProcessor::~NagloedProcessor()
{
    stopTimer();
}

void NagloedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        gloopRecordingActive[(size_t) lane] = false;
        gloopActualMode[(size_t) lane].store ((int) gloop::Mode::Idle,
                                              std::memory_order_relaxed);
    }

    engine.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    setLatencySamples (engine.getLatencySamples());

    // A project may have restored gestures before the host prepared the audio
    // engine; hand them over again now that the audio side is ready.
    gloopEnginePrepared = true;
    republishGloopGestures();

    VisualState init;
    visualChannel.publish (init);
}

void NagloedProcessor::releaseResources()
{
    gloopEnginePrepared = false;
}

bool NagloedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    const bool inOk  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void NagloedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    auto params = readParams (apvts);

    // Gloop modes are performance state, deliberately not host parameters.
    // The engine gets a consistent per-block snapshot of them.
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
        params.gloopLanes[(size_t) lane].mode = gloopRequestedMode[(size_t) lane].load (
            std::memory_order_relaxed);

    params.gloopRandomize = gloopRandomizeCounter.load (std::memory_order_relaxed);

    engine.setParams (params);
    engine.process (buffer, midi);

    VisualState v;
    engine.getVisual (v);

    // Publish the engine's actual lane state back to the message thread so the
    // recorder/UI can settle e.g. a completed FADE to READY.
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
        gloopActualMode[(size_t) lane].store (v.gloop.lanes[(size_t) lane].mode,
                                              std::memory_order_relaxed);

    visualChannel.publish (v);
}

// =============================================================================
// Gloop performance state
// =============================================================================

void NagloedProcessor::setGloopMode (int lane, int mode)
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return;

    mode = juce::jlimit (0, (int) gloop::Mode::Count - 1, mode);
    gloopRequestedMode[(size_t) lane].store (mode, std::memory_order_relaxed);
}

int NagloedProcessor::getGloopMode (int lane) const
{
    if (lane < 0 || lane >= gloop::kNumLanes)
        return (int) gloop::Mode::Idle;

    return gloopRequestedMode[(size_t) lane].load (std::memory_order_relaxed);
}

void NagloedProcessor::gloopRandomize()
{
    gloopRandomizeCounter.fetch_add (1, std::memory_order_relaxed);
}

bool NagloedProcessor::hasGloopGesture (int lane) const noexcept
{
    return lane >= 0 && lane < gloop::kNumLanes && gloopGestureValid[(size_t) lane];
}

void NagloedProcessor::timerCallback()
{
    gloopTick (juce::Time::getMillisecondCounterHiRes() * 0.001);
}

void NagloedProcessor::startGloopRecording (int lane, double nowSeconds,
                                            const EngineParams& params)
{
    const auto idx = (size_t) lane;
    const int target = params.gloopLanes[idx].target;

    if (target <= (int) gloop::Target::Off || target >= gloop::kNumTargets)
    {
        // An Off lane is allowed to be silent; it cannot record a gesture.
        gloopRequestedMode[idx].store ((int) gloop::Mode::Idle, std::memory_order_relaxed);
        return;
    }

    auto& gesture = gloopGestures[idx];
    gesture.count = 0;
    gesture.durationSeconds = 0.0f;

    gloopGestureValid[idx] = false;
    gloopPublishPending[idx] = false;
    gloopRecordingTarget[idx] = target;
    gloopRecordingStartNorm[idx] = gloopTargetRange (target).convertTo0to1 (
        gloopTargetValue (params, target));
    gloopRecordingNextTime[idx] = nowSeconds;
    gloopRecordingActive[idx] = true;

    // Capture the first sample immediately: the gesture starts at the knob's
    // current centre, so the loop's first sample is a zero offset.
    captureGloopSamples (lane, nowSeconds, params);
}

void NagloedProcessor::captureGloopSamples (int lane, double nowSeconds,
                                            const EngineParams& params)
{
    const auto idx = (size_t) lane;
    auto& gesture = gloopGestures[idx];
    const int target = gloopRecordingTarget[idx];

    if (gesture.count >= gloop::kMaxGestureSamples)
        return;

    const auto range = gloopTargetRange (target);
    const float norm = range.convertTo0to1 (gloopTargetValue (params, target));
    const float offset = juce::jlimit (-1.0f, 1.0f, norm - gloopRecordingStartNorm[idx]);

    double& next = gloopRecordingNextTime[idx];

    if (nowSeconds < next - 1.0)
        next = nowSeconds;   // monotonic clock restarted

    // Never do unbounded catch-up work on the message thread. A stalled timer
    // is treated as a short pause, not as thousands of repeated samples.
    if (nowSeconds - next > 0.04)
        next = nowSeconds;

    int guard = 0;
    const double period = 1.0 / (double) gloop::kGestureSampleRate;

    while (nowSeconds >= next
           && gesture.count < gloop::kMaxGestureSamples
           && guard++ < 8)
    {
        gesture.offsets[(size_t) gesture.count++] = offset;
        next += period;
    }

    if (gesture.count >= gloop::kMaxGestureSamples)
    {
        // A full take finalises into a loop rather than recording forever.
        gloopRequestedMode[idx].store ((int) gloop::Mode::Loop, std::memory_order_relaxed);
    }
}

void NagloedProcessor::finishGloopRecording (int lane, int requestedMode)
{
    const auto idx = (size_t) lane;
    gloopRecordingActive[idx] = false;

    auto& gesture = gloopGestures[idx];

    if (requestedMode != (int) gloop::Mode::Idle && gesture.count >= 2)
    {
        gesture.durationSeconds = (float) gesture.count / gloop::kGestureSampleRate;
        gloopGestureValid[idx] = true;

        if (! gloopEnginePrepared
            || ! engine.publishGloopGesture (lane, gesture.offsets.data(),
                                             gesture.count, gesture.durationSeconds))
            gloopPublishPending[idx] = true;
    }
    else
    {
        // A cancelled or too-short take has no motion to offer.
        gloopGestureValid[idx] = false;
        gesture.count = 0;
        gesture.durationSeconds = 0.0f;

        if (requestedMode == (int) gloop::Mode::Loop
            || requestedMode == (int) gloop::Mode::Fade)
            gloopRequestedMode[idx].store ((int) gloop::Mode::Idle,
                                           std::memory_order_relaxed);
    }
}

void NagloedProcessor::gloopTick (double nowSeconds)
{
    if (! (nowSeconds == nowSeconds) || nowSeconds < 0.0)
        return;

    const auto params = readParams (apvts);

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        int requested = gloopRequestedMode[idx].load (std::memory_order_relaxed);
        const int actual = gloopActualMode[idx].load (std::memory_order_relaxed);

        // A preset (or host automation) can reset the target to Off while a lane
        // is still looping. A lane with no target is silent and must not trap
        // the UI: settle it to Idle so the target combo becomes usable again.
        // The take is kept, so the lane is READY once a new target is chosen.
        const int laneTarget = params.gloopLanes[idx].target;

        if (laneTarget <= (int) gloop::Target::Off
            && requested != (int) gloop::Mode::Idle)
        {
            requested = (int) gloop::Mode::Idle;
            gloopRequestedMode[idx].store (requested, std::memory_order_relaxed);
        }

        // The engine reports a completed fade as Idle; settle the request so the
        // lane is genuinely READY for the next REC.
        if (requested == (int) gloop::Mode::Fade && actual == (int) gloop::Mode::Idle)
        {
            requested = (int) gloop::Mode::Idle;
            gloopRequestedMode[idx].store (requested, std::memory_order_relaxed);
        }

        if (requested == (int) gloop::Mode::Record)
        {
            if (! gloopRecordingActive[idx])
                startGloopRecording (lane, nowSeconds, params);
            else
                captureGloopSamples (lane, nowSeconds, params);
        }
        else if (gloopRecordingActive[idx])
        {
            finishGloopRecording (lane, requested);
            // A cancelled / too-short take settles to Idle; use that settled
            // request so the engine never fades an older gesture by accident.
            requested = gloopRequestedMode[idx].load (std::memory_order_relaxed);
        }

        // Loop with no data is a silent lane, not an error.
        if (requested == (int) gloop::Mode::Loop && ! gloopGestureValid[idx])
        {
            requested = (int) gloop::Mode::Idle;
            gloopRequestedMode[idx].store (requested, std::memory_order_relaxed);
        }

        // A failed hand-off (queue full) is retried on the next tick. The lane
        // keeps its gesture, so nothing is lost.
        if (gloopEnginePrepared && gloopPublishPending[idx] && gloopGestureValid[idx])
        {
            const auto& gesture = gloopGestures[idx];
            if (engine.publishGloopGesture (lane, gesture.offsets.data(), gesture.count,
                                            gesture.durationSeconds))
                gloopPublishPending[idx] = false;
        }
    }
}

void NagloedProcessor::republishGloopGestures()
{
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        gloopPublishPending[idx] = false;

        if (! gloopGestureValid[idx])
            continue;

        if (! gloopEnginePrepared)
        {
            // The queue can be written before prepare, but the engine will reset
            // its playback state in prepare(); publishing now would just leave a
            // duplicate message behind. prepareToPlay() republishes instead.
            gloopPublishPending[idx] = true;
            continue;
        }

        const auto& gesture = gloopGestures[idx];
        if (! engine.publishGloopGesture (lane, gesture.offsets.data(), gesture.count,
                                          gesture.durationSeconds))
            gloopPublishPending[idx] = true;
    }
}

void NagloedProcessor::clearGloopState()
{
    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        gloopRecordingActive[idx] = false;
        gloopGestureValid[idx] = false;
        gloopPublishPending[idx] = false;
        gloopGestures[idx].count = 0;
        gloopGestures[idx].durationSeconds = 0.0f;
        gloopRequestedMode[idx].store ((int) gloop::Mode::Idle, std::memory_order_relaxed);
    }
}

juce::String NagloedProcessor::serialiseGloopState() const
{
    bool any = false;

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
        any = any || gloopGestureValid[(size_t) lane];

    if (! any)
        return {};

    juce::MemoryOutputStream out;
    out.writeInt (1);   // version

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        const auto& gesture = gloopGestures[idx];
        const bool valid = gloopGestureValid[idx] && gesture.count >= 2;

        // REC and FADE are transient performance gestures. Persist only the
        // stable musical states: READY (Idle + gesture) or LOOP.
        int mode = (int) gloop::Mode::Idle;

        if (valid && ! gloopRecordingActive[idx]
            && gloopRequestedMode[idx].load (std::memory_order_relaxed) == (int) gloop::Mode::Loop)
            mode = (int) gloop::Mode::Loop;

        out.writeInt (mode);
        out.writeInt (valid ? 1 : 0);
        out.writeInt (valid ? gesture.count : 0);
        out.writeFloat (valid ? gesture.durationSeconds : 0.0f);

        if (valid)
        {
            for (int i = 0; i < gesture.count; ++i)
            {
                const int quantised = juce::jlimit (-32767, 32767,
                                                    juce::roundToInt (gesture.offsets[(size_t) i]
                                                                      * 32767.0f));
                out.writeShort ((short) quantised);
            }
        }
    }

    return juce::Base64::toBase64 (out.getData(), out.getDataSize());
}

bool NagloedProcessor::deserialiseGloopState (const juce::String& text)
{
    if (text.isEmpty())
        return false;

    juce::MemoryOutputStream decoded;

    if (! juce::Base64::convertFromBase64 (decoded, text))
        return false;

    if (decoded.getDataSize() < sizeof (int))
        return false;

    juce::MemoryInputStream in (decoded.getData(), decoded.getDataSize(), false);

    if (in.readInt() != 1)
        return false;

    clearGloopState();

    for (int lane = 0; lane < gloop::kNumLanes; ++lane)
    {
        const auto idx = (size_t) lane;
        const int mode = in.readInt();
        const int valid = in.readInt();
        const int count = in.readInt();
        const float duration = in.readFloat();

        if (count < 0 || count > gloop::kMaxGestureSamples
            || in.getNumBytesRemaining() < (juce::int64) count * 2)
        {
            clearGloopState();
            return false;
        }

        auto& gesture = gloopGestures[idx];
        gesture.count = count;
        gesture.durationSeconds = duration;

        for (int i = 0; i < count; ++i)
            gesture.offsets[(size_t) i] = (float) in.readShort() / 32767.0f;

        gloopGestureValid[idx] = valid != 0 && count >= 2 && duration > 0.0f;

        // Only stable states survive; REC/FADE never start automatically on load.
        gloopRequestedMode[idx].store (
            gloopGestureValid[idx] && mode == (int) gloop::Mode::Loop
                ? (int) gloop::Mode::Loop
                : (int) gloop::Mode::Idle,
            std::memory_order_relaxed);

        gloopRecordingActive[idx] = false;
        gloopPublishPending[idx] = false;
    }

    republishGloopGestures();
    return true;
}

juce::AudioProcessorEditor* NagloedProcessor::createEditor()
{
#if defined (NAGLOED_HEADLESS) && NAGLOED_HEADLESS
    return nullptr;   // console tools link the processor without the GUI
#else
    return ui::createEditor (*this, apvts, *this, *this, *this).release();
#endif
}

int NagloedProcessor::getNumPrograms() { return presets::count(); }
int NagloedProcessor::getCurrentProgram() { return currentPreset; }

void NagloedProcessor::setCurrentProgram (int index)
{
    loadPreset (index);
}

const juce::String NagloedProcessor::getProgramName (int index)
{
    return presets::info (index).name;
}

void NagloedProcessor::loadPreset (int index)
{
    if (presets::applyTo (apvts, index))
        currentPreset = juce::jlimit (0, presets::count() - 1, index);
}

int NagloedProcessor::getNumPresets() const { return presets::count(); }
PresetInfo NagloedProcessor::getPresetInfo (int index) const { return presets::info (index); }

void NagloedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // copyState() flushes the APVTS parameter timer first. createCopy() can
    // serialise stale values when the host saves right after a parameter change
    // (caught by pluginval's setStateInformation test).
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset, nullptr);

    // Recorded gestures and READY/LOOP states are performance work, not preset
    // data; they travel with the session in a compact base64 blob. Omitted when
    // no lane has a take, so ordinary sessions keep their small state.
    const auto gloopState = serialiseGloopState();
    if (gloopState.isNotEmpty())
        state.setProperty ("gloopState", gloopState, nullptr);
    else
        state.removeProperty ("gloopState", nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NagloedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
            // Read before replaceState: the Gloop blob is a root property, and
            // an old session without one must clear this session's gestures.
            const auto gloopState = tree.getProperty ("gloopState").toString();

            apvts.replaceState (tree);

            if (gloopState.isNotEmpty())
                deserialiseGloopState (gloopState);
            else
                clearGloopState();

            // Push every restored value out to the host explicitly. A VST3 host
            // can cache an odd normalised value for a discrete parameter that it
            // previously sent; APVTS then skips notifying because its own cached
            // value already matches the restored one, leaving the host stuck on
            // the odd value (pluginval "Freeze not restored on setStateInformation").
            // APVTS stores parameters as <PARAM id="..." value="..."/> children.
            static const juce::Identifier paramType ("PARAM");
            static const juce::Identifier idProperty ("id");
            static const juce::Identifier valueProperty ("value");

            for (const auto& child : tree)
            {
                if (! child.hasType (paramType))
                    continue;

                auto* param = apvts.getParameter (child.getProperty (idProperty).toString());
                if (param == nullptr)
                    continue;

                const auto denormalised = (float) (double) child.getProperty (
                    valueProperty, param->getDefaultValue());
                param->setValueNotifyingHost (param->convertTo0to1 (denormalised));
            }

            currentPreset = juce::jlimit (0, presets::count() - 1,
                                          (int) tree.getProperty ("currentPreset", 0));
        }
    }
}

} // namespace nagloed

// -----------------------------------------------------------------------------
// Plugin entry point.
// -----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nagloed::NagloedProcessor();
}
