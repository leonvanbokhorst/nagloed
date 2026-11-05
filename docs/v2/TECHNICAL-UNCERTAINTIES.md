# NAGLOED v2 — technical uncertainties and assumptions

Register for the exploration of issue #14. **Out of scope for v1.0.4**; no
implementation obligation. This file records what we think we know, what is
proven in this checkout, and what must be measured first. The architecture
choices are in `docs/v2/ARCHITECTURE-OPTIONS.md`.

## Legend

- **Type of assumption:** `[I]` artistic intent · `[T]` technical
  solution/hypothesis · `[O]` open research question.
- **Fact/hypothesis:** *Fact* = demonstrable in this repo, the tests or an
  external specification. *Hypothesis* = not yet measured. *Partly* = partly
  proven, partly not.
- **Impact:** high = touches the core of the experience or v1 safety;
  medium = touches feasibility or quality; low = locally adjustable.
- **Status:** *proven* · *partly proven* · *open* · *phase-1 test* (offline
  experiment) · *phase-2 test* (in-plugin/companion).

---

## A. Time, control rate and perceived kinship

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U01 | `[T]` A grid of 16 samples (≈0.33 ms at 48 kHz) is fine enough as an event seam for shared audio/light events. The interval exists as a fact (`src/dsp/EngineDSP.h:73`, absolute sample counter in `EngineDSP.cpp:580-598`), but has never been used as an *event* grid. | Partly fact | The engine computes on this grid, but there is no measurement of the observable quality of events at this resolution; nor is it clear whether a coarser grid (1–5 ms) would suffice. | Renders with event grid 0.33/1/5/20 ms; listening/viewing test with the same cause and different offsets. | Medium | Open |
| U02 | `[I]` Kinship arises from shared cause and shared history, not from frame synchrony. This is the artistic premise of issue #14 ("related, but neither is a literal illustration"). | Hypothesis (intent) | The premise has never been tested in a working render; it is conceivable that the audience experiences no relation at all without any synchrony. | Two renders of the same score: (a) tightly synchronised, (b) shared cause with an 80–200 ms offset; comparative listening. | High | Phase-1 test |
| U03 | `[O]` Delta times of 0–50 ms are experienced as "simultaneous", 100–500 ms as "response", above ≈1 s as "memory". | Hypothesis | No perceptual measurement; moreover this differs per medium (light has no real onset like audio) and per person. | Structured A/B with Kasina + Ganzfeld, fixed deltas (0/16/50/120/250/600/1500 ms), observation form. | High | Open |
| U04 | `[O]` The sketch "abrupt extinction on the left → auditory impact" requires sample-accurate coupling. | Hypothesis | Follows from U03; if 50 ms already feels "simultaneous", sample accuracy is artistically irrelevant and technically expensive. | Render with 1 sample vs 1 frame (16 samples) vs 1 block offset; is the difference audible? | Low/medium | Open |
| U05 | `[T]` Reproducible renders require the event scheduler to work in sample time, not in host blocks. The DSP result is already block-size independent within 2% RMS (`tests/DspTests.cpp:699-735`). | Partly fact | Block invariance is proven for audio; nothing is proven for events (sorting, refractory periods, RNG derivation). Moreover 2% RMS is not a bit-identical guarantee. | Render the score at 128/512/4096 blocks; compare WAV and `events.csv`; then bit-identical re-render on the same platform. | High | Partly proven |
| U06 | `[T]` The existing offline path (`tools/render_main.cpp:45-115`, `tools/DemoScore.h:13-26`) is a usable basis for a v2 score renderer. | Partly fact | The path exists and works, but only knows audio automation; light, events and the post-DSP problem are missing. | Prototype in `experiments/`; measure whether the same score structure and WAV output are reusable. | Low | Phase-1 test |

