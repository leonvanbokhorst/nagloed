# NAGLOED v2 — exploration: Geborgen Geluid

> **Status:** artistic and technical exploration, **not a v2 specification and not a
> release plan**. NAGLOED 1.0.4 is and remains the stable reference. Nothing has
> changed in the DSP contracts, parameter IDs, presets, build or release branch.
> The exploration was explicitly opened on 5 November 2026 by Lonn as a research
> assignment; issue #14 remains open and will not be implemented. The guardrails
> from that issue still apply. Everything here is an invitation, a hypothesis or
> an open question — not an implementation obligation.
>
> **Language:** English is the canonical documentation language for this
> exploration and for all new project artefacts. The underlying research was
> conducted and first written in Dutch; the Dutch originals remain in git
> history. Dutch proper names and the names of NAGLOED's engines (Sintel,
> Ruimte, EAR) are kept unchanged.
>
> **Reading path:** this README, [ARTISTIC-VISION.md](ARTISTIC-VISION.md),
> [EXPERIMENT-01.md](EXPERIMENT-01.md) and [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md)
> form the core. [ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md),
> [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md),
> [SPECTRASTROBE.md](SPECTRASTROBE.md), [DISTRIBUTED.md](DISTRIBUTED.md) and
> [IDEAS.md](IDEAS.md) are in-depth appendices, not a gate and not a backlog.

**The core in one sentence:** NAGLOED is not going to become an audiovisual
instrument that translates sound into light; it wants to become an environment
with its own memory and attention, in which sound, light, machines and people
can surprise one another — including by doing nothing.

---

## 1. Summary

- **The question is not what features could be added.** The question is how
  NAGLOED becomes an environment in which sound, light, machines and people can
  truly surprise one another. Artistic intent comes first; technology is
  subordinate to what proves musically worthwhile.
- **Kinship, not synesthesia.** Light and sound are kin with a shared cause or
  memory, not two channels of the same signal. They may respond to each other
  immediately, delayed, probabilistically or not at all.
- **Three research lines.** (1) granular audiovisual composition with
  SpectraStrobe/Kasina/DeepVision Ganzfeld; (2) musical agency and generative
  composition through an event/memory model; (3) distributed sonic environments
  as a long-term direction.
- **First experiment: *Eerste Gloed*.** A 72-second offline study with one
  written pair (light extinction + 180 ms late audio response), one late memory
  and one unanswered light grain. SpectraStrobe WAV, verified in advance (52 of
  53 PASS, 1 honest INFO, 0 FAIL), with a conservative hardware protocol and an
  honest separation between "written kinship" and "system autonomy".
- **Architecture: v1 untouchable.** The first step is a separate offline
  renderer (A1) that covers the signal half; a real event/memory engine and an
  in-plugin seam (B) are later phases with hard gates. A2, C and D are
  appendices, not a plan.
- **Top technical risks.** Perceived kinship at delta times (U02/U03),
  SpectraStrobe integrity across host/chain (U12/U13), runaway between audio and
  light (U19), the 12 s memory limit (U10) and the decoder brightness scale with
  a per-ear modulated reference (U31).
- **Independently reviewed.** Two reviewers (technical; artistic/safety/scope)
  reached REQUEST CHANGES at the review point. Their blockers have been
  addressed; the residual risks are explicitly still open in
  [REVIEWS.md](REVIEWS.md).
- **No v2 release planning.** Only once the first experiment does something
  musically will a second small experiment follow. Not before.

---

## 2. The artistic movement

NAGLOED 1.0.4 is an instrument of residue: capture, Sintel grains (*sintel* —
ember), Ruimte (room/space), EAR. The audiovisual direction is a later artistic
discovery, not a product requirement. *Geborgen geluid* — sheltered sound, the
child under the table at a family party — is the image: familiar, strange,
porous, temporary. The world goes on, but for a moment you are in it
differently.

The exploration sharpens the question of issue #14. What is genuinely new is
the dramaturgy of non-response, memory and forgetting as compositional
material, the attention budget instead of a duty cycle, and the choice to
experiment offline first before the plug-in is touched. The rest is ordering
and deepening. The full vision is in [ARTISTIC-VISION.md](ARTISTIC-VISION.md);
the ideas that go beyond the assignment are in [IDEAS.md](IDEAS.md).

**Artistic success criteria** are a compass, not a measuring stick: the moment
when you forget whether light and sound belong together; a silence that feels
right; a recognisable return that you cannot place; contrast with weight;
shared agency; and afterwards, not "nice effect" but "what was that". Per
experiment they are operationalised into observable questions and decision
rules (see [EXPERIMENT-01.md](EXPERIMENT-01.md) §5).

