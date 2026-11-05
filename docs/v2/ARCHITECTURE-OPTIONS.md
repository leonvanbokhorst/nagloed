# NAGLOED v2 — architecture options

Exploration, not a work order. This document belongs to issue #14
(`leonvanbokhorst/nagloed`) and is **explicitly out of scope for v1.0.4**. It
describes what the current code does, which decompositions are conceivable and
which direction gives the first experiment the least risk. Nothing here is an
implementation proposal for `src/`, `CMakeLists.txt`, presets or release.

Reading guide: every claim is marked as **[F] fact in this checkout**,
**[T] technical solution/hypothesis**, **[I] artistic intent** or
**[O] open research question**. Assumptions with an ID (U01, U02, …) are worked
out in `docs/v2/TECHNICAL-UNCERTAINTIES.md`. This document is an **appendix**
to the exploration, not a gate and not a backlog: only the artistic vision, the
experiment and the safety sheet are currently authoritative.

---

## 1. Current architecture as found

### 1.1 Signal path and layers

| Layer | Location | What it does | Evidence |
|---|---|---|---|
| Contracts | `src/core/` | Frozen public types: parameter IDs, ranges, `EngineParams`, `VisualState`, `PresetAccess` | `docs/ARCHITECTURE.md:17-24` |
| DSP | `src/dsp/` | `Engine` (pimpl) around `EngineDSP`; Sintel → Ruimte → EAR | `src/dsp/EngineDSP.h:28-47` |
| GUI | `src/ui/` | Talks only via `VisualSource` + APVTS; never includes `src/dsp` | `README.md:184-186`, `docs/ARCHITECTURE.md:32-34` |
| Plugin glue | `src/plugin/` | `NagloedProcessor`: APVTS, bus layout, state, presets | `src/plugin/PluginProcessor.h:15-67` |
| Tools | `tools/` | `nagl_tests`, `nagl_render`, `nagl_preview` compile the same sources | `README.md:127-138`, `CMakeLists.txt:112-143` |

The audio path per block: clear extra channels, `readParams(apvts)` →
`engine.setParams(...)` → `engine.process(...)` → `engine.getVisual(v)` →
`visualChannel.publish(v)` (`src/plugin/PluginProcessor.cpp:41-55`). Inside the
engine: capture + grains (Sintel), FDN (Ruimte), analogue chain + output limiter
(EAR), with a hard ±2.0 clamp as the final safety stage
(`src/dsp/EngineDSP.cpp:600-664`). The latency is 51 samples at 48 kHz
(`docs/VALIDATION.md:52-53`).

### 1.2 The real seams

The question from issue #14 is whether `VisualState` can serve as an event bus.
It cannot: it is a **display channel**. The seams that actually exist:

| Seam | Location | Direction | Resolution | Suitable as an event bus? |
|---|---|---|---|---|
| `VisualState` / `SnapshotChannel` | `src/core/Contracts.h:27-57`, `src/core/Snapshot.h:9-32`; publication `PluginProcessor.cpp:52-54` | audio → GUI | 1× per block, 256 `GrainView` records | **No.** Display copy; the engine may send a subset (`docs/ARCHITECTURE.md:34`); no timestamps, no identity, no history, lossy |
| `EngineParams` / APVTS | `src/core/EngineParams.h:12-76`, `src/core/Parameters.h:193-260` | host/message → audio | 1× per block; parameters are set at block start | **No.** Current control values only; no events, no `t`, no cause |
| Audio block boundary | `PluginProcessor.cpp:41`, `EngineDSP.cpp:502-504` | host ↔ plugin | 128–4096 samples | **No.** Audio I/O only; no metadata channel |
| MIDI buffer | `PluginProcessor.h:32`, `EngineDSP.cpp:502-504` | host → plugin | — | **No.** `acceptsMidi()` is `false`; the engine explicitly ignores the buffer |
| `getVisual()` | `EngineDSP.cpp:671-701` | audio thread, immediately after `process()` | 1× per block | **No.** Same display purpose, solely for the GUI |
| Capture ring / grain pool | `src/dsp/GrainEngine.h:150-166` | internal, per sample | 12 s ring, 256 voices | **No.** Private behind the pimpl (`Engine.h:39-43`) |
| Offline score/automation | `tools/DemoScore.h:13-26`, `tools/render_main.cpp:81-106` | score → processor | per block, time-driven | **Almost.** Existing reproducible render path, but without a light or event model |
| APVTS state/presets | `PluginProcessor.cpp:88-136`, `presets/*.json` | storage | per save/load | **No.** Session state, no runtime channel |

