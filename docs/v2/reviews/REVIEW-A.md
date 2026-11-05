# Independent technical review A — NAGLOED v2 exploration

> Editorial note: this review was originally written in Dutch. It has been translated to English for the canonical documentation language; the Dutch original is preserved in git history (commit 58b1cd8).

> **Reviewer:** reviewer A (technical feasibility and signal integrity).
> **Not involved** in preparing the exploration or the PoC.
> **Branch:** `feature/v2-exploration`, commit `983d48c` (based on `develop`, v1.0.4).
> **Date:** 2026-11-05. **Environment:** macOS, CPython 3.14.5, no numpy/scipy.
> **Scope:** `docs/v2/*`, `experiments/spectrastrobe-poc/*`, issue #14, and the
> v1.0.4 baseline in `src/`.
>
> Every claim in this review file was re-measured by me or checked against the
> source. Where I could not verify something, this is stated explicitly.

---

## 1. Verdict

**REQUEST CHANGES.**

One blocker and eight major findings. The **audio signal itself is clean and
safe**: I independently recomputed all carriers, levels, pan modulation, peaks,
determinism and band-limiting, and they check out. The blocking points are in
the **safety and verification claims**: the PoC claims to follow the hard
encoder requirements in `SPECTRASTROBE.md` §6 but violates three of them, and
its own measurement suite rewards one of the violations. In addition, M2, M6,
M7 and M14 are not the measurements that the binding §3 table prescribes.

The required changes are small and concrete (add a clamp or split §6, correct
the measurement methods, retract claims). Once addressed, in my view this
becomes an APPROVE. The hardware step with the existing WAV is technically
justified; that is separate from this verdict.

---

## 2. v1.0.4 baseline — untouched, checked myself

| Control | Command | Result |
|---|---|---|
| Working tree | `git status --short --branch` | only `?? docs/v2/`, `?? experiments/` |
| Diff against develop | `git diff develop --stat` | empty |
| v1 surface | `git diff develop -- src CMakeLists.txt CMakePresets.json presets tests scripts .github README.md .gitignore` | empty (exit 0) |
| Tests | `./build/nagl_tests_artefacts/Release/nagl_tests` | **167 checks, 0 failures**, exit 0 |

The test output also confirms the architecture facts claimed by the documents:
`audio-thread allocations: 0`, `reported latency 51 samples`, identical RMS
`0.068621` for block sizes 128/512/4096, and CPU 2.04% of one core. Those claims
in `docs/v2/README.md` §4.1 and `ARCHITECTURE-OPTIONS.md` §1.3 are therefore
correct.

The PoC runs: `cd experiments/spectrastrobe-poc && ./run.sh` → **exit 0**,
31 PASS / 0 FAIL, corruption matrix 12 rows (8 PASS, 3 EXPECTED-DEGRADED,
1 INFO, 0 ADVISORY-FAIL).

---

## 3. What I verified myself (own measurements, no reuse of verify.py)

I wrote four of my own scripts (pure Python stdlib, own FFT/Goertzel, own RIFF
parser) and measured the WAVs again. Reproducible core figures:

| Measurement | Specification | My measurement | Assessment |
|---|---|---|---|
| 48k/24-bit RIFF chunks | no metadata | only `fmt ` (16 B) and `data` | OK |
| Carrier frequencies 48k (own FFT 65536, Hann, parabolic) | 18200/18700/19200/19700 ±1 Hz | 18199.99 / 18700.04 / 19199.97 / 19699.99 Hz | OK |
| Carrier frequencies 44.1k/16-bit | idem | 18200.03 / 18700.03 / 19200.04 / 19700.04 Hz | OK |
| Reference peak amplitude | −30.0 dBFS ± 0.5 | exactly −30.00 rendered; verify (5 ms) −29.96; my 1 ms estimator −29.69 (known bias) | OK |
| Reference RMS (literal §3-M2 method) | −30.0 dBFS ± 0.5 | **−33.0 dBFS** (combined 2-channel) | **deviates by 3 dB** |
| Colour ratio relative to reference (4 probes) | follows score ±0.5 dB | max deviation 0.33 dB | OK |
| Max. colour coupling (actual peak, not probe) | ≤ 25% | **20.0%** (L1 red, t=13.99 s) | OK, but higher than reported (18.6%) |
| Sum of 3 colour carriers in one ear (L1 peak) | ≤ 25%? | **29.5%** | definition unclear |
| Pan-LFO | 20 Hz ± 0.5, depth ≥ 99% | **20.000 Hz**, depth 100%, balance ±1.000 | OK |
| Equal-power panning | total carrier energy constant | √mean(aL²+aR²) = −29.995 dBFS, constant | OK |
| Sample peak 48k / 44.1k / programme | ≤ −6 dBFS, no clipping | −17.399 / −16.960 / −19.099 dBFS, 0 samples on the rail | OK |
| True peak (4×, 32-tap, 14–18 s) | ≤ −6 dBTP | −17.399 dBTP (= sample peak) | OK |
| Programme 17–20 kHz | ≤ −60 dBFS | −162 dBFS (but see M7 finding) | not probative |
| 8th-order lowpass @15.5 kHz (impulse response) | ≥48 dB/oct at 16 kHz | −3.01 @15.5k; −16.22 @17k; −30.84 @18.2k; −53.22 @19.7k; −108 @22k | steep enough, but see M7 |
| Metadata / dither | no metadata, no dither | 27,430 (24-bit) and 25,200 (16-bit) samples exactly equal to `clamp(round(float·scale))` | OK |
| 44.1k is native (no SRC) | native render | 16-bit values exactly equal to the float render at 44.1 kHz | OK |
| Determinism | two renders byte-identical | two fresh renders: all 6 files identical; also identical to `out/` | OK |
| Manifest | sha256 per output | all hashes correct; no timestamp | OK |
| Reference lead-in | ≥ 1 s | reference continuous from sample 0 (250 ms fade-in), first light at 8.0 s | OK |
| L1 extinction click-free | no discontinuity | max \|x[n]−x[n−1]\| = 0.047 at 14.0 s vs normal carrier slope 0.074 | OK |