---

## 3. Three interrelated research lines

### 3.1 Granular audiovisual composition

**Intent.** Independent audio and light grains that arise, evolve and influence
one another. SpectraStrobe, Kasina and DeepVision Ganzfeld are the first
concrete environments: an output medium, not the conceptual model. The Ganzfeld
turns light into a diffuse field of colour instead of an image of LEDs.

**Current status.** The SpectraStrobe signal is well documented: four
high-frequency carriers (18.2 kHz reference with 20 Hz stereo panning;
18.7/19.2/19.7 kHz for R/G/B), brightness as amplitude relative to the
reference, six light channels (L/R per colour). What is missing is the decoder
characteristic of the Kasina and the actual signal integrity across
hosts, resampling and codecs. That is why measuring before integrating is a
hard gate. See [SPECTRASTROBE.md](SPECTRASTROBE.md).

**First concrete questions.** Does the signal survive a 44.1 ↔ 48 kHz path and
a DAW bounce? What is the minimum perceivable brightness? How does DeepVision
brightness compare with the standard Ganzfeld frames? Which delta between light
and sound still feels like kinship? And: how does the decoder derive brightness
from a per-ear modulated reference (U31)?

### 3.2 Musical agency and generative composition

**Intent.** An event/memory model that makes musical contrasts, autonomy,
chance, repetition, memory and silence possible — without making the realtime
DSP dependent on AI inference.

**Hypothesis: a small, bounded vocabulary.** Eight event kinds
(`audio_grain`, `light_grain`, `score_gesture`, `rest`, `remember`, `forget`,
`echo`, `ignore`), fixed properties, sample time, one seed, refractory periods,
attention budgets and causal depth ≤ 3. `rest` and `ignore` are equivalent
events, not an absence of data. This vocabulary is entirely separate from
`EngineParams`, `VisualState` and the existing DSP; it is a testable hypothesis,
not an API. The numbers are illustrative; alternatives (continuous memory vs.
event memory, budget vs. probability) remain open until a listening test
decides. Elaboration:
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §5.

**AI outside the realtime loop.** An interesting pattern is the *offstage
composer*: a script or model compiles a score offline; on the floor, only the
validated, read-only score is played. No inference, no network, no model in the
room. That makes AI an author rather than an unpredictable layer in the audio
thread. See [IDEAS.md](IDEAS.md) #1.

**Open questions.** When does a non-reaction feel like intent rather than a bug?
How long may a memory last before it becomes sentimental? How do we prevent a
shared cause from becoming a feedback loop? How do attention budgets relate to
musical tension?

### 3.3 Distributed sonic environments

**Intent.** Multiple NAGLOED instances, computers, rooms and performers
collaborating. What travels is not audio but a telegram: a short description of
an event with time, kind, colour and a degree of certainty. A memory can travel
between rooms, be misunderstood or never arrive. Local autonomy is the default;
connection is the exception.

**Technical direction (hypothesis).** Event-only over OSC/UDP, logical star with
a hub that only logs, bounded queues, rate limits, TTL/confidence decay, no
shared clock in the first test. Network audio (AES67/Dante/AVB) and
DMX/Art-Net/sACN are later, separate questions. Distribution is a distant
research direction, not a third step in a plan. Details:
[DISTRIBUTED.md](DISTRIBUTED.md).

---

## 4. Architecture: what to reuse, what to keep standalone

### 4.1 What the current code really is

- `VisualState` / `SnapshotChannel` is a **display channel** (once per block,
  lossy, no timestamps or identity), **not an event bus**. The existing
  `getVisual()` is for the GUI only.
- The only real realtime seam is the audio block boundary plus block-quantised
  APVTS parameters. The internal control rate of 16 samples is a computation
  rate, not a public event resolution.
- In v1 there is no light engine, no SpectraStrobe encoder, no event or memory
  model, no post-DSP routing and no network layer.
- Proven properties to preserve: 0 allocations in `process()`, 51 samples
  latency at 48 kHz, block-size independence within 2% RMS, 12 s capture ring,
  256 grains.

### 4.2 Four decompositions

| Option | What | Reuse | Standalone | Verdict |
|---|---|---|---|---|
| **A** | v1 untouched + external companion/offline renderer | v1 in full; pattern from `tools/render_main.cpp` | score, renderer, encoder, verification; later event/light engine | **First step** |
| **B** | in-plugin control-rate event seam + lightweight engine | audio path, capture ring | light engine, scheduler, memory, encoder | Later phase, after A + hardware |
| **C** | extracted core + new v2 host | GrainEngine, Ruimte, EAR, EngineDSP | core library + new host | Appendix / medium term |
| **D** | distributed peer model | event vocabulary and encoder per node | network layer, local clock | Distant research direction |

