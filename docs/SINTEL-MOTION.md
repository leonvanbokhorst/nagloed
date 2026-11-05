# Sintel motion — what moves in the grain field, and why

> Working document for issue [#32](https://github.com/leonvanbokhorst/nagloed/issues/32).
> It describes why the Sintel field looked static, how the UI now derives motion
> from the data it already receives, and what that motion does and does not
> mean. The issue stays **open**: whether the grains feel alive while you play is
> Lonn's judgement, not a benchmark result (see [§9](#9-open-acceptance--for-lonn)).
>
> Dutch concept names are kept as written: **NAGLOED**, **Sintel**, **Ruimte**,
> **EAR**. "Grain" and "capture buffer" follow the vocabulary in
> [BRAND.md](BRAND.md#terminology).

This document covers the first playable iteration from issue #32. It is not a
description of the whole future instrument.

## 1. Why the field looked static

There were **two independent causes**, and both are now fixed. Fixing the render
alone would still have left no grain life to see; fixing the motion alone would
still have been invisible on macOS.

### 1.1 Cause one: on macOS the phosphor was frozen

`GrainCloud::PhosphorField::ensure()` built its image with the default
`juce::Image` constructor. On macOS that selects the native (CoreGraphics) image
type, while the field is written every frame through one persistent
`juce::Image::BitmapData`. A native image caches its `CGImage` on first draw and
does not invalidate it for those raw writes, so `Screen::paint` kept drawing an
old frame: the phosphor *pixels* kept changing (the buffer underneath was written
and decayed every tick), but the displayed image did not.

The proof is a hash comparison: the phosphor pixel hash changed on every tick
while the `Screen` component-snapshot hash stayed constant. The old `--frames`
run showed the same thing from outside — 98% of consecutive frames were
pixel-identical; the few frames that differed were cache/boundary frames, and the
exact trigger was not isolated (the 480-frame capture never reallocated the
phosphor buffer).

The fix is explicit software backing: `juce::Image (..., juce::SoftwareImageType())`.
The consequence is worth stating plainly: **the shipped 1.0.2–1.0.4 grain field
was effectively frozen on macOS.** That is part of why issue #32 described
Sintel as static, and it is fixed in the same change that adds motion.

### 1.2 Cause two: the visual path had no motion to show

With rendering working, the old field still had nothing to draw motion from.
Nothing below was a bug; each step did what it was built to do.

#### 1.2.1 The UI receives snapshots, not events

The audio thread publishes the whole `VisualState` once per audio block through
`SnapshotChannel` (single-writer/single-reader double buffer, last writer wins —
[src/core/Snapshot.h](../src/core/Snapshot.h)). `EngineDSP::getVisual()` rebuilds
that state from scratch each block
([src/dsp/EngineDSP.cpp](../src/dsp/EngineDSP.cpp), `getVisual`), and
`GrainEngine::copyGrainViews()` fills the grain array
([src/dsp/GrainEngine.cpp](../src/dsp/GrainEngine.cpp), `copyGrainViews`).

A `GrainView` is a flat, anonymous record — `pos`, `progress`, `pan`, `pitch`,
`env`, `brightness` ([src/core/Contracts.h](../src/core/Contracts.h)). There is:

- **no grain id**, so the UI could not tell "this is the same grain as last
  frame";
- **no event stream** (birth/death messages), only the current active set;
- **no position history** and no velocity.

The active views are emitted in grain-pool slot order, not in a stable order the
UI could rely on. Two consecutive frames of `VisualState` were two photographs of
a crowd, with no names on the faces.

#### 1.2.2 The grain dot was a static projection of slow parameters

`GrainCloud::drawGrains()` (removed by this change) mapped each view to a fixed
screen coordinate:

```
x = pos * fieldWidth + pan * 6 px
y = pitchToY(pitch)          // pitch mapped linearly over ±24 semitones
```

For one grain, `pan` and `pitch` are constant. `pos` is the grain's read position
inside the capture buffer, and it advances only as the grain plays:
`copyGrainViews()` computes `current = sourcePos + age * rate` and normalises it
against the buffer length, so `pos` moves by `rate / (60 * bufferSeconds)` of the
field per 60 Hz frame. With the default 80 ms grain
([src/core/Parameters.h](../src/core/Parameters.h), `ranges::size`) a grain lives
about five frames, and at a 12-second buffer its dot advanced only about a pixel
per frame — for the whole life of the grain, roughly five pixels. For most of its
life it painted the same place.

`progress` and `env` were available but only modulated radius and energy; they
did not drive birth, travel, clustering or death.

#### 1.2.3 The phosphor integrated the repetition into a stationary glow

The field is a phosphor persistence buffer: splats are added additively and
faded by a fraction each frame (`GrainCloud::PhosphorField::splat` / `fade`;
the old live path used 1/32 per frame — a glow half-life of roughly a third of
a second). That is the right material for trails, but the old renderer drew each
barely-moving grain at the same pixel frame after frame, so the persistence
buffer summed the repeats into a small stationary ember. The afterglow, meant to
reveal motion, flattened it instead.

#### 1.2.4 The only real motion was decorative dust

The `Mote` field (`spawnMote` / `advanceMote` / `drawDust`) drifted slowly around
the focus point, loosely scaled by density, spray and pitch. It is deterministic
set dressing for the idle engine, permitted by the brand as a cold, pre-warmed
ember bed ([BRAND.md](BRAND.md#shipped-interface-decisions-10)). It was not born
from grain data and could not show a grain's life.

With rendering frozen (cause one) and no motion model (cause two), the result
read as *slow ambient dust plus blinking stationary dots* — or, on macOS, as a
still image.

## 2. What changed

The change is **UI-only**. `src/ui/GrainCloud.h` and `src/ui/GrainCloud.cpp`
gained a particle layer that turns the stream of anonymous `GrainView` frames
into continuous tracks, and the phosphor buffer is now explicitly
software-backed so what is drawn actually reaches the screen (see
[§1.1](#11-cause-one-on-macos-the-phosphor-was-frozen)). Nothing in `src/core`, `src/dsp`, `src/plugin`, the
parameter set, the preset format or the snapshot format changed. The audio thread
still publishes exactly the same `VisualState`; it simply had more to say than
the old renderer listened for.

The phosphor still owns trail persistence: particles splat along their velocity
vector and the existing persistence buffer draws the path. Motion is produced by
where the splats go, not by a new renderer.

The model is a fixed array of 360 `Particle` values
(`GrainCloud::particles`). Each frame, `updateParticles()` recomputes a smoothed
activity level, `matchGrainViews()` pairs the incoming views with existing
particles (or starts/stops them), and `integrateParticles()` advances the
survivors. `drawParticles()` splats the cores and `drawParticleTrail()` adds one or two
trail splats along each particle's velocity. Nothing is allocated per frame.

`GrainCloud::tick()` keeps its signature; the particle update runs from the
existing 60 Hz editor timer (or the frame-stepped preview path in
`NagloedEditor::stepVisualsForPreview`). `GrainCloud::resetForDeterministicPreview()`
also clears particle state and reseeds the deterministic RNG, so preview frames
stay reproducible.

The two QA hooks required by the brief are public on `GrainCloud` and UI-only:
`particleStatsForTest()` (live/matched/afterglow/stray counts, mean speed,
travel, age and track, activity, luminance proxy, finite flag) and
`copyParticlePositionsForTest()` (normalised field coordinates; edge embers may
sit slightly outside 0..1 while they fade, and slot order is explicitly not
stable).

## 3. Data versus interpretation

Every visible ember starts as a real `GrainView`. The flight is an interpretation
of that grain's life; it is not a second source of data. The rule from
[BRAND.md](BRAND.md#the-visual-language-a-warm-room-at-night) still holds: the
embers are data, not bokeh.

| Visual property | Real source (must stay true) | Artistic interpretation |
|---|---|---|
| Birth position | `GrainView.pos` (capture position) plus `pan` lateral offset | small scatter and birth jitter |
| Vertical home | `GrainView.pitch` maps to field y | buoyancy; the ember arcs away from home |
| Colour | `GrainView.pitch` through `pitchColour()` | warm head, cooler trail |
| Brightness, size | `GrainView.env` and the grain-size parameter | ember heat, core radius |
| Lifetime, fading | `GrainView.progress`; the grain vanishing from the snapshot | afterglow; a stray ember drifting on |
| Population, heat | `VisualState.numGrains`, `grainRate`, out RMS | turbulence, wind, gust |
| Flow bias | `VisualState.lfo`, the Scan parameter, `pan` | updraft direction, eddy |
| Idle field | the pre-existing deterministic dust bed | must stay cold and dim; never suggest absent audio |

What is **data**: the grain exists, where it is in the buffer, its pan, pitch,
age, envelope amplitude and brightness, and the overall activity of the engine.

What is **interpretation**: the ember's flight path, the buoyant arc, the
afterglow when its grain ends, the stray embers, the gust from output peaks.
These are visual choices. They follow the data, but a different set of choices
would be equally "correct".

What the fields do **not** claim:

- `GrainView.pos` is the read position in the capture buffer, not a physical
  position in a room. The field is a picture of the buffer and the grain's
  place in it.
- Colour is a mapping from pitch, not a spectrum analyser.
- Ember speed is not the literal playback rate of the grain; it is derived from
  activity and envelope, then damped and curved for legibility.
- A live grain's ember is *held near its data home*, not flown arbitrarily: the
  moving trajectories come mostly from the release after the grain ends (see
  [§5](#5-behaviour)). The cluster position is the data; the flight is the
  interpretation.
- Grain identity is **approximated**, not known — see
  [§4](#4-continuity-matching-anonymous-grains).

Heat still encodes data: more grains means more amber; an older or quieter grain
is dimmer.

## 4. Continuity: matching anonymous grains

Because there are no stable ids, the particle layer matches this frame's views to
last frame's particles. `matchGrainViews()` is a two-pass greedy matcher, not a
global assignment:

1. Each existing, still-attached particle looks for the best unclaimed view. The
   match key is a weighted distance:
   `3.2 × |Δ(capture pos + pan offset)| + 2.4 × |Δpitch row| + 0.25 × |Δpan| +
   0.02 × |Δpitch| + progress`.
   `progress` is the age fingerprint: a forward step is weighted at 1.1, while a
   backwards step (> 0.08) or a jump (> 0.65) is treated as a new voice plus a
   fixed penalty. A pair is accepted only under a fixed score threshold.
2. An existing particle with no acceptable view means its grain ended: it enters
   **afterglow** and is never matched again. This includes a grain that appeared
   in only one snapshot — it keeps a short fading ember instead of vanishing.
3. A view that stays unclaimed spawns a new particle at its data home.

This is an approximation, and it is documented as one:

- the matcher is **greedy in particle-slot order**, so it is not globally
  optimal; two grains with similar position, pitch and age can be **swapped**,
  and the visual result is two embers exchanging paths within the same cluster;
- a grain whose parameters jump can be treated as a death plus a birth;
- a very dense cloud can saturate the matcher's discrimination;
- when the particle cap is reached, the faintest *released* ember is recycled for
  a new grain; if every ember is still attached to a live grain, the new view is
  dropped (the population stays capped rather than glitching).

The design target is graceful degradation, never a glitch: unmatched views
always fall back to a birth at the data home, and unmatched particles — including
one-frame grains — always fall back to a fading afterglow. Deterministic RNG
seeding (the existing seeded `mt19937`) keeps preview frames and the CI hero
reproducible.

## 5. Behaviour

### 5.1 Emergence

A particle is born at its grain's data home: capture position plus a small pan
offset and jitter, at the grain's pitch row. Its radius comes from `env` and the
Grain (size) parameter; its energy comes from `env`. A small outward kick at
birth (0.045 field widths/s, partly following the real pan) and a gentle upward
impulse make the arrival visible. No particle is born without a `GrainView`.

While the grain is alive, its ember is **anchored** near that home by a spring
(6/s) with a thermal wobble whose amplitude grows with activity. That is what
produces stable cluster cores: the grains' real read positions stay legible
instead of drifting off into decoration.

### 5.2 Flight

When the grain ends, the ember is released. From that point it flies: buoyant
updraft that grows with activity (and with pitch — high grains rise, low ones are
less buoyant or sink), curl/eddy turbulence, a small push from `pan`, lateral bias from
`lfo` and `scan`, and air drag. Integration is sub-stepped (at most 25 ms per
step) so fast frames do not overshoot. A soft cooling layer near the top of the
field slows embers down and spreads them instead of letting them pile against the
edge. Trails are placed back along the velocity vector — one or two splats,
spaced by 0.022 s of flight — so the phosphor draws the path rather than a stack
of dots.

In short: the grain's life is the cluster; the ember's flight is the release.

### 5.3 Clustering and scatter

Grains that share `pos`/`pitch` share homes, so they overlap as cores with a
sparse fringe. Dense material raises smoothed activity, turbulence and the gust
term from output peaks (a peak jump above 0.45 adds a gust that decays at
1.6/s); sparse material gets fewer births, less turbulence, and a higher chance
of long-lived stray embers.

### 5.4 Extinction, afterglow and stray embers

When a view disappears the grain has ended. Its ember fades out with a smooth
curve over **0.35–0.80 s**. A small fraction become **stray embers**: 0.4% at
high density up to about 4.9% in sparse material, at most three at once, lasting
**2.0–3.4 s**, slightly larger and dimmer before they go out. Embers that leave
the field through the outer margin are dissolved early rather than piling up at
the edge.

### 5.5 Silence and release

No new embers are born without grain data. After the sound stops, attached
embers finish their afterglow: all but the rare strays are gone within about
0.8 s, and even the strays (at most three at once, up to 3.4 s) are gone within
about 3.4 s. In practice the field is effectively quiet within about 2 s, but a
few strays can linger. The cold, dim idle dust bed remains but stays low.
Release is not a freeze: embers cool and drift out, and silence is allowed to
look like silence.

One nuance: the engine's density floor (0.5 grain/s) can still let an occasional
grain read the buffer, so silence falls to a minimum rather than a literal zero.
The check below is built around that: births drop to the minimum and the
existing embers must decay.

The automated silence check runs on the same processor and editor pipeline
immediately after the dense phase (no reset), so it measures the embers that
were actually alive while playing. It steps 140 visual frames of digital silence
(≈2.33 s of visual time, ≈2.99 s of engine audio) and requires the live
population to fall to a handful or fewer, with no matched grains and a low
ember-energy proxy. Measured: dense peak 174 live embers / 138 in afterglow /
luminance proxy 1.000; at the end of the silence window **1 live ember (in
afterglow), 0 matched grains, luminance proxy 0.001** (caps 8.7 live, 6.9
afterglow, 0.100 proxy). That is effectively quiet, with a dim dust bed left
over.

### 5.6 Idle dust

The pre-existing `Mote` bed is unchanged in role. It is deterministic, cold and
dim, and it never pretends there is audio that is not there. When the engine is
cold it is deliberately quieter than in 1.0: energy is scaled to 42% and drift
speed to 65% at zero activity, rising to full strength as grains appear. When
grains are present it yields to them rather than competing.

### 5.7 Musical mapping

| What you play | What the field does |
|---|---|
| Note attack, rising density | new embers appear; population and heat rise |
| Dense texture | more overlap, faster motion and more amber (crowd-normalised so it does not wash out) |
| Sparse texture | fewer, slower clusters; more dark; more chance of a stray ember lingering |
| Pitch / tuning | vertical home and colour follow the grain pitch; high embers are more buoyant |
| Pan / spread | lateral offset and release direction; the data's stereo placement |
| Focus / spray | set where and how the engine reads the buffer (and the idle bed); the ember homes follow the resulting `pos`/`pan` views |
| LFO / scan | slow lateral flow bias on released embers; not a metronome |
| Freeze | grain data continues while input stops; the field follows the grains, not the input |
| Release / silence | births fall to the engine's minimum rate; clusters release and cool within about 0.8 s |

The motion is not beat-synced and not a random screensaver. It has its own
character, driven by Sintel's musical behaviour. Full-field modulation stays
below the brand's ~3 Hz flicker limit; no strobe.

### 5.8 Controls

Motion has no new parameters and no new UI controls. It is inferred from what
exists: Density (activity/turbulence/stray chance), Grain size (ember radius),
Pitch (home, colour, buoyancy), Scan and LFO (release drift), plus the grain
statistics in `VisualState`. Focus and Spray act upstream in the engine (where
and how wide it reads), so they reach the field through the resulting views.
Existing parameter contracts and automation are untouched.

### 5.9 Tuning constants

The numbers above are compile-time constants at the top of
[src/ui/GrainCloud.cpp](../src/ui/GrainCloud.cpp):

| Constant | Value | Effect |
|---|---|---|
| particle cap | 360 | hard bound on simulated embers |
| phosphor fade | 1/16 per frame (live) | short trails; the phosphor carries the afterglow |
| motion `dt` clamp | 1–50 ms | slow frames slow the motion instead of exploding it |
| integration substep | ≤ 3 steps, ≤ 25 ms | stable fast-frame integration |
| afterglow | 0.35–0.80 s | normal ember fade-out |
| stray ember | 2.0–3.4 s, max 3 | rare long drift |
| trail | one or two splats, 0.022 s apart | phosphor path rather than dots |
| match threshold | score ≤ 1.0 | acceptance bound for continuity |

## 6. Performance and real-time safety

- **UI only.** No changes to the audio callback, DSP code, contracts, snapshot
  format or parameter layout. The audio thread's work per block is unchanged.
- **Fixed-size particle storage.** `std::array<Particle, 360>` inside
  `GrainCloud`; there are no per-frame heap allocations. The matching pass uses a
  stack-local claimed-views array. The existing dust vector, phosphor buffer and
  caches behave as before.
- **Bounded cost.** The particle cap and a bounded number of splats bound the
  render cost: three core splats and up to two trail splats per visible
  particle, plus the unchanged dust bed. A mild crowd normalisation keeps a dense
  cloud from washing out to a bright plate. The phosphor fades 1/16 per frame
  (short trails), so the persistence does not accumulate into a static glow.
  The population does not grow with grain count above the cap.
- **Frame-rate robustness.** `updateParticles()` clamps `dt` to 1–50 ms and
  sub-steps the integration, so a slow host changes the speed of motion but not
  its character and cannot launch embers off screen. (The editor's own tick clamp
  is wider — 1–100 ms — the motion path tightens it further.)
- **Resize safety.** Particle coordinates are normalised field coordinates
  (0..1), so a resize does not disturb them; the phosphor buffer is reallocated
  through the existing guarded path. The motion QA checks run at 900×520,
  1400×800 and 1180×680.
- **Determinism.** `resetForDeterministicPreview()` clears particles, seeded
  state and the phosphor, and reseeds the RNG. The preview/CI hero path is fully
  timer-free: `advanceForPreview(0)` stops the wall clock, then 40 chunks of
  0.1 s of audio are fed while six fixed 1/60 s frames are stepped per chunk.
  There is no `runDispatchLoopUntil()` before the capture, so the hero does not
  depend on timer scheduling.
- **Stability.** Particle state is sanitised and clamped; the QA stats expose a
  `finite` flag so a non-finite particle fails a test rather than corrupting the
  frame. The `--motion` harness also hashes the rendered field frames and fails
  if they stop changing, so a frozen-screen regression cannot hide behind
  correct particle stats.
- **Cost budget checked in CI.** `--motion` fails if the dense-phase tick exceeds
  8 ms mean / 20 ms p95, or if tick + paint exceeds 16.7 ms mean (one 60 Hz
  frame). Measured at maximum density: tick mean 1.61 ms (p95 1.65 ms), offscreen
  paint mean 1.51 ms, tick + paint mean 3.12 ms.

## 7. Evidence and validation

All measurement is deterministic and needs no audio device. The scenario is a
480-frame sequence at 60 fps — 8.0 s of *visual* time. Each visual frame pushes
2×512 samples through the processor (the second block is zero-padded when the
chunk is shorter), so the engine actually advances ≈0.02133 s per frame: 491,520
samples = **10.24 s of engine audio** over the run. Timings in the tables are
visual-frame times; exact sample accounting is in
[EVIDENCE.md](issue-32/evidence/EVIDENCE.md). The same harness ran unchanged on
the old renderer (baseline, captured before the change) and on the new one.

The capture is reproducible too: two independent `--frames` runs on the new
renderer produced byte-identical `metrics.csv`
(sha256 `66f9c8494a75ed734956562852220b979aaf50ea09b90aa2701a006476f689d0`).

Evidence: [EVIDENCE.md](issue-32/evidence/EVIDENCE.md) has the method, tables and
commands; [before/](issue-32/evidence/before/) and
[after/](issue-32/evidence/after/) hold the stills and per-frame metrics. GIFs:
[before (frozen render)](issue-32/evidence/before/grain-field.gif),
[after (living particles)](issue-32/evidence/after/grain-field.gif),
[side by side](issue-32/evidence/before-after.gif). Logs:
[motion](issue-32/evidence/after/motion.log),
[soak](issue-32/evidence/after/soak.log),
[headless host](issue-32/evidence/after/headless-host.log),
[unit tests](issue-32/evidence/after/nagl_tests.log).

### 7.1 Before/after measurements

The same fixed scenario (480 visual frames = 8 s at 60 fps; dense → sparse →
near silence → return) was rendered with the old renderer (baseline, captured
before the change) and the new one. Per phase the harness records mean luminance,
bright-pixel centroid drift, mean absolute frame delta, static fraction, coverage
and active coverage.

| Metric | Before (1.0.4 renderer) | After (new renderer) |
|---|---|---|
| Mean luminance (dense / sparse / silence / return) | 0.0596 / 0.0548 / 0.0505 / 0.0501 | 0.0518 / 0.0499 / 0.0474 / 0.0493 |
| Bright-pixel centroid drift, dense phase | 0.28 px/frame mean (median 0.00) | 0.50 px/frame mean (median 0.18) |
| Mean absolute frame delta, dense phase | 0.00047 | 0.00054 (median 0.000302, p95 0.000697) |
| Frames effectively static (delta < 1e-4) | 98.1% (9 of 480 frames changed at all) | dense 0.7% (1 of 144, frame 0); sparse 48.3%; silence 92.5%; return 13.5% |
| Active coverage, dense phase | 0.33% | 0.56% |

The baseline mean luminance is slightly *higher* in dense material (0.0596 vs
0.0518). That is not evidence of a brighter or busier field: the two paths differ
in phosphor fade (1/32 before, 1/16 after), crowd normalisation and splat
energies, so the after field is deliberately more compact and dimmer. Brightness
is not the "more alive" measure; the movement metrics are: every comparable dense
frame now changes (median frame delta 0.000302, no static dense frames except
frame 0 which has no predecessor), active coverage rises from 0.33% to 0.56%, and
the field still goes quiet in silence.

| phase | median frame delta (before → after) | static frames (before → after) | active coverage (before → after) |
|---|---|---|---|
| dense | 0.000000 → 0.000302 | 97.9% → 0.7% | 0.33% → 0.56% |
| sparse | 0.000000 → 0.000107 | 98.3% → 48.3% | 0.24% → 0.30% |
| silence | 0.000000 → 0.000020 | 98.3% → 92.5% | 0.14% → 0.13% |
| return | 0.000000 → 0.000296 | 97.9% → 13.5% | 0.26% → 0.45% |

The high static figure in silence means different things before and after: before
it was a frozen render; after it is a field that has correctly decayed to
stillness (and the engine's minimum grain rate means "quiet", not "dead"). In
sparse material 48.3% of frames are still — late sparse material genuinely rests
between events (restraint, not a freeze). The motion and render regression checks
run in the dense phase, where the guard belongs.

### 7.2 Automated checks

```bash
./build/nagl_tests_artefacts/Release/nagl_tests
./build/nagl_preview_artefacts/Release/nagl_preview --motion
./build/nagl_preview_artefacts/Release/nagl_preview --soak
./build/nagl_preview_artefacts/Release/nagl_preview --headless-host
```

Results (full output in [EVIDENCE.md](issue-32/evidence/EVIDENCE.md)):

- `nagl_tests`: 167 checks, 0 failures.
- `--motion`: **PASS**. Dense phase: mean speed 0.1124 field widths/s, median
  nearest-neighbour displacement 1.09 px/frame, continuity 1.00, mean track
  0.16 s, 37.5 matched embers. Render guard: 60 distinct dense field hashes
  (floor 54) and 59/59 consecutive frames differ (floor 90%), so a frozen screen
  fails even if the particle stats still move. Silence (same pipeline, no reset):
  dense peak 174 live / 138 afterglow / luminance proxy 1.000; after 140 visual
  frames (≈2.33 s visual / ≈2.99 s engine audio): live 1, afterglow 1, matched 0,
  luminance proxy 0.001 (caps 8.7 / 6.9 / 0.100). Dense-vs-sparse matched ratio
  32.63× (37.0 vs 1.1). Cost: tick mean 1.61 ms, p95 1.65 ms; offscreen paint
  mean 1.51 ms; tick + paint mean 3.12 ms (cap 16.7 ms). Determinism: identical
  frame hash on two runs (`baf8b4e3c0e843fb`). Stability: finite state and
  positions within the field's edge tolerance at 900×520, 1400×800 and 1180×680.
- `--soak`: **SOAK PASS** (meanLuminance 0.07, whiteRatio 0.0000,
  clippedRatio 0.00000, animates 1, alphaOk 1).
- `--headless-host`: **HEADLESS-HOST PASS** (timerRunning 1, cloudAnimates 1,
  curveChanged 1, lfoChanged 1).
- Deterministic hero render: byte-identical across serial runs, sha256
  `550a0ef9dbdb502afe5a35f52c7e78fb0307101b5b6ac263e2f3631da7f38297`,
  1,862,593 bytes. The hero is fully timer-free (no wall-clock dispatch loop
  before the capture), so it shows the playing instrument deterministically
  rather than depending on timer scheduling.

The `--motion` checks cover: motion while playing (mean speed, frame-to-frame
nearest-neighbour displacement, continuity, mean track length), a render guard
(distinct dense field hashes, so a frozen screen fails even when particle stats
still move), silence decay on the same pipeline, dense-versus-sparse contrast,
bounded tick/paint cost, determinism (two runs, identical frame hash), and
stability (finite state, edge tolerance, resizes).

### 7.3 What the checks can and cannot show

The automated checks can show that embers move while audio plays, that identity
persists frame to frame, that the rendered field actually changes (the render
guard), that activity decays in silence, that dense material produces more active
embers than sparse material, that two runs are identical, that the tick cost is
bounded and that the state stays finite within the field's edge tolerance.

One nuance belongs here rather than in the pass/fail list: the motion check runs
in dense material on purpose. In very sparse material some consecutive frames
are genuinely still — restraint, not a freeze.

They **cannot** show that the motion feels connected to the playing, that the
grains fly rather than jitter, or that the movement leaves room for silence.
Those are perceptual questions with a listener in front of the instrument, and
they are exactly what issue #32 leaves open.

## 8. Scope and separation

- This is the **Sintel grain-field visualisation**, nothing else. No
  SpectraStrobe, no Ganzfeld, no audiovisual event bus, no general visual
  architecture.
- It is **independent of PR #29** (`feature/v2-exploration`); the two branches
  are not stacked and neither blocks the other. No v2 exploration files are
  touched.
- It changes no DSP algorithm, parameter id or range, preset, state-recall
  format, automation contract or real-time guarantee. No new build target; the
  QA harness extends the existing `nagl_preview` tool.
- The audio path is unchanged, so this iteration cannot change the sound. It
  changes what Sintel shows you about the sound.

## 9. Open acceptance — for Lonn

Issue #32 stays open until Lonn has played it. Automated checks are evidence of
mechanical behaviour, not artistic acceptance.

The question is not "do pixels move?" — the checks answer that. It is:

- Do these grains feel connected to what I'm playing?
- Do they fly, or merely jitter?
- Does the movement leave room for silence?

A practical first pass:

1. Play a chord, hold it, let the density build. The field should thicken and
   move, not just brighten.
2. Go sparse. The embers should slow, thin out and leave dark, not become a
   faint version of the dense cloud.
3. Stop. The afterglow should cool within about 2 s; the idle bed should be
   visibly cold, not a leftover performance.
4. Try freeze and release. Freeze should follow the grains; release should cool
   rather than cut.
5. Watch for strobe, jitter without travel, embers that ignore the playing, or
   constant random busyness.

If any of that is wrong, tune the behaviour with Lonn before expanding the
architecture.

## 10. Where the code lives

| File | Role |
|---|---|
| [src/ui/GrainCloud.cpp](../src/ui/GrainCloud.cpp) | particle tuning constants, `matchGrainViews()`, `spawnParticle()`, `beginAfterglow()`, `integrateParticles()`, `drawParticles()` / `drawParticleTrail()`, plus the unchanged dust and phosphor |
| [src/ui/GrainCloud.h](../src/ui/GrainCloud.h) | `Particle`, `ParticleStats`, `kMaxParticles`, QA hooks (UI only) |
| [src/ui/NagloedEditor.cpp](../src/ui/NagloedEditor.cpp) | 60 Hz tick and the deterministic preview frame-step |
| [src/core/Contracts.h](../src/core/Contracts.h) | `GrainView` / `VisualState` (unchanged) |
| [tools/preview_main.cpp](../tools/preview_main.cpp) | `--frames` capture and `--motion` QA harness |
| [docs/issue-32/evidence/](issue-32/evidence/) | stills, GIF, `EVIDENCE.md`, metrics |
| [docs/issue-32/BRIEF.md](issue-32/BRIEF.md) | internal design brief and evidence protocol |

Related reading: [BRAND.md](BRAND.md) (visual language),
[ARCHITECTURE.md](ARCHITECTURE.md) (layers and contracts),
[VALIDATION.md](VALIDATION.md) (existing automated checks).
