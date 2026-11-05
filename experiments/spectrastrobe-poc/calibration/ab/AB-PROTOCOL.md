# A/B protocol — colour calibration for issue #31

> **EXPERIMENT / DIAGNOSTIC — NOT PART OF NAGLOED 1.0.4, THE ARTISTIC
> COMPOSITION *EERSTE GLOED*, OR THE BASE COLOUR CALIBRATION.**
> This is a controlled A/B stimulus set in `calibration/ab/`. It does not modify
> the base calibration, Eerste Gloed, the parent `out/`, v1.0.4 or any production
> DSP. The colour levels below are a **diagnostic ladder**, not artistic
> settings and not a recommendation. No medical or wellness claims, and no
> causal claim about the hardware.
>
> **Safety:** `../../HARDWARE-PROTOCOL.md` is the **binding** safety reference
> for every hardware session. Read it before this protocol. This document does
> not replace it.

> **Correction — owner decision, 5 November 2026.** The former conservative
> research ceiling of **25% of the reference peak per active colour carrier**
> corresponds, with the A/B reference fixed at **−36 dBFS**, to a colour carrier
> at **−48.04 dBFS**. Of the original variants, only `direct-m54p1` (12.4% of
> reference) is below it; the new boundary variants (`direct-m48p1`,
> `overlap-m48p1`, `lr-m48p1`, ~24.8%) are also below it. `direct-m42p0` (50.1%),
> `direct-m36p0` (100%), `direct-m30p0` (199.5%) and the `−24` variants
> (398.1% per carrier) are **above** it. The earlier version of
> this protocol incorrectly listed only `m30p0`, `m24p0`, `overlap-24` and
> `lr-24` as above the ceiling. The above-ceiling variants are **gated from
> hardware playback** — do not play them, even with lower physical controls,
> until a separate explicit decision and a revised safety rationale. New
> under-ceiling stimuli were added: `direct-m48p1`, `overlap-m48p1` and
> `lr-m48p1` at ~24.8% of reference. The 25% ceiling is a **conservative
> research convention**, not a proven physical safety threshold and not a
> MindPlace decoder specification.

## 1. Research question

Hardware observation of the base colour calibration: the nine states are
visible, but the **crossfades appear as fade-to-black** on the Kasina, while
offline verification says there is **no encoded black gap** (colour-carrier
gaps 0.0 ms; see `../out/calibration-report.md`). Which of these separable
factors is involved?

1. **Ratio / level** — is the colour level (and its ratio to the reference)
   below a decoder/LED threshold at the transition midpoint?
2. **Transition shape** — does the 50%/50% direct crossfade dip below a
   perceptual or hardware threshold, whereas an overlap shape (both carriers
   near full at the midpoint) does not?
3. **Left/right behaviour** — does the fade-to-black depend on one eye/side,
   or on L/R alternation?

This A/B holds the sequence, the reference level and the total timing fixed and
varies exactly those three factors. It produces measured stimuli and a
measurement report. **It does not claim a cause**; only the hardware
observation can select among the hypotheses in §8.

## 2. Primary-source scaling and the three levels

Primary source for the scaling: MindPlace Support Forum, guidance by **Scott
Hendrickson (neuroasis)**. The explicit level statement is in the thread
*"Creating Spectrastrobe signal using Kyma software"* (20 May 2015):
<https://mindplacesupport.com/forum/showthread.php?2508-Creating-Spectrastrobe-signal-using-Kyma-software>
— reference normalised to −36 dB, each light channel to −24 dB, a 4:1
dynamic range (12 dB). The same developer states in thread 3173 (2019) that
colour tones representing 100% brightness are −24 dB and the reference is 1/4
of the colour tones:
<https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content>.
These are **forum guidance from an encoder developer, not a formal decoder
specification**; the Kasina's exact decoder thresholds and transfer function
are unknown.

Keep three levels apart:

| Level | What it is | Measured in this experiment? |
|---|---|---|
| **Digital ratio** | The encoded amplitudes in the WAV (colour peak vs reference peak) | Yes, offline (this report) |
| **Hardware output** | What the Kasina decoder/LEDs/diffuser actually emit; includes undetermined thresholds and non-linear LED transfer | No |
| **Perception** | What a person sees; includes colour naming, adaptation, individual colour vision | No |

A correct digital ratio does **not** imply the same hardware ratio, and
neither implies the same perceived colour. The base calibration used reference
−30 dBFS with colours at −54.1 dBFS (a 24 dB ratio); this A/B fixes the
reference at −36 dBFS and rises to the primary-source scale (−24 dBFS, 4:1).