This table is an **architecture map, not a phasing or a backlog**. Only A1 is
relevant now; the rest is hypothesis.

### 4.3 Recommended direction

**A1 — separate offline renderer, without v1 code.** A declarative score, its
own deterministic renderer, a SpectraStrobe encoder and a verification step;
output as 48 kHz/24-bit WAV, 44.1 kHz/16-bit variant, dim/mid variants for the
hardware ladder, programme-only A/B WAV, `events.csv` and timeline. **A1 covers
the signal half, not the event engine or the memory model**: a real scheduler
with budgets, refractory periods and `remember`/`forget` has not been built
yet. The executable form is in `experiments/spectrastrobe-poc/` and
[EXPERIMENT-01.md](EXPERIMENT-01.md).

**A2 — the same score through the real v1 processor.** Only if A1 succeeds
musically: a headless render tool in the spirit of `tools/render_main.cpp`,
without changing `src/`. A new target conflicts with the frozen
`CMakeLists.txt`; that build path is an open point (U30). A2 does not reuse the
A1 programme literally: v1 material is more broadband, which makes the M7 gate
all the more challenging.

**Hard gates.** No integration without (1) SpectraStrobe verification on real
hardware, conservatively and with an immediate stop; (2) byte-identical
deterministic rerender; (3) demonstrably bounded anti-runaway scenarios — the
last of these is only testable once an event engine exists. If the carriers do
not survive a host bounce or hardware detection is unreliable: stop, and do not
move to B. B only with Lonn's explicit permission; C as a new branch; D as a
research line.

### 4.4 Do not touch

`src/core/ParamIDs.h`, parameter layout and ranges, `presets/*.json`, the
`Engine`/`EngineDSP` contract, the `GrainEngine` contract (256 voices, 12 s),
`VisualState`/`SnapshotChannel`, signal order and latency, `CMakeLists.txt`,
`CMakePresets.json`, `scripts/`, `.github/`, test thresholds, `Demo/` and the
release artefacts, `main`, `release/1.0.4` and tag `v1.0.4`, and the GPL/JUCE
licence terms. The full list with evidence is in
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §4.

---

## 5. Technical uncertainties

The full register counts 31 assumptions (U01–U31) with fact/hypothesis, test,
impact and status: [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md).
The most important:

| ID | Uncertainty | Status | First test |
|---|---|---|---|
| U02/U03 | Does kinship arise without synchrony, and at which delta? | Open | Two renders: tightly synchronous vs. shared cause with an 80–200 ms offset; hardware/listening test |
| U10 | The 12 s capture ring is too short for memories on a compositional scale | Open | Scenario test: fragment at t=0, return at t=90 s |
| U12/U13 | SpectraStrobe does not survive the destructive chain and host bounce; routing is unproven | Phase 1/2 | Corruption matrix and host matrix with the analyser from the PoC |
| U15 | Carriers are "inaudible" and acceptable to bystanders | Open | Sine test, spectrum analysis, comfort measurement |
| U17/U18 | A bounded event engine can be realtime-safe and allocation-free | To be tested later | Prototype with allocation counter and CPU measurement |
| U19 | Refractory periods and budgets prevent runaway between audio and light | To be tested later | Adversarial scores, invariant check |
| U22 | One seed yields reproducible audio and light renders | Partly | Byte-identical rerender; second platform with tolerance |
| U23 | AI stays outside the realtime loop | Open | Score compiler PoC; review of the data flow |
| U30 | A2 can work without a CMake change and with the same score | Open | Work out the build path; translate v1 sources |
| U31 | The decoder brightness scale with a 20 Hz panned reference is conservative | Open | Photodiode/hardware measurement; lower the coupling if needed |

**SpectraStrobe in brief.** The carriers live at 18.2–19.7 kHz, close to Nyquist
at 44.1 kHz. Any resampling, DAW low-pass, saturation, limiter, lossy codec,
mono sum or OS audio EQ can damage them; the encoder must therefore come after
all processing, and every file must receive a verification report. The Kasina's
decoder characteristic is not public; the ratio at 100% brightness is also
contradictory in the sources. The chosen 25% coupling **per carrier** is
conservative under both interpretations; the sum per ear may be higher and is
monitored separately. The 17–20 kHz programme requirement is met for the
current, narrowband material; for broadband v1 material it is **not**
demonstrated (honestly reported as INFO). See
[SPECTRASTROBE.md](SPECTRASTROBE.md).

