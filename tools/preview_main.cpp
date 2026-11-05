// Renders the real editor offscreen for documentation, and mounts it the way a
// host does to regression-test that the visualisers keep repainting.
//
//   nagl-preview                 -> docs/ui-preview.png (1180x680 at 2x,
//                                   deterministic for a given build/platform)
//   nagl-preview 2400 1500       -> custom size
//   nagl-preview --embedded      -> windowed host-style mount test (local; needs a
//                                   desktop window server); prints EMBEDDED PASS/FAIL
//   nagl-preview --headless-host -> CI-safe version of --embedded: performs the DAW
//                                   hide/show lifecycle without a peer, asserts the
//                                   animation timer survived and steps fixed visual
//                                   frames; prints HEADLESS-HOST PASS/FAIL
//   nagl-preview --soak          -> 8 s phosphor regression, frame-stepped with audio
//                                   flowing; prints SOAK PASS/FAIL
//   nagl-preview --frames <dir>  -> deterministic 8 s musical scenario (dense ->
//                                   sparse -> near silence -> return) stepped at 60 Hz;
//                                   writes grain-field frames + per-frame metrics and a
//                                   metrics.csv into <dir>; used for issue #32 before/
//                                   after evidence (same harness, old and new renderer)
//   nagl-preview --motion        -> deterministic motion-QA checks with PASS/FAIL
//                                   (needs the GrainCloud QA hooks, issue #32)
//   nagl-preview --gloop         -> docs/gloop-preview.png: the Gloop overlay open,
//                                   with a fixed synthetic lane state so the layout
//                                   and the target-knob rings are reviewable (issue #33)
//
// --embedded/--headless-host catch the 1.0.0 frozen-visualiser bug; --soak catches
// the 1.0.2 alpha-decay/white-disc bug.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

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
// grain field has something to show. `startTime` keeps the oscillator phase
// continuous when the harness feeds the scenario in fixed per-frame chunks.
void feedChordFrom (nagloed::NagloedProcessor& processor, double sampleRate, int blockSize,
                    double startTime, double seconds)
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
            const double t = startTime + (start + s) / sampleRate;
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

void feedChord (nagloed::NagloedProcessor& processor, double sampleRate, int blockSize,
                double seconds)
{
    feedChordFrom (processor, sampleRate, blockSize, 0.0, seconds);
}

// Advances the engine through `seconds` of digital silence. The engine must keep
// running (the buffer has to play out and the visual snapshot has to keep
// updating) even though no new material is arriving.
void feedSilence (nagloed::NagloedProcessor& processor, double sampleRate, int blockSize,
                  double seconds)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    const int totalSamples = (int) (seconds * sampleRate);

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - start);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }
}

// =============================================================================
// Issue #32 evidence harness: deterministic scenario capture and metrics.
//
// `--frames <dir>` plays a fixed 8 s musical scenario (dense -> sparse -> near
// silence -> return) with frame-stepped, timer-free presentation and records what
// the rendered grain field actually looks like frame by frame. It is deliberately
// renderer-agnostic: the same binary/model is run on the pre-change renderer
// (baseline) and the post-change renderer, so the numbers are comparable.
// =============================================================================
juce::String getOptionValue (int argc, char** argv, const juce::String& option)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == option)
            return juce::String (argv[i + 1]);

    return {};
}

// The grain field is the GrainCloud's only opaque child (its phosphor screen);
// everything else in the panel is chrome and ribbon controls. Snapshotting the
// screen directly gives the field pixels without header/timeline dilution.
juce::Component* findFieldComponent (juce::Component& cloud)
{
    juce::Component* best = nullptr;
    long bestArea = 0;

    for (auto* child : cloud.getChildren())
    {
        if (child == nullptr || ! child->isOpaque())
            continue;

        const auto area = (long) child->getWidth() * (long) child->getHeight();

        if (area > bestArea)
        {
            bestArea = area;
            best = child;
        }
    }

    return best;
}

constexpr double kBrightLuminance = 0.10;   // ember threshold above the 0x05070a bed
constexpr double kActiveDelta      = 0.02;  // |luminance delta| that counts as motion

struct FieldMetrics
{
    int    pixels = 0;
    int    brightPixels = 0;
    int    activePixels = 0;                  // pixels that changed vs previous frame
    double meanLum = 0.0;
    double centroidX = 0.0, centroidY = 0.0;   // bright-pixel centroid, field pixels
    double centroidDelta = 0.0;               // centroid movement vs previous frame, px
    double meanAbsDelta = 0.0;                // mean |luminance delta| vs previous frame
    double coverage = 0.0;                    // brightPixels / pixels
    double activeCoverage = 0.0;              // activePixels / pixels (temporal activity)
};

