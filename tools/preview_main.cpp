// Renders the real editor offscreen for documentation, and mounts it the way a
// host does to regression-test that the visualisers keep repainting.
//
//   nagl-preview                 -> docs/ui-preview.png (1180x680 at 2x)
//   nagl-preview 2400 1500       -> custom size
//   nagl-preview --embedded      -> host-style mount test; prints EMBEDDED PASS/FAIL
//
// --embedded is what would have caught the 1.0.0 frozen-visualiser bug: it puts
// the editor inside a peer-owning parent component (like a DAW does), changes
// cutoff and LFO shape, and fails unless the EAR curve, LFO scope and grain field
// all repaint.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <memory>

#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "ui/EditorFactory.h"
#include "ui/GrainCloud.h"
#include "ui/NagloedEditor.h"

namespace
{
constexpr auto nagloedpid_cutoff   = "ear.cutoff";
constexpr auto nagloedpid_lfoShape = "mod.lfoShape";

void writePng (const juce::File& out, const juce::Image& image)
{
    out.getParentDirectory().createDirectory();
    out.deleteFile();   // JUCE's POSIX FileOutputStream appends

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());
    if (stream == nullptr || ! png.writeImageToStream (image, *stream))
        std::fprintf (stderr, "failed to write %s\n", out.getFullPathName().toRawUTF8());
}

bool hasArg (int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == flag)
            return true;
    return false;
}

juce::Component* findByTitle (juce::Component& root, const juce::String& title)
{
    if (root.getTitle() == title)
        return &root;

    for (auto* child : root.getChildren())
        if (child != nullptr)
            if (auto* found = findByTitle (*child, title))
                return found;

    return nullptr;
}

juce::Image snapshot (juce::Component& c)
{
    return c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
}

bool imagesDiffer (const juce::Image& a, const juce::Image& b)
{
    if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight())
        return true;

    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (a.getPixelAt (x, y).getARGB() != b.getPixelAt (x, y).getARGB())
                return true;

    return false;
}

void setParam (nagloed::NagloedProcessor& processor, const char* id, float value)
{
    if (auto* p = processor.getAPVTS().getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

// Plays a warm chord through the processor so the capture buffer fills and the
// grain field has something to show.
void feedChord (nagloed::NagloedProcessor& processor, double sampleRate, int blockSize,
                double seconds)
{
    static const float chordHz[] = { 110.0f, 130.81f, 164.81f, 220.0f, 261.63f };

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    const int totalSamples = (int) (seconds * sampleRate);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - start);
        buffer.clear();

        for (int s = 0; s < n; ++s)
        {
            const double t = (start + s) / sampleRate;
            float l = 0.0f, r = 0.0f;

            for (int note = 0; note < 5; ++note)
            {
                const auto phase = (float) (t * chordHz[note] * 2.0 * juce::MathConstants<double>::pi);
                const float detune = (note % 2 == 0) ? 1.0f : 1.0015f;
                const float swell = 0.6f + 0.4f * (float) std::sin (t * 0.7 + note);
                const float voice = 0.11f * swell * (std::sin (phase * detune)
                                                   + 0.35f * std::sin (phase * 2.0f * detune)
                                                   + 0.18f * std::sin (phase * 3.0f));
                l += voice * ((note % 2 == 0) ? 0.9f : 0.6f);
                r += voice * ((note % 2 == 0) ? 0.6f : 0.9f);
            }

            buffer.setSample (0, s, l);
            buffer.setSample (1, s, r);
        }

        processor.processBlock (buffer, midi);
    }
}

