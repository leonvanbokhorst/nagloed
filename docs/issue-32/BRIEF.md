# Issue #32 — Sintel: make the grains fly — working brief

> Status: internal working brief for the issue #32 iteration. It records the
> diagnosis, the design contract and the evidence protocol. The user-facing
> deliverable is [docs/SINTEL-MOTION.md](../SINTEL-MOTION.md); this file may be
> folded into it before the PR is final.
>
> Branch: `feature/32-sintel-grains-fly`, based on `develop` (NOT on
> `feature/v2-exploration` / PR #29 — the two are independent).

## 1. Assignment (from issue #32)

- Investigate why Sintel currently looks static. Inspect the real UI/Sintel
  visual path: `GrainView`, `VisualState`, `SnapshotChannel`, rendering,
  update rate and the information actually available.
- Implement a first *playable* iteration of living Sintel particles:
  emerge, move, cluster, scatter, fade; sparse and dense material must feel
  different; silence and release must remain meaningful; an occasional stray
  ember.
- Motion is driven by Sintel's musical behaviour but may have its own character.
  Not a random screensaver, not beat-synced eye candy.
- Keep audio/DSP output, parameter contracts, state recall and real-time
  guarantees intact. No allocations/locks in the audio callback; bounded
  UI/render cost.
- Document what the visuals actually represent versus what is artistic
  interpretation.
- Validate with existing tests + a short visual demo for Lonn.
- Leave issue #32 OPEN for Lonn's experiential judgement. Do not claim artistic
  acceptance from automated checks.

## 2. Why it looks static today (diagnosis to verify and document)

The visual path is honest but static, for structural reasons:

1. **The UI has no grain identity.** `EngineDSP::getVisual()` copies a fresh
   `VisualState` once per audio block (lock-free `SnapshotChannel`, last writer
   wins). `GrainEngine::copyGrainViews()` emits only the *active* views, in
   pool-slot order. There is no persistent grain id, no event stream, no
   position history. `GrainView` (src/core/Contracts.h) carries
   `pos/progress/pan/pitch/env/brightness` only.
2. **The grain dot is a static projection of slow parameters.**
   `GrainCloud::drawGrains()` plots each view at
   `x = pos * fieldWidth + pan * 6px`, `y = pitchY(pitch)`. For one grain,
   `pan` and `pitch` are constant and `pos` advances by only
   `rate / (60 * bufferSeconds)` of the field per frame (~1.5 px/frame at a
   12 s buffer). Each grain therefore paints the *same pixel* for its whole
   life.
3. **`progress` and `env` are available but unused for motion.** They modulate
   radius/energy only; there is no birth, travel, cluster or death behaviour.
4. **The phosphor buffers the repetition.** A grain splats the same spot for
   several frames and the persistence buffer integrates it into a stationary
   glow. The afterglow, intended to show motion, instead flattens it.
5. **The only motion is decorative dust.** The `Mote` field drifts slowly and
   is (loosely) scaled by density/pitch, but it is not born from grain data and
   cannot show grain life. So the field reads as "slow ambient dust + blinking
   stationary dots", not as "grains flying".

Verification for the doc/PR: capture the rendered field over a fixed 8 s
scenario on the current code and measure temporal activity / bright-pixel
centroid drift / coverage; compare with the same scenario after the change.

## 3. Design contract (v1 of the motion model)

All work is **UI-side** (`src/ui/GrainCloud.*`). No changes to
`src/core/Contracts.h`, `src/dsp/*`, `src/plugin/*`, parameter ids/ranges,
presets, DSP behaviour or snapshot format. No new build target: extend the
existing `nagl_preview` tool. Do not touch the v2 exploration files.

### 3.1 Data vs interpretation (must be documented)

| Visual property | Real source (must stay true) | Artistic interpretation |
|---|---|---|
| Birth position | `GrainView.pos` (capture position) + `pan` lateral offset | small scatter, birth jitter |
| Vertical home | `GrainView.pitch` (field y = pitch) | buoyancy, arc away from home |
| Colour | `GrainView.pitch` via `pitchColour()` | warm head / cooler trail |
| Brightness, size | `GrainView.env`, grain size param | ember heat, core radius |
| Lifetime / fading | `GrainView.progress`, grain disappears from snapshot | afterglow, stray ember |
| Population / heat | `numGrains`, `grainRate`, output RMS | turbulence, wind, gust |
| Flow bias | `lfo`, `scan`, `pan` | ember updraft, eddy direction |
| Idle field | pre-existing deterministic dust bed (brand: allowed) | must stay cold/dim, never suggest absent audio |

Rule of thumb from `docs/BRAND.md`: *the embers are data, not bokeh*. Every
visible particle starts as a real `GrainView`; its flight is an interpretation
of that grain's life, never a synthetic decoration. Heat still encodes data
(more grains → more amber; older → dimmer). No strobe, no >3 Hz full-field
modulation.

### 3.2 Continuity strategy (no engine change)

Anonymous views are matched frame-to-frame by a lightweight nearest-neighbour
matcher:

- match key: predicted `progress` (monotonic, forward-only), then distance in
  `pos`, `pan`, `pitch`, `env`;
- unmatched view → spawn a particle at its data home;
- unmatched particle → grain ended → enter afterglow (fade out, keep flying);
- at particle cap: retire the oldest/faintest afterglow first;
- document that this is an approximation (no stable ids; two overlapping grains
  can be swapped). It must degrade gracefully, never glitch.

Particles are stored in a fixed-size `std::array` (no per-frame allocation).
Deterministic RNG seeded in `resetForDeterministicPreview()` so preview frames
and CI hero stay reproducible.

### 3.3 Motion character

- **Emergence**: a particle is born at its grain's data home with a small
  impulse, scaled by `env` and activity.
- **Flight**: buoyant updraft + lateral push from `pan` + curl/turbulence from
  activity; drag, gravity falling off; trail splats along the velocity vector
  so the phosphor draws the path.
- **Clustering**: grains that share `pos`/`pitch` share homes → overlapping
  cores with a sparse fringe; low-activity grains linger, hot grains fly.
- **Scatter**: density/activity drives turbulence and a gust term from output
  peaks; sparse material gets slow, isolated, long-lived embers.
- **Extinction**: when the grain disappears the ember cools (dim, slower) over
  ~0.4–1.2 s; a small fraction become stray embers (longer, dimmer drift).
- **Silence**: no births without grain data; afterglow decays within ~2 s; the
  cold, dim dust bed remains but must not pretend there is audio. Release =
  particles cool and fall, not freeze.
- **Bounded cost**: particle cap ~360; one or two splats per particle per
  frame; cost independent of host frame rate (dt-scaled), safe on resize.

### 3.4 Public QA hooks (UI only, in `GrainCloud.h`)

```cpp
struct ParticleStats
{
    int   live = 0, matched = 0, afterglow = 0, stray = 0;
    float meanSpeed = 0.0f;    // field widths per second
    float meanTravel = 0.0f;   // field widths from birth home
    float activity = 0.0f;     // 0..1 smoothed musical activity
    float luminanceProxy = 0.0f; // 0..1 estimate of ember energy drawn this frame
    bool  finite = true;       // false on any non-finite particle state
};
ParticleStats particleStatsForTest() const noexcept;
int copyParticlePositionsForTest (juce::Point<float>* out, int maxOut) const noexcept;
```

## 4. Evidence protocol

`nagl_preview` gets (all deterministic, no audio device):

1. `--frames <dir>` — fixed 8 s musical scenario (dense → sparse → near
   silence → return), writes field frames; prints per-frame metrics (mean
   luminance, bright-pixel centroid, mean abs frame delta, coverage). The same
   harness must run on the *old* renderer (baseline, captured before the change)
   and the new one.
2. `--motion` — QA checks with PASS/FAIL:
   - motion: while playing, particles move (mean speed / nearest displacement
     above a floor) and identity persists between frames;
   - silence: after audio stops, live embers decay to (near) zero within ~2 s;
   - density contrast: dense scenario has clearly more active embers than
     sparse;
   - determinism: two runs produce identical frames (CI can compare hashes);
   - bounded cost: mean tick time at max density under a documented cap;
   - stability: finite state, no out-of-bounds particle positions, resize safe.
3. `--soak` and the existing `--headless-host` must keep passing.
4. CI: add the `--motion` step to `.github/workflows/ci.yml` (and
   `scripts/validate.sh`).
5. Evidence assets: `docs/issue-32/` before/after stills + a short GIF (ffmpeg),
   plus a metrics table. Baseline assets must be captured BEFORE the renderer
   changes.

## 5. Deliverables / ownership

| Area | Files | Owner |
|---|---|---|
| Motion model + rendering | `src/ui/GrainCloud.h`, `src/ui/GrainCloud.cpp` | Lead |
| QA harness, frame capture, CI, evidence assets | `tools/preview_main.cpp`, `.github/workflows/ci.yml`, `scripts/validate.sh`, `docs/issue-32/evidence/` | harness-dev |
| User-facing doc + PR narrative | `docs/SINTEL-MOTION.md` | docs-writer |
| Independent review / acceptance check | (report only) | reviewer |

Do not edit another owner's files. Coordinate builds through the Lead: one
build directory (`build/`) is shared, do not run two builds at once.