## 3. Variants

All variants use the same nine-state sequence and timing:
Red → Green → Blue → Yellow → Cyan → Magenta → White → Red → Green,
**1.0 s per state**, **250 ms smoothstep crossfades centred on the boundaries**,
**300 ms fade-in/out**, **no encoded black gaps**. Reference is fixed at
**−36 dBFS peak** with a **20 Hz sine equal-power pan LFO**; colour channels are
centre-panned (both ears equal) except in the L/R file. Colour peak level is
**per active carrier**.

| Variant id | File | Sequence | Colour level | Ratio to −36 dB ref | % of ref | % of MindPlace full (−24 dBFS) | Hardware exposure | Crossfade shape |
|---|---|---|---|---|---|---|---|---|
| `direct-m54p1` | `ab-direct-m54p1-44k1-16bit.wav` | main | −54.1 dBFS | −18.10 dB | 12.45% | 3.13% | **under ceiling** | direct |
| `direct-m48p1` | `ab-direct-m48p1-44k1-16bit.wav` | main | −48.1 dBFS | −12.10 dB | 24.83% | 6.25% | **under ceiling** | direct |
| `overlap-m48p1` | `ab-overlap-m48p1-44k1-16bit.wav` | main | −48.1 dBFS | −12.10 dB | 24.83% | 6.25% | **under ceiling** | overlap |
| `lr-m48p1` | `ab-lr-m48p1-44k1-16bit.wav` | L/R | −48.1 dBFS | −12.10 dB | 24.83% | 6.25% | **under ceiling** | direct (L/R) |
| `direct-m42p0` | `ab-direct-m42p0-44k1-16bit.wav` | main | −42.0 dBFS | −6.00 dB | 50.12% | 12.59% | **gated (above)** | direct |
| `direct-m36p0` | `ab-direct-m36p0-44k1-16bit.wav` | main | −36.0 dBFS | 0.00 dB | 100.00% | 25.12% | **gated (above)** | direct |
| `direct-m30p0` | `ab-direct-m30p0-44k1-16bit.wav` | main | −30.0 dBFS | +6.00 dB | 199.53% | 50.12% | **gated (above)** | direct |
| `direct-m24p0` | `ab-direct-m24p0-44k1-16bit.wav` | main | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | **gated (above)** | direct |
| `overlap-24` | `ab-overlap-24-44k1-16bit.wav` | main | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | **gated (above)** | overlap |
| `lr-24` | `ab-lr-24-44k1-16bit.wav` | L/R | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | **gated (above)** | direct (L/R) |

The filename tag `m` means minus (e.g. `m54p1` = −54.1 dBFS). All files are
44.1 kHz / 16-bit stereo PCM, no dither, no metadata, **no normalisation**.
Total sample/true peak is well below −6 dBFS in every file (worst measured
about **−13.95 dBFS** for the −24 dBFS variants); the renderer hard-fails if a
variant would exceed −6 dBFS.

**Safety note on the level ladder.** The 25%-of-reference per-carrier ratio used
in the exploration and the base calibration is a **self-imposed conservative
ceiling, not a MindPlace hardware limit**. `direct-m42p0` and `direct-m36p0`
are also above it, in addition to `direct-m30p0`, `direct-m24p0`, `overlap-24`
and `lr-24`. All six above-ceiling variants are **offline-only/gated diagnostic
stimuli**: do not play them on hardware, even at lower physical controls, until
Lonn makes a separate explicit decision and a revised safety rationale exists.
The under-ceiling files (`direct-m54p1`, `direct-m48p1`, `overlap-m48p1`,
`lr-m48p1`) are the only files intended for the first hardware pass; they do
not change the production/exposure ceiling, any preset, any score or the base
calibration.

### Timing (identical to the base calibration)

| # | State | Start | End | Expected RGB | Active carriers |
|---|---|---|---|---|---|
| 0 | Red | 0.30 s | 1.30 s | 1, 0, 0 | red |
| 1 | Green | 1.30 s | 2.30 s | 0, 1, 0 | green |
| 2 | Blue | 2.30 s | 3.30 s | 0, 0, 1 | blue |
| 3 | Yellow | 3.30 s | 4.30 s | 1, 1, 0 | red + green |
| 4 | Cyan | 4.30 s | 5.30 s | 0, 1, 1 | green + blue |
| 5 | Magenta | 5.30 s | 6.30 s | 1, 0, 1 | red + blue |
| 6 | White | 6.30 s | 7.30 s | 1, 1, 1 | red + green + blue |
| 7 | Red | 7.30 s | 8.30 s | 1, 0, 0 | red |
| 8 | Green | 8.30 s | 9.30 s | 0, 1, 0 | green |

