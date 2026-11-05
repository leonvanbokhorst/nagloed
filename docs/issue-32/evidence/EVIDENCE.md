# Issue #32 — evidence: does Sintel actually move?

This directory holds the before/after measurement evidence for issue #32
("Sintel — make the grains fly"). It is the measurement layer only: it proves
that the rendered grain field changes over time and how it behaves under a fixed
musical scenario. It does **not** claim artistic acceptance — that is Lonn's
judgement, and the issue stays open for it.

## Two root causes (read this first)

The baseline capture exposed **two** independent reasons why Sintel looked
static, and the before/after must not be misread as a single fix:

1. **The render was literally frozen on macOS.** `GrainCloud::PhosphorField`
   created its image with the default `juce::Image` constructor
   (`NativeImageType`, i.e. CoreGraphics) and then wrote pixels through a
   persistent `juce::Image::BitmapData`. On macOS the native image caches its
   `CGImage` on first draw and does not invalidate it for raw `BitmapData`
   writes, so `Screen::paint` kept redrawing the first cached frame. Measured
   directly: the phosphor pixel hash changed every tick while the `Screen`
   component snapshot hash stayed constant. The baseline `--frames` numbers below
   are therefore **what the application actually put on screen** — a stale image
   plus a handful of cache/buffer-boundary frames — not a subtle-looking field.
   Fixed by creating the phosphor image with `juce::SoftwareImageType()`.
2. **The motion model was missing.** Every `GrainView` was projected to the same
   pixel for its whole life (`x = pos * fieldWidth + pan * 6px`,
   `y = pitchY(pitch)`); `progress` and `env` were only used for radius/energy,
   and the phosphor integrated the repeated splats into a stationary glow.
   Fixed by the UI-side particle system (birth, flight, clustering, scatter,
   extinction, stray embers) with a bounded particle pool.

The baseline evidence is honest: it is what Lonn saw. The after capture reflects
the true animated renderer. Both captures use the same harness and the same audio
feeding (see the timing note below), so the comparison is like-for-like.

## What is measured

`nagl_preview --frames <dir>` renders the real editor offscreen, frame-stepped at
60 fps with no wall clock and no audio device, through a fixed musical scenario
of 480 visual frames. One audio chunk is pushed through the real
`NagloedProcessor` per visual frame and the engine's `VisualState` is
snapshotted between frames.

**Timing note (method).** Each visual frame feeds two whole 512-sample blocks
(`feedChordFrom` / `feedSilence` process complete blocks), i.e. 1024 samples =
0.02133 s of engine audio per visual frame. The scenario is therefore **8.00 s of
visual-frame time** but **≈10.24 s of engine audio** (480 x 0.02133 s). Phase
boundaries and the times in the table below are visual-frame times; in engine
audio the phases last ≈3.07 s / 2.56 s / 2.56 s / 2.05 s. This was always the
case and is unchanged for the baseline capture, so before/after remain
comparable; only the description here is explicit about it.

Scenario (preset 07 "Wolkenkaart"):

| phase | visual time | audio | density (grain/s) | size | spray |
|---|---|---|---|---|---|
| dense | 0.0 – 2.4 s | warm 5-note chord | 150 | 140 | 0.55 |
| sparse | 2.4 – 4.4 s | warm chord | 6 | 140 | 0.55 |
| silence | 4.4 – 6.4 s | digital silence (engine still processed) | 0.5 | 140 | 0.55 |
| return | 6.4 – 8.0 s | warm chord | 150 | 140 | 0.55 |

Per frame, on the grain-field component (the phosphor screen child of the grain
cloud, 557 x 280 logical px at the default 1180 x 680 editor size; the snapshot
includes the static engraved overlay, which is why coverage never reaches zero):

- **meanLum** — mean luminance of the field, 0..1;
- **centroid (x, y)** — centroid of bright pixels (luminance >= 0.10), field px;
- **meanAbsDelta** — mean absolute luminance change per pixel vs the previous
  frame (0 = pixel-identical frame);