FieldMetrics measureField (const juce::Image& image, const std::vector<float>* previous,
                           std::vector<float>& luminanceOut)
{
    FieldMetrics m;

    if (! image.isValid())
        return m;

    const int w = image.getWidth(), h = image.getHeight();
    const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
    m.pixels = w * h;
    luminanceOut.assign ((size_t) m.pixels, 0.0f);

    const bool usePrevious = previous != nullptr && (int) previous->size() == m.pixels;

    double lumSum = 0.0, brightXSum = 0.0, brightYSum = 0.0, deltaSum = 0.0;
    int idx = 0;

    for (int y = 0; y < h; ++y)
    {
        const auto* line = data.getLinePointer (y);

        for (int x = 0; x < w; ++x, ++idx)
        {
            const auto* p = line + (ptrdiff_t) x * data.pixelStride;

            // ARGB images store the bytes little-endian as b, g, r, a.
           #if JUCE_BIG_ENDIAN
            const double b = p[3] / 255.0, g = p[2] / 255.0, r = p[1] / 255.0;
           #else
            const double b = p[0] / 255.0, g = p[1] / 255.0, r = p[2] / 255.0;
           #endif
            const double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;

            luminanceOut[(size_t) idx] = (float) lum;
            lumSum += lum;

            if (lum >= kBrightLuminance)
            {
                ++m.brightPixels;
                brightXSum += x + 0.5;
                brightYSum += y + 0.5;
            }

            if (usePrevious)
            {
                const double change = std::abs (lum - (double) (*previous)[(size_t) idx]);
                deltaSum += change;

                if (change >= kActiveDelta)
                    ++m.activePixels;
            }
        }
    }

    if (m.pixels > 0)
    {
        m.meanLum = lumSum / m.pixels;
        m.coverage = (double) m.brightPixels / m.pixels;
        m.activeCoverage = (double) m.activePixels / m.pixels;

        if (m.brightPixels > 0)
        {
            m.centroidX = brightXSum / m.brightPixels;
            m.centroidY = brightYSum / m.brightPixels;
        }

        if (usePrevious)
            m.meanAbsDelta = deltaSum / m.pixels;
    }

    return m;
}

