#include "GloopProcessorTests.h"
#include "TestHarness.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/Gloop.h"
#include "core/Parameters.h"
#include "plugin/PluginProcessor.h"

namespace
{
using nagl_test::check;
using nagl_test::checkNear;
using nagl_test::info;

constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

void setReal (nagloed::NagloedProcessor& processor, const char* id, float value)
{
    if (auto* param = processor.getAPVTS().getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

nagloed::VisualState processBlock (nagloed::NagloedProcessor& processor)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock (buffer, midi);

    nagloed::VisualState visual;
    processor.copyVisual (visual);
    return visual;
}

void prepare (nagloed::NagloedProcessor& processor)
{
    processor.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);
}

// Records a rising Focus gesture over `seconds` at the recorder's 100 Hz grid.
// Returns the time after the take has been finalised.
double recordFocusGesture (nagloed::NagloedProcessor& processor, double startTime,
                           double seconds = 1.0)
{
    setReal (processor, nagloed::pid::focus, 0.5f);
    setReal (processor, nagloed::pid::gloopTarget[0], 1.0f);   // Focus
    setReal (processor, nagloed::pid::gloopAmount[0], 1.0f);
    setReal (processor, nagloed::pid::gloopSpeed[0], 1.0f);

    processor.setGloopMode (0, (int) nagloed::gloop::Mode::Record);
    processor.gloopTick (startTime);

    const int steps = (int) std::lround (seconds * 100.0);

    for (int i = 1; i <= steps; ++i)
    {
        const float focus = 0.5f + 0.4f * (float) i / (float) steps;
        setReal (processor, nagloed::pid::focus, focus);
        processor.gloopTick (startTime + (double) i * 0.01);
    }

    processor.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);
    processor.gloopTick (startTime + seconds + 0.01);

    return startTime + seconds + 0.01;
}
} // namespace