Transitions (crossfade midpoint) at **1.3, 2.3, 3.3, 4.3, 5.3, 6.3, 7.3,
8.3 s**; fade-out 9.3–9.6 s. The **direct** shape fades the outgoing carrier
out while the incoming fades in (both ≈50% at the midpoint). The **overlap**
shape holds the outgoing carrier while the incoming rises (both ≈100% at the
midpoint), then lets the outgoing fall; there is no full-colour gap in either
shape. The **L/R** file has six 1.0 s states: R-L, R-R, G-L, G-R, B-L, B-R
(6.6 s), same fades and 250 ms crossfades.

## 4. Commands

```bash
cd experiments/spectrastrobe-poc/calibration/ab
./run_ab.sh
```

This renders all variants and verifies them independently. Outputs (git-ignored,
not committed): `out/ab-report.md`, `out/ab-report.json`,
`out/ab-transition-envelopes.csv`, `out/ab-timeline.svg`, **ten** WAVs and
`out/ab-manifest.json` (hashes, levels, ratios, peaks, timings, command,
hardware-exposure classification).

## 5. Fixed conservative hardware procedure

Binding safety reference: `../../HARDWARE-PROTOCOL.md`. In addition:

1. **One operator, one watcher.** The operator never watches through the
   glasses and keeps the stop ready. No person with (suspected)
   photosensitivity or epilepsy in the room. Room lit, no other flicker
   sources, no strobing.
2. **Fixed, low Kasina LED brightness.** Choose one low position, note it, and
   keep it for **all** variants. **Never use maximum as a safe target.**
   **Do not raise brightness to make a colour appear.** If green/yellow does not
   appear at the fixed setting, record that; do not escalate the brightness.
3. **Moderate AUX/playback level**, fixed and noted for all variants. Same
   routing, same player, same device for every file. No EQ, normalisation,
   spatial audio, loudness processing, Bluetooth or lossy compression; do not
   run the files through a DAW/limiter after rendering.
4. **Play one pass, then pause.** Each file has a 300 ms fade-in/out; a gapless
   loop turns the file boundary into a quiet gap. Pause between variants and
   replay only if needed. Do not loop while judging a transition.
5. **First hardware pass — under-ceiling files only.** Start with
   `direct-m54p1` (the level at which the fade-to-black was first observed),
   then `direct-m48p1` (the boundary case), then `overlap-m48p1` (same boundary
   ratio, overlap shape) and `lr-m48p1` (same boundary ratio, L/R). These are
   the only files for this pass. **Do not play any variant marked `gated
   (above)`** — `direct-m42p0`, `direct-m36p0`, `direct-m30p0`,
   `direct-m24p0`, `overlap-24` or `lr-24` — even with lower physical controls.
   Those require a separate explicit decision and a revised safety rationale.
6. **Separate the A/B from physical light brightness.** This A/B compares
   encoded ratios and shapes at **one fixed physical brightness/AUX setting**;
   do not change Kasina brightness or AUX between files, and do not use the
   hardware brightness control to emulate a higher signal ratio. Record the
   actual settings. A physical-brightness comparison at a fixed file is a
   different, gated experiment; it is not part of this A/B.
7. **Standard Ganzframes (eyes closed) first.** DeepVision (eyes open) only if
   the session is comfortable, and at the same or a *lower* Kasina brightness.
8. **Immediate stop:** stop playback, Kasina brightness to minimum, glasses off
   or cover one eye. Stop at any discomfort, headache, dizziness, an
   unexplained flash or loss of detection. Stopping is always allowed.
9. **No medical/wellness claim.** This is a perceptual/technical diagnostic.
10. **One variable at a time.** Keep physical brightness, AUX level, routing,
    seating and glasses type fixed between the under-ceiling variants. If
    something must change, note it and restart the comparison from
    `direct-m54p1`.

## 6. Observation log per transition

For every variant, listen to each of the eight transitions and record one row.
Use the categories: **continuous** (no perceived dip), **brief dark** (a short
darkening, estimate duration), **black** (a clear black gap), and record the
**colour path** you see (e.g. red→dark→green, red→orange→green, red→yellow→green).

