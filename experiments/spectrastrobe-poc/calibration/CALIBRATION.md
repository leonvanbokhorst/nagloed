# Colour calibration — SpectraStrobe diagnostic

> **EXPERIMENT / DIAGNOSTIC — NOT PART OF NAGLOED 1.0.4 AND NOT PART OF THE
> ARTISTIC COMPOSITION *EERSTE GLOED*.**
> This is a separate, offline diagnostic in `experiments/spectrastrobe-poc/calibration/`.
> It does not change the Eerste Gloed score, renderer, verification or `out/`.
> It is not integrated into the plug-in and touches no DSP contract. No medical
> or wellness claims.
>
> **Safety:** `../HARDWARE-PROTOCOL.md` remains the **binding** safety reference
> for any hardware session. Read it first. This document does not replace it.

## Why this diagnostic exists

Real Kasina + DeepVision listening/watching of the *Eerste Gloed* WAVs showed
red/blue/violet and left/right stereo asymmetry, but **no convincing
green/yellow**. Two possibilities must be separated before drawing artistic
conclusions:

1. **Signal problem** — a colour carrier is missing, too weak, unequal, or the
   transitions/ordering are wrong in the file.
2. **Hardware/perception problem** — the decoder, LEDs, diffuser, glasses or
   individual colour vision do not reproduce the intended colour even though the
   encoded spectrum is correct.

This diagnostic addresses (1) objectively and gives a controlled, dim stimulus
for a structured hardware observation that can address (2). A correct spectrum
does **not** prove that a person sees green or yellow.

## What it is — and is not

- **Is:** a nine-state colour sequence with a continuous reference carrier, a
  dim exposure, equal ratios inside composites, explicit timestamps, and an
  independent verification report.
- **Is not:** an artistic experiment, a v2 feature, a plug-in change, a
  replacement for Eerste Gloed, or a clinical/medical test.
- **Does not** explain the earlier observation by itself: it measures the file,
  not the LEDs or the eye.

## State sequence (9 states, 1.0 s each)

Fade-in 300 ms; each state 1000 ms; crossfades **250 ms, centred on each
boundary**; fade-out 300 ms. Total duration **9.6 s**. No black gap between
colour carriers: during every crossfade at least one colour carrier is active.

| # | State | Start | End | Expected RGB | Active carriers | Crossfade out | Measurement window |
|---|---|---|---|---|---|---|---|
| 0 | Red | 0.30 s | 1.30 s | 1, 0, 0 | red | 1.175–1.425 s | 0.65–0.95 s |
| 1 | Green | 1.30 s | 2.30 s | 0, 1, 0 | green | 2.175–2.425 s | 1.65–1.95 s |
| 2 | Blue | 2.30 s | 3.30 s | 0, 0, 1 | blue | 3.175–3.425 s | 2.65–2.95 s |
| 3 | Yellow | 3.30 s | 4.30 s | 1, 1, 0 | red + green (1:1) | 4.175–4.425 s | 3.65–3.95 s |
| 4 | Cyan | 4.30 s | 5.30 s | 0, 1, 1 | green + blue (1:1) | 5.175–5.425 s | 4.65–4.95 s |
| 5 | Magenta | 5.30 s | 6.30 s | 1, 0, 1 | red + blue (1:1) | 6.175–6.425 s | 5.65–5.95 s |
| 6 | White | 6.30 s | 7.30 s | 1, 1, 1 | red + green + blue (1:1:1) | 7.175–7.425 s | 6.65–6.95 s |
| 7 | Red | 7.30 s | 8.30 s | 1, 0, 0 | red | 8.175–8.425 s | 7.65–7.95 s |
| 8 | Green | 8.30 s | 9.30 s | 0, 1, 0 | green | 9.30–9.60 s (fade-out) | 8.65–8.95 s |

The sequence returns to Red and Green at the end so a repeated observation can
check consistency without changing the stimulus type.

### Optional L/R eye check (`calibration-lr-44k1-16bit.wav`, 6.6 s)

Six states of 1.0 s, same fade/crossfade rules; one colour carrier in one ear
per state:

| # | State | Time | Carrier | Ear |
|---|---|---|---|---|
| 0 | Red left | 0.30–1.30 s | red | left only |
| 1 | Red right | 1.30–2.30 s | red | right only |
| 2 | Green left | 2.30–3.30 s | green | left only |
| 3 | Green right | 3.30–4.30 s | green | right only |
| 4 | Blue left | 4.30–5.30 s | blue | left only |
| 5 | Blue right | 5.30–6.30 s | blue | right only |

## Encoding

| Parameter | Value |
|---|---|
| Format | 44.1 kHz / 16-bit PCM stereo, no metadata, no dither |
| Reference | 18.200 Hz at **−30 dBFS peak**, continuous, 20 Hz sine equal-power pan LFO |
| Colour carriers | red 18.700 Hz, green 19.200 Hz, blue 19.700 Hz |
| Exposure | **6.25% per carrier** of the reference peak (≈ −54.1 dBFS per carrier), hard ceiling 25% |
| Composites | yellow/cyan/magenta equal 1:1; white 1:1:1 (each carrier still 6.25%) |
| Colour panning | centre (both ears equal) in the main file; hard left/right in the L/R file |
| Transitions | 250 ms smoothstep crossfades centred on the boundaries; 300 ms fade-in/out |
| Black gaps | none between colour carriers (crossfade overlap) |