Conclusion **[F]**: there is no sample-accurate event bus and no light output in
v1. The only real realtime seam is the audio block boundary plus parameter
updates at block start. A v2 event seam must be built, not reused. The internal
control rate is indeed 16 samples (`src/dsp/EngineDSP.h:73`, absolute sample
counter in `src/dsp/EngineDSP.cpp:580-598`), but that is an *internal*
computation rate; the plugin does not emit events at that resolution.

### 1.3 Realtime and threading facts

- **[F]** `prepare()` allocates; `process()`/`setParams()`/`getVisual()` do not.
  Proven with a global `operator new` counter: 0 allocations
  (`tests/DspTests.cpp:1060-1119`, `docs/VALIDATION.md:48-49`).
- **[F]** The audio result is block-size independent within 2% RMS for
  128/512/4096 (`tests/DspTests.cpp:699-735`).
- **[F]** CPU ≈ 1.9–2.2% of one core at 48 kHz/128 samples with a 64-grain
  cloud; ≈ 5.6% at 200 grains (`docs/VALIDATION.md:50-51`).
- **[F]** Parameters are read at block start; host automation is therefore
  block-quantised, even though the engine computes internally on 16-sample
  frames (`src/core/Parameters.h:190-260`).
- **[F]** The GUI cannot reach the engine outside `VisualSource`; the pimpl
  keeps `EngineDSP` private (`src/dsp/Engine.h:14-44`).
- **[F]** Determinism is only demonstrable per build/platform (offscreen
  editor image byte-identical, `README.md:148-150`); the demo render is an
  offline 48 kHz/24-bit WAV (`tools/render_main.cpp:49-51,74-76`).

### 1.4 What v2 lacks

**[F]** No event/gesture representation, no scheduler with timestamps, no
memory/forgetting, no light engine or SpectraStrobe encoder, no post-DSP
routing, no network transport, no consent/privacy layer. These are precisely
the seven research questions from issue #14 (Representation, Timing and
history, Audio architecture, SpectraStrobe encoding, Where in the signal chain,
Host behaviour, Human/hardware experience).

---

## 2. Four candidate decompositions

### 2.0 Overview

| | Reuse | Becomes independent | Boundary | Realtime | Biggest risk | Verdict |
|---|---|---|---|---|---|---|
| **A** External companion/offline renderer | v1 in full, render-tool pattern | score, event engine, light, encoder, renderer | file (score → WAV/CSV) | none (offline) | no live experience; hardware not yet proven | **Recommended first experiment** |
| **B** In-plugin control-rate event seam + light engine | audio path + capture ring as source | light engine, scheduler, memory, encoder | new internal API + bounded queue; build flag | audio thread or lock-free worker, 0.33 ms grid | touching v1, host routing, runaway | Phase 2, only after A + hardware |
| **C** Extracted reusable core + new v2 host | GrainEngine, Ruimte, EAR, EngineDSP | core library + separate v2 host | library API + event API; separate CMake target | same discipline, host chooses grid | major refactor, regression, double QA | Medium term, new branch |
| **D** Distributed peer model | event vocabulary + encoder per node | peers, network layer, local clock | bounded telegram protocol | network thread never in the audio path | runaway between rooms, drift, security | Horizon; detail in `docs/v2/DISTRIBUTED.md` |

### 2.A v1 untouched + external companion/offline renderer

**[I]** The first artistic proof is not "the plugin does light", but "one
shared event feels related without being synchronised" (issue #14). That
requires no plugin change.

**[T] Reuse.** The v1 plugin keeps running as it is (AU/VST3/standalone,
`README.md:64-68`). The existing offline pattern is reused:
`tools/render_main.cpp:45-115` shows a host-less loop with a declarative
score (`tools/DemoScore.h:13-26`), block size 512, 48 kHz and 24-bit WAV. For
audio that really has to go through v1: link the same sources headless as
`NAGLOED_HEADLESS` already does (`PluginProcessor.cpp:57-64`).

**[T] What becomes independent.** A score format (declarative, versioned), an
event/gesture vocabulary (hypothesis), a SpectraStrobe encoder, a renderer and
a verification step — all outside the plugin, in a separate directory or tool.
No new parameter IDs, no APVTS, no preset change. **What is not there yet:** a
real event/memory engine and a self-contained light engine with its own state.
The first PoC covers the signal half (encoder + integrity); the event engine is
a next step, not something already built.