- **coverage** — fraction of pixels with luminance >= 0.10;
- **activeCoverage** — fraction of pixels that changed by >= 0.02 vs the previous
  frame (temporal activity, the cleanest "is it moving" signal).

`metrics.csv` holds the per-frame rows (`frame,time_s,phase,numGrains,inRms,outRms,meanLum,centroidX,centroidY,centroidDelta,meanAbsDelta,coverage,activeCoverage`).
The tables below are aggregated from those files. `--frames` measures **rendered
activity**; `--motion` (below) measures **particle state** through the QA hooks.

## How to reproduce

```bash
# build (from the repo root; one build at a time)
cmake --build build -j"$(sysctl -n hw.ncpu)" --target nagl_preview

# deterministic 480-visual-frame scenario -> frames + metrics
# (raw frame sequences land in /tmp and are not committed: ~110 MB per run)
./build/nagl_preview_artefacts/Release/nagl_preview --frames /tmp/issue32-before   # old renderer
./build/nagl_preview_artefacts/Release/nagl_preview --frames /tmp/issue32-after    # new renderer

# motion QA (PASS/FAIL, uses the GrainCloud QA hooks)
./build/nagl_preview_artefacts/Release/nagl_preview --motion

# aggregate a metrics.csv into the tables below
python3 docs/issue-32/evidence/summarise_metrics.py \
    docs/issue-32/evidence/before/metrics.csv "Baseline (unmodified renderer)"
```

The capture is reproducible, not just the QA mode: two independent `--frames`
runs on the new renderer produced byte-identical `metrics.csv`
(sha256 `66f9c8494a75ed734956562852220b979aaf50ea09b90aa2701a006476f689d0`), so
the before/after comparison is not run-to-run noise.

Committed stills are 1180 px wide JPEGs of the 2x editor snapshot at frames 60
(dense), 210 (sparse), 330 (silence) and 450 (return). GIFs are 20 fps (every
third 60 fps frame), 128-colour palette-optimised:

```bash
# per-state GIF (720 px wide)
ffmpeg -framerate 20 -pattern_type glob -i '/tmp/issue32-after/ui/*.png' \
  -vf "scale=720:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3" \
  -loop 0 docs/issue-32/evidence/after/grain-field.gif

# labelled side-by-side (480 px per side)
ffmpeg -framerate 20 -pattern_type glob -i '/tmp/issue32-before/ui/*.png' \
       -framerate 20 -pattern_type glob -i '/tmp/issue32-after/ui/*.png' \
  -filter_complex "[0:v]scale=480:-1:flags=lanczos,drawtext=fontfile=/System/Library/Fonts/Supplemental/Arial.ttf:text='BEFORE — frozen render':x=10:y=10:fontsize=18:fontcolor=white:box=1:boxcolor=black@0.65:boxborderw=6[a];\
[1:v]scale=480:-1:flags=lanczos,drawtext=fontfile=/System/Library/Fonts/Supplemental/Arial.ttf:text='AFTER — living particles':x=10:y=10:fontsize=18:fontcolor=white:box=1:boxcolor=black@0.65:boxborderw=6[b];\
[a][b]hstack,split[x][y];[x]palettegen=max_colors=128:stats_mode=diff[p];[y][p]paletteuse=dither=bayer:bayer_scale=3" \
  -loop 0 docs/issue-32/evidence/before-after.gif
```

## Baseline (unmodified renderer)

Git `983d48c7`; `src/ui/GrainCloud.cpp` sha256
`cf586b8c4fc44900fcac1a13d75a3936c4cbafc562c5a382b72d2b300476c639`,
`src/ui/GrainCloud.h` sha256
`6788dec3e6075e9721e720cc3851bfa95610daa2ea2c3dad015041cf40f7d3e3`.