The 44.1 kHz variant is therefore demonstrably a **genuine native render**: the
16-bit values match `clamp(round(float·32767))` of the float render at 44.1 kHz
exactly on every checked sample. An SRC of the 48 kHz master would not do that.
The "geen dither" ("no dither") requirement is also proven by this (dither would cause a
deviation of up to 1 LSB; I see 0).

External sources: I retrieved the Lumasonic *format specification* and
*production-and-mixing*. They confirm the four frequencies, the requirement of
"sine, 20 Hz, offset 0%, amplitude 100%" for the reference panning, the
6-channel model (three stereo colour tracks), the "4 stereo tracks, each −32 to
−24 dBFS" headroom, and the advice to taper the programme from 13 kHz with the
steepest cutoff around 17 kHz. The documents cite those sources correctly.

Sample of ±40 `file:line` references in `ARCHITECTURE-OPTIONS.md` and
`TECHNICAL-UNCERTAINTIES.md`: almost all are correct (e.g. `EngineDSP.cpp:600-664`
does contain the ±2.0 clamp on lines 643–644; `getVisual` at 671–701;
`kControlInterval = 16` at `EngineDSP.h:73`; `PluginProcessor.h:32`
`acceptsMidi() { return false; }`). Two references are wrong, see m12.

**The claim "VisualState is een displaykanaal, geen eventbus" ("VisualState is a display channel, not an event bus") is correct.**
`src/core/Contracts.h:27-57` contains only a 256-record `GrainView` array,
RMS/peak scalars and a `frame` counter; no per-event timestamp, no identity,
no history. `Snapshot.h:9-32` is a double buffer that is overwritten every
block. The A1/A2 recommendation (offline first, in-plugin only afterwards) is
architecturally the right order.

---

## 4. Findings

### BLOCKER

**B1 — The PoC claims to follow `SPECTRASTROBE.md` §6 but violates §6 in three
places; its own measurement suite rewards one of the violations.**

`experiments/spectrastrobe-poc/README.md:132` states it follows the "harde
encoder-eisen (§6)" ("hard encoder requirements (§6)"); `SPECTRASTROBE.md:284-286`
literally calls §6 "de implementatie-opdracht voor
`experiments/spectrastrobe-poc/`" ("the implementation brief for
`experiments/spectrastrobe-poc/`"). Three requirements are not met:

1. **The 25% colour ceiling is not enforced.** `SPECTRASTROBE.md:298-301` and
   `:335-339`: "De encoder begrenst zichzelf, ook als de artistieke score meer
   vraagt: max 25 % koppeling" ("The encoder limits itself, even if the artistic
   score asks for more: max 25% coupling"). `render.py:336-342` does
   `amp = amp_ref * brightness` with no clamp; `max_colour_ratio` is only copied
   into the manifest (`render.py:653`) and printed (`render.py:681-682`).
   Own test: score with `rgb = 0.90` → measured red carrier ≈ 90% of the
   reference peak (3.6× the ceiling). The *Eerste Gloed* ("First Glow") score
   stays at 20.0%, within the ceiling, so the artifact is currently safe; the
   claim "de encoder begrenst zichzelf" ("the encoder limits itself") is
   false. `SAFETY-PRIVACY.md:102` (R3) also
   says "het plafond gaat vóór artistieke wens" ("the ceiling takes precedence
   over artistic intent").
2. **The ≥ 20 ms envelope requirement is violated and the check inverts it.**
   `SPECTRASTROBE.md:321-322` ("gladde attacks/releases, ≥ 20 ms, geen stappen"
   — "smooth attacks/releases, ≥ 20 ms, no steps") and `SAFETY-PRIVACY.md:103`
   (R4). `score.json:90` has L1 fall from 1.0 to 0.0 in **8 ms**;
   `score.json:101` has L2 start at 0.35 with a **0 ms attack** (a step from 0 to
   0.35 × 0.055 = 1.9% brightness). `verify.py:502-504` explicitly passes the
   extinction with the criterion "≤ 10 ms" — exactly the opposite of the hard
   requirement. There is no check at all on envelope continuity, R4 or R5.
3. **A panic stop ≤ 50 ms does not exist.** `SPECTRASTROBE.md:323-324` requires
   carriers and reference to go click-free to zero within 50 ms. `render.py`
   contains no stop/panic function at all (grep for `panic|stop` only finds
   `fade_out`); the only stop is the 400 ms fade-out at the end
   (`score.json:23`). `HARDWARE-PROTOCOL.md:48` tests the *player's* stop, not
   the encoder's. For an offline WAV a real panic stop is not applicable; §6
   should then move that requirement explicitly to phase B instead of presenting
   it as "gehaald" ("achieved").

*Why a blocker:* this is the safety paragraph of the only document that
authorises the hardware step. A false compliance claim on a safety rule does not
belong in an exploration that positions itself as "conservatief, geen
overclaims" ("conservative, no overclaims").
The fix is small: clamp + hard-fail in `render.py`, reconcile the envelopes (or
document the 8 ms gesture explicitly as a one-off exception to R4), and move the
stop requirement to phase B.

### MAJOR

**M1 — M2 is measured differently from what the binding §3 table prescribes.**

`SPECTRASTROBE.md:168` prescribes: "RMS van 18,2 kHz-band, −30,0 dBFS
± 0,5 dB" ("RMS of the 18.2 kHz band, −30.0 dBFS ± 0.5 dB"). `verify.py:566-595`
however measures the **peak of the complex hypot-envelope** (−29.96 dBFS) and
mentions RMS only for information. My own measurement of the literal band RMS
(sum over L and R) is **−33.00 dBFS**; verify.py itself reports "totale RMS
−32.97 dBFS" ("total RMS"). An implementation that executes M2 literally per
the table would therefore **FAIL** by 3.0 dB. The cause is physical: with
equal-power panning the summed RMS of a carrier with peak amplitude A equals
A/√2 (−3.01 dB). The §6 definition ("−30 dBFS piekamplitude",
`SPECTRASTROBE.md:292` — "peak amplitude") is met; the §3 table is wrong or
unclear. Because §3 is called "bindend" ("binding"), it must become one of the
two: change the text to "piekamplitude", or actually measure RMS and adjust the
tolerance.

**M2 — M7 is substantively empty: the programme has no high-frequency content.**

`SPECTRASTROBE.md:173` requires ≤ −60 dBFS in 17–20 kHz for the programme-only
render. The PoC achieves −162 dBFS, but that is not due to the lowpass: the
synthesised programme is intrinsically narrow-band. Own band analysis of
`program-only-48k-24bit.wav`: 100–1000 Hz = −50.9 dB, 1–5 kHz = −110.9 dB,
5–10 kHz = −157.9 dB, 14–16 kHz = −164.4 dB, 17–20 kHz = −162.3 dB. So there is
nothing to suppress.

The filter actually used was measured (impulse response of the code itself):
−3.01 dB @15.5 kHz, −16.22 @17 kHz, −30.84 @18.2 kHz, −53.22 @19.7 kHz. That is
steep (bilinear frequency warping makes it much steeper near Nyquist than the
analogue 8th order), but for **broadband** programme material −60 dBFS in
17–20 kHz is not demonstrated and probably not achievable: at 17 kHz the
attenuation is 16 dB. This directly affects A2, because v1 has broadband
sources (`src/dsp/GrainEngine.cpp:408-411`, noise/drone; U09). §6's "taper vanaf
13 kHz" ("taper from 13 kHz") is also not met: at 13 kHz the response is
−0.02 dB. `verify.py` measures M7 only on this single, coincidentally
narrow-band render; the requirement is therefore not tested but "coincidentally
met" ("coincidentally met"). Add a broadband test (noise or a real v1 render)
and reformulate the requirement.

**M3 — M6 does not measure what it says: it is sideband energy, not inter-carrier crosstalk.**

`SPECTRASTROBE.md:172` asks for "bandenergie op de andere dragers ± 100 Hz"
("band energy at the other carriers ± 100 Hz"). `verify.py:651-667` however
compares within **the same** carrier the ±100 Hz band with the ±5 Hz band
(`leak = broad − narrow`). That is the energy in the sidebands around the
carrier itself, not the energy at the other carriers (which lie 500 Hz further
away and therefore fall outside ±100 Hz). Because the carriers are pure sines,
inter-carrier crosstalk is structurally zero; my measurement in a colour-free
window (29.5 s) gives red/green/blue at **−188/−180/−193 dBFS**. The reported
"crosstalk −58.5 dB" is therefore a mislabelled metric that by definition
cannot detect real channel leakage. A correct M6 renders one colour at a time
and measures the other carrier bands.

**M4 — M14 (corruption matrix) does not cover the binding §4.4 and does not test the colours.**

`SPECTRASTROBE.md:249-263` lists ten variants and `:180` requires "elk geval
gedocumenteerd als pass/fail" ("every case documented as pass/fail"). Three of
the ten are missing: cheap/naive SRC, L/R swapped, and MP3 128 kbps/joint
stereo; AAC 256 was added but is not in the table. More importantly, the matrix
**does not check the colour carriers**. `verify.py:871-878` (`preserved`)
requires for frequencies only `hz == hz and abs(hz - round(hz)) < 5000` — which
is tautologically true for every plausible frequency (a 1 kHz error would still
pass). The colour amplitudes and ratios are not tested at all. The conclusion
"MP3 320 / AAC 256 intact" is therefore unsupported for the channels that carry
brightness, while precisely U12/U13 (does the signal survive the chain?) is the
hard gate. In addition the matrix is advisory and `run.sh:17` runs `verify.py`
without `--strict-corruption`, so the exit code stays 0 even if every row fails.

**M5 — A1 is claimed as an "executable form", but the PoC implements only the
renderer + encoder.**

`ARCHITECTURE-OPTIONS.md:245-252` and `docs/v2/README.md:173-178` define A1 as:
declarative score, deterministic renderer, **light engine**, and a
SpectraStrobe encoder. `ARCHITECTURE-OPTIONS.md` §5 ties to it an event
vocabulary of eight kinds (`audio_grain`, `light_grain`, `score_gesture`,
`rest`, `remember`, `forget`, `echo`, `ignore`) that should let A and later B
speak the same language. The PoC contains virtually none of that: it is a
fixed, hand-written timeline with four light and three audio events — no light
engine with its own state, no scheduler, no budget, no refractory period, no
`remember`/`forget`/`echo`/`ignore`, no invariant check. Hard gate 3
("aantoonbaar begrensde anti-runaway-scenario's" — "demonstrably bounded
anti-runaway scenarios", `ARCHITECTURE-OPTIONS.md:269`, `docs/v2/README.md:186-187`)
is therefore not testable at this stage. That is not a reproach to the PoC — it
is useful and well made — but it is a scope overclaim in the documents. State
explicitly that A1 currently covers only the signal-integrity half.

**M6 — A2 is under-specified and conflicts with the freeze it is bound by.**

`ARCHITECTURE-OPTIONS.md:253-257`: A2 is "een headless render-tool in de geest
van `tools/render_main.cpp` ... Geen wijziging aan `src/`; hooguit een nieuw
tool-bestand in de verkenning." ("a headless render tool in the spirit of
`tools/render_main.cpp` ... No change to `src/`; at most a new tool file in the
exploration"). But tools are built via `CMakeLists.txt:112-143`, and that file
is on the do-not-touch list (`ARCHITECTURE-OPTIONS.md:292`). A new target
therefore requires either a CMake change or a separate build project; that is
an unresolved contradiction. In addition, A2 cannot reuse the A1 programme: A1
synthesises its own sweep + filtered noise (`render.py:183-218`), while v1 has
GrainEngine sources. "Dezelfde score door de echte v1-processor" ("the same score through the real v1 processor") therefore
yields different material, not the same. And the M2/M7 analysis above makes A2
with broadband v1 material precisely the risky part. Work out the build path
and the material mapping before A2.

**M7 — The recommended playback version is verified less than the master.**

`verify.py:1216-1217` runs `analyse_core` with `exhaustive=True` only on the
48k master; the 44.1k/16-bit variant skips M4 (isolation), M7 (programme leak)
and M8 (noise floor). That is exactly the file that, according to
`experiments/spectrastrobe-poc/README.md:73` and `HARDWARE-PROTOCOL.md:34`,
goes to the microSD card as the "veilige, aanbevolen weergaveversie" ("safe,
recommended playback version"). Precisely at 16-bit the noise floor in the
colour bands (M8) is relevant. Run the full set on both files.

**M8 — Documentation integrity: `REVIEWS.md` does not exist and the README
contains a literal placeholder.**

`docs/v2/README.md:308-311` states in the present tense that the exploration
"onafhankelijk is gereviewd door twee reviewers" ("has been independently
reviewed by two reviewers") and refers to `REVIEWS.md`; that file does not
exist, and the line ends with `[REVIEW-PLACEHOLDER]`. At the time of writing the
two review files (`REVIEW-A.md`, `REVIEW-B.md`) do exist, but `REVIEWS.md` has
not yet been created. This is easy to fix, but it is a claim about completed
work that is not yet complete.

### MINOR

- **m1 — "onhoorbaar" as fact.** `experiments/spectrastrobe-poc/README.md:47`
  says "De dragers zijn onhoorbaar hoog" ("The carriers are inaudibly high").
  `TECHNICAL-UNCERTAINTIES.md:55` (U15) in fact calls that same audibility
  **open** and mentions age, hearing, transducers and hearing damage. The PoC
  text should adopt the U15 qualification.
- **m2 — "Geen flikkering in 3–30 Hz" is unqualified.**
  `experiments/spectrastrobe-poc/README.md:37-39`. My measurement: the file
  contains a **20.000 Hz, 100%-deep L/R modulation** of the reference
  (balance ±1.000). That is the reference panning required by the format
  specification, not colour brightness — but `SAFETY-PRIVACY.md:100-103`
  (R1/R2) formulates a hard limit in exactly that band without an exception for
  the reference, and `SAFETY-PRIVACY.md:70-73` does mention the risk but does
  not measure it. Whether the decoder passes this through as brightness
  modulation is unmeasured (see §6). Formulate the claim as "geen *bedoelde*
  luminantiemodulatie in de kleurkanalen" ("no *intended* luminance modulation
  in the colour channels") and register the reference panning as an open
  assumption.
- **m3 — "veilig" in EXPERIMENT-01.** `docs/v2/EXPERIMENT-01.md:24`: "Het is
  **veilig**" ("It is **safe**"). `SAFETY-PRIVACY.md:111` (R12) explicitly
  forbids the word "veilig" ("safe"). Even though the context is "geen realtime licht/netwerk/microfoon" ("no realtime light/network/microphone"), the rule is thereby violated.
- **m4 — "ver onder de drempel" is too strong.**
  `experiments/spectrastrobe-poc/README.md:106`. The measured rest is −38.2 dB
  against a threshold of −30 dB: an 8 dB margin, not "far below".
- **m5 — Two different tolerance sets for the same experiment.**
  `EXPERIMENT-01.md:81-90` uses "volgt de score binnen enkele dB" ("follows the
  score within a few dB"), "voldoende onderdrukt" ("sufficiently suppressed")
  and "20 Hz detecteerbaar" ("20 Hz detectable"); `SPECTRASTROBE.md:165-180`
  (binding) uses ±0.5 dB, ≤ −60 dBFS and depth ≥ 99%. A reader can obtain a
  different PASS per document. Have EXPERIMENT-01 refer to the binding table
  instead of listing its own, looser criteria.
- **m6 — M4 probes are tautological.** `verify.py:686-714` measures mid
  balance on L2 around 16.05 s, where the score pan is exactly 0, and
  hard-pan isolation on L1 8.5–8.65 s, where `gain_r` is exactly 0. The test
  confirms the score values, not the encoder's channel isolation.
- **m7 — M3 does not report the true maximum coupling.** The report cites
  18.6%; the actual peak is 20.0% (L1, t=13.99 s). The four probes do not sit
  at the envelope peak. Scan the whole render or at least the envelope peak.
- **m8 — Define "25%" per carrier or per ear.** My measurement: the sum of the
  three colour carriers in one ear reaches **29.5%** of the reference at the
  L1 peak (0.20+0.075+0.02). The 25% cap currently applies per carrier; the
  text reads in some places as a total ceiling.
- **m9 — Score fields and functions that are not used.**
  `score.json:19` `pan_lfo_offset` is not read by `render.py`;
  `render.py:112-126` defines `pan_equal_power` and `pan_linear_0db` with
  extensive docstrings, but the colour panning is re-implemented inline
  (`render.py:330-335`). So the documented pan-law function is not the one
  executed. Bind the colours to `pan_linear_0db` or remove the dead function.
- **m10 — The manifest does not identify the code.** `render.py:638-666`
  contains `"generator": "render.py"` and the score hash, but no version or
  hash of `render.py` itself. A later code change cannot be traced from the
  manifest.
- **m11 — Hard-coded counts in the timeline.** `render.py:514` writes
  "4 lichtgebeurtenissen · 3 audiogebeurtenissen" ("4 light events · 3 audio
  events") and `render.py:502-504` hard-codes section labels. With a different
  score the SVG is no longer correct.
- **m12 — Two incorrect `file:line` references.**
  `ARCHITECTURE-OPTIONS.md:296` refers to `CONTRIBUTING.md:6-15`; the file is
  `docs/CONTRIBUTING.md` (content at 6-15 is correct). Likewise
  `ARCHITECTURE-OPTIONS.md:293` `preview_main.cpp:235-388`; the file is
  `tools/preview_main.cpp` (395 lines; the range exists). The other ±40 sampled
  references are correct.
- **m13 — M5 rest is an analysis artefact.** The off-phase is exactly zero at
  the LFO extreme; the reported −38.2 dB is how close a 1 ms block gets to the
  extreme. Robust across block sizes (B=24…192: −38.2 to −61.4 dB, always
  PASS), but it is not a residual-carrier measurement. State the measurement
  convention.
- **m14 — Lonn's activation is not recorded.** Issue #14 has **0 comments** and
  the documents state "de verkenning is expliciet geopend door Lonn" ("the
  exploration was explicitly opened by Lonn") (`docs/v2/README.md:6-7`). If
  that is correct it is a process point; record the activation in the issue,
  because the issue itself says that only Lonn can do that and that research
  subagents must not be started by default.

### NITS

- `docs/v2/EXPERIMENT-01.md:106`: typo "hetgeluid".
- `experiments/spectrastrobe-poc/README.md:43-45` and
  `HARDWARE-PROTOCOL.md:18-20` still refer to `SAFETY-PRIVACY.md` as
  "parallel opgesteld / zolang dat document er niet is" ("written in parallel /
  as long as that document does not exist"); it now exists.
- `render.py:293-294` reads `pan_lfo_shape` but not `pan_lfo_offset`; the score
  does contain that field.

---

## 5. Positive points (brief)

- v1.0.4 is demonstrably untouched; 167/167 tests green; the exploration sits
  entirely in new, separate directories.
- The audio signal is clean and reproducible: frequencies within 0.04 Hz,
  level exactly −30 dBFS peak, colour ratios within 0.33 dB of the score,
  per-sample 20 Hz equal-power panning, no clipping, no metadata, no dither,
  byte-identical rerenders.
- The 44.1 kHz variant is a genuine native render, not an SRC — that is proven
  hard by the quantisation equality, not merely asserted.
- The order "programme fully processed first, then carriers, then nothing" is
  enforced in the code (`render.py:353-367`) and is correct.
- Good documentation hygiene: `[bewijs]/[inferentie]/[open]`
  (evidence/inference/open) markers, source links, open questions, failure
  modes, a hardware protocol with stop criteria and an explicit "wat dit niet
  bewijst" ("what this does not prove") section. No wellness or therapy claims.
  The architecture facts about v1 are correct.
- The A1-first recommendation is the right low-risk first step; nothing in the
  PoC touches the plugin, CMake, presets or release.

---

## 6. What I did not verify

- **Hardware (M11–M13)**: no Kasina/DeepVision available. Detection, brightness,
  colour, flicker, stop latency and the photodiode measurement are unmeasured.
- **Host/DAW behaviour and playback chain**: no DAW bounce, no audio interface,
  no Bluetooth/lossy playback. I saw the corruption matrix of `verify.py` run
  via `run.sh`, but did not independently recompute the MP3/AAC results.
- **Decoder behaviour**: passband, amplitude scale, behaviour on sudden stop,
  and whether the 20 Hz reference panning comes through as luminance
  modulation.
- **Cross-platform determinism**: tested only on this machine and CPython
  3.14.5; libm differences between platforms were not investigated (U22).
- **Full C++ build from scratch**: I used the existing Release binary.
- **External sources partly**: I retrieved the Lumasonic format and production
  documents and the core claims are correct; the forum conflict over 1:1 vs
  1:4, the "dithering uit" ("dither off") advice, the SpiraStrober code and the Kasina firmware
  claims I did not verify.
- **Artistic and safety considerations beyond the signal**: that is the
  territory of the parallel reviewer (REVIEW-B).

---

## 7. The three most important remaining uncertainties

1. **How does the decoder derive the brightness scale from a 20 Hz-panned
   reference?** The spec says "amplitude van de kleur ten opzichte van de referentie" ("amplitude of the colour relative to the reference"), but the reference is hard L/R alternated per ear. If the decoder
   measures per ear, the reference is continuously amplitude-modulated per ear
   and zero at the extremes, and the 25% coupling becomes much larger in
   practice (my measurement: 31.5% under the per-ear RMS interpretation). If it
   measures mono/peak, then 20% is conservative. This determines both the
   artistic band and flicker safety, and it is nowhere registered as an
   assumption.
2. **Does the signal survive the real playback chain?** The offline WAV is
   clean, but the card/Kasina DAC/microSD chain, 44.1 vs 48 kHz and 16 vs 24-bit
   are unmeasured, and the existing corruption matrix does not test the colour
   channels. The U12/U13 gate is therefore still open, even though it appears
   to be covered by "PASS".
3. **Does A2 with real v1 material achieve carrier protection?** The measured
   programme lowpass gives −16 dB at 17 kHz and −31 dB at 18.2 kHz; v1 has
   broadband noise/drone sources. With a broadband programme the −60 dBFS
   requirement in 17–20 kHz is not automatically met, and the build path for A2
   (new tool target vs. frozen `CMakeLists.txt`) is not yet resolved.

---

## 8. Recommended actions, in order

1. **B1**: implement the 25% clamp (or hard-fail) in `render.py` and a global
   M3 scan; reconcile §6/R4 with the 8 ms and 0 ms envelopes (document an
   explicit one-off exception or change the score); move the panic-stop
   requirement to phase B and say so in §6.
2. **M1/M3/M4**: fix the M2 method (RMS or peak, consistent with the table),
   implement M6 as true inter-carrier isolation, and extend M14 to all ten
   §4.4 variants including colour testing; make the integrity rows
   non-advisory.
3. **M2/M6**: add a broadband M7 test (noise or a real v1 render) and work out
   the A2 build path and the material mapping.
4. **M5/M8**: scope A1 explicitly as "encoder + signaalintegriteit, nog geen event-engine" ("encoder + signal integrity, not yet an event engine"), and fill in `REVIEWS.md` / remove the placeholder.
5. **m1–m5, m12**: retract the overclaims ("onhoorbaar", "geen flikkering in
   3–30 Hz", "veilig"), align the tolerances and fix the two citations.

---

## 9. Addendum — targeted re-check after remediation

**Date:** 2026-11-05. **Commit:** `6c9e831` ("fix(v2): address independent
review findings"), delta `0f6422e..6c9e831`. **Method:** targeted re-check of
only the blocker and the most important majors, not a full re-review. I re-read
the changed code and documents, ran `./run.sh` myself (exit 0, 52 PASS / 1 INFO
/ 0 FAIL, 15-row corruption matrix), independently recomputed the adversarial
clamp, and re-checked the v1 baseline.

### Resolved?

| Point | Status | Evidence |
|---|---|---|
| **B1.1** 25% colour ceiling enforced | **YES** | `render.py:398-411`: `requested = rgb × colour_scale`, `applied = min(requested, max_ratio)`, clamp log + manifest. Independent test (own script, score `rgb=0.90`): all 12 requests logged with `applied_ratio = 0.25`; colour carrier = exactly 0.25 × reference peak amplitude. `sa-clamp-adversarieel` PASS, measured max 24.44%. |
| **B1.2** Envelopes R4/R5 | **YES** | `score.json`: L1 extinction 20 ms (`[5.98,1.0]→[6.0,0.0]`), L2 fade-in 50 ms (`[0,0]→[0.05,0.35]`). `render.py:133-196` (`validate_score`) rejects a score if an env does not start at (0,0) / end at (duration,0) or if attack/release < 20 ms. `score-envelope-regels` PASS, `score-L1-extinctie` 20.0 ms PASS, `score-L2-fadein` 50 ms PASS. |
| **B1.3** Panic-stop claim | **YES** (correctly retracted) | `score.json` safety.stop: "GEEN encoder-panic en GEEN geteste 50 ms-softwareramp" ("NO encoder panic and NO tested 50 ms software ramp"); `SPECTRASTROBE.md:332-336` and `:357`; `HARDWARE-PROTOCOL.md:34-37,138`. The 50 ms requirement is explicitly in phase B. |
| **B1.4** Brightness ladder | **YES** | `render.py:45-47` exposures full 20% / mid 12.5% / dim 6.25%; separate `spectrastrobe-{full,mid,dim}-44k1-16bit.wav` (present in `out/`); `ladder-full/mid/dim` PASS (max deviation ≤ 0.10 dB) and `ladder-orde` PASS (6.20% < 12.38% < 19.79%). |
| **M1** M2 peak instead of RMS | **YES** | `SPECTRASTROBE.md:168` now measures the "piekamplitude van de 18,2 kHz-band" ("peak amplitude of the 18.2 kHz band"); band RMS reported separately for information. `m2-referentieniveau-48k` PASS: peak −29.96 dBFS, band RMS −36.02 dBFS informational. |
| **M2** M7 broadband | **YES** (honest INFO) | New broadband probe (noise through the same LPF): 17 kHz −19.2 dB, 18.2 kHz −34.0 dB, 19.7 kHz −56.3 dB, labelled **INFO** (`m7-breedband-eerlijk`), with explicit text that the −60 dBFS requirement is not demonstrated for broadband v1 material. No false PASS. |
| **M3** M6 inter-carrier | **YES** | `verify.py:734-757` + `intercarrier_probe`: renders one probe per colour with one colour at a time and measures the other carrier bands. `m6-inter-drager-isolatie` PASS: −159.1 / −151.1 / −152.4 dB; measurement floor and open decoder bandwidth explicitly noted. |
| **M4** M14 extended + colours | **YES** | 15 rows, including `mp3_128_joint` (EXPECTED-DEGRADED, colour error 14.5 dB), `naive_src` (EXPECTED-DEGRADED), `lr_swap`; `preserved()` now checks colour frequencies ±1 Hz and colour ratios ±2 dB. The matrix remains advisory (documented); colour errors are reported per row. |
| **M5** A1 scope | **YES** | `ARCHITECTURE-OPTIONS.md:250-259,281` and `docs/v2/README.md:184-188` now state explicitly that A1 covers only the signal half (encoder + integrity), not the event engine, the memory or the budgets; gate 3 is "pas toetsbaar zodra er een event-engine bestaat" ("only testable once an event engine exists"). |
| **M6** A2 build path | **YES** (as an open point) | Registered as U30 (`TECHNICAL-UNCERTAINTIES.md`), with an explicit note that A2 requires a separate build project or CMake exception and does not literally reuse the A1 programme (`ARCHITECTURE-OPTIONS.md:261-266`, `docs/v2/README.md:193-196`). |
| **M7** 44.1/16 full set | **YES** | `m4-balans-44k1`, `m7-programmaleak-44k1`, `m8-ruisvloer-44k1` now run; additional `program-only-44k1-16bit.wav` present. |
| **M8** review claim/REVIEWS.md | **YES** | `docs/v2/REVIEWS.md` exists and processes the findings; `docs/v2/README.md:344-356` refers to the review point and the residual risks; placeholder removed. |
| **m1/m2/m3/m5** overclaims | **YES** | "onhoorbaar hoog" ("inaudibly high"), "geen flikkering in 3–30 Hz" ("no flicker in 3–30 Hz") and "Het is veilig" ("It is safe") have disappeared from the PoC README/EXPERIMENT-01; `SAFETY-PRIVACY.md`/`SPECTRASTROBE.md` sharpen the reference panning as an open assumption. |
| **m8** 20% vs 25% per carrier | **YES** | `SPECTRASTROBE.md:303-306` explicitly states "per kleurdrager" ("per colour carrier"); `m3-max-koppeling` reports per carrier (red 20.0%, green 7.5%, blue 10.0%) and the sum per ear (L 22.7% / R 29.5%). |
| **m12** citations | **YES** | `CONTRIBUTING.md` → `docs/CONTRIBUTING.md`, `preview_main.cpp` → `tools/preview_main.cpp`; both corrected. |

Also checked: the manifest now contains `scripts` hashes of `render.py` and
`verify.py`, `poc_version 0.2.0`, the clamp status and the exposures ladder;
`m10-manifest` PASS. v1.0.4 still untouched
(`git diff develop -- src CMakeLists.txt CMakePresets.json presets tests scripts
.github README.md .gitignore` empty; `nagl_tests` again **167 checks,
0 failures**).

### Remaining points (non-blocking)

- **m4 remains**: `experiments/spectrastrobe-poc/README.md:147` still says "ver
  onder de drempel" ("far below the threshold") for the rest, while the measured
  value is −38.2 dB against a threshold of −30 dB (8 dB margin). Formulate as
  "≥ 30 dB onder de drempel" ("≥ 30 dB below the threshold").
- **M14 remains advisory**: the matrix does not affect the exit code. That is
  now explicitly documented and the core (M1–M10) is hard; for a hardware gate,
  in my view the integrity rows should be hard. Acceptable as a deliberate
  choice, not as evidence.
- **M5 rest** remains an analysis artefact (1 ms block grid, not a true
  residual-carrier measurement); robust across block sizes, but the measurement
  convention is not yet stated in the report.
- **M4 probes** (mid balance/hard pan) I did not re-check; they were
  tautological relative to the score and appear unchanged.
- **Not re-checked**: the full document delta, the EXPERIMENT-01 revision
  (phase 0/1, N≥5, blinded controls, non-answer rule) and the expanded safety
  protocol. That is REVIEW-B's territory; I checked only the parts that touch
  the encoder (stop claim, clamp, ladder, envelopes). Hardware, DAW/host and
  decoder also remain unverified.

### Updated verdict

**APPROVE WITH COMMENTS.**

The blocker is resolved and all majors I reported are demonstrably addressed,
with own measurements as evidence. The remaining points are small and
documented; they are no reason to hold up the exploration or the hardware step.
The earlier remark stands: the audio signal is clean, but the decoder and chain
questions (per-ear reference scale, U12/U13) are and remain open and should be
recorded as such in `REVIEWS.md`.