void runGloopProcessorTests()
{
    info ("Gloop processor / recorder tests");

    // --- Parameter contract --------------------------------------------------
    {
        nagloed::NagloedProcessor processor;
        auto& apvts = processor.getAPVTS();

        check (nagloed::gloopTargetNames().size() == nagloed::gloop::kNumTargetChoices,
               "Gloop target choice list matches the target choices");
        check (nagloed::gloopTargetNames()[nagloed::gloop::kAutoTarget]
                   == "Multi",
               "the Multi target is appended without shifting old choices");

        for (int lane = 0; lane < nagloed::gloop::kNumLanes; ++lane)
        {
            check (apvts.getParameter (nagloed::pid::gloopTarget[lane]) != nullptr,
                   std::string ("Gloop target parameter resolves for lane ")
                       + std::to_string (lane + 1));
            check (apvts.getParameter (nagloed::pid::gloopAmount[lane]) != nullptr,
                   std::string ("Gloop amount parameter resolves for lane ")
                       + std::to_string (lane + 1));
            check (apvts.getParameter (nagloed::pid::gloopSpeed[lane]) != nullptr,
                   std::string ("Gloop speed parameter resolves for lane ")
                       + std::to_string (lane + 1));
        }

        check (apvts.getParameter (nagloed::pid::gloopSmooth) != nullptr,
               "Gloop smoothing parameter resolves");

        for (int target = 1; target < nagloed::gloop::kNumTargets; ++target)
        {
            const auto* id = nagloed::gloopTargetParamId (target);
            check (id != nullptr && apvts.getParameter (id) != nullptr,
                   std::string ("Gloop ring target resolves: ")
                       + nagloed::gloopTargetName (target));
        }

        check (nagloed::gloop::kNumLanes == 8, "Gloop exposes eight lanes");
        checkNear (nagloed::gloop::kMinSpeed, 0.25, 1.0e-6, "Gloop minimum speed is 0.25x");
        checkNear (nagloed::gloop::kMaxSpeed, 2.0, 1.0e-6, "Gloop maximum speed is 2x");
    }

    // --- Empty/Off lane is silent, not an error ------------------------------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        setReal (processor, nagloed::pid::gloopTarget[0], 0.0f);   // Off
        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);

        double t = 10.0;
        for (int i = 0; i < 4; ++i)
        {
            processor.gloopTick (t);
            t += 0.01;
        }

        auto visual = processBlock (processor);
        check (visual.gloop.lanes[0].mode == (int) nagloed::gloop::Mode::Idle,
               "Loop with no gesture settles to Idle");
        checkNear (visual.gloop.lanes[0].offset, 0.0, 1.0e-6,
                   "empty Gloop lane contributes zero offset");
        checkNear (visual.gloop.lanes[0].amount, 0.0, 1.0e-6,
                   "empty Gloop lane contributes zero amount");
    }

    // --- A preset resetting the target to Off does not trap a looping lane ----
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        recordFocusGesture (processor, 50.0);

        // Init and some presets reset gloop.N.target to Off while the lane is
        // still in Loop. The lane must become silent and READY (target combo
        // usable again) without losing the recorded take.
        setReal (processor, nagloed::pid::gloopTarget[0], 0.0f);
        processor.gloopTick (52.0);

        check (processor.getGloopMode (0) == (int) nagloed::gloop::Mode::Idle,
               "target Off settles a looping lane to Idle");
        check (processor.hasGloopGesture (0),
               "target Off keeps the recorded take for a later target");

        for (int block = 0; block < 4; ++block)
        {
            const auto visual = processBlock (processor);
            check (visual.gloop.lanes[0].mode == (int) nagloed::gloop::Mode::Idle,
                   "engine reports Idle for an Off-target lane");
            checkNear (visual.gloop.lanes[0].offset, 0.0, 1.0e-6,
                       "an Off-target lane contributes zero offset");
        }
    }

    // --- Record, loop and live-centre playback -------------------------------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        const auto afterTake = recordFocusGesture (processor, 100.0);
        check (processor.hasGloopGesture (0), "recording produces a gesture");
        check (processor.getGloopMode (0) == (int) nagloed::gloop::Mode::Loop,
               "REC then STOP leaves the lane in Loop");

        // Play with the knob centre at 0.5; the recorded rise must orbit it.
        setReal (processor, nagloed::pid::focus, 0.5f);

        float maxFocus = 0.0f;
        float minFocus = 1.0f;
        float firstPhase = -1.0f;
        bool phaseAdvanced = false;

        for (int block = 0; block < 160; ++block)
        {
            const auto visual = processBlock (processor);
            processor.gloopTick (afterTake + 0.05 + (double) block * 0.01);

            if (visual.gloop.lanes[0].mode != (int) nagloed::gloop::Mode::Loop)
                continue;

            checkNear (visual.gloop.lanes[0].centreNorm, 0.5, 0.03,
                       "Gloop reports the live Focus centre");

            if (firstPhase < 0.0f)
                firstPhase = visual.gloop.lanes[0].phase;
            else if (std::abs (visual.gloop.lanes[0].phase - firstPhase) > 0.01f)
                phaseAdvanced = true;

            maxFocus = std::max (maxFocus, visual.focus);
            minFocus = std::min (minFocus, visual.focus);
        }

        check (phaseAdvanced, "looping phase advances independently");
        check (maxFocus > 0.55f, "recorded Focus gesture moves the live parameter upward");
        check (minFocus >= 0.0f && maxFocus <= 1.0f, "Gloop output stays bounded");

        // Amount 0 must silence the lane without destroying its phase.
        setReal (processor, nagloed::pid::gloopAmount[0], 0.0f);

        // The offset glide is ~40 ms, so allow it to settle before asserting;
        // the first blocks after the change still carry the old influence.
        float amountZeroLast = 1.0f;
        for (int block = 0; block < 60; ++block)
        {
            const auto visual = processBlock (processor);
            amountZeroLast = std::abs (visual.gloop.lanes[0].offset);
        }

        check (amountZeroLast < 0.01f, "Amount 0 withdraws the lane's modulation");

        // Speed changes apply while looping and do not restart the take.
        setReal (processor, nagloed::pid::gloopAmount[0], 1.0f);
        setReal (processor, nagloed::pid::gloopSpeed[0], 2.0f);

        const auto beforeSpeed = processBlock (processor).gloop.lanes[0].phase;
        auto afterSpeed = beforeSpeed;
        for (int block = 0; block < 20; ++block)
            afterSpeed = processBlock (processor).gloop.lanes[0].phase;

        check (afterSpeed != beforeSpeed, "speed change keeps the lane moving");
        checkNear (processor.getAPVTS().getRawParameterValue (nagloed::pid::gloopSpeed[0])->load(),
                   2.0, 0.01, "speed parameter is live at playback");
    }

    // --- Fade withdraws influence and returns to READY -----------------------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        const auto afterTake = recordFocusGesture (processor, 200.0);

        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Fade);

        double t = afterTake + 0.02;
        int actualIdle = -1;
        float maxOffsetAfterIdle = 0.0f;

        for (int block = 0; block < 260; ++block)
        {
            const auto visual = processBlock (processor);
            processor.gloopTick (t);
            t += 0.0107;

            if (visual.gloop.lanes[0].mode == (int) nagloed::gloop::Mode::Idle)
            {
                if (actualIdle < 0)
                    actualIdle = block;
                maxOffsetAfterIdle = std::max (maxOffsetAfterIdle,
                                               std::abs (visual.gloop.lanes[0].offset));
            }
        }

        check (actualIdle >= 0, "Fade completes and the engine reports Idle");
        check (maxOffsetAfterIdle < 0.01f, "an Idle lane contributes no offset");
        check (processor.getGloopMode (0) == (int) nagloed::gloop::Mode::Idle,
               "a completed Fade settles the lane to Idle/READY");
        check (processor.hasGloopGesture (0),
               "the take survives a Fade so the lane is READY");
    }

    // --- Motionless take is silent but valid ---------------------------------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        setReal (processor, nagloed::pid::focus, 0.5f);
        setReal (processor, nagloed::pid::gloopTarget[0], 1.0f);
        setReal (processor, nagloed::pid::gloopAmount[0], 1.0f);

        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Record);
        processor.gloopTick (300.0);

        for (int i = 1; i <= 100; ++i)
            processor.gloopTick (300.0 + (double) i * 0.01);   // no knob movement

        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);
        processor.gloopTick (301.02);

        check (processor.hasGloopGesture (0), "a motionless take is still a valid take");

        float maxOffset = 0.0f;
        for (int block = 0; block < 80; ++block)
            maxOffset = std::max (maxOffset,
                                  std::abs (processBlock (processor).gloop.lanes[0].offset));

        check (maxOffset < 0.01f, "a motionless take contributes zero, not invented motion");
    }

    // --- Session state round-trip -------------------------------------------
    {
        nagloed::NagloedProcessor original;
        prepare (original);

        recordFocusGesture (original, 400.0);
        check (original.hasGloopGesture (0), "state test has a take to save");

        juce::MemoryBlock saved;
        original.getStateInformation (saved);
        check (saved.getSize() > 0, "Gloop state serialises with the session");

        nagloed::NagloedProcessor restored;
        // Restore before prepare: many hosts call setStateInformation first.
        restored.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        check (restored.hasGloopGesture (0), "gesture is recalled from session state");
        check (restored.getGloopMode (0) == (int) nagloed::gloop::Mode::Loop,
               "Loop state is recalled from session state");

        restored.prepareToPlay (kSampleRate, kBlockSize);

        auto restoredVisual = processBlock (restored);
        check (restoredVisual.gloop.lanes[0].mode == (int) nagloed::gloop::Mode::Loop,
               "restored lane plays after prepareToPlay");
        check (restoredVisual.gloop.lanes[0].hasGesture,
               "restored visual reports the gesture");
    }

    // --- Multi: one REC captures several knobs, played back together ---------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        setReal (processor, nagloed::pid::focus, 0.50f);
        setReal (processor, nagloed::pid::spray, 0.25f);
        setReal (processor, nagloed::pid::gloopTarget[0],
                 (float) nagloed::gloop::kAutoTarget);
        setReal (processor, nagloed::pid::gloopAmount[0], 1.0f);
        setReal (processor, nagloed::pid::gloopSpeed[0], 1.0f);

        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Record);
        processor.gloopTick (600.0);

        // Sequential gesture: Focus first, then (while holding Focus) Spray.
        for (int i = 1; i <= 60; ++i)
        {
            setReal (processor, nagloed::pid::focus, 0.50f + 0.004f * (float) i);
            processor.gloopTick (600.0 + (double) i * 0.01);
        }

        for (int i = 1; i <= 40; ++i)
        {
            setReal (processor, nagloed::pid::spray, 0.25f + 0.010f * (float) i);
            processor.gloopTick (600.6 + (double) i * 0.01);
        }

        processor.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);
        processor.gloopTick (601.01);

        check (processor.hasGloopGesture (0), "multi: the take is valid");
        check (processor.getGloopMode (0) == (int) nagloed::gloop::Mode::Loop,
               "multi: the lane loops after the take");

        auto visual = processBlock (processor);
        check (visual.gloop.lanes[0].target == nagloed::gloop::kAutoTarget,
               "multi: the lane reports the Multi target");
        check (visual.gloop.lanes[0].numTracks == 2,
               "multi: two moved knobs produced two tracks");

        bool sawFocus = false;
        bool sawSpray = false;

        for (int t = 0; t < visual.gloop.lanes[0].numTracks; ++t)
        {
            const auto& track = visual.gloop.lanes[0].tracks[(size_t) t];
            sawFocus = sawFocus || track.target == (int) nagloed::gloop::Target::Focus;
            sawSpray = sawSpray || track.target == (int) nagloed::gloop::Target::Spray;
        }

        check (sawFocus, "multi: the Focus movement is a track");
        check (sawSpray, "multi: the Spray movement is a track");

        // Both tracks share the lane phase and play together.
        float maxFocusOffset = 0.0f;
        float maxSprayOffset = 0.0f;

        for (int block = 0; block < 160; ++block)
        {
            visual = processBlock (processor);

            if (visual.gloop.lanes[0].numTracks == 2)
            {
                maxFocusOffset = std::max (maxFocusOffset,
                                            std::abs (visual.gloop.lanes[0].tracks[0].offset));
                maxSprayOffset = std::max (maxSprayOffset,
                                            std::abs (visual.gloop.lanes[0].tracks[1].offset));
            }
        }

        check (maxFocusOffset > 0.02f, "multi: Focus moves during playback");
        check (maxSprayOffset > 0.02f, "multi: Spray moves during playback");

        // Version-2 session state must carry both tracks.
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);

        nagloed::NagloedProcessor restored;
        restored.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        check (restored.hasGloopGesture (0), "multi: the take is recalled");
        restored.prepareToPlay (kSampleRate, kBlockSize);

        const auto restoredVisual = processBlock (restored);
        check (restoredVisual.gloop.lanes[0].numTracks == 2,
               "multi: both tracks are recalled from session state");
    }

    // --- Multi with no movement stays empty; the fourth-track cap holds -----
    {
        nagloed::NagloedProcessor silent;
        prepare (silent);

        setReal (silent, nagloed::pid::gloopTarget[0],
                 (float) nagloed::gloop::kAutoTarget);
        silent.setGloopMode (0, (int) nagloed::gloop::Mode::Record);
        silent.gloopTick (700.0);

        for (int i = 1; i <= 30; ++i)
            silent.gloopTick (700.0 + (double) i * 0.01);   // no knob movement

        silent.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);
        silent.gloopTick (700.31);

        check (! silent.hasGloopGesture (0),
               "multi: a take with no movement is empty, not invented");
        check (silent.getGloopMode (0) == (int) nagloed::gloop::Mode::Idle,
               "multi: an empty Multi take settles to Idle");
    }

    {
        nagloed::NagloedProcessor cap;
        prepare (cap);

        setReal (cap, nagloed::pid::gloopTarget[0],
                 (float) nagloed::gloop::kAutoTarget);
        setReal (cap, nagloed::pid::gloopAmount[0], 1.0f);
        setReal (cap, nagloed::pid::gloopSpeed[0], 1.0f);
        cap.setGloopMode (0, (int) nagloed::gloop::Mode::Record);
        cap.gloopTick (800.0);

        const auto moveFor = [&cap] (const char* id, float from, float to,
                                     double start, int steps)
        {
            for (int i = 1; i <= steps; ++i)
            {
                setReal (cap, id, from + (to - from) * (float) i / (float) steps);
                cap.gloopTick (start + (double) i * 0.01);
            }
        };

        moveFor (nagloed::pid::focus,  0.50f, 0.85f, 800.00, 20);
        moveFor (nagloed::pid::spray,  0.25f, 0.85f, 800.20, 20);
        moveFor (nagloed::pid::width,  0.50f, 0.90f, 800.40, 20);
        moveFor (nagloed::pid::shape,  0.15f, 0.90f, 800.60, 20);
        moveFor (nagloed::pid::tone,   0.50f, 0.90f, 800.80, 20);

        cap.setGloopMode (0, (int) nagloed::gloop::Mode::Loop);
        cap.gloopTick (801.01);

        const auto visual = processBlock (cap);
        check (visual.gloop.lanes[0].numTracks == 4,
               "multi: the fourth-track cap is enforced");

        bool sawTone = false;

        for (int t = 0; t < visual.gloop.lanes[0].numTracks; ++t)
            sawTone = sawTone || visual.gloop.lanes[0].tracks[(size_t) t].target
                                     == (int) nagloed::gloop::Target::Tone;

        check (! sawTone, "multi: the fifth moved target is ignored");
    }

    // --- Version-1 session blobs still load as a single track ---------------
    {
        nagloed::NagloedProcessor legacy;
        setReal (legacy, nagloed::pid::gloopTarget[0], 1.0f);   // Focus

        juce::MemoryOutputStream blob;
        blob.writeInt (1);   // version 1

        for (int lane = 0; lane < nagloed::gloop::kNumLanes; ++lane)
        {
            const bool hasTake = lane == 0;
            blob.writeInt (hasTake ? 2 : 0);   // Loop / Idle
            blob.writeInt (hasTake ? 1 : 0);   // valid
            blob.writeInt (hasTake ? 100 : 0); // count
            blob.writeFloat (hasTake ? 1.0f : 0.0f);

            if (hasTake)
            {
                for (int i = 0; i < 100; ++i)
                    blob.writeShort ((short) juce::roundToInt (0.25f * 32767.0f));
            }
        }

        auto state = legacy.getAPVTS().copyState();
        state.setProperty ("gloopState",
                           juce::Base64::toBase64 (blob.getData(), blob.getDataSize()),
                           nullptr);

        juce::MemoryBlock saved;

        if (auto xml = state.createXml())
            legacy.copyXmlToBinary (*xml, saved);

        nagloed::NagloedProcessor restored;
        restored.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
        restored.setStateInformation (saved.getData(), (int) saved.getSize());

        check (restored.hasGloopGesture (0),
               "legacy: a version-1 take loads as a single track");
        check (restored.getGloopMode (0) == (int) nagloed::gloop::Mode::Loop,
               "legacy: the version-1 Loop state is recalled");

        restored.prepareToPlay (kSampleRate, kBlockSize);

        float maxOffset = 0.0f;
        int   numTracks = 0;
        int   trackTarget = (int) nagloed::gloop::Target::Off;

        for (int block = 0; block < 40; ++block)
        {
            const auto visual = processBlock (restored);
            numTracks = visual.gloop.lanes[0].numTracks;

            if (numTracks > 0)
            {
                trackTarget = visual.gloop.lanes[0].tracks[0].target;
                maxOffset = std::max (maxOffset,
                                      std::abs (visual.gloop.lanes[0].tracks[0].offset));
            }
        }

        check (numTracks == 1, "legacy: the old take has one track");
        check (trackTarget == (int) nagloed::gloop::Target::Focus,
               "legacy: the old take keeps the lane's target");
        check (maxOffset > 0.05f, "legacy: the old take still moves the target");
    }

    // --- Randomize is gentle and leaves idle lanes alone ---------------------
    {
        nagloed::NagloedProcessor processor;
        prepare (processor);

        const auto afterTake = recordFocusGesture (processor, 500.0);
        setReal (processor, nagloed::pid::focus, 0.5f);

        // Let the loop settle.
        for (int block = 0; block < 20; ++block)
            (void) processBlock (processor);

        const auto before = processBlock (processor).gloop.lanes[0].amount;

        processor.gloopRandomize();
        const auto immediate = processBlock (processor).gloop.lanes[0].amount;
        check (std::abs (immediate - before) < 0.02f,
               "Randomize does not jerk the effective amount");

        float maxStep = 0.0f;
        float last = immediate;
        for (int block = 0; block < 240; ++block)
        {
            const auto visual = processBlock (processor);
            processor.gloopTick (afterTake + 0.1 + (double) block * 0.0107);

            const auto now = visual.gloop.lanes[0].amount;
            maxStep = std::max (maxStep, std::abs (now - last));
            last = now;

            checkNear (visual.gloop.lanes[1].amount, 0.0, 1.0e-6,
                       "Randomize never starts an idle lane");
        }

        check (maxStep < 0.02f, "Randomize glides instead of jumping");
        check (std::abs (last - before) > 0.005f,
               "Randomize changes the lane's depth or speed relationship over time");
    }
}