| phase | frames | mean grains | meanLum | mean abs delta | median delta | p95 delta | static frames (<1e-4) | coverage | active coverage |
|---|---|---|---|---|---|---|---|---|---|
| dense | 144 | 37.2 | 0.0596 | 0.00047 | 0.000000 | 0.000000 | 141 (97.9%) | 0.1587 | 0.0033 |
| sparse | 120 | 5.2 | 0.0548 | 0.00026 | 0.000000 | 0.000000 | 118 (98.3%) | 0.1491 | 0.0024 |
| silence | 120 | 0.3 | 0.0505 | 0.00011 | 0.000000 | 0.000000 | 118 (98.3%) | 0.1416 | 0.0014 |
| return | 96 | 32.5 | 0.0501 | 0.00035 | 0.000000 | 0.000000 | 94 (97.9%) | 0.1320 | 0.0026 |

Only **9 of 480 frames** (1.9%) have a mean absolute delta above 1e-4. They are
cache/boundary frames, not content-driven motion; the exact trigger was not
isolated (the 480-frame capture never reallocated the phosphor buffer), so the
honest description is "a frozen image with a few non-motion refreshes". Median delta is exactly 0: the field is
pixel-identical from one frame to the next for ~98% of the scenario, in every
phase. (The baseline dense meanLum is slightly *higher* than the after value —
0.0596 vs 0.0518 — but that is not evidence of a brighter or busier field: the
two paths differ in phosphor fade (1/32 before, 1/16 after), crowd normalisation
and splat energies, so the after field is deliberately more compact and dimmer.
Brightness is not the "more alive" measure; the movement metrics are.)

- stills: `before/stills/still_{0060,0210,0330,0450}.jpg`
- GIF: `before/grain-field.gif`

## After (new renderer)

Same harness, same scenario, same audio feeding, recorded after the motion
model, the `SoftwareImageType` render fix and the QA hooks landed.

| phase | frames | mean grains | meanLum | mean abs delta | median delta | p95 delta | static frames (<1e-4) | coverage | active coverage |
|---|---|---|---|---|---|---|---|---|---|
| dense | 144 | 37.2 | 0.0518 | 0.00054 | 0.000302 | 0.000697 | 1 (0.7%) | 0.1402 | 0.0056 |
| sparse | 120 | 5.2 | 0.0499 | 0.00028 | 0.000107 | 0.000263 | 58 (48.3%) | 0.1302 | 0.0030 |
| silence | 120 | 0.3 | 0.0474 | 0.00011 | 0.000020 | 0.000116 | 111 (92.5%) | 0.1217 | 0.0013 |
| return | 96 | 32.5 | 0.0493 | 0.00043 | 0.000296 | 0.000344 | 13 (13.5%) | 0.1322 | 0.0045 |

The single "static" dense frame is frame 0, which has no predecessor by
definition — **every comparable dense frame changes**. Headline comparison:

| phase | median frame delta (before → after) | static frames (before → after) | active coverage (before → after) |
|---|---|---|---|
| dense | 0.000000 → 0.000302 | 97.9% → 0.7% | 0.33% → 0.56% |
| sparse | 0.000000 → 0.000107 | 98.3% → 48.3% | 0.24% → 0.30% |
| silence | 0.000000 → 0.000020 | 98.3% → 92.5% | 0.14% → 0.13% |
| return | 0.000000 → 0.000296 | 97.9% → 13.5% | 0.26% → 0.45% |

Reading the table:

- **Dense playing**: the field is now in continuous motion (median delta 0.00030,
  no static frames). Active coverage rises ~1.7x even though total luminance
  falls slightly — energy is spread along moving trails and correctly decays
  instead of piling into frozen discs.
- **Sparse playing**: motion is real but intermittent (48.3% of frames are still).
  Late sparse material genuinely rests between events; that is restraint, not a
  freeze. The `--motion` QA checks motion in the dense phase, where the
  regression guard belongs.
- **Silence**: the field becomes still *after decaying* (92.5% static). The QA
  hook run below shows the live population falling from a dense peak of 174 to 1
  and the luminance proxy from 1.000 to 0.001 over 2.33 s of silence. The cold
  dust bed remains visible but dim, and the minimum grain rate (0.5 grain/s) can
  still read the captured buffer, so this is "births drop to the minimum and the
  embers decay", not "zero activity".