**[T] Boundary/contract.** File-based and one-directional: score in →
WAV + `events.csv` + timeline out. One seed per render. No shared memory, no
realtime coupling, no plugin ABI change. The score format is the only new
"API" and it stays within the exploration.

**[T] Realtime/threading.** No realtime requirement in v1. The renderer is
single-threaded and deterministic; latency is the duration of a render, not
audio latency. Optionally a separate realtime companion process may come later,
but that is a second step.

**Risks.** No live experience; the artistic question "does this feel real in
person" stays open until the hardware test. Risk that a parallel architecture
grows that cannot be integrated later — mitigated by using the same event
vocabulary (§5) as B. SpectraStrobe routing in a host remains untested; that is
why hardware verification is a hard gate, not a detail. See
`docs/v2/SPECTRASTROBE.md` for signal integrity and tolerances.

**Assumptions to test.** U05, U06, U12, U14, U22 (determinism), plus the
artistic U02/U03.

**Verdict.** Design choice with the least risk for v1.0.4. This is also
literally the "suggested first experiment" from issue #14.

### 2.B In-plugin control-rate event seam + self-contained light engine

**[I]** If A convinces artistically, the next question is whether the coupling
can live *inside* the plugin without breaking the existing stability.

**[T] Reuse.** The full audio path and the capture ring as source. The existing
control rate of 16 samples (`EngineDSP.h:73`) can serve as the grid for an
event tap: one event frame per 16 samples (≈0.33 ms at 48 kHz), provided the
engine offers that as an *extra* channel next to `VisualState`.

**[T] What becomes independent.** A light engine with its own state, scheduler,
RNG and memory; an encoder; an optional event log. The audio DSP itself remains
untouched.

**[T] Boundary/contract.** A new internal API, for example
`EventSink::push(const Event&) noexcept` (audio thread) plus
`LightEngine::render(...)`, with fixed capacity and an explicit drop policy.
**Do not** reuse or extend `VisualState`. Everything behind a build flag
(`NAGLOED_V2_EXPERIMENTAL`) and never on the v1.0.x release branch.
SpectraStrobe carriers cannot pass through the existing chain
(`EngineDSP.cpp:600-664` contains low-pass, saturation, limiter); they must be
added after the DSP or via a separate bus/offline export. That is a routing
question, not a DSP question — see `docs/v2/SPECTRASTROBE.md`.

**[T] Realtime/threading.** Event engine on the audio thread or on a lock-free
worker with a bounded queue; never block, never allocate. CPU must stay within
the existing budget (U17). Refractory periods and budgets (U18/U19) are not an
optimisation here but a safety condition.

**Risks.** Touching `src/plugin` and `CMakeLists.txt` is unavoidable; that is
exactly what the guardrails of issue #14 forbid as long as v1 is not finished.
Host routing of carriers is unproven. An audio↔light loop can run away. The v1
realtime guarantees (0 allocations, block invariance) must be proven again with
the extra module.

**Assumptions to test.** U01–U04, U12, U13, U17–U21.

**Verdict.** Phase 2, only after a successful A + hardware verification. Not to
be built in this exploration.

### 2.C Extracted reusable core + new v2 host

**[I]** In the long term NAGLOED may not be one plugin but a granular core with
several hosts: the v1 plugin, an installation app, a renderer.

**[T] Reuse.** `GrainEngine`, `RuimteReverb`, `AnalogChain`, `Modulation`,
`DspUtil` and the `EngineDSP` orchestration; the tests largely. Extraction into
a library target (`nagl_core`) with a stable, host-agnostic API.

**[T] What becomes independent.** The core library; plus a new v2 host
(standalone or new plugin) that owns the event engine, light, routing and UI.
v1 stays frozen on the release branch.

**[T] Boundary/contract.** Library API (`prepare/reset/setParams/process/
getVisual`) plus event API; buffer ownership; no GUI dependency in the core.
Note: the current `Engine` signature uses `juce::AudioBuffer` and
`juce::MidiBuffer` (`src/dsp/Engine.h:34`), so "JUCE-free" is a separate
decision, not automatic.

**[T] Realtime/threading.** Same discipline as v1; the new host chooses block
size and control rate. Distribution and licensing do not change
(GPL-3.0-or-later, own JUCE licence; `README.md:205-213`).

**Risks.** Major refactor, CMake churn, regression risk, double QA and
maintenance, and a second product with its own release/validation. For the
first experiment this is overkill.

