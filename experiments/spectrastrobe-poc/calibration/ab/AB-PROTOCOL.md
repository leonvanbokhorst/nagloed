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

| Variant id | File | Sequence | Colour level | Ratio to −36 dB ref | % of ref | % of MindPlace full (−24 dB) | Crossfade shape | Expected midpoint |
|---|---|---|---|---|---|---|---|---|
| `direct-m54p1` | `ab-direct-m54p1-44k1-16bit.wav` | main | −54.1 dBFS | −18.10 dB | 12.45% | 3.13% | direct | outgoing/incoming ≈50% |
| `direct-m42p0` | `ab-direct-m42p0-44k1-16bit.wav` | main | −42.0 dBFS | −6.00 dB | 50.12% | 12.59% | direct | outgoing/incoming ≈50% |
| `direct-m36p0` | `ab-direct-m36p0-44k1-16bit.wav` | main | −36.0 dBFS | 0.00 dB | 100.00% | 25.12% | direct | outgoing/incoming ≈50% |
| `direct-m30p0` | `ab-direct-m30p0-44k1-16bit.wav` | main | −30.0 dBFS | +6.00 dB | 199.53% | 50.12% | direct | outgoing/incoming ≈50% |
| `direct-m24p0` | `ab-direct-m24p0-44k1-16bit.wav` | main | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | direct | outgoing/incoming ≈50% |
| `overlap-24` | `ab-overlap-24-44k1-16bit.wav` | main | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | overlap | outgoing/incoming ≈100% |
| `lr-24` | `ab-lr-24-44k1-16bit.wav` | L/R | −24.0 dBFS | +12.00 dB | 398.11% | 100.00% | direct | intended ear on, other off |

The filename tag `m` means minus (e.g. `m54p1` = −54.1 dBFS). All files are
44.1 kHz / 16-bit stereo PCM, no dither, no metadata, **no normalisation**.
Total sample/true peak is well below −6 dBFS in every file (worst measured
about **−13.95 dBFS** for the −24 dBFS variants); the renderer hard-fails if a
variant would exceed −6 dBFS.

**Safety note on the level ladder.** The 25%-of-reference per-carrier ratio used
in the exploration and the base calibration is a **self-imposed conservative
ceiling, not a MindPlace hardware limit**. The variants above that ratio
(`direct-m30p0`, `direct-m24p0`, `overlap-24`, `lr-24`) are **research-only
diagnostic stimuli**; playing them requires Lonn's explicit authorisation under
`../../HARDWARE-PROTOCOL.md`, fixed low hardware brightness, and the same stop
rules. They do not change the production/exposure ceiling, any preset, any
score, or the base calibration.

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
`out/ab-transition-envelopes.csv`, `out/ab-timeline.svg`, the seven WAVs and
`out/ab-manifest.json` (hashes, levels, ratios, peaks, timings, command).

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
5. **Order: start with the dimmest file** (`direct-m54p1`, the level at which
   the fade-to-black was first observed), then `direct-m42p0`, `direct-m36p0`,
   `direct-m30p0`, `direct-m24p0`, `overlap-24`, `lr-24`. This is also the
   conservative safety order (lowest exposure first). Do not reorder to a
   brighter file first.
6. **Standard Ganzframes (eyes closed) first.** DeepVision (eyes open) only if
   the session is comfortable, and at the same or a *lower* Kasina brightness.
7. **Immediate stop:** stop playback, Kasina brightness to minimum, glasses off
   or cover one eye. Stop at any discomfort, headache, dizziness, an
   unexplained flash or loss of detection. Stopping is always allowed.
8. **No medical/wellness claim.** This is a perceptual/technical diagnostic.
9. **One variable at a time.** Do not change Kasina brightness, AUX level,
   routing, seating or glasses type between variants. If something must change,
   note it and restart the comparison from the dimmest file.

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

| Observation | Possible reading (hypothesis, not a conclusion) | Next check |
|---|---|---|
| Fade-to-black at **all** levels, including −24 | Ratio/threshold is not the dominant factor; look at transition shaping, L/R behaviour, decoder dynamics or the observation setup | Compare direct vs overlap at −24; then L/R file |
| Fade-to-black only at the **lowest** levels (−54.1, −42) and not at −36/−30/−24 | A level/ratio threshold in the decoder or LED transfer may be involved | Repeat at −36 exactly; record where it disappears |
| Direct at −24 looks black, **overlap at −24 looks continuous** | Midpoint amplitude / crossfade shaping is a possible factor | Compare the two files back-to-back at fixed settings |
| **Both** direct and overlap at −24 look black | Midpoint amplitude is probably not the explanation; look at decoder dynamics, LED response, or L/R | Run `lr-24`; check one eye |
| L/R file shows dark only on one eye or one side | L/R routing, eye assignment or channel balance may be involved | Re-check cable/ear assignment and the parent hardware protocol |
| Nothing reproduces the observation in this set | The earlier impression may depend on source material, mastering, playback level or the specific session | Record the exact chain and conditions; repeat the original file |

These rows are **hypotheses to discriminate**, not findings. Only the recorded
hardware observation, ideally repeated by a second observer, can select among
them; a photodiode measurement would be the objective next step.

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