- **Return**: grain activity resumes and motion restarts (13.5% static, mostly
  the first moments after the density change).

- stills: `after/stills/still_{0060,0210,0330,0450}.jpg`
- GIF: `after/grain-field.gif`
- side-by-side: `before-after.gif`

## Motion QA (`--motion`)

Deterministic in content (fixed dt/audio/RNG), no audio device, ~4 s runtime. Full log: `after/motion.log`.

```
[PASS] motion      meanSpeed=0.1124 fw/s (floor 0.015) medianNN=1.09 px/frame (floor 0.10)
                   continuity=1.00 (floor 0.50) meanTrack=0.16s (floor 0.10s) matched=37.5
[PASS] render      dense field frames: 60 distinct (floor 54), 59/59 consecutive frames differ (floor 90%)
[PASS] silence     dense peak live=174 afterglow=138 proxy=1.000; after 2.33 s silence:
                   live=1 (cap 8.7) afterglow=1 (cap 6.9) matched=0 proxy=0.001 (cap 0.100)
[PASS] density     dense matched=37.0 vs sparse matched=1.1 (ratio 32.63x, floor 2.0x)
[PASS] cost        dense tick mean=1.61 ms p95=1.65 ms (cap 8.0/20.0),
                   paint mean=1.51 ms, tick+paint mean=3.12 ms (cap 16.7)
[PASS] determinism runA=baf8b4e3c0e843fb runB=baf8b4e3c0e843fb
[PASS] stability   state finite, positions in field
MOTION PASS
MOTION-HASH baf8b4e3c0e843fb
```

What the checks mean:

- **motion** — dense phase, 0.5 s warm-up then 1 s sampled frame by frame:
  meanSpeed is field widths per second from the particle model; medianNN is the
  median nearest-neighbour displacement between consecutive particle sets (the
  identity order is not stable, so this is a set-distance, not index matching);
  continuity is the fraction of particles with a neighbour within 0.10 field
  widths; meanTrack is the mean seconds of continuous matched identity.
- **render** — a render-blind particle test could pass while the screen is
  frozen (the original macOS native-image bug). This check hashes the actual
  grain-field snapshot every sampled dense frame and requires that the field
  changes: at least 90% of consecutive sampled frames must differ and at least
  90% of sampled frames must have distinct hashes. A revert of the
  `SoftwareImageType` fix makes this line FAIL even if the particle stats still
  move.
- **silence** — runs on the **same processor+editor pipeline immediately after
  the dense phase, with no reset**, so it measures the decay of the embers that
  were actually alive while playing (a reset field could pass vacuously). It
  records the dense-phase peaks (live 174, afterglow 138, luminanceProxy 1.000),
  plays 2.33 s of digital silence (density at its 0.5 grain/s minimum), and
  requires the final live population <= max(5, 5% of dense peak), afterglow
  <= max(5, 5% of dense peak afterglow), matched == 0 and luminanceProxy <= max(0.02,
  10% of dense peak). A field whose afterglow never decayed would fail.
- **density** — dense material must have clearly more live matched embers than
  sparse material.