**Assumptions to test.** U05, U17, U20, U21.

**Verdict.** Medium term, as a separate branch and new target — never as an
in-place restructuring of v1.

### 2.D Distributed peer model

**[I]** Rooms as partially autonomous listeners: telegrams, memories that
travel between rooms, deliberate drift (issue #14; detail in
`docs/v2/DISTRIBUTED.md`).

**[T] Reuse.** The event vocabulary and the encoder per node; each node runs
its own audio and light world.

**[T] What becomes independent.** The peer/network layer: bounded telegrams
over OSC, local clock autonomy, drop policy, optional shared clock. No network
audio in phase 1.

**[T] Boundary/contract.** Minimal telegram (id, kind, time, parameters, seed,
cause), rate limits, trusted local network, no PII. The network thread never
touches the audio path directly.

**Risks.** Runaway feedback between rooms, clock drift, duplicates, security,
and much greater complexity than the first experiment justifies. This
reinforces U25–U28.

**Verdict.** Horizon. Only meaningful once the event vocabulary from A/B is
stable. Cross-ref: `docs/v2/DISTRIBUTED.md`.

---

## 3. Recommended direction for the first experiment

**[T]** Choose **A**, in two strictly separated steps, and keep v1.0.4
untouched:

1. **A1 — standalone offline renderer, without v1 code.** A declarative score
   of 60–90 s, an own deterministic renderer, a SpectraStrobe encoder and a
   verification step; output 48 kHz/24-bit stereo plus a 44.1 kHz/16-bit
   variant, dim/mid variants for the hardware ladder, a program-only A/B WAV,
   `events.csv` and a timeline. Substantively minimal: one light grain with
   movement and abrupt extinction, one auditory response with a *non-identical*
   envelope and deliberate delay, one transformed return much later, and at
   least one deliberately unanswered light event. **In this exploration A1
   covers the signal half, not the event engine or the memory model:** a real
   scheduler with budgets, refractory periods and `remember`/`forget` is not
   yet built and is a next step.
2. **A2 — the same score through the real v1 processor.** Only if A1 succeeds:
   a headless render tool in the spirit of `tools/render_main.cpp` that
   produces v1 audio, so that the same score material sounds with real NAGLOED
   material. No change to `src/`; though a separate build project or an
   explicitly approved CMake exception is needed, because a new target in
   `CMakeLists.txt` conflicts with the freeze. A2 does not reuse the A1
   programme literally: v1 has different sources (broadband noise/drone),
   which makes the M7 gate more challenging rather than less.

The concrete elaboration of A1 is worked out in `docs/v2/EXPERIMENT-01.md`
("Eerste Gloed"); the executable form belongs in `experiments/spectrastrobe-poc/`.

Hard gates **[T]**:

- SpectraStrobe verification on real hardware (Kasina + DeepVision Ganzfeld),
  conservative, with direct stop; without that measurement, no integration.
  Protocol and tolerances: `docs/v2/SPECTRASTROBE.md`.
- Deterministic re-render: byte-identical WAV and `events.csv` at equal seed,
  platform and sample rate (U22).
- Anti-runaway scenarios must remain demonstrably bounded (U19). This gate is
  only testable once an event engine exists; A1 does not cover it.
- If the carriers do not survive a host bounce or hardware detection is
  unreliable: **stop**, and do not go to B.

Only after that, and only with Lonn's explicit permission: **B** behind a
build flag, then **C** as a new branch, **D** as a research line.

---

## 4. Do not touch

This list is binding for the exploration. Changes to it go through a separate,
reviewed PR outside these documents.