int runFrameCapture (nagloed::NagloedProcessor& processor, juce::AudioProcessorEditor& editor,
                     const juce::String& directory, double sampleRate, int blockSize)
{
    auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (&editor);
    if (nagloedEditor == nullptr)
    {
        std::fprintf (stderr, "frames: editor type mismatch\n");
        return 1;
    }

    auto* cloud = dynamic_cast<nagloed::ui::GrainCloud*> (findByTitle (editor, "Grain cloud"));
    if (cloud == nullptr)
    {
        std::fprintf (stderr, "frames: grain cloud component not found\n");
        return 1;
    }

    auto* field = findFieldComponent (*cloud);
    if (field == nullptr)
    {
        std::fprintf (stderr, "frames: grain-field screen child not found\n");
        return 1;
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto outDir = juce::File::isAbsolutePath (directory) ? juce::File (directory)
                                                               : cwd.getChildFile (directory);
    const auto fieldDir  = outDir.getChildFile ("field");
    const auto uiDir     = outDir.getChildFile ("ui");
    const auto stillsDir = outDir.getChildFile ("stills");
    fieldDir.createDirectory();
    uiDir.createDirectory();
    stillsDir.createDirectory();

    // Timer-free deterministic run: reset the particle RNG / phosphor, then step
    // one fixed 1/60 s visual frame per fed audio chunk.
    nagloedEditor->advanceForPreview (0);

    constexpr int    kFrames     = 480;   // 8 s at 60 fps
    constexpr double kDt         = 1.0 / 60.0;
    constexpr int    kDenseEnd   = 144;   // 0.0 - 2.4 s: dense material
    constexpr int    kSparseEnd  = 264;   // 2.4 - 4.4 s: sparse material
    constexpr int    kSilenceEnd = 384;   // 4.4 - 6.4 s: near silence (no audio fed)
    //                                     6.4 - 8.0 s: short return

    // Preset 07 "Wolkenkaart" is loaded by main() before the editor is created.
    setParam (processor, nagloed::pid::density, 150.0f);
    setParam (processor, nagloed::pid::size,    140.0f);
    setParam (processor, nagloed::pid::spray,    0.55f);

    static const char* phaseNames[4] = { "dense", "sparse", "silence", "return" };

    struct PhaseAccum
    {
        const char* name = "";
        double lum = 0.0, delta = 0.0, coverage = 0.0, activeCoverage = 0.0, grains = 0.0;
        int frames = 0;
        std::vector<double> deltas;   // per-frame meanAbsDelta, for median/p95
    };

    PhaseAccum phases[4] = { { "dense" }, { "sparse" }, { "silence" }, { "return" } };

    nagloed::VisualState visual;
    std::vector<float> previousLum, currentLum;
    FieldMetrics previousMetrics;
    bool havePrevious = false;

    juce::StringArray csv;
    csv.add ("frame,time_s,phase,numGrains,inRms,outRms,meanLum,centroidX,centroidY,"
             "centroidDelta,meanAbsDelta,coverage,activeCoverage");

    const auto startedMs = juce::Time::getMillisecondCounterHiRes();

    for (int frame = 0; frame < kFrames; ++frame)
    {
        if (frame == kDenseEnd)
            setParam (processor, nagloed::pid::density, 6.0f);
        else if (frame == kSparseEnd)
            setParam (processor, nagloed::pid::density, 0.5f);
        else if (frame == kSilenceEnd)
            setParam (processor, nagloed::pid::density, 150.0f);

        const int phaseIndex = frame < kDenseEnd   ? 0
                             : frame < kSparseEnd  ? 1
                             : frame < kSilenceEnd ? 2
                                                   : 3;

        // Fixed-size audio chunks, one per visual frame: 8 s of music, then
        // digital silence (the engine still runs so grains ring out and die).
        if (phaseIndex == 2)
            feedSilence (processor, sampleRate, blockSize, kDt);
        else
            feedChordFrom (processor, sampleRate, blockSize, frame * kDt, kDt);

        processor.copyVisual (visual);
        nagloedEditor->stepVisualsForPreview (1, kDt);

        const auto fieldImage = snapshot (*field);
        auto m = measureField (fieldImage, havePrevious ? &previousLum : nullptr, currentLum);
        previousLum.swap (currentLum);

        if (havePrevious && m.brightPixels > 0 && previousMetrics.brightPixels > 0)
            m.centroidDelta = std::hypot (m.centroidX - previousMetrics.centroidX,
                                          m.centroidY - previousMetrics.centroidY);

        // Lean output: every third frame (60/3 = 20 fps, enough for the GIF) for
        // the field and full-UI sequences; full-resolution stills only at four
        // representative frames.
        if ((frame % 3) == 0)
        {
            writePng (fieldDir.getChildFile (juce::String::formatted ("frame_%04d.png", frame)),
                      fieldImage);
            writePng (uiDir.getChildFile (juce::String::formatted ("frame_%04d.png", frame)),
                      snapshot (editor));
        }

        if (frame == 60 || frame == 210 || frame == 330 || frame == 450)
            writePng (stillsDir.getChildFile (juce::String::formatted ("still_%04d.png", frame)),
                      editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f));

        csv.add (juce::String::formatted ("%d,%.4f,%s,%d,%.5f,%.5f,%.6f,%.2f,%.2f,%.4f,%.6f,%.5f,%.5f",
                                          frame, frame * kDt, phaseNames[phaseIndex],
                                          visual.numGrains, visual.inL, visual.outL,
                                          m.meanLum, m.centroidX, m.centroidY,
                                          m.centroidDelta, m.meanAbsDelta, m.coverage,
                                          m.activeCoverage));

        auto& phase = phases[phaseIndex];
        phase.lum += m.meanLum;
        phase.delta += m.meanAbsDelta;
        phase.coverage += m.coverage;
        phase.activeCoverage += m.activeCoverage;
        phase.grains += visual.numGrains;
        ++phase.frames;

        if (havePrevious)
            phase.deltas.push_back (m.meanAbsDelta);

        std::printf ("frame %3d t=%5.2fs %-7s grains=%3d in=%.3f out=%.3f "
                     "meanLum=%.4f centroid=(%6.1f,%6.1f) dCentroid=%.3f "
                     "meanAbsDelta=%.5f coverage=%.4f active=%.4f\n",
                     frame, frame * kDt, phaseNames[phaseIndex], visual.numGrains,
                     (double) visual.inL, (double) visual.outL, m.meanLum,
                     m.centroidX, m.centroidY, m.centroidDelta, m.meanAbsDelta, m.coverage,
                     m.activeCoverage);

        previousMetrics = m;
        havePrevious = true;
    }

    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - startedMs;
    csv.add ("");
    outDir.getChildFile ("metrics.csv").replaceWithText (csv.joinIntoString ("\n") + "\n");

    std::printf ("\nframes: field %d x %d logical px, ui %d x %d, %d frames @ 60 fps "
                 "(%.2f s of scenario)\n",
                 field->getWidth(), field->getHeight(), editor.getWidth(), editor.getHeight(),
                 kFrames, kFrames * kDt);

    for (const auto& phase : phases)
    {
        if (phase.frames == 0)
            continue;

        // Median/p95 frame delta separate a steady moving field (every frame
        // changes) from a static one that only blips when grain identity changes.
        double medianDelta = 0.0, p95Delta = 0.0, staticFraction = 0.0;

        if (! phase.deltas.empty())
        {
            auto sorted = phase.deltas;
            std::sort (sorted.begin(), sorted.end());
            medianDelta = sorted[sorted.size() / 2];
            p95Delta = sorted[juce::jmin (sorted.size() - 1,
                                          (size_t) std::floor (sorted.size() * 0.95))];

            int staticFrames = 0;
            for (auto d : sorted)
                if (d < 1.0e-4)
                    ++staticFrames;

            staticFraction = (double) staticFrames / sorted.size();
        }

        std::printf ("frames: %-7s meanLum=%.4f meanAbsDelta=%.5f medianDelta=%.6f "
                     "p95Delta=%.6f staticFrac=%.3f coverage=%.4f activeCoverage=%.4f "
                     "meanGrains=%.1f\n",
                     phase.name, phase.lum / phase.frames, phase.delta / phase.frames,
                     medianDelta, p95Delta, staticFraction,
                     phase.coverage / phase.frames, phase.activeCoverage / phase.frames,
                     phase.grains / phase.frames);
    }

    std::printf ("frames: harness wall time %.2f s (%.2f ms/frame, includes PNG output)\n",
                 elapsedMs / 1000.0, elapsedMs / kFrames);
    std::printf ("frames: wrote %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}


// =============================================================================
// Issue #32 motion QA (`--motion`).
//
// All checks are deterministic (fixed dt, fixed audio, no wall-clock timer) and
// use only the public GrainCloud QA hooks. The hook detection below keeps this
// translation unit compiling on the pre-change renderer too, so the tool can be
// built at any point; `--motion` reports SKIP until the hooks exist.
// =============================================================================
struct MotionStats
{
    int   live = 0, matched = 0, afterglow = 0, stray = 0;
    float meanSpeed = 0.0f, meanTravel = 0.0f, meanAge = 0.0f, meanTrack = 0.0f;
    float activity = 0.0f, luminanceProxy = 0.0f;
    bool  finite = true;
};

template <typename Cloud, typename = void>
struct HasParticleHooks : std::false_type {};

template <typename Cloud>
struct HasParticleHooks<Cloud, std::void_t<
    decltype (std::declval<const Cloud&>().particleStatsForTest()),
    decltype (std::declval<const Cloud&>().copyParticlePositionsForTest (
        std::declval<juce::Point<float>*>(), 0))>> : std::true_type {};

// Copies the hook stats into the local mirror struct. The if-constexpr body is
// only instantiated when the hooks exist, so this compiles pre-change as well.
template <typename Cloud>
MotionStats readParticleStats (const Cloud& cloud)
{
    MotionStats out;

    if constexpr (HasParticleHooks<Cloud>::value)
    {
        const auto s = cloud.particleStatsForTest();
        out.live = s.live;
        out.matched = s.matched;
        out.afterglow = s.afterglow;
        out.stray = s.stray;
        out.meanSpeed = s.meanSpeed;
        out.meanTravel = s.meanTravel;
        out.meanAge = s.meanAge;
        out.meanTrack = s.meanTrack;
        out.activity = s.activity;
        out.luminanceProxy = s.luminanceProxy;
        out.finite = s.finite;
    }

    return out;
}

template <typename Cloud>
int readParticlePositions (const Cloud& cloud, juce::Point<float>* out, int maxOut)
{
    if constexpr (HasParticleHooks<Cloud>::value)
        return cloud.copyParticlePositionsForTest (out, maxOut);

    return 0;
}

constexpr int    kMaxMotionPoints     = 512;
constexpr double kMotionDt            = 1.0 / 60.0;
constexpr double kMotionSpeedFloor    = 0.015;  // field widths per second
constexpr double kMotionNNFrameFloor  = 0.10;   // px per frame (nearest neighbour)
constexpr double kMotionTrackFloor    = 0.10;   // seconds of sustained continuity
constexpr double kMotionContinuityFloor = 0.50; // fraction of particles kept
constexpr double kSilenceLiveCapFloor       = 5.0;   // absolute floor: a few grains may be born
constexpr double kSilenceLivePeakFraction   = 0.05;  // <= 5% of the dense-phase peak
constexpr double kSilenceAfterglowCapFloor  = 5.0;
constexpr double kSilenceProxyPeakFraction  = 0.10;  // <= 10% of the dense-phase peak
constexpr double kSilencePeakMinLive        = 10.0;  // dense peak must be testable
constexpr double kSilencePeakMinProxy       = 0.05;
constexpr double kRenderDifferFloor         = 0.90;  // consecutive field frames that must differ
constexpr double kDensityContrast     = 2.0;    // dense matched >= 2x sparse matched
constexpr double kTickMeanCapMs       = 8.0;
constexpr double kTickP95CapMs        = 20.0;
constexpr double kCombinedMeanCapMs   = 16.7;    // one 60 Hz frame

juce::uint64 hashImage (juce::uint64 hash, const juce::Image& image)
{
    if (! image.isValid())
        return hash;

    const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

    for (int y = 0; y < image.getHeight(); ++y)
    {
        const auto* line = data.getLinePointer (y);
        const int bytes = image.getWidth() * data.pixelStride;

        for (int i = 0; i < bytes; ++i)
        {
            hash ^= line[i];
            hash *= 1099511628211ull;
        }
    }

    return hash;
}

struct FrameSample
{
    MotionStats stats;
    int numPositions = 0;
    std::vector<juce::Point<float>> positions;
    juce::uint64 imageHash = 1469598103934665603ull;
    double tickMs = 0.0, paintMs = 0.0;
};

FrameSample runOneMotionFrame (nagloed::NagloedProcessor& processor,
                               nagloed::ui::NagloedEditor& editor,
                               nagloed::ui::GrainCloud& cloud,
                               juce::Component& field,
                               double sampleRate, int blockSize,
                               int audioFrame, bool feedAudio)
{
    if (feedAudio)
        feedChordFrom (processor, sampleRate, blockSize, audioFrame * kMotionDt, kMotionDt);
    else
        feedSilence (processor, sampleRate, blockSize, kMotionDt);

    const auto tickStart = juce::Time::getMillisecondCounterHiRes();
    editor.stepVisualsForPreview (1, kMotionDt);
    const auto tickEnd = juce::Time::getMillisecondCounterHiRes();

    const auto fieldImage = snapshot (field);
    const auto paintEnd = juce::Time::getMillisecondCounterHiRes();

    FrameSample sample;
    sample.tickMs = tickEnd - tickStart;
    sample.paintMs = paintEnd - tickEnd;
    sample.positions.resize ((size_t) kMaxMotionPoints);
    sample.stats = readParticleStats (cloud);
    sample.numPositions = readParticlePositions (cloud, sample.positions.data(), kMaxMotionPoints);
    sample.imageHash = hashImage (sample.imageHash, fieldImage);
    return sample;
}

struct NeighbourMetrics
{
    double medianDist = -1.0;   // median nearest-neighbour distance (normalised)
    double continuity = 0.0;    // fraction of particles kept within 0.10 field widths
};

NeighbourMetrics comparePositions (const std::vector<juce::Point<float>>& a, int na,
                                   const std::vector<juce::Point<float>>& b, int nb)
{
    NeighbourMetrics out;

    if (na <= 0 || nb <= 0)
        return out;

    std::vector<double> distances;
    distances.reserve ((size_t) na);
    int kept = 0;

    for (int i = 0; i < na; ++i)
    {
        double best = 1.0e9;

        for (int j = 0; j < nb; ++j)
            best = juce::jmin (best, (double) a[(size_t) i].getDistanceFrom (b[(size_t) j]));

        distances.push_back (best);

        if (best <= 0.10)
            ++kept;
    }

    std::sort (distances.begin(), distances.end());
    out.medianDist = distances[distances.size() / 2];
    out.continuity = (double) kept / na;
    return out;
}

struct PhaseRun
{
    std::vector<FrameSample> samples;
    std::vector<NeighbourMetrics> neighbours;

    double meanSpeed() const
    {
        double sum = 0.0;
        for (const auto& s : samples)
            sum += s.stats.meanSpeed;

        return samples.empty() ? 0.0 : sum / (double) samples.size();
    }

    double meanMatched() const
    {
        double sum = 0.0;
        for (const auto& s : samples)
            sum += s.stats.matched;

        return samples.empty() ? 0.0 : sum / (double) samples.size();
    }

    double meanTrack() const
    {
        double sum = 0.0;
        for (const auto& s : samples)
            sum += s.stats.meanTrack;

        return samples.empty() ? 0.0 : sum / (double) samples.size();
    }

    double medianNeighbourPx (double fieldWidth) const
    {
        std::vector<double> d;
        for (const auto& n : neighbours)
            if (n.medianDist >= 0.0)
                d.push_back (n.medianDist * fieldWidth);

        if (d.empty())
            return 0.0;

        std::sort (d.begin(), d.end());
        return d[d.size() / 2];
    }

    double medianNeighbourNorm() const
    {
        std::vector<double> d;
        for (const auto& n : neighbours)
            if (n.medianDist >= 0.0)
                d.push_back (n.medianDist);

        if (d.empty())
            return -1.0;

        std::sort (d.begin(), d.end());
        return d[d.size() / 2];
    }

    double meanContinuity() const
    {
        double sum = 0.0;
        for (const auto& n : neighbours)
            sum += n.continuity;

        return neighbours.empty() ? 0.0 : sum / (double) neighbours.size();
    }
};

// Runs `warmupFrames` + `sampleFrames` fixed frames after a deterministic reset.
PhaseRun runMotionPhase (nagloed::NagloedProcessor& processor,
                         nagloed::ui::NagloedEditor& editor,
                         nagloed::ui::GrainCloud& cloud,
                         juce::Component& field,
                         double sampleRate, int blockSize,
                         int warmupFrames, int sampleFrames, bool feedAudio,
                         bool resetState = true)
{
    // resetState=false continues on the current processor+editor pipeline: needed
    // for the silence check so it measures the decay of the embers that were
    // actually alive during playback, not a freshly-emptied field.
    if (resetState)
        editor.advanceForPreview (0);

    PhaseRun run;
    run.samples.reserve ((size_t) sampleFrames);
    run.neighbours.reserve ((size_t) sampleFrames);

    for (int i = 0; i < warmupFrames; ++i)
        runOneMotionFrame (processor, editor, cloud, field, sampleRate, blockSize, i, feedAudio);

    for (int i = 0; i < sampleFrames; ++i)
    {
        auto sample = runOneMotionFrame (processor, editor, cloud, field, sampleRate, blockSize,
                                        warmupFrames + i, feedAudio);

        if (! run.samples.empty())
            run.neighbours.push_back (comparePositions (run.samples.back().positions,
                                                       run.samples.back().numPositions,
                                                       sample.positions, sample.numPositions));

        run.samples.push_back (std::move (sample));
    }

    return run;
}

double meanOf (const std::vector<double>& values)
{
    if (values.empty())
        return 0.0;

    double sum = 0.0;
    for (auto v : values)
        sum += v;

    return sum / (double) values.size();
}

double percentile (std::vector<double> values, double q)
{
    if (values.empty())
        return 0.0;

    std::sort (values.begin(), values.end());
    const auto index = juce::jlimit ((size_t) 0, values.size() - 1,
                                     (size_t) std::floor ((double) values.size() * q));
    return values[index];
}

// Determinism probe: a complete fresh pipeline (processor + editor + the same
// pre-warm as the hero path) replaying exactly the same fixed scenario.
juce::uint64 deterministicScenarioHash (double sampleRate, int blockSize)
{
    nagloed::NagloedProcessor processor;
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    processor.loadPreset (7);
    setParam (processor, nagloed::pid::density,    150.0f);
    setParam (processor, nagloed::pid::size,       140.0f);
    setParam (processor, nagloed::pid::spray,       0.55f);
    setParam (processor, nagloed::pid::spaceMix,    0.72f);
    setParam (processor, nagloed::pid::macroGrain,  0.85f);
    setParam (processor, nagloed::pid::macroGlow,   0.80f);
    feedChordFrom (processor, sampleRate, blockSize, 0.0, 3.2);

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    if (editor == nullptr)
        return 0;

    editor->setSize (1180, 680);

    auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get());
    auto* cloud = dynamic_cast<nagloed::ui::GrainCloud*> (findByTitle (*editor, "Grain cloud"));

    if (nagloedEditor == nullptr || cloud == nullptr)
        return 0;

    auto* field = findFieldComponent (*cloud);
    if (field == nullptr)
        return 0;

    nagloedEditor->advanceForPreview (0);

    juce::uint64 hash = 1469598103934665603ull;

    for (int i = 0; i < 60; ++i)
    {
        const auto sample = runOneMotionFrame (processor, *nagloedEditor, *cloud, *field,
                                               sampleRate, blockSize, i, true);
        hash = hashImage (hash, snapshot (*field));
    }

    return hash;
}

bool positionStateOk (const FrameSample& sample, juce::String& detail)
{
    bool ok = sample.stats.finite;

    if (! ok)
        detail = "non-finite particle state";
    else if (sample.numPositions > kMaxMotionPoints)
        detail = "position count exceeds buffer";

    double minX = 1.0, maxX = 0.0, minY = 1.0, maxY = 0.0;

    for (int i = 0; i < sample.numPositions; ++i)
    {
        const auto p = sample.positions[(size_t) i];

        if (! std::isfinite (p.x) || ! std::isfinite (p.y))
        {
            ok = false;
            detail = "non-finite particle position";
            break;
        }

        minX = juce::jmin (minX, (double) p.x);
        maxX = juce::jmax (maxX, (double) p.x);
        minY = juce::jmin (minY, (double) p.y);
        maxY = juce::jmax (maxY, (double) p.y);
    }

    // The motion model may let an ember drift just outside the field before it
    // dissolves at the edge (it is retired around +/-0.10 field widths), so the
    // bounds check catches gross errors, not the intended edge softness.
    if (ok && (minX < -0.12 || maxX > 1.12 || minY < -0.12 || maxY > 1.12))
    {
        ok = false;
        detail = juce::String::formatted ("positions out of field: x %.2f..%.2f y %.2f..%.2f",
                                          minX, maxX, minY, maxY);
    }

    return ok;
}

int runMotionChecks (nagloed::NagloedProcessor& processor, juce::AudioProcessorEditor& editor,
                     double sampleRate, int blockSize)
{
    auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (&editor);

    if (nagloedEditor == nullptr)
    {
        std::fprintf (stderr, "motion: editor type mismatch\n");
        return 1;
    }

    auto* cloud = dynamic_cast<nagloed::ui::GrainCloud*> (findByTitle (editor, "Grain cloud"));

    if (cloud == nullptr)
    {
        std::fprintf (stderr, "motion: grain cloud component not found\n");
        return 1;
    }

    auto* field = findFieldComponent (*cloud);

    if (field == nullptr)
    {
        std::fprintf (stderr, "motion: grain-field screen child not found\n");
        return 1;
    }

    if (! HasParticleHooks<nagloed::ui::GrainCloud>::value)
    {
        std::printf ("motion: QA hooks unavailable in this build "
                     "(particleStatsForTest/copyParticlePositionsForTest missing)\n");
        std::printf ("MOTION SKIP\n");
        return 1;
    }

    const auto fieldWidth = (double) field->getWidth();

    bool allOk = true;
    auto check = [&allOk] (bool ok, const char* name, const juce::String& detail)
    {
        std::printf ("[%s] %-11s %s\n", ok ? "PASS" : "FAIL", name, detail.toRawUTF8());
        allOk = allOk && ok;
    };

    // --- 1. motion while playing ---------------------------------------------
    // Preset + dense material, 0.5 s warm-up, then 1 s sampled frame by frame.
    setParam (processor, nagloed::pid::density, 150.0f);
    setParam (processor, nagloed::pid::size,    140.0f);
    setParam (processor, nagloed::pid::spray,    0.55f);

    const auto dense = runMotionPhase (processor, *nagloedEditor, *cloud, *field,
                                       sampleRate, blockSize, 30, 60, true);

    const auto motionSpeed = dense.meanSpeed();
    const auto motionTrack = dense.meanTrack();
    const auto nnPx = dense.medianNeighbourPx (fieldWidth);
    const auto continuity = dense.meanContinuity();
    const auto motionMatched = dense.meanMatched();

    const bool motionOk = motionSpeed >= kMotionSpeedFloor
                       && nnPx >= kMotionNNFrameFloor
                       && continuity >= kMotionContinuityFloor
                       && motionTrack >= kMotionTrackFloor
                       && motionMatched >= 3.0;

    check (motionOk, "motion",
           juce::String::formatted ("meanSpeed=%.4f fw/s (floor %.3f) medianNN=%.2f px/frame "
                                    "(floor %.2f) continuity=%.2f (floor %.2f) "
                                    "meanTrack=%.2fs (floor %.2fs) matched=%.1f",
                                    motionSpeed, kMotionSpeedFloor, nnPx, kMotionNNFrameFloor,
                                    continuity, kMotionContinuityFloor, motionTrack,
                                    kMotionTrackFloor, motionMatched));

    // --- 2. render: the field pixels must actually change on screen ----------
    // Independent of particle state this guards the macOS native-image freeze
    // regression: if a revert of the SoftwareImageType fix froze Screen::paint,
    // particle stats could still move while the snapshot hashes stopped changing.
    int consecutivePairs = 0, consecutiveDifferent = 0;
    std::vector<juce::uint64> frameHashes;
    frameHashes.reserve (dense.samples.size());
    juce::uint64 previousHash = 0;
    bool haveHash = false;

    for (const auto& sample : dense.samples)
    {
        if (haveHash)
        {
            ++consecutivePairs;

            if (sample.imageHash != previousHash)
                ++consecutiveDifferent;
        }

        previousHash = sample.imageHash;
        haveHash = true;
        frameHashes.push_back (sample.imageHash);
    }

    std::sort (frameHashes.begin(), frameHashes.end());
    const auto distinctHashes = (int) (std::unique (frameHashes.begin(), frameHashes.end())
                                       - frameHashes.begin());
    const auto differFraction = consecutivePairs > 0
                                    ? (double) consecutiveDifferent / consecutivePairs : 0.0;
    const auto minDistinct = juce::jmax (2, (int) std::floor (kRenderDifferFloor
                                                              * (double) dense.samples.size()));
    const bool renderOk = consecutivePairs >= 20
                       && differFraction >= kRenderDifferFloor
                       && distinctHashes >= minDistinct;

    check (renderOk, "render",
           juce::String::formatted ("dense field frames: %d distinct (floor %d), "
                                    "%d/%d consecutive frames differ (floor %.0f%%)",
                                    distinctHashes, minDistinct, consecutiveDifferent,
                                    consecutivePairs, kRenderDifferFloor * 100.0));

    // --- 3. silence: the live population decays from the playing peak --------
    // This runs on the SAME processor+editor pipeline immediately after the dense
    // phase, with no reset, so it measures the decay of the embers that were
    // actually alive while playing. Density drops to its minimum (0.5 grain/s),
    // so a few grains can still be born from the captured buffer; the claim under
    // test is "births drop to the minimum and the ember population decays", not
    // "zero births".
    double densePeakLive = 0.0, densePeakAfterglow = 0.0, densePeakProxy = 0.0;
    for (const auto& sample : dense.samples)
    {
        densePeakLive = juce::jmax (densePeakLive, (double) sample.stats.live);
        densePeakAfterglow = juce::jmax (densePeakAfterglow, (double) sample.stats.afterglow);
        densePeakProxy = juce::jmax (densePeakProxy, (double) sample.stats.luminanceProxy);
    }

    setParam (processor, nagloed::pid::density, 0.5f);
    const auto silence = runMotionPhase (processor, *nagloedEditor, *cloud, *field,
                                         sampleRate, blockSize, 0, 140, false,
                                         /* resetState */ false);
    // The 140 sampled frames are ~2.33 s of digital silence on the same pipeline.
    const auto silenceLast = silence.samples.empty() ? MotionStats {} : silence.samples.back().stats;

    const auto liveCap = juce::jmax (kSilenceLiveCapFloor,
                                     kSilenceLivePeakFraction * densePeakLive);
    const auto afterglowCap = juce::jmax (kSilenceAfterglowCapFloor,
                                          kSilenceLivePeakFraction * densePeakAfterglow);
    const auto proxyCap = juce::jmax (0.02, kSilenceProxyPeakFraction * densePeakProxy);
    const auto silenceTestable = densePeakLive >= kSilencePeakMinLive
                              && densePeakProxy >= kSilencePeakMinProxy;
    const auto silenceOk = silenceTestable
                        && (double) silenceLast.live <= liveCap
                        && (double) silenceLast.afterglow <= afterglowCap
                        && silenceLast.matched == 0
                        && (double) silenceLast.luminanceProxy <= proxyCap;

    check (silenceOk, "silence",
           juce::String::formatted ("dense peak live=%.0f afterglow=%.0f proxy=%.3f; "
                                    "after 2.33 s silence: live=%d (cap %.1f) "
                                    "afterglow=%d (cap %.1f) matched=%d proxy=%.3f (cap %.3f)%s",
                                    densePeakLive, densePeakAfterglow, densePeakProxy,
                                    silenceLast.live, liveCap, silenceLast.afterglow,
                                    afterglowCap, silenceLast.matched,
                                    (double) silenceLast.luminanceProxy, proxyCap,
                                    silenceTestable ? "" : " [dense peak too small to test]"));

    // --- 4. density contrast: dense >> sparse active embers ------------------
    setParam (processor, nagloed::pid::density, 6.0f);
    setParam (processor, nagloed::pid::size,    140.0f);
    setParam (processor, nagloed::pid::spray,    0.55f);

    const auto sparse = runMotionPhase (processor, *nagloedEditor, *cloud, *field,
                                        sampleRate, blockSize, 24, 60, true);

    setParam (processor, nagloed::pid::density, 150.0f);
    const auto denseAgain = runMotionPhase (processor, *nagloedEditor, *cloud, *field,
                                            sampleRate, blockSize, 24, 60, true);

    const auto denseMatched = denseAgain.meanMatched();
    const auto sparseMatched = sparse.meanMatched();
    const bool densityOk = denseMatched >= juce::jmax (3.0, kDensityContrast * sparseMatched)
                        && denseMatched > sparseMatched;

    check (densityOk, "density",
           juce::String::formatted ("dense matched=%.1f vs sparse matched=%.1f "
                                    "(ratio %.2fx, floor %.1fx)",
                                    denseMatched, sparseMatched,
                                    sparseMatched > 0.0 ? denseMatched / sparseMatched : 999.0,
                                    kDensityContrast));

    // --- 5. bounded cost (dense phase, per-field-frame tick + paint) ---------
    std::vector<double> tickTimes, paintTimes, combinedTimes;

    for (const auto& sample : denseAgain.samples)
    {
        tickTimes.push_back (sample.tickMs);
        paintTimes.push_back (sample.paintMs);
        combinedTimes.push_back (sample.tickMs + sample.paintMs);
    }

    const auto tickMean = meanOf (tickTimes);
    const auto tickP95 = percentile (tickTimes, 0.95);
    const auto paintMean = meanOf (paintTimes);
    const auto combinedMean = meanOf (combinedTimes);
    const bool costOk = tickMean <= kTickMeanCapMs
                     && tickP95 <= kTickP95CapMs
                     && combinedMean <= kCombinedMeanCapMs;

    check (costOk, "cost",
           juce::String::formatted ("dense tick mean=%.2f ms p95=%.2f ms (cap %.1f/%.1f), "
                                    "paint mean=%.2f ms, tick+paint mean=%.2f ms (cap %.1f)",
                                    tickMean, tickP95, kTickMeanCapMs, kTickP95CapMs,
                                    paintMean, combinedMean, kCombinedMeanCapMs));

    // --- 6. determinism: two fresh pipelines, identical frames ---------------
    const auto hashA = deterministicScenarioHash (sampleRate, blockSize);
    const auto hashB = deterministicScenarioHash (sampleRate, blockSize);
    const bool determinismOk = hashA != 0 && hashA == hashB;

    check (determinismOk, "determinism",
           juce::String::formatted ("runA=%016llx runB=%016llx",
                                    (unsigned long long) hashA, (unsigned long long) hashB));
    std::printf ("MOTION-HASH %016llx\n", (unsigned long long) hashA);

    // --- 7. stability: finite/in-bounds state, resize safe -------------------
    juce::String stabilityDetail = "state finite, positions in field";
    bool stabilityOk = true;

    for (const auto& sample : dense.samples)
    {
        juce::String detail;

        if (! positionStateOk (sample, detail))
        {
            stabilityOk = false;
            stabilityDetail = "dense phase: " + detail;
            break;
        }
    }

    if (stabilityOk)
    {
        const juce::Rectangle<int> sizes[] = { { 900, 520 }, { 1400, 800 }, { 1180, 680 } };

        for (const auto& size : sizes)
        {
            editor.setSize (size.getWidth(), size.getHeight());
            setParam (processor, nagloed::pid::density, 150.0f);

            for (int i = 0; i < 30; ++i)
            {
                const auto sample = runOneMotionFrame (processor, *nagloedEditor, *cloud, *field,
                                                       sampleRate, blockSize, i, true);
                juce::String detail;

                if (! positionStateOk (sample, detail))
                {
                    stabilityOk = false;
                    stabilityDetail = juce::String::formatted ("resize %dx%d: ",
                                                               size.getWidth(), size.getHeight())
                                      + detail;
                    break;
                }
            }

            if (! stabilityOk)
                break;
        }
    }

    check (stabilityOk, "stability", stabilityDetail);

    std::printf ("%s\n", allOk ? "MOTION PASS" : "MOTION FAIL");
    return allOk ? 0 : 1;
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
    const auto framesDir = getOptionValue (argc, argv, "--frames");

    // Only bare numbers are positional size args: an option's value (e.g. the
    // --frames directory) must never be parsed as a width/height.
    juce::StringArray positional;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--frames")
        {
            ++i;
            continue;
        }

        if (! arg.startsWith ("--"))
            positional.add (arg);
    }

    const int width  = positional.size() > 0 ? juce::jmax (320, positional[0].getIntValue()) : 1180;
    const int height = positional.size() > 1 ? juce::jmax (200, positional[1].getIntValue()) : 680;

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

    // Issue #32 evidence capture: deterministic 8 s musical scenario.
    if (framesDir.isNotEmpty())
        return runFrameCapture (processor, *editor, framesDir, sampleRate, blockSize);

    if (embedded)
        return runEmbeddedTest (processor, editor, width, height);

    editor->setVisible (true);

    // --gloop: documentation frame for the Gloop overlay (issue #33). Once the
    // wall clock is stopped the overlay, its lane rings and the knob rings are
    // all a pure function of the lane state, so that state is injected instead
    // of depending on a live recording session. Audio still flows, so the
    // instrument behind the overlay is plainly playing.
    if (hasArg (argc, argv, "--gloop"))
    {
        auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get());
        if (nagloedEditor == nullptr)
        {
            std::fprintf (stderr, "gloop: editor type mismatch\n");
            return 1;
        }

        nagloedEditor->advanceForPreview (0);   // stop the timer, reset visuals
        nagloedEditor->setGloopPanelOpen (true);

        // A plausible performance state: three lanes moving, one READY, one
        // empty, one withdrawing.
        setParam (processor, nagloed::pid::gloopTarget[0], 3.0f);   // Cutoff
        setParam (processor, nagloed::pid::gloopTarget[1], 1.0f);   // Focus
        setParam (processor, nagloed::pid::gloopTarget[2], 4.0f);   // Space
        setParam (processor, nagloed::pid::gloopTarget[3], 6.0f);   // Spray
        setParam (processor, nagloed::pid::gloopTarget[5], 2.0f);   // Pitch

        nagloed::gloop::Visual lanes;

        const auto setLane = [&lanes] (int i, nagloed::gloop::Target target,
                                       nagloed::gloop::Mode mode, bool hasGesture,
                                       float phase, float offset, float amount)
        {
            auto& l = lanes.lanes[(size_t) i];
            l.target = (int) target;
            l.mode = (int) mode;
            l.hasGesture = hasGesture;
            l.phase = phase;
            l.offset = offset;
            l.amount = amount;
            l.durationSeconds = 2.4f;
        };

        setLane (0, nagloed::gloop::Target::Cutoff, nagloed::gloop::Mode::Loop,   true, 0.42f,  0.17f, 0.65f);
        setLane (1, nagloed::gloop::Target::Focus,  nagloed::gloop::Mode::Record, true, 0.13f, -0.09f, 0.50f);
        setLane (2, nagloed::gloop::Target::Space,  nagloed::gloop::Mode::Loop,   true, 0.78f,  0.06f, 0.80f);
        setLane (3, nagloed::gloop::Target::Spray,  nagloed::gloop::Mode::Idle,   true, 0.00f,  0.00f, 0.40f);
        setLane (5, nagloed::gloop::Target::Pitch,  nagloed::gloop::Mode::Fade,   true, 0.61f, -0.03f, 0.30f);

        for (int chunk = 0; chunk < 24; ++chunk)
        {
            feedChordFrom (processor, sampleRate, blockSize, chunk * 0.1, 0.1);
            nagloedEditor->stepVisualsForPreview (6);
        }

        // Injected last, so no frame step can overwrite it.
        nagloedEditor->setGloopVisualForPreview (lanes);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("docs")
                             .getChildFile ("gloop-preview.png");
        writePng (out, image);

        std::printf ("wrote %s (%d x %d, synthetic gloop lanes)\n",
                     out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
        return 0;
    }

    // --motion: deterministic motion-QA checks using the GrainCloud QA hooks
    // (issue #32). Prints PASS/FAIL per check; needs the post-change renderer.
    if (hasArg (argc, argv, "--motion"))
        return runMotionChecks (processor, *editor, sampleRate, blockSize);

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
        // Coarse audio chunks (0.1 s per six frames) keep grain overlap/density
        // at the live-soak level; feeding 0.02 s per single frame reduced overlap
        // so much that the clipped-ember regression stopped reproducing.
        for (int i = 0; i < 80; ++i)
        {
            feedChord (processor, sampleRate, blockSize, 0.1);
            nagloedEditor->stepVisualsForPreview (6);
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

    // Deterministic documentation frame: no wall-clock timer, fixed audio chunks
    // and a fixed frame count, fixed RNG seed (the dust generator/particles are
    // seeded in GrainCloud). Audio must keep flowing while the frames are
    // stepped: a frozen VisualState would pin every grain (and, with the new
    // motion model, hold the particles at their homes), so the committed hero
    // would not show the instrument actually playing. This is the same
    // soak-style pattern (0.1 s audio per six 1/60 s frames).
    if (auto* nagloedEditor = dynamic_cast<nagloed::ui::NagloedEditor*> (editor.get()))
    {
        nagloedEditor->advanceForPreview (0);   // stop timer, reset visual state

        for (int chunk = 0; chunk < 40; ++chunk)
        {
            feedChordFrom (processor, sampleRate, blockSize, chunk * 0.1, 0.1);
            nagloedEditor->stepVisualsForPreview (6);
        }
    }

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