## B. Audio, capture and the inaudible source

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U07 | `[I]` The dry input need not be audible; the real environment remains present through the listener's own ears. Issue #14 states this explicitly ("without requiring NAGLOED to manufacture an artificial always-on ambient passthrough"). | Hypothesis (intent) | An insert effect with only a wet signal is technically trivial, but whether that gives enough "porosity" artistically is untested; host routing (dry/wet) also determines this. | Mix test: source-only vs wet-only vs mix; listening test in a quiet and a noisy room. | High | Open |
| U08 | `[T]` The capture ring can serve as an exclusive grain source without the dry input leaking into the output. | Partly fact | The mix and input parameters exist (`src/core/EngineParams.h:48-50`), but "completely inaudible" is a routing and measurement question, not a parameter question; bleed via reverb/EAR is possible. | Measurement with `mix=0`: autocorrelation and spectral comparison between input and output; test with and without direct monitoring. | Medium | Phase-1 test |
| U09 | `[T]` The self-generated sources (Noise/Drone, `src/dsp/GrainEngine.cpp:408-491`) are artistically sufficient as an internal source without input. | Partly fact | The code exists and is tested for stability, but aesthetic suitability has never been assessed; these are existing NAGLOED sounds, not a v2 design. | Renders without input, 3–5 minutes, assessed for "Geborgen Geluid" (sheltered sound); A/B against capture material. | Medium | Open |
| U10 | `[T]` The 12 s capture ring plus freeze is enough for memory on a composition scale (minutes). | Hypothesis | The ring is hard 12 s (`src/dsp/GrainEngine.cpp:11`, `src/core/Version.h:18`); a "memory" that returns later requires either a second, slower buffer, or events/`remember` that keep material outside the ring. That is a design decision, not a setting. | Scenario test: fragment at t=0, return at t=90 s; audible difference with/without extra memory. | High | Open |
| U11 | `[T]` Grain feedback cannot run away under event-driven, dense scenarios. The existing DSP limits feedback (0–0.95, `src/dsp/GrainEngine.h:66`; scaling correction in `src/dsp/EngineDSP.cpp:413-414`, applied in `src/dsp/GrainEngine.cpp:196-204`) and tests prove decay for the existing configuration (`tests/DspTests.cpp:1267-1327`). | Partly fact | Proven for fixed parameters; not for an event engine combining density, feedback and light echoes. | Fuzz: 10,000 random scores with maximum density/feedback; assert decay and finite output. | High | Phase-2 test |
| U29 (supplementary to U10) | `[I/T]` Forgetting is a composable parameter (TTL, explicit `forget`, decay of a memory), not just the technical end of the capture ring. | Hypothesis (intent + technique) | The vocabulary in `ARCHITECTURE-OPTIONS.md` §5 proposes `remember`/`forget`, but whether listeners experience forgetting as musical material is untested; a TTL without audible decay is also artistically meaningless. | Two renders of the same score: one with hard `forget`, one with gradual decay; compare with "remembering nothing". | Medium | Phase-1 test |

## C. Light, SpectraStrobe and signal integrity

> Signal model, tolerances and the hardware protocol are in
> `docs/v2/SPECTRASTROBE.md`; here only the architectural uncertainty. Do not
> duplicate.

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U12 | `[T]` SpectraStrobe carriers do not survive the existing insert chain (low-pass, saturation, limiter, `src/dsp/EngineDSP.cpp:600-664`); light encoding must therefore happen after the NAGLOED DSP or offline. | Hypothesis with strong indication | The chain demonstrably contains destructive stages, but the exact damage per stage and per sample rate has not been measured; the carriers live at 18.2–19.7 kHz, where the chain intervenes by definition. | Corruption matrix: resample 44.1↔48, low-pass 16 kHz, saturation, MP3/AAC, gain; measure carrier amplitude and frequency. | High | Phase-1 test |
| U13 | `[T]` Post-processing injection or an alternative bus is reliable enough in AU/VST3 hosts. Issue #14 question 6 explicitly puts this in doubt. | Hypothesis | Hosts differ in bus layout, bounce path, realtime vs offline and sample-rate conversion; an encoder that works in one DAW does not automatically work in another. | Matrix: 2–3 DAWs × 44.1/48 kHz × realtime/offline bounce; measure with the analyser from the PoC. | High | Phase-2 test |
| U14 | `[T]` An offline export step suffices for the first experiment; live emission is not needed to answer the artistic question. | Hypothesis (design choice) | Practically attractive, but it is unknown whether an offline workflow gives the same experience as a live installation; the "little GLOW machine" idea also suggests playability. | Compare offline WAV playback on hardware with a realtime companion; observe the difference in the feel of playing. | Medium | Phase-1 test |
| U15 | `[O]` The carriers are inaudible to the listener and acceptable to bystanders. | Hypothesis | Audibility depends on age, hearing, transducers and playback level; "inaudible" is not a guarantee, and at loud playback 18–20 kHz can still be annoying or damaging to hearing. | Sine test with the target group, spectrum analysis on the hardware, level and comfort measurement. | Medium | Open |
| U16 | `[I/T]` Light is an independent world with its own colour, envelope and timing, not an illustration of audio. That requires a separate scheduler and state, not an extension of `VisualState`. | Hypothesis (intent + technique) | The intent is fixed; the technical form (own engine vs `VisualState` extension) is a design choice that has not been validated. | PoC with separate light envelopes and an audio envelope that deliberately deviates; assess whether the kinship remains. | High | Phase-1 test |