| Transition | Time | From → to | Category (continuous / brief dark / black) | Duration estimate | Colour path | Notes |
|---|---|---|---|---|---|---|
| 1 | 1.3 s | Red → Green | | | | |
| 2 | 2.3 s | Green → Blue | | | | |
| 3 | 3.3 s | Blue → Yellow | | | | |
| 4 | 4.3 s | Yellow → Cyan | | | | |
| 5 | 5.3 s | Cyan → Magenta | | | | |
| 6 | 6.3 s | Magenta → White | | | | |
| 7 | 7.3 s | White → Red | | | | |
| 8 | 8.3 s | Red → Green | | | | |

Repeat this form for each variant (or copy the form and write the variant id at
the top). Record at least: variant id, Kasina brightness position, AUX level,
firmware, Ganzframe type, date, and whether it was the first or a repeat pass.
Duration estimates can be coarse: <50 ms, 50–150 ms, 150–300 ms, >300 ms.

For the L/R file, record per state: which eye shows the colour (left/right/
both/none) and whether the transition to the next state darkens.

## 7. What the automated checks prove — and do not prove

**Prove (offline, from the WAV):** the reference is −36 dBFS with an intact
20 Hz pan LFO; each variant's active carriers are at the documented absolute
level (±0.5 dB) and ratio; unintended carriers are ≤ −40 dB down; there is no
encoded colour-carrier gap > 20 ms; direct midpoints are ≈50% and overlap
midpoints ≈100% of the colour level; fade-in/out is present; sample/true peak
is well below −6 dBFS with no clipping; and the render is deterministic.

**Do not prove:** what the Kasina decoder does, what the LEDs/diffuser emit, or
what is perceived. In particular, the observation that transitions "look black"
despite a correct encoded spectrum cannot be explained by the file alone.

## 8. Decision matrix (hypotheses only)

The first hardware pass is restricted to the under-ceiling variants:
`direct-m54p1` (12.4% of reference) and the boundary set `direct-m48p1`,
`overlap-m48p1`, `lr-m48p1` (~24.8%). The above-ceiling variants are gated and
must not be played in this pass.

| Observation | Possible reading (hypothesis, not a conclusion) | Next check |
|---|---|---|
| Black at `direct-m54p1`, continuous at `direct-m48p1` | Within-ceiling level/ratio may be involved; the boundary is just below the former 25% ceiling | Repeat both at the same fixed hardware settings; record where the dip disappears |
| Black at both `direct-m54p1` and `direct-m48p1` | Ratio change in the permitted range was not enough; threshold may lie above the ceiling or another factor dominates | Compare `overlap-m48p1` and `lr-m48p1`; do not escalate to gated levels |
| Direct at boundary black, **overlap at boundary continuous** | Midpoint amplitude / transition shaping is a possible factor at a fixed ratio and level | Compare back-to-back at fixed hardware settings |
| Both direct and overlap at boundary black | Midpoint shaping is probably not sufficient; decoder dynamics, LED response, L/R behaviour or perception may dominate | Run `lr-m48p1`; check one eye; record settings |
| `lr-m48p1` shows dark only on one eye/side | L/R routing, eye assignment or channel balance may be involved (note: the inactive eye is expected to be dark) | Re-check cable/ear assignment and the parent hardware protocol |
| Nothing reproduces the observation under the ceiling | The earlier impression may depend on source material, mastering, playback level, the maximum-hardware session or expectation | Record the exact chain/conditions; repeat the original file before considering any gated variant |

These rows are **hypotheses to discriminate**, not findings. Only the recorded
hardware observation, ideally repeated by a second observer, can select among
them; a photodiode measurement would be the objective next step. Gated
above-ceiling variants are not part of this matrix until a separate explicit
decision and revised safety rationale exist.


## 9. Limitations

- No hardware, photodiode or observer data are available here; this is an
  offline stimulus set plus verification only.
- The forum guidance (−36 dB reference, −24 dB light channel, 4:1) is primary
  guidance from an encoder developer, not a formal decoder specification; the
  Kasina's actual thresholds and transfer are unknown.
- Digital ratio ≠ hardware output ≠ perception. A "green" that looks yellow may
  be a mixture of LED spectra, diffuser/glasses behaviour, adaptation or colour
  naming; this experiment cannot separate those.
- A single observer's report is anecdotal. Repeat passes, fixed settings and a
  second observer improve reliability; do not change brightness between
  variants.
- No medical or wellness claim is made; this is not a therapy or entrainment
  protocol.