int runEmbeddedTest (nagloed::NagloedProcessor& processor,
                     std::unique_ptr<juce::AudioProcessorEditor>& editor,
                     int width, int height)
{
    // Mimic a host: the editor is a CHILD of a component that owns the peer, not
    // a peer of its own. isShowing() becomes true through the parent.
    juce::Component holder;
    holder.setSize (width, height);
    holder.addToDesktop (0);
    holder.setTopLeftPosition (80, 80);
    holder.setVisible (true);
    holder.addAndMakeVisible (*editor);
    editor->setBounds (0, 0, width, height);
    editor->setVisible (true);

    juce::MessageManager::getInstance()->runDispatchLoopUntil (500);

    std::printf ("embedded: visible=%d showing=%d peer=%d\n",
                 (int) editor->isVisible(), (int) editor->isShowing(),
                 editor->getPeer() != nullptr);

    auto* cloud = findByTitle (*editor, "Grain cloud");
    auto* curve = findByTitle (*editor, "EAR low-pass response");
    auto* lfo   = findByTitle (*editor, "LFO shape");

    bool ok = curve != nullptr && lfo != nullptr;

    if (cloud != nullptr)
    {
        const auto cloudBefore = snapshot (*cloud);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (350);
        const bool animates = imagesDiffer (cloudBefore, snapshot (*cloud));
        std::printf ("embedded: cloudAnimates=%d\n", (int) animates);
        ok = ok && animates;
    }

    if (curve != nullptr && lfo != nullptr)
    {
        const auto curveBefore = snapshot (*curve);
        const auto lfoBefore   = snapshot (*lfo);

        setParam (processor, nagloed::pid::cutoff, 185.786f);
        setParam (processor, nagloed::pid::lfoShape, 3.0f);   // Saw Down
        juce::MessageManager::getInstance()->runDispatchLoopUntil (700);

        const bool curveChanged = imagesDiffer (curveBefore, snapshot (*curve));
        const bool lfoChanged   = imagesDiffer (lfoBefore, snapshot (*lfo));
        std::printf ("embedded: curveChanged=%d lfoChanged=%d\n",
                     (int) curveChanged, (int) lfoChanged);
        ok = ok && curveChanged && lfoChanged;
    }

    holder.removeChildComponent (editor.get());
    std::printf ("%s\n", ok ? "EMBEDDED PASS" : "EMBEDDED FAIL");
    return ok ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const double sampleRate = 48000.0;
    const int blockSize = 512;

    const bool embedded = hasArg (argc, argv, "--embedded");
    const int width  = (argc > 1 && ! juce::String (argv[1]).startsWith ("--"))
                           ? juce::String (argv[1]).getIntValue() : 1180;
    const int height = (argc > 2 && ! juce::String (argv[2]).startsWith ("--"))
                           ? juce::String (argv[2]).getIntValue() : 680;

    nagloed::NagloedProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    if (! embedded)
    {
        // Documentation image: the instrument playing, not an idle instance.
        processor.loadPreset (7);   // 07 - Wolkenkaart
        setParam (processor, nagloed::pid::density,    150.0f);
        setParam (processor, nagloed::pid::size,       140.0f);
        setParam (processor, nagloed::pid::spray,       0.55f);
        setParam (processor, nagloed::pid::spaceMix,    0.72f);
        setParam (processor, nagloed::pid::macroGrain,  0.85f);
        setParam (processor, nagloed::pid::macroGlow,   0.80f);
        feedChord (processor, sampleRate, blockSize, 3.2);
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setSize (width, height);

    if (embedded)
        return runEmbeddedTest (processor, editor, width, height);

    editor->setVisible (true);

    // --headless-host: CI-safe variant of the host-mount regression. It performs
    // the same lifecycle a DAW does to the editor (hide/show while the component
    // has no peer, i.e. isShowing() stays false) and then checks that the timer
    // keeps running and the visualisers repaint. This is what caught the 1.0.0
    // bug; unlike --embedded it does not need a desktop window, so it works on
    // headless CI runners.
    if (hasArg (argc, argv, "--headless-host"))
    {
        auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get());
        if (nagloedEditor == nullptr)
        {
            std::fprintf (stderr, "headless-host: editor type mismatch\n");
            return 1;
        }

        // The lifecycle a DAW performs: hide/show while the component has no peer,
        // so isShowing() stays false. The 1.0.0 bug stopped the timer here for good.
        editor->setVisible (false);
        editor->setVisible (true);

        const bool timerRunning = nagloedEditor->isAnimationTimerRunningForTest();

        auto* cloud = findByTitle (*editor, "Grain cloud");
        auto* curve = findByTitle (*editor, "EAR low-pass response");
        auto* lfo   = findByTitle (*editor, "LFO shape");

        if (cloud == nullptr || curve == nullptr || lfo == nullptr)
        {
            std::fprintf (stderr, "headless-host: components not found\n");
            return 1;
        }

        // Deterministic frame stepping: independent of runner timer delivery.
        const auto cloudA = snapshot (*cloud);
        nagloedEditor->stepVisualsForPreview (30);
        const bool animates = imagesDiffer (cloudA, snapshot (*cloud));

        const auto curveBefore = snapshot (*curve);
        const auto lfoBefore   = snapshot (*lfo);

        setParam (processor, nagloedpid_cutoff, 185.786f);
        setParam (processor, nagloedpid_lfoShape, 3.0f);   // Saw Down
        nagloedEditor->stepVisualsForPreview (12);

        const bool curveChanged = imagesDiffer (curveBefore, snapshot (*curve));
        const bool lfoChanged   = imagesDiffer (lfoBefore, snapshot (*lfo));

        const bool ok = timerRunning && animates && curveChanged && lfoChanged;
        std::printf ("headless-host: visible=%d showing=%d timerRunning=%d cloudAnimates=%d curveChanged=%d lfoChanged=%d\n",
                     (int) editor->isVisible(), (int) editor->isShowing(), (int) timerRunning,
                     (int) animates, (int) curveChanged, (int) lfoChanged);
        std::printf ("%s\n", ok ? "HEADLESS-HOST PASS" : "HEADLESS-HOST FAIL");
        return ok ? 0 : 1;
    }

    // --soak renders after several seconds of live ticking, which exposes
    // phosphor/decay problems that a short snapshot hides.
    const bool soak = hasArg (argc, argv, "--soak");
    if (soak)
    {
        auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get());
        if (nagloedEditor == nullptr)
        {
            std::fprintf (stderr, "soak: editor type mismatch\n");
            return 1;
        }

        // Deterministic long run with audio flowing: alpha decay bugs, additive
        // saturation and flicker only show up after many frames. Stepping frames
        // explicitly keeps this independent of runner timer delivery.
        for (int i = 0; i < 480; ++i)
        {
            feedChord (processor, sampleRate, blockSize, 0.02);
            nagloedEditor->stepVisualsForPreview (1);
        }

        auto* cloud = findByTitle (*editor, "Grain cloud");
        if (cloud == nullptr)
        {
            std::fprintf (stderr, "soak: grain cloud component not found\n");
            return 1;
        }
        auto& target = *cloud;

        const auto a = target.createComponentSnapshot (target.getLocalBounds(), true, 1.0f);
        nagloedEditor->stepVisualsForPreview (20);
        const auto b = target.createComponentSnapshot (target.getLocalBounds(), true, 1.0f);

        // Measure inside the phosphor field only: the component also contains
        // header chrome and the timeline, which would dilute the ratios.
        auto field = juce::Rectangle<int> (a.getWidth(), a.getHeight()).toFloat();
        field = field.reduced (field.getWidth() * 0.10f, field.getHeight() * 0.06f);
        field.removeFromBottom (field.getHeight() * 0.30f);
        const auto fieldArea = field.toNearestInt().getIntersection (a.getBounds());

        double luminanceSum = 0.0;
        int whitePixels = 0, clippedPixels = 0, pixels = 0;
        for (int y = fieldArea.getY(); y < fieldArea.getBottom(); ++y)
            for (int x = fieldArea.getX(); x < fieldArea.getRight(); ++x)
            {
                const auto c = a.getPixelAt (x, y);
                luminanceSum += 0.2126 * c.getFloatRed() + 0.7152 * c.getFloatGreen()
                              + 0.0722 * c.getFloatBlue();
                if (c.getRed() > 235 && c.getGreen() > 235 && c.getBlue() > 235)
                    ++whitePixels;

                // A "clipped ember": red and green both pinned near max is the
                // pale-disc failure mode reviewers caught in 1.0.2 RC1.
                if (c.getRed() >= 250 && c.getGreen() >= 248)
                    ++clippedPixels;

                ++pixels;
            }

        const auto meanLuminance = pixels > 0 ? luminanceSum / pixels : 0.0;
        const auto whiteRatio = pixels > 0 ? (double) whitePixels / pixels : 1.0;
        const auto clippedRatio = pixels > 0 ? (double) clippedPixels / pixels : 1.0;
        const bool animates = imagesDiffer (a, b);

        const auto* gc = dynamic_cast<nagloed::ui::GrainCloud*> (cloud);
        const bool alphaOk = gc != nullptr && gc->alphaInvariantHeld();

        const auto soakFile = juce::File ("/tmp/nagl-soak.png");
        writePng (soakFile, editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f));

        std::printf ("soak: meanLuminance=%.2f whiteRatio=%.4f clippedRatio=%.5f animates=%d alphaOk=%d -> %s\n",
                     meanLuminance, whiteRatio, clippedRatio, (int) animates, (int) alphaOk,
                     soakFile.getFullPathName().toRawUTF8());

        // A washed-out/white field or a frozen cloud is a failure. Luminance is
        // 0..1; a healthy ember field is dark with a little glow (roughly
        // 0.02..0.35), and almost no pixel should be near pure white.
        const bool ok = alphaOk && whiteRatio < 0.01 && clippedRatio < 0.0002
                     && meanLuminance > 0.01 && meanLuminance < 0.4 && animates;
        std::printf ("%s\n", ok ? "SOAK PASS" : "SOAK FAIL");
        return ok ? 0 : 1;
    }

    // Let the editor's timers ingest the engine's visual state and repaint
    // before we snapshot, so the grain field is alive in the capture.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);

    // Deterministic documentation frame: no wall-clock timer, fixed frame count,
    // fixed RNG seed (the dust generator is seeded in GrainCloud).
    if (auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get()))
        nagloedEditor->advanceForPreview (240);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("docs")
                         .getChildFile ("ui-preview.png");
    writePng (out, image);

    std::printf ("wrote %s (%d x %d, preset %d \"%s\")\n",
                 out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight(),
                 processor.getCurrentPreset(),
                 processor.getProgramName (processor.getCurrentPreset()).toRawUTF8());
    return 0;
}