## D. Event engine, realtime safety and budgets

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U17 | `[T]` A bounded event engine can run on the audio thread without allocation or locks, within the existing CPU budget (≈1.9–2.2% at 64 grains; `docs/VALIDATION.md:50-51`). | Hypothesis | The existing engine proves 0 allocations (`tests/DspTests.cpp:1060-1119`), but a scheduler with queues, refractory timers and a light engine is new code with its own risks. | Prototype with the same allocation counter; CPU measurement at maximum budgets. | High | Phase-2 test |
| U18 | `[T]` Bounded queues with fixed capacity and an explicit drop policy suffice; event growth is never unbounded. | Hypothesis | Drop behaviour can be artistically disruptive (falling light grains) or technically invisible; the right policy is unknown. | Overload test: 10× budget, measure drops, audibility and stability; compare drop-oldest/drop-newest/coalesce. | High | Phase-2 test |
| U19 | `[T]` Refractory periods, rate budgets and causal depth ≤ 3 prevent runaway feedback between audio and light. | Hypothesis | A limit on paper is not proof; an adversarial score can consume budgets exactly or bypass them via `echo` chains. | Adversarial scores, formal invariant check afterwards, property-based testing on the scheduler. | High | Phase-2 test |
| U20 | `[T]` The event engine can stand apart from the existing DSP without changing `EngineParams`, `VisualState` or parameter IDs. | Hypothesis | In A this is self-evident; in B the engine touches `src/plugin` and possibly the contracts. The boundary has not yet been made hard. | Test compile-time isolation; the diff of `src/core` and `src/plugin` must stay empty outside a flag. | Medium | Phase-2 test |
| U21 | `[T]` v1 remains bit-for-bit untouched if v2 sits behind a build flag or in a separate binary. | Hypothesis | Build isolation is demonstrable via artefact hashes, but a flag in shared sources can still change behaviour (initialisation, layout, timing). | Hash of the v1.0.4 bundles before and after the addition; rerun tests; pluginval/auval. | High | To be tested |

## E. Determinism, reproducibility and AI

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U22 | `[T]` One seed per render plus an RNG per `(seed, eventId)` gives reproducible audio and light renders. | Partly fact | The existing grain RNG is seeded (`src/dsp/GrainEngine.cpp:88`, "nagl") and the editor hero is deterministic per build/platform (`README.md:148-150`), but cross-platform bit-identity is not proven, and float order and libm differences can spoil the result. | Re-render on the same platform (byte-identical) and on a second platform (compare with tolerance); note which parts are not bit-identical. | High | Phase-1 test |
| U23 | `[T]` AI stays outside the realtime loop: a score or parameter plan is compiled offline, there is no runtime inference. | Hypothesis (architecture choice) | No implementation yet; the boundary "generated offline" vs "generated at runtime" must be explicit in the design and in the documentation, otherwise inference creeps in after all. | Score-compiler PoC; check that the renderer contains no network/model calls; review of the data flow. | High | Open |
| U24 | `[O]` An offline render and a live performance feel artistically similar enough. | Hypothesis | Offline is deterministic and repeatable; live is not. It is unknown whether the audience experiences that difference as loss or as quality. | A/B: the same score offline vs live companion; observation and conversation with Lonn. | Medium | Open |

## F. Distributed and human/privacy

> Topology, protocol and failure modes are in `docs/v2/DISTRIBUTED.md`; safety
> and consent in `docs/v2/SAFETY-PRIVACY.md`. Here only the assumptions.