The sum per ear in White is 3 × 6.25% = 18.75% (amplitude sum). The 6.25%
ceiling applies **per carrier**; the hard ceiling is 25% per carrier.

## Commands

```bash
cd experiments/spectrastrobe-poc/calibration
./run_calibration.sh
```

This renders and verifies, and writes to `out/`:

| File | Content |
|---|---|
| `calibration-44k1-16bit.wav` | nine-state colour sequence (9.6 s) |
| `calibration-lr-44k1-16bit.wav` | optional six-state left/right eye check (6.6 s) |
| `calibration-manifest.json` | hashes, timings, exposure, command |
| `calibration-report.md` / `.json` | PASS/FAIL report, per-state measurements |
| `colour-timeline.csv` | timestamped expected vs measured states |
| `colour-timeline.svg` | timeline of states, crossfades, reference and measurements |

`out/` is git-ignored and is not committed.

## Hardware observation procedure (Kasina + DeepVision)

**Read `../HARDWARE-PROTOCOL.md` first; it is the binding safety reference.**
In short:

1. **Safety first.** No person with (suspected) photosensitivity or epilepsy in
   the room. One operator, one watcher. Room lit, no other flicker sources.
   Immediate stop: stop playback + Kasina brightness to minimum + glasses off
   (or cover one eye, R11). No medical or wellness claims.
2. **Dim first.** This file is already the dim exposure (6.25% per carrier).
   Start with the Kasina LED brightness at a fixed, noted low position; do not
   raise it during the first pass. A brighter exposure would require a new,
   explicitly rendered file — do not just turn up the hardware brightness.
3. **Play once, then pause.** Do not loop the file with a gapless player: the
   end fades to zero and the start fades in; that quiet boundary is deliberate,
   not a colour cue. Play one pass, pause, then repeat if needed.
4. **Detection.** The Kasina display should show SpectraStr detection with the
   reference carrier. If not, stop: that is a signal/detection problem.
5. **Standard Ganzframes (eyes closed) first.** Observe the nine states and note
   for each state: which colour you perceive (or none), whether it is weak/
   strong, and whether the intended colour appears at all. Pay special attention
   to states 3 (Yellow) and 1/8 (Green).
6. **One eye (R11).** If a colour is weak or asymmetric, repeat with one eye
   covered and note which eye.
7. **Optional L/R file.** Use the `-lr` file to check whether left/right eye
   assignment matches the intended ear. Note any swapped or missing side.
8. **DeepVision (eyes open) only if the standard Ganzframes were comfortable.**
   Use the same or a *lower* Kasina brightness, because DeepVision appears much
   brighter. Do not increase brightness to "make green appear".
9. **The sequence is smooth and slow; that is not permission for faster
   flashing.** Keep to the 1 s states and 250 ms crossfades. Do not add strobing
   or faster colour changes during the session.

Record the state index and timestamp for every observation so it can be
compared with `out/colour-timeline.csv`.

## What the automated checks prove — and do not prove

**Prove (offline, on this machine, from the WAV alone):** the reference is at
−30 dBFS with an intact 20 Hz pan LFO; every primary state activates only its
own carrier (unintended carriers ≤ −40 dB); Yellow contains both red and green
within ±0.5 dB and no blue; Cyan/Magenta/White contain the intended equal
ratios; crossfades do not exceed the dim exposure; there is no abrupt luminance
pulse (max 10 ms envelope change reported); there is no black gap between
colour carriers; the fade-in/out is present; headroom is ample (sample/true
peak well below −6 dBFS); and the render is deterministic.

**Do not prove:** that the Kasina decodes these spectra correctly, that the LEDs
and diffuser emit the intended colours, or that a listener perceives
red/green/blue/yellow/cyan/magenta/white. A correct spectrum is necessary but
not sufficient. The reported green/yellow asymmetry can come from the LEDs, the
decoder, the diffuser/glasses, or individual colour vision; separating those
causes requires hardware observation, ideally with a photodiode, and is outside
this offline diagnostic.

## Limitations

- No hardware, no photodiode and no observer data are available here; this is a
  file-level diagnostic only.
- The Kasina's internal decoder bandwidth, LED spectra and DeepVision filter
  behaviour are unknown; the measurement floor of the analysis is documented in
  the report.
- "Yellow" on an RGB additive display is a perceptual mixture of red and green;
  a viewer may report it as orange, greenish or absent depending on the LEDs and
  the diffuser. Compare the reported colour with the expected carrier ratios,
  not with a printed colour chart.
- This document and the generated report make no medical, therapeutic or
  wellness claim.

## Follow-up — issue #31 (fade-to-black crossfades)

This document and the 41/41 report remain the historical record of the first
calibration. After the hardware observation that the transitions appeared to
fade to black, the investigation continued separately:

- `RATIO-RESEARCH.md` — primary-source research on the intended reference/colour
  scaling, the three levels (digital ratio, hardware output, perception), and the
  hypothesis that the base calibration is underdriven.
- `ab/` — a controlled A/B stimulus set with reference fixed at −36 dBFS and
  colour levels −54.1 / −42 / −36 / −30 / −24 dBFS, plus an overlap-shape and a
  matched L/R control; offline verification 72/72 PASS, hardware observation
  pending.

Those files do not change this calibration's levels or its report; they are a
separate diagnostic for the transition question.