- **determinism** — two fresh processor+editor pipelines replaying the same
  pre-warm and 60-frame scenario must produce bit-identical field frames
  (FNV-1a over every frame's pixels). `MOTION-HASH` is printed so CI or a human
  can also compare separate invocations.
- **cost** — per-frame UI tick and offscreen paint at maximum density, measured
  in the preview harness on the development machine. The cap is one 60 Hz frame
  (16.7 ms) for tick+paint; the measured cost is comfortably below it. Wall-clock
  costs vary by a few hundredths of a millisecond between runs; the determinism
  check covers frame content, not timings.
- **stability** — finite particle state, positions within the field's intended
  edge tolerance, and safe resizes through 900x520, 1400x800 and 1180x680.

## Other regressions (same build)

- `nagl_tests`: **167 checks, 0 failures** (`after/nagl_tests.log`).
- `--soak`: **SOAK PASS** — meanLuminance 0.07, whiteRatio 0.0000, clippedRatio
  0.00000, animates 1, alphaOk 1 (`after/soak.log`).
- `--headless-host`: **HEADLESS-HOST PASS** — timerRunning 1, cloudAnimates 1,
  curveChanged 1, lfoChanged 1 (`after/headless-host.log`).
- Deterministic hero (`nagl-preview`, run **serially** twice): byte-identical both
  times, sha256 `fa6ab2b2391ff3987fee2e7cdd4f0e9979ac09e37c8f81136d56b8208d357d3f`,
  1,862,612 bytes (`docs/ui-preview.png`). The hero is fully timer-free: the old
  wall-clock `runDispatchLoopUntil(700)` before the deterministic advance was
  removed, and the hero uses the audio-fed path (40 x 0.1 s chord + 6 fixed
  1/60 s frames per chunk after `advanceForPreview(0)`), so it shows the
  instrument playing rather than a frozen `VisualState`. Removing the dispatch
  loop changed 0.0045% of pixels vs the pipelined hero (PSNR 57.7 dB), i.e. the
  image is visually unchanged but no longer depends on timer scheduling.

## Release re-check (NAGLOED 1.0.5, 5 November 2026)

The after capture, the motion QA, soak, headless host-mount check, unit suite,
AU validation and pluginval strictness 10 were re-run on the **1.0.5 release
build** (the same renderer plus the version stamp). The after capture stayed
byte-identical (`metrics.csv` sha256
`66f9c8494a75ed734956562852220b979aaf50ea09b90aa2701a006476f689d0`), the hero
stayed byte-identical across serial runs (new sha256 `fa6ab2b2…`, 1,862,612
bytes; it changed only because the footer now reads 1.0.5), and the full-UI
stills were re-rendered with the 1.0.5 stamp. The field frames and GIFs are
unchanged: the renderer did not change.

## Interpretation

### What this proves

- On the old renderer the grain field was effectively a still image: ~98% of
  consecutive frames were pixel-identical under a fully deterministic, audio-fed
  scenario. On the new renderer the dense playing field changes on every
  comparable frame, restarts after silence, and returns to stillness in silence.
- The two layers are covered separately: `--frames` measures rendered activity
  (pixels actually change, and the render guard would catch a freeze regression),
  while `--motion` measures particle state through the public hooks
  (birth/flight/afterglow counts, mean speed, travel, continuity, decay). Neither
  alone would tell the whole story; together they show the moving pixels come
  from a bounded, finite, decaying particle simulation.
- Silence remains meaningful (live embers decay from a dense peak of 174 to 1
  within ~2.3 s, matched == 0, proxy 0.001 of 1.000), and density contrast is
  large (32.6x more matched embers dense vs sparse).
- Regressions are covered: the existing soak, headless host-mount, unit tests and
  the deterministic hero all still pass, and hero determinism is preserved.

### What it cannot prove

- It cannot prove that Sintel *feels* alive or connected to the playing. That is
  the issue's acceptance question and needs Lonn's judgement.
- It is a synthetic, fixed scenario (preset 7, chord, fixed 60 fps cadence,
  0.02133 s of engine audio per visual frame), not a real host session: host
  frame-rate jitter, real performance gestures, other presets and other window
  sizes are only approximated (the QA mode does resize through 900x520, 1400x800
  and 1180x680).
- The cost numbers are measured on the development machine inside the preview
  harness (tick + offscreen paint), not on a loaded host or CI runner; they are a
  regression guard, not a real-time guarantee.
- Frame delta/coverage shows that pixels changed; it cannot by itself
  distinguish "grains flying" from "some other animation" — that is why the
  motion QA also asserts particle-state decay and continuity. The QA hooks are a
  proxy for particle state, and the continuity strategy is nearest-neighbour
  matching of anonymous `GrainView`s, not stable engine grain ids (see
  `docs/SINTEL-MOTION.md` for the data-vs-interpretation split).