| ID | Assumption | Fact/hypothesis | Why uncertain | How to test | Impact | Status |
|---|---|---|---|---|---|---|
| U25 | `[T]` Event telegrams on a local network suffice for "rooms as co-listeners"; network audio is not needed in phase 1. | Hypothesis | It is unknown whether event exchange without shared audio gives enough coherence; OSC on a trusted network is also not automatically safe or reliable. | Two processes on one network, OSC telegrams, measure jitter/loss and assess the experience. | Medium | Open |
| U26 | `[T]` A loose clock (PTP/NTP/Ableton Link) is sufficient; sample-accurate sync is not required. | Hypothesis | Follows from U02/U03, but is not measured; if U03 turns out differently, this may tip. | Jitter and latency measurement between two nodes; listening test with different sync errors. | Medium | Open |
| U27 | `[I/O]` Duplicates, loss and drift of events are artistically acceptable as long as they are bounded (intentional drift). | Hypothesis (intent + research) | "Acceptable" is an aesthetic judgement that differs per situation; unbounded drift can sound like a fault. | Chaos test: packet loss 0/1/5/20%, duplicates, delay; observe and ask. | Medium | Open |
| U28 | `[I/T]` Consent and privacy are non-negotiable: recording visitors/environment only with permission, locally, no PII in telegrams, minimal retention. | Hypothesis (intent + design) | The intent is fixed, but the technical enforcement (data minimisation, logging, retention, deletion) has not been worked out or tested. | Design checklist + review against `docs/v2/SAFETY-PRIVACY.md`; inspection of events and logs for PII. | High | Open |
| U30 | `[T]` A2 ("the same score through the real v1 processor") can be done without changing `CMakeLists.txt` or `src/`. | Hypothesis | A new tool target requires a CMake change, and that is on the do-not-touch list; a separate build project is conceivable but untested. A2 also cannot literally replay the A1 programme, because v1 has different sources. | Work out the build path (separate project or approved exception); translate the A1 score to v1 sources and measure again. | Medium | Open |
| U31 | `[O]` The decoder derives the brightness scale from a 20 Hz-panned reference in a way that keeps the 25% coupling conservative. | Hypothesis (open) | The reference is continuously amplitude-modulated per ear and is zero at the extremes; a per-ear RMS interpretation makes 25% about 31.5% in practice. The decoder characteristic is not public. | Measurement on hardware (photodiode + comparative colour levels); otherwise choose the coupling more conservatively. | High | Open |

---

## Proven facts in this checkout (baseline)

These points need not be tested again; they do bound what a v2 design may
assume.

- The audio path is Sintel → Ruimte → EAR with output limiter and ±2.0 clamp
  (`src/dsp/EngineDSP.cpp:600-664`); latency 51 samples at 48 kHz
  (`docs/VALIDATION.md:52-53`).
- 12 s capture ring, 256 preallocated grains, per-sample scheduling
  (`src/dsp/GrainEngine.cpp:11-18,129-233`; `src/core/Version.h:17-18`).
- Control rate 16 samples on an absolute sample counter, hence block-size
  independent (`src/dsp/EngineDSP.h:73`, `EngineDSP.cpp:580-598`); 2% RMS
  invariance across 128/512/4096 (`tests/DspTests.cpp:699-735`).
- 0 allocations in `process()`/`setParams()`/`getVisual()`
  (`tests/DspTests.cpp:1060-1119`).
- `VisualState` is a display copy per block; the engine may send a subset
  (`src/core/Contracts.h:27-57`, `docs/ARCHITECTURE.md:34`).
- The plugin accepts no MIDI (`src/plugin/PluginProcessor.h:32`) and the
  engine ignores the MIDI buffer (`src/dsp/EngineDSP.cpp:502-504`).
- Parameters are read at block start; host automation is block-quantised
  (`src/core/Parameters.h:190-260`).
- The GUI reaches the engine exclusively via `VisualSource` and APVTS
  (`README.md:184-186`, `docs/ARCHITECTURE.md:21-25`).

## Top 5 uncertainties (greatest influence on direction)

1. **U02/U03 — perceived kinship at delta times.** If shared cause without
   synchrony does not work, the artistic core of the whole exploration falls
   away; if it does work, sample accuracy (U01/U04) is probably superfluous.
2. **U10 — memory beyond the 12 s ring.** Without extra memory, "return after
   the silence" is limited to what the ring contains; this determines whether
   A is only an encoder test or a real composition experiment.
3. **U12/U13 — SpectraStrobe does not survive the chain/host.** This is the
   hard technical gate: without reliable emission there is no light to
   compose with. Measure first, integrate later.
4. **U19 — runaway between audio and light.** The existing DSP is proven
   safe, but a new mutual coupling is precisely the scenario that issue #14
   explicitly forbids from running away.
5. **U23/U28 — AI and consent outside the loop.** Both are conditions, not
   features: no runtime inference and no recording without permission. They
   must be hard in the design, not in a guideline.

## Status of this register

This register is a snapshot at the start of the exploration (November 2026).
It is only valid as long as no measurements have been added; every completed
test should change the status and name the measurement setup used. Open points
are not a blocker for v1.0.4 — they are input for the question whether, and
when, Lonn activates the exploration.

**After the independent reviews (5 November 2026).** The technical review
largely confirmed the PoC's signal measurements, but found that M2 (peak vs
RMS), M6 (sidebands vs inter-carrier isolation), M7 (narrowband programme) and
M14 (incomplete matrix) were not the binding measurement they claimed to be.
These have been corrected in `SPECTRASTROBE.md` and the PoC; U30 and U31 were
added. The artistic review found that *Eerste Gloed* tests a written kinship,
not system autonomy or memory; that is now explicit in `EXPERIMENT-01.md`. See
`REVIEWS.md`.