**Open technical questions (selection).** What is the decoder passband? Does the
Kasina resample internally between 44.1 and 48 kHz? Which DAW/export step
produces a clean, unprocessed stereo bus? Is sample accuracy artistically
necessary, or is 50 ms already "simultaneous"? How long may an event or memory
queue be before drop behaviour becomes audible? How do we prevent an echo of an
echo from becoming a loop?

---

## 6. First experiment — *Eerste Gloed*

**The question.** Can one shared gesture — a light grain that fades out and an
auditory response that deliberately does not coincide with it — create a felt
kinship without frame synchrony? And is a deliberate non-response to a later
light grain perceivable as composition rather than as absence or error?

**The composition (72 s).** An amber light grain drifts from left to right and
fades out in 20 ms (the R4 minimum; still perceivable as abrupt). A low audio
impact answers 180 ms later with a different, much longer envelope. The light
then lingers briefly as a diffuse field. Between 28 and 46 s almost nothing
happens; one weak light grain at 38 s is deliberately left unanswered. At 46 s a
transformed memory returns — reversed, lowered, dimmer — with a light echo that
does not coincide. The piece ends in silence, without a final chord.

**What it is and is not.** The relationships are **written**, not emergent: the
composer places them, the renderer executes them. The memory at 46 s does not
fit inside v1's 12 s ring; that is a deliberate compositional choice and at the
same time an open design question. *Eerste Gloed* tests an experience, not
system autonomy.

**Why this is small and safe.** Rendered offline; no plug-in change, no host
integration, no microphone, no network, no visitors. Conservative brightness
(score 20%, hard clamp at 25% per carrier, dim/mid variants), short duration,
and a human stop in the protocol. Everything is in
`experiments/spectrastrobe-poc/`, outside the build and outside the release.

**Verification before hardware.** The PoC measures carrier frequencies, relative
amplitudes, real inter-carrier isolation, the 20 Hz pan modulation, programme
energy above 17 kHz (including an honest broadband probe), peak/true peak,
clipping, safety clamp, envelopes and determinism. Latest run: **52 of 53 PASS,
1 INFO, 0 FAIL**; corruption matrix 15 rows (9 PASS, 5 EXPECTED-DEGRADED, 1
INFO). Only then does the hardware protocol follow: Kasina + DeepVision
Ganzfeld, brightness ladder **dim 6.25% → mid 12.5% → full 20%**, first
44.1 kHz/16-bit, then 48 kHz, DeepVision only after comfort. See
[SPECTRASTROBE.md](SPECTRASTROBE.md) §4 and
`experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md`.

**What we measure.** Technical: the measurements above with explicit
tolerances. Perceptual in two phases: phase 0 is an explicit author test
without evidence; phase 1 is a pre-registered test with at least 5 non-authors,
randomised and blind conditions, three control versions (0 ms delay; 38 s grain
actually answered; programme-only), open questions before targeted questions
and pre-defined decision rules. The non-response condition is falsifiable: if
nobody notices the missing answer, that condition has not succeeded.

**What it does not prove.** No realtime plug-in feasibility, no host routing, no
distributed operation, no AI, no system autonomy or emergent memory, no
definitive architecture. Hardware validation must be done by Lonn; without a
Kasina/Ganzfeld that remains an open gate.