| Do not touch | Why | Evidence |
|---|---|---|
| `src/core/ParamIDs.h` | Parameter IDs are permanent API; automation, presets and sessions address them | `ParamIDs.h:3-4` |
| Parameter layout, ranges, defaults, choice tables | GUI, preset validator and DSP adapter read from them; drift is a release bug | `src/core/Parameters.h:24-56,84-186` |
| `presets/*.json` and the state format | Sessions and pluginval depend on them | `PluginProcessor.cpp:88-136` |
| `Engine`/`EngineDSP` contract and pimpl | Realtime contract and information density of the DSP layer | `Engine.h:14-44`, `EngineDSP.h:34-47` |
| `GrainEngine` contract, 256 voices, 12 s ring | Frozen musical and realtime properties | `GrainEngine.h:43-47,105,150-152` |
| `VisualState`, `GrainView`, `SnapshotChannel` | Display contract; must not be reused as an event bus | `Contracts.h:27-57`, `Snapshot.h:9-32` |
| Signal order Sintel → Ruimte → EAR, latency 51 samples | Sound and host compensation | `ARCHITECTURE.md:3-13`, `EngineDSP.cpp:600-664`, `VALIDATION.md:52-53` |
| `CMakeLists.txt`, `CMakePresets.json`, `scripts/`, `.github/` | Build and release gates; tools compile the same sources | `CMakeLists.txt:70-143`, `README.md:140-163` |
| `tests/` thresholds and harnesses | Prove the realtime and determinism claims | `tests/DspTests.cpp:699-735,1060-1119`, `tools/preview_main.cpp:235-388` |
| `Demo/`, `docs/ui-preview.png`, `docs/demo-spectrogram.png` | Release artefacts with a deterministic hero claim | `README.md:144-168` |
| `README.md`, `docs/ARCHITECTURE.md`, `.gitignore` | Only additive in `docs/v2/`; no rewriting | task agreement |
| `main`, `release/1.0.4`, tag `v1.0.4` | v1 is the basis; the exploration is a separate branch | `docs/CONTRIBUTING.md:6-15` |
| GPL-3.0-or-later and the JUCE licence terms | Legal basis of the project | `README.md:205-213` |

---

## 5. Hypothesis: minimal, bounded event/gesture vocabulary

> **Status: hypothesis.** This is not an API, not code and not a proposal for
> `src/core/`. It is a testable vocabulary for an offline experiment, intended
> to let A and later B speak the same language. All numbers are
> **illustrative**, not starting values and not requirements. At least two
> alternatives remain explicitly open until a listening test decides:
> (a) continuous memory versus event memory, and (b) attention budget versus
> probability per event.

### 5.1 Principles