**Follow-up only if it does something musically.** First a second small
experiment on memory and decay (idea #2) or on ignoring (idea #3); only then a
conversation about architecture. No v2 release planning.

---

## 7. Safety, ethics and consent

Light close to and around the field of view is a different risk from a screen.
The Ganzfeld fills the field of view and sits close to the eyes; the DeepVision
is extra bright. That is why hard rules apply, not UI advice: by default no
repeated flicker, never more than three flashes per second, no sustained
3–30 Hz modulation in the colour channels, colour channels at most 25% of the
reference **per carrier** (default 6.25%), smooth envelopes (≥ 20 ms hard,
≥ 50 ms artistic), never from black to full, red extra carefully, every session
starting dark and low, and a stop that is always reachable by a human. The
prescribed 20 Hz reference panning is not visible colour modulation, but remains
registered as an open assumption. A real software panic of ≤ 50 ms is a
requirement for a later realtime phase, not for the offline PoC. No "safe for
everyone" claim. See [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) §3–4.

For visitor interaction, consent, privacy and temporariness apply: captured
sound is in principle fleeting, keeping it is a deliberate choice, no cloud and
no profiling, and bystanders are not automatically participants. A physical sign
(such as a card on the table) can turn listening into an agreed state.
Distributed telegrams contain no audio and no PII; network mode is off by
default and works only on a trusted local network. No medical, therapeutic or
wellness claims, including in preset names or UI. The carriers are not
guaranteed to be inaudible (U15); play at a fixed, notated level and do a short
comfort listening before the session.

---

## 8. Independent review

At review point commit `0f6422e` the exploration was independently reviewed by
two reviewers who did not contribute to the content: one on technical
feasibility and signal integrity, one on artistic intent, safety and
scope. Both reached **REQUEST CHANGES**. Their blockers — the
safety/verification claims (clamp, envelopes, stop, ladder, measurement
definitions) and the perceptual design (N=1, cueing, non-falsifiable) — have
been addressed. After a targeted re-check at commit `6c9e831`, **both reviewers
gave APPROVE WITH COMMENTS**; their addenda are in the review files. The
residual risks are explicitly still open: hardware M11–M13, decoder brightness
scale, host/DAW chain, broadband M7 for A2, the perceptual phase 1 that has not
yet been carried out, and cross-platform determinism. See
[REVIEWS.md](REVIEWS.md) for the full resolution and the original review files
in [reviews/](reviews/).

---

## 9. What we are not doing, and what is on the table now

**Explicitly not.** No change to NAGLOED 1.0.4 or its DSP contracts; no new
parameter IDs, presets or UI; no integration into the release branch; no v2
release planning or backlog; no AI inference in the realtime DSP; no always-on
ambient passthrough; no colour organ or one-to-one mapping; no
therapy/meditation claims; no network, DMX or network-audio code in the plug-in;
no cloud.

**What is on the table now.** (1) Lonn assesses this exploration, the reviews
and the PoC. (2) If the artistic direction is right: the hardware verification
with Kasina + DeepVision Ganzfeld, conservatively and with an immediate stop.
(3) Only with a green light and a musically convincing first experiment: a
second small experiment, and later perhaps A2. No large-scale build before
something has been discovered that is musically worthwhile.

---

## 10. Glossary

- **event** — a named moment in the composition (audio or light), with time,
  duration and cause.
- **gesture / pair** — one shared cause that expresses itself in two domains; in
  *Eerste Gloed* the extinction plus the 180 ms late impact.
- **light grain** — an independent light event with colour, brightness,
  duration, movement and position.
- **non-response / `ignore` / `rest`** — a deliberate non-response; a valid
  event, not missing data.
- **memory / `remember` / `forget`** — material that returns later, distorted or
  not; forgetting is an active process.
- **attention budget** — a bounded number of interventions per unit of time; an
  alternative to a fixed probability per event.
- **kinship** — a felt relationship between light and sound without frame
  synchrony or one-to-one mapping.

---

## 11. Documents and sources

| Document | Content |
|---|---|
| [ARTISTIC-VISION.md](ARTISTIC-VISION.md) | Sharpened artistic vision, success criteria, open artistic questions |
| [EXPERIMENT-01.md](EXPERIMENT-01.md) | *Eerste Gloed*: question, score, measurements, perceptual phases, success criteria |
| [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) | Photosensitivity, hard rules R1–R12, stop, consent, privacy, retention |
| [SPECTRASTROBE.md](SPECTRASTROBE.md) | Signal model, integrity matrix, measurement plan M1–M14, hardware protocol, encoder requirements |
| [ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) | Current architecture with evidence, decompositions A–D, event vocabulary, do-not-touch list |
| [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md) | 31 assumptions U01–U31 with test and status |
| [DISTRIBUTED.md](DISTRIBUTED.md) | Telegrams between rooms, topology, time, failure modes, security |
| [IDEAS.md](IDEAS.md) | Ten possibilities beyond the assignment, each with its smallest test |
| [REVIEWS.md](REVIEWS.md) | Review point, findings, resolution and residual risks |
| [reviews/](reviews/) | Original independent reviews A and B |
| `experiments/spectrastrobe-poc/` | Executable offline PoC, verification and hardware protocol |
| Issue [#14](https://github.com/leonvanbokhorst/nagloed/issues/14) | Original artistic direction and guardrails |

Main external sources: [Lumasonic SpectraStrobe specification](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[production/mixing notes](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md),
[SpiraStrober](https://github.com/Syluminose/SpiraStrober),
[OSC 1.0](https://ccrma.stanford.edu/groups/osc/spec-1_0.html),
[Art-Net 4](https://art-net.org.uk/downloads/art-net.pdf),
[AES67-2018 PICS](https://aes.org/wp-content/uploads/2024/05/AES67-2018-PICS-Summary_Merging-Technologies_HORUS_3.9_Audio-interface-to-and-from-AoIP-for-Recording.pdf),
[W3C WCAG 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html),
[Epilepsy Society](https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf).