- **[I]** Relations are expressed as *events with a cause and a delta*, not as
  continuous mapping. A light event "belongs to" an audio event because they
  share the same cause or because one later echoes the other — not because
  they are frame-synchronised (issue #14).
- **[I]** Non-occurrence is a valid event (`rest`, `ignore`), as important as
  responding.
- **[T]** The vocabulary is bounded: fixed structure, fixed capacities,
  explicit budgets, one seed, sample time. Anything that does not fit is
  rejected or dropped, never silently unbounded.
- **[T]** It is completely separate from the existing DSP: it does not replace
  `EngineParams`, does not extend `VisualState`, adds no parameter IDs and does
  not compile into v1.
- **[T]** The term *gesture* collides with the existing brand vocabulary
  (BRAND.md defines "gesture" as a deliberate performance move: macro, freeze,
  scan sweep; `docs/BRAND.md:267`). In this vocabulary it is therefore called
  `score_gesture`: a sampled, named movement, not continuous modulation.

### 5.2 Event kinds

| Kind | Domain | Meaning | Required | Typical duration |
|---|---|---|---|---|
| `audio_grain` | audio | A single grain or short grain group from a source | source, position, duration, envelope | 5–500 ms |
| `light_grain` | light | A single light grain with colour, brightness, movement, decay | RGB, brightness, duration, position | 20 ms–8 s |
| `score_gesture` | both | Named, bounded movement (freeze, scan sweep, macro) | name, duration, target | 0.5–20 s |
| `rest` | — | Intentional non-event; blocks events in its window | start, duration | 0.5–30 s |
| `remember` | both | Capture material for later use | `memoryId`, source, lifetime | direct |
| `forget` | both | Release/expire a memory | `memoryId` | direct |
| `echo` | both | Later return of an event, transformed | `parentId`, delay, transformation | delay 10 ms–60 s |
| `ignore` | both | Deliberate non-response; terminates a causal chain | `causeId` | direct |

### 5.3 Properties (fixed, small struct)

Common: `schemaVersion`, `id` (monotonic u32), `kind`, `domain`,
`t` (int64 samples in the score sample rate), `durationSamples`, `source`
(input/capture/noise/drone/memory), `causeId` (0 = none), `delaySamples`, `seed`
(derived), `budgetClass` (`sparse`/`normal`/`dense`), `priority`.

Per domain, optional: `pitchSemis`, `position` (0..1 in the capture),
`pitchSpread`, `pan` (-1..1), `gain` (0..1), and for light `rgb` (3×0..1),
`brightness` (0..1), `eye` (-1..1), `movement` (target position at the end),
`envelope` (shape id), `persistence` (0..1). Empty = default value. No
free-text fields, no PII, no file paths in events.

### 5.4 Scheduling, refractory period, budgets

- **[T]** Events are sorted by `(t, id)` and played in sample time; the
  scheduler knows no wall clock and no host blocks. Late events are dropped or
  clamped forward according to an explicit policy, never caught up without
  bound.
- **[T]** **Refractory:** per `(kind, source)` and per causal chain a minimum
  spacing applies (starting value: 20 ms for `light_grain`, 40 ms for
  `audio_grain`, 250 ms for `echo`). Within the period an event is rejected,
  not delayed.
- **[T]** **Budgets:** at most 8 events/s total in `sparse`, 32/s in `normal`,
  128/s in `dense`; at most 16 simultaneous `light_grain`s; at most 8
  memories; at most 256 simultaneous audio grains (existing pool,
  `GrainEngine.h:105`); an attention budget per second in which each kind has
  a fixed cost (artistic grounding: "attention: a budget, not a duty cycle",
  `docs/v2/ARTISTIC-VISION.md` §4.2).
- **[T]** **Anti-runaway:** causal depth ≤ 3 (audio→light→audio counts as 2);
  `echo` of an `echo` only if the budget allows it; `ignore` terminates the
  chain; no event may find its own `causeId` chain; the renderer checks the
  invariants afterwards and fails hard on violation.
- **[T]** **Seed determinism:** one `seed` in the score; every event RNG is
  derived from `(seed, id)` with a deterministic hash, so the outcome does not
  depend on execution order. No `std::random_device`, no wall clock, fixed
  float order, sample rate fixed in the score.

### 5.5 How this stays separate from the existing DSP

- **[T]** `EngineParams` (`src/core/EngineParams.h:12-76`) remains the only
  parameter input of the audio engine; the vocabulary does not touch it.
- **[T]** `VisualState` stays display-only; no extra field is added
  (`src/core/Contracts.h:27-50`).
- **[T]** In A the renderer maps events onto its own synthesis (or later onto
  v1 automation via the `tools/render_main.cpp` pattern); in B a separate
  module would consume events. In both cases nothing changes in the existing
  DSP contracts or parameter IDs.
- **[T]** The first acceptance test is substantive, not technical: one shared
  event with non-identical envelopes and deliberate delay, one remembered
  return, one unanswered light event, and an adversarial scenario that shows
  the budgets hold.

### 5.6 Illustration (indicative, not an API)

```json
{
  "schemaVersion": 1,
  "sampleRate": 48000,
  "seed": 20261105,
  "durationSamples": 3840000,
  "events": [
    { "id": 1, "kind": "light_grain", "domain": "light", "t": 480000,
      "durationSamples": 14400, "rgb": [0.9, 0.25, 0.1], "brightness": 0.35,
      "eye": -0.6, "movement": 0.8, "envelope": "sharp-extinct" },
    { "id": 2, "kind": "echo", "domain": "audio", "t": 513600,
      "parentId": 1, "delaySamples": 19200, "transform": "low-transient" },
    { "id": 3, "kind": "rest", "domain": "both", "t": 700000,
      "durationSamples": 240000 },
    { "id": 4, "kind": "ignore", "domain": "light", "t": 1180000,
      "causeId": 3 }
  ]
}
```

---

## 6. Open questions and references

- **[O]** What is the smallest observable delta at which audio and light still
  count as related, and when does it become "response" or "coincidence"? (U03)
- **[O]** How long may a memory last before it loses its meaning, given the
  12 s capture ring? (U10)
- **[O]** Does a carrier survive a real host bounce, and if not, which export
  step is artistically acceptable? (`docs/v2/SPECTRASTROBE.md`; U12/U13)
- **[O]** Is an in-plugin event seam ever needed, or does an external
  companion suffice for the desired experience? (A vs. B)
- **[O]** What degree of network loss and drift is artistically desirable
  rather than disruptive? (`docs/v2/DISTRIBUTED.md`; U25–U27)

Related documents: `docs/v2/TECHNICAL-UNCERTAINTIES.md`,
`docs/v2/ARTISTIC-VISION.md`, `docs/v2/EXPERIMENT-01.md`,
`docs/v2/SPECTRASTROBE.md`, `docs/v2/DISTRIBUTED.md`,
`docs/v2/SAFETY-PRIVACY.md`, `docs/v2/IDEAS.md`, issue #14.
