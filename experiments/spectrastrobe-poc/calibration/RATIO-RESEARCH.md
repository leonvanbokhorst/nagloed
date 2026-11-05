# SpectraStrobe reference/colour ratio research — issue #31

> **Status:** focused research note for issue #31, "SpectraStrobe crossfades appear
> to fade to black on Kasina". **Analysis only.** It does not modify
> `calibration.json`, the calibration renderer/verifier, *Eerste Gloed*, NAGLOED
> v1.0.4, production DSP or release planning. The A/B ladder described in §6 is
> **implemented** in `calibration/ab/` (see `ab/AB-PROTOCOL.md`); its offline
> verification is 72/72 PASS. Hardware observation is still pending, so no
> causal conclusion is claimed. Any hardware exposure remains bound by
> `../HARDWARE-PROTOCOL.md`; this note is not medical or legal advice.
>
> **Markers:** **[PM-doc]** MindPlace published documentation; **[PM-dev]**
> MindPlace developer statement or developer tooling (Scott Hendrickson /
> *neuroasis*); **[PM-forum]** other MindPlace forum/support statement;
> **[Lum-doc]** Lumasonic published documentation; **[enc-dev]** encoder
> developer statement on the Lumasonic/MindPlace forum; **[inference]**;
> **[open]**.

> **Correction — owner decision, 5 November 2026.** With the A/B reference fixed
> at **-36 dBFS peak**, the former conservative research ceiling of **25% of
> the reference peak per active colour carrier** corresponds to a colour carrier
> at **-48.04 dBFS**. Of the original variants, only `direct-m54p1` (12.4% of
> reference) is below it; the new boundary variants `direct-m48p1`,
> `overlap-m48p1` and `lr-m48p1` (~24.8%) are also below it. `direct-m42p0`
> (50.1%), `direct-m36p0` (100%), `direct-m30p0` (199.5%) and the `-24` variants
> (398.1% per carrier) are **above** it. The earlier issue/PR narrative that
> listed only `-30/-24/overlap/LR` as above the ceiling was wrong. The
> above-ceiling variants remain **offline-only/gated**: do not play them, even
> at lower physical controls, until a separate explicit decision and revised
> safety rationale. The 25% ceiling is a **conservative research convention**,
> not a proven physical safety threshold or a MindPlace decoder specification.

---

## 1. Question and boundary

Issue #31 records a discrepancy: the calibration WAV has **no encoded black gap**
(verifier: 41/41 PASS, longest colour-carrier gap 0.0 ms), yet the transitions
were *perceived* as fade-to-black → next colour rather than as continuous
crossfades. The calibration specifies 1 s states, 250 ms centred smoothstep
crossfades and 300 ms fades. The separate left/right diagnostic is a control
for eye assignment, not evidence about the crossfade observation (see §6.4).

This note asks one narrow question: **what reference-to-colour amplitude
relationship does SpectraStrobe expect, and could the calibration's very low
colour level explain a perceived black dip during a crossfade?** It does not
attempt a general theory of the decoder, and it does not claim a cause before
hardware observation.

What must not be concluded yet: the observation is a *perceptual* report from one
session with qualitative, non-time-synchronised hardware observation. Encoded
amplitude ratios do not determine perceived brightness (see §2). The black
crossfade is treated here as a **hypothesis under test**.

---

## 2. Three layers that must not be conflated

| Layer | What it is | What it can be measured with | What it cannot tell us |
|---|---|---|---|
| **L1 — digital file** | Peak/RMS amplitude in dBFS; per-carrier amplitude relative to the reference peak; envelope shape | Offline FFT/envelope analysis of the WAV (already done: 41/41 PASS) | Whether the decoder or LEDs respond, or what a person sees |
| **L2 — hardware output** | AUX input gain and input stage, decoder detection/threshold/AGC/compression, LED driver minimum drive, Kasina brightness and per-colour balance | Controlled hardware observation; ideally a photodiode + interface | Perceived colour; eye adaptation; individual colour vision |
| **L3 — perception** | LED spectra, diffuser/glasses, eye temporal integration, colour adaptation, expectations | Structured human observation, repeated passes, one-eye control | Any file-level dBFS value directly; brightness is not derivable from dBFS |

Supporting evidence for L2/L3 separation:

- Lumasonic's encoder exposes **LED Gamma** "to help align the expected output
  with the perceived levels in LED glasses" — vendor acknowledgement that
  amplitude and perceived brightness are not the same curve. **[Lum-doc]** —
  [Prism settings](https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/03-settings-screen.md)
- The Kasina has a separate **LED brightness** control and per-colour **Visuals**
  balance (default 100% per LED, both eyes) — two L2/L3 gain stages between file
  and eye. **[PM-doc]** — [Kasina Quickstart](https://mindplacesupport.com/download/1244/)
- Our own calibration report explicitly states it proves the spectrum, not the
  decode, the LEDs or the perception. **[inference]** — `out/calibration-report.md`

Consequence: "the file has no black gap" and "the light went black" can both be
true. The task is to find which layer introduces the dip.

---

## 3. The MindPlace developer record

### 3.1 The key statement

MindPlace forum thread *"Creating Spectrastrobe signal using Kyma software."*,
reply by **neuroasis (Scott Hendrickson, MindPlace)**, 20 May 2015, post id
13542. Verbatim (original spelling preserved):

> "The reference signal acts as an identifier, as the Kasina performs auto-sensing
> between SS and AS. … The reference track also sets the dynamic range of the
> signal in a ratio of 4:1. That way, if the user changes the volume of the source
> (on an AUX for instance) the full range of brightness is still delivered.
>
> In pracitice, the total amplitude of the reference signal is normalized to -36db
> and each light channel normalized to -24db. This keeps the tones inaudible and
> allows around -2db of headroom for the soundtrack (which should be normalized to
> this at max)"

Sources:
[show view, thread 2508](https://mindplacesupport.com/forum/showthread.php?2508-Creating-Spectrastrobe-signal-using-Kyma-software),
[print view](https://mindplacesupport.com/forum/printthread.php?t=2508&pp=10).
Post anchor: `…showthread.php?2508-…&p=13542#post13542`. **[PM-dev]**

Reading: reference total amplitude ≈ **-36 dBFS**; each light channel ≈ **-24
dBFS**; therefore full-brightness colour sits **+12.04 dB (≈4× in amplitude)**
above the reference. "4:1" and "-36/-24" are two descriptions of the same
12 dB relationship. The same post says the reference is an 18.2 kHz sine
modulated by a pulse, with the channels 180° out of phase ("autopan LFO set to
20hz"); the 10 Hz/20 Hz wording is inconsistent in the post, but the stereo
alternation requirement is clear and matches the format spec.

### 3.2 Corroboration in the same developer's own tooling

Scott Hendrickson's GPL-licensed Audacity tools implement the convention directly.
The suite's README identifies him as "Scott Hendrickson of MindPlace Inc"; the
Nyquist plug-ins carry GPLv2 headers and the repository is GPL-3.0. **[PM-dev]**
(code artefacts):

| Artefact | Value | Source |
|---|---|---|
| SpectraStrobe reference generator | `(scale 0.015 (hzosc 18200))` on both channels → **-36.48 dBFS** peak | [tone-generator-ss-ref.ny](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Generate%20Plugins/SpectraStrobe/tone-generator-ss-ref.ny) |
| Reference normalise chain | Normalize to **-36.5 dB** | [SS Ref Normalize.txt](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Audacity%20Chains/SS%20Ref%20Normalize.txt) |
| Reference panning chain | `rate=20`, `waveform=pulse`, `duty=50`, left=0, right=100 | [SS Ref LFO.txt](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Audacity%20Chains/SS%20Ref%20LFO.txt) |
| Single colour tone normalise | Normalize to **-22.0 dB** peak | [SS Normalize Single Tone.txt](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Audacity%20Chains/SS%20Normalize%20Single%20Tone.txt) |
| Two colour tones normalise | Normalize to **-16.5 dB** peak | [SS Normalize 2 tones.txt](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Audacity%20Chains/SS%20Normalize%202%20tones.txt) |
| Three colour tones normalise | Normalize to **-13.0 dB** peak | [SS Normalize 3 tones.txt](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Audacity%20Chains/SS%20Normalize%203%20tones.txt) |
| Channel generator | each colour generated at `scale 0.015` before normalising | [tone-generator-ss-channels.ny](https://github.com/scottosx/Audacity-Mind-Machine-Suite/blob/master/1%20Audacity%20Mind%20Machine%20Suite/Generate%20Plugins/SpectraStrobe/tone-generator-ss-channels.ny) |

Reading the normalise chains: a single colour bus is targeted around **-22 dBFS**
peak, two summed tones at **-16.5 dBFS** and three at **-13.0 dBFS**. Those sums
imply a per-carrier level of roughly **-22.5 dBFS** (two: -16.5 − 6.0; three:
-13.0 − 9.5). That is ~1.5–2.5 dB hotter than the forum's -24 dB, but it is in
the same region and **far above** our calibration's -54.1 dBFS. (Audacity
Normalize defaults to peak; if these chains were configured for RMS the levels
shift by roughly 3 dB, which does not change the conclusion.) The blog that
publishes the templates also warns: "Note the volume of light tracks need to be
low. An ideal balance has been set so you won't want to go higher than this. If
you go lower it alters the brightness and balance of the lights." **[PM-dev]** —
[blog entry](https://mindplacesupport.com/forum/entry.php?65-GNaural-SpectraStrobe-and-AudioStrobe-templates&bt=281)

### 3.3 A second encoder-developer statement

Encoder developer **TomasC**, MindPlace forum, 12 April 2019:

> "Signal levels of the RGB color tones representing 100% brightness are -24 dB,
> and standard ref. signal level is 1/4 of the color tones."

**[enc-dev]** —
[thread 3173](https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content).

This is **exactly the same 4:1 relationship** as the MindPlace developer
statement: colour at -24 dB, reference at 1/4 amplitude (-36 dB equivalent).
Two encoder developers, four years apart, state the same convention.

### 3.4 What the consumer documentation says

The Kasina Quickstart/Manual give **no numeric reference or light-channel
levels**; they only say encoded files need high bitrates and low compression.
**[PM-doc]** — [Kasina Quickstart](https://mindplacesupport.com/download/1244/).
A 2025 forum question about reference level was answered by MindPlace support
with a link to the Lumasonic production document rather than a number.
**[PM-forum]** —
[thread 3737](https://mindplacesupport.com/forum/showthread.php?3737-Strobe-reference-level).
So the numeric convention lives in **developer guidance and tools**, not in the
consumer documentation.

---

## 4. Cross-check: Lumasonic docs and the current v2 statement

| Source | Reference level | Full-brightness colour level | Implied colour/reference ratio |
|---|---|---|---|
| **[PM-dev]** Hendrickson 2015 | "total amplitude … -36db" | "each light channel … -24db" | **4:1** (+12.04 dB) |
| **[PM-dev]** Hendrickson Audacity tooling 2015 | generator 0.015 ≈ -36.5 dBFS; normalise -36.5 dB | single -22 dBFS (per-carrier ≈ -22.5 dBFS in sums) | **≈4:1 to 5:1** (+12 to +14 dB) |
| **[enc-dev]** TomasC 2019 | "1/4 of the color tones" (≈ -36 dB) | "-24 dB = 100% brightness" | **4:1** (+12.04 dB) |
| **[Lum-doc]** production/mixing table | "reference tone dB level" | colour track **on reference level** = full brightness | **1:1** (0 dB) |
| **[Lum-doc]** headroom note | — | "4 stereo tracks, each with output between **-32 dB and -24 dB** when fully on" | reference would be inside the same -32…-24 range → **≈1:1** [inference] |

Sources: [Lumasonic format spec](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[production and mixing](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md);
the conflict is already recorded as **[open]** in `docs/v2/SPECTRASTROBE.md`
§1.2 and §7.

**Reconciliation hypotheses (inference, not established):**

- **H-doc-1 — fader level vs file level.** The Lumasonic table may describe
  *DAW track fader* positions (reference fader and colour fader equal), while the
  encoder applies about -12 dB to the reference internally. That would reconcile
  1:1 (user-facing) with 4:1 (file-level).
- **H-doc-2 — different measurement points.** "-36 total", "-24 per light
  channel", "-32…-24 per track" may mix RMS/peak and total/per-ear measures. The
  reference is hard-panned (one ear at a time) while in our calibration the
  colour channels are centred (both ears equal); "total amplitude" vs "per light channel" may not be the same
  quantity. The 4:1 figure is the one two developers state explicitly.
- **H-doc-3 — documentation drift.** The Lumasonic table is newer but generic;
  the Prism plug-in exposes separate **Reference Gain** and **Color Gain**
  controls, so fader-level instructions do not pin the file-level ratio.
  **[Lum-doc]** — [Prism settings](https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/03-settings-screen.md)

Despite the remaining 1:1 vs 4:1 documentation conflict, the **weight of the
developer record is 4:1**: two developers and one developer's own code state that
full-brightness colour is about 12 dB above the reference, not equal to it. The
hardware A/B in §6 is designed to resolve this empirically for the actual
Kasina/Ganzfeld, firmware and settings.

---

## 5. What the current calibration does under that convention

Current file (`calibration.json`): reference **-30 dBFS peak**; each active colour
carrier **6.25% of the reference peak**. Measured by the verifier: reference
-30.00 dBFS; carrier ratio 6.25% (max 6.26%); peak -28.57 dBFS.

| Quantity | Current calibration | MindPlace 4:1 convention | Difference |
|---|---|---|---|
| Reference peak | -30.0 dBFS | ≈ -36 dBFS | **+6.0 dB** hotter |
| Per-carrier colour level | **-54.08 dBFS** | **-24 dBFS** at full brightness | **30.08 dB below full** |
| Colour/reference ratio | 0.0625 = **-24.08 dB (1/16)** | 4.0 = **+12.04 dB (4×)** | **36.12 dB below** |
| Colour level as fraction of full brightness (if brightness ∝ ratio) | **~1.56% of full** | 100% | — |
| Per-carrier level at crossfade midpoint (50% amplitude) | ≈ **-60.1 dBFS** (≈32 LSB at 16-bit) | ≈ -30 dBFS | — |
| Production hard ceiling (25% of reference) | ratio 0.25 (-12.04 dB) | 4:1 = +12.04 dB | still **24.08 dB below full** (6.25% of full) |

**Hypothesis for the black crossfade (H-level), not a finding.** At 6.25% of the
reference the file already sits about 30 dB below the developer-record
full-brightness level; during a 250 ms amplitude crossfade each carrier drops a
further ~6 dB, to about -60 dBFS. If the Kasina decoder, LED driver or eye has a
minimum usable drive / nonlinear transfer around or above that region, the
overlap can read as black even though the carrier envelopes never reach zero.
The same crossfade at the developer-record level (-24 dBFS colour, -36 dBFS
reference) would keep each carrier at -30 dBFS mid-fade — far above that
region — and should read as a mix. The observation that **steady states were
recognised** (including green and mixed colours) while **transitions looked
black** is consistent with a dip crossing a threshold, not with a broken file.

**Alternative and confounding explanations that must stay on the table:**

- **H-hardware (confound).** Issue #31 reports MacBook output at maximum, Kasina
  AUX amplification at maximum (10) and Kasina brightness at maximum. Maximum
  input gain can clip or change the decoder's operating point; the reference is
  hard-panned while the colours are centred, so a clipped input may interact
  differently with the reference. This session is **not a clean ratio test**.
- **H-shape.** A 250 ms centred smoothstep crossfade reduces *each* channel to
  50% at the boundary; if brightness is sublinear in amplitude or the decoder
  has hysteresis/AGC, the total light can dip more than the envelopes suggest.
- **H-perception.** 1 s states with a 250 ms dip may be perceived as
  "fade to black" because of contrast adaptation and the unequal luminance of
  the LEDs (LEDs can differ in luminance at equal drive; green is often less
efficient than red), without
  the light literally reaching zero.
- **H-doc.** If the decoder actually implements 1:1 (Lumasonic table), the
  current 6.25% is 6.25% of full rather than ~1.56%; still dim, but the ratio
  error would be "only" 24 dB instead of 36 dB. The A/B must distinguish this.

The numbers in the table assume: brightness linear in the colour/reference ratio,
full brightness at 4× reference, and a per-carrier reading of "-24 dB". All three
are **[inference]/[open]**; the decoder transfer curve is unpublished.
Quantisation is unlikely to be the dominant cause: at -54 dBFS a 16-bit carrier
still has ~65 LSB per sample, and the decoder filters are narrow-band. It becomes
marginally tighter at -60 dBFS (~32 LSB) but is not the first suspect.

---

## 6. Controlled A/B ladder — implemented in `ab/`

> **Implementation status (5 November 2026, corrected):** the under-ceiling
> hardware set is implemented in `calibration/ab/`: `direct-m54p1` (12.4% of
> reference), `direct-m48p1` (~24.8%), `overlap-m48p1` (~24.8%) and
> `lr-m48p1` (~24.8%). The higher ladder variants (`-42/-36/-30/-24`, plus
> `overlap-24`/`lr-24`) are offline-verified but **gated from hardware playback**
> until a separate explicit decision. The hard-switch S2 variant and the
> optional reference/rate controls were not rendered: an abrupt 0 ms switch adds
> a transient without being necessary, and extra variants increase exposure.
> `ab/run_ab.sh` exits 0. See `ab/out/ab-report.md`,
> `ab/out/ab-transition-envelopes.csv` and `ab/AB-PROTOCOL.md` for the corrected
> procedure.

**Goal:** determine whether the perceived black transition is a level/threshold
effect (and which reference-to-colour convention the hardware follows), using
otherwise identical files at a fixed, conservative hardware setting.

### 6.1 Design principles

- **One primary variable:** colour-carrier level. Everything else identical:
  same 9-state sequence, 1 s states, 250 ms centred smoothstep crossfades, 300 ms
  fades, 9.6 s total, same frequencies/phases/panning, same 44.1 kHz/16-bit PCM,
  no metadata, no dither, no normalisation.
- **Reference fixed at -36 dBFS peak** (the developer-record value), 18.2 kHz
  sine, 20 Hz sine pan, depth 100%.
- **Colour channels centred** (equal L/R) in the main ladder, as in the current
  calibration.
- **No normalisation as a variable**; loudness normalisation is explicitly not
  applied. No overall gain tweaks on the Kasina or the Mac between files.
- **Ascending exposure**; one pass at a time; fixed, recorded low hardware
  settings; maximum hardware settings are not a target and must not be used.

### 6.2 Ladder levels (reference -36 dBFS peak)

| Step | Colour carrier level | Ratio to reference | Ratio (dB) | Role |
|---|---|---|---|---|
| C0 (anchor) | **-54.08 dBFS** with reference **-30 dBFS** | 1/16 | -24.08 | Exact reproduction of the observed calibration file (current state) |
| L0 | **-54.1 dBFS** | 1/8 | -18.10 | Current *absolute* carrier level; still ~30 dB below developer full |
| L0.5 *(optional, cap-respecting)* | **-48.1 dBFS** | 1/4 | -12.04 | Highest level still inside the production 25% ratio ceiling |
| L1 | **-42 dBFS** | 1/2 | -6.00 | +12 dB over L0; first "recovery" test |
| L2 | **-36 dBFS** | 1:1 | 0.00 | Tests the Lumasonic 1:1 table interpretation |
| L3 | **-30 dBFS** | 2:1 | +6.00 | Midpoint between the two conventions |
| L4 | **-24 dBFS** | 4:1 | +12.04 | MindPlace/TomasC full-brightness anchor |

Notes: C0 is the baseline the black-crossfade observation came from; it differs
in reference level, so it is an anchor, not part of the ratio ladder. A strict
reproduction of the *current ratio* at -36 dBFS reference would need -60.1 dBFS;
L0 deliberately starts at the current *absolute* level instead, as requested for
issue #31. At L4, a white state (three carriers) sums to about -14.5 dBFS per
ear plus the -36 dBFS reference; peak stays far below -6 dBFS. Sixteen-bit
resolution is ample at every step.

**Ceiling correction and the playable subset.** The former conservative research
ceiling is **25% of the reference peak per active colour carrier**, which at a
-36 dBFS reference means **-48.04 dBFS**. Only **L0 (-54.1 dBFS, 12.4%)** and the
new **L0.5 boundary (-48.1 dBFS, ~24.8%)** are under/at that ceiling. **L1
(-42 dBFS, 50.1%), L2 (-36 dBFS, 100%), L3 (-30 dBFS, 199.5%) and L4
(-24 dBFS, 398.1%) are all above the ceiling** — the earlier note that listed
only L3/L4 (and the -24 shape/LR variants) as above was wrong. The above-ceiling
files are **offline-only/gated**: do not play them, even with lower physical
controls, until a separate explicit decision and revised safety rationale. The
25% ceiling is a conservative research convention, not a proven physical safety
threshold and not a MindPlace decoder specification.

### 6.3 Shape variant at one level (L4)

Same sequence, same levels, three transition shapes:

- **S1 — current shape:** 250 ms centred smoothstep amplitude crossfade.
- **S2 — hard switch:** 0 ms crossfade at the state boundary (single isolated
  transition; dim; never repeated faster than the 1 s state rate).
- **S3 — hold-both / make-before-break:** the incoming carrier reaches full level
  before the outgoing carrier starts to fall, so both are at full level for at
  least 100 ms. Document that the effective transition is longer than 250 ms;
  keep the 1 s dwell so the comparison is still perceptual.

Purpose: separate *level threshold* from *envelope shape*. Because L4 is gated
from hardware playback, the first under-ceiling shape comparison is at the
boundary (`overlap-m48p1` vs `direct-m48p1`, both at ~24.8% of reference and
fixed hardware settings); the L4 shape variants remain offline-only. If the
boundary shape comparison is continuous but direct is black, the midpoint/shape
hypothesis gains support; if both are black, shape alone is not sufficient.

### 6.4 Matched L/R alternation control

Same nine-state timing and levels, but each state hard-pans its colour to one ear
only, alternating left/right per state (a ladder version of the existing
`calibration-lr` file). The under-ceiling comparison is `lr-m48p1` at ~24.8% of
reference; the L0/L4 ladder versions are offline-only/gated. Purpose: an
inactive eye is *expected* to be dark; the control prevents that expected
darkness from being counted as the main-file black-crossfade finding, and checks
eye assignment. The control is not itself evidence about crossfades.

### 6.5 Optional secondary controls

- **Reference-level control:** at one colour level (e.g. L1), compare reference
  -36 dBFS vs -30 dBFS to test whether reference level alone (detection/AGC)
  changes visibility. This is not a ratio variable; it isolates the C0 anchor
  difference.
- **Rate control:** 44.1 vs 48 kHz at one level, to check resampling effects; not
  a primary variable.

### 6.6 Hardware settings (fixed, conservative)

- **Kasina LED brightness:** fixed low position (e.g. the lowest two steps),
  recorded once and never changed within a session. Do not use maximum.
- **Kasina AUX amplification:** fixed mid position (e.g. 5 of 10 if that is the
  scale; record the actual value). Do not use maximum 10.
- **Source level:** fixed moderate MacBook output (e.g. ~50–60%), same player,
  same cable, same routing for all files; no OS EQ, spatial audio or normalisation.
- **Glasses:** standard Ganzframes, eyes closed, first; DeepVision only after the
  standard glasses were comfortable, at the same or lower brightness.
- **One operator, one watcher; stop within reach** (stop playback, Kasina
  brightness to minimum, glasses off). Room lit, no other flicker sources.
- Record firmware version, Kasina settings, player, file order and date.
- **Keep the A/B separate from physical brightness.** The under-ceiling A/B is
  run at one fixed Kasina brightness and AUX setting; do not change hardware
  brightness between files and do not use it to emulate a higher signal ratio.
  A physical-brightness comparison at a fixed file is a separate, gated
  experiment and requires an explicit decision; it is not part of this A/B.

### 6.7 Observation and analysis

- Two or three passes per file, ascending colour level, with breaks; if a second
  person is available, shuffle and blind the file order.
- For each of the 8 boundaries record: continuous mix / visible dip / perceived
  black / flash; approximate duration; which colours; whether the following
  state was correctly recognised; repeat consistency.
- Compare with the generated timeline for each ladder file (the same
  `colour-timeline.csv`/manifest approach as the calibration).
- Separately judge one-colour→one-colour transitions and transitions involving
  composites (yellow/cyan/magenta/white), since the observation concerns both.

### 6.8 Decision matrix

| Observed pattern | Supported hypothesis | Still unproven |
|---|---|---|
| Crossfades become continuous somewhere between L0 and L4, monotonically | Level/threshold (H-level) | Exact threshold; decoder curve; cause-and-effect vs correlated settings |
| L2 (-36, 1:1) already looks full/continuous and L4 is not brighter | Lumasonic 1:1 table reading (H-doc) | Whether fader vs file explains the docs; decoder mapping |
| Only L4 (-24, 4:1) looks full/continuous | MindPlace/TomasC 4:1 convention | Absolute calibration point; whether 25% cap is too low for this hardware |
| S2/S3 continuous but S1 black at the same level | Envelope dip / threshold (H-shape) | Whether the decoder has hysteresis/AGC; production envelope redesign |
| L/R control shows black only in the inactive eye, main file still black in both | The main finding is not eye-assignment | Crossfade level threshold still open |
| No level or shape removes the black; single colours stay visible | Decoder nonlinearity or perception (H-perception) | Decoder transfer curve, LED minimum drive, observer effects |
| Reference -30 vs -36 changes visibility at the same colour level | Detection/AGC dependence on reference level | Decoder operating point; not a ratio answer |

No outcome in this matrix is a production decision. Any change to the encoder's
ratio, exposure ceiling or envelopes requires a separate safety and design review.

---

## 7. What remains unproven

- **[open]** The Kasina decoder transfer function: linear in amplitude, gamma,
  threshold, AGC or something else. No public specification.
- **[open]** Whether "reference total amplitude -36 dB" means peak or RMS, per
  ear or summed across both channels. The 4:1 reading is the most coherent with
  the tooling (0.015 ≈ -36.5 dBFS per channel) and with TomasC's "1/4 of the
  colour tones".
- **[open]** Whether the hardware follows 4:1 or 1:1. This note argues the
  developer record favours 4:1; the Lumasonic table still implies 1:1.
- **[open]** The minimum visible/decodeable drive of the decoder and the LED
  driver, per colour; green may differ from red.
- **[open]** Whether the 2026 Kasina firmware behaves like the 2015/2019
  developer descriptions.
- **[open]** Whether maximum AUX/brightness in the issue #31 session distorted
  the result (input clipping, decoder saturation, brightness-knob position).
- **[open]** What "fade to black" meant perceptually: true zero light, a dark
  dip, or a contrast/adaptation effect. Only structured repeated observation,
  ideally with a photodiode and time-synchronised capture, can separate these.
- **[open]** Whether the production 25% per-carrier ratio ceiling is compatible
  with useful brightness under a 4:1 decoder; if not, the calibration strategy
  (reference level vs hardware brightness vs ceiling) needs review — not in this
  note.

---

## 8. Safety and governance note

- Ladder steps L1–L4 deliberately explore ratios **above the production 25%
  per-carrier ceiling** in `docs/v2/SPECTRASTROBE.md` §6 and
  `docs/v2/SAFETY-PRIVACY.md` R3; the optional L0.5 sits exactly at that ceiling. They are **research-only diagnostic files**,
  not production defaults: absolute levels stay low (highest colour -24 dBFS,
  total peak far below -6 dBFS), hardware brightness is fixed low, each file is
  short and played one pass at a time, and nothing is looped or sped up. They
  require explicit authorisation under `../HARDWARE-PROTOCOL.md` before any
  hardware session, plus the normal one-operator/one-watcher and immediate-stop
  rules.
- No file from this proposal may be shipped as a NAGLOED preset, integrated into
  *Eerste Gloed*, or used as a default exposure.
- No medical or wellness claims; issue #31 is a signal/perception diagnostic.

---

## 9. References

MindPlace developer statements and tooling:

- [MindPlace forum, *Creating Spectrastrobe signal using Kyma software*,
  neuroasis (Scott Hendrickson), 20 May 2015](https://mindplacesupport.com/forum/showthread.php?2508-Creating-Spectrastrobe-signal-using-Kyma-software)
  · [print view](https://mindplacesupport.com/forum/printthread.php?t=2508&pp=10)
- [Audacity Mind Machine Suite (GPL-licensed tools by Scott Hendrickson)](https://github.com/scottosx/Audacity-Mind-Machine-Suite)
  · [wiki](https://github.com/scottosx/Audacity-Mind-Machine-Suite/wiki)
  · reference generator and normalise chains as linked in §3.2
- [MindPlace forum blog, *GNaural SpectraStrobe and AudioStrobe templates*](https://mindplacesupport.com/forum/entry.php?65-GNaural-SpectraStrobe-and-AudioStrobe-templates&bt=281)

Lumasonic documentation and developer statement:

- [Lumasonic, *SpectraStrobe format specification*](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md)
- [Lumasonic, *Production and mixing* (1:1 table and -32…-24 dBFS headroom note)](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md)
- [Lumasonic, *Prism settings* (Reference Gain, Color Gain, LED Gamma)](https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/03-settings-screen.md)
- [MindPlace forum, *Recommendations on maintaining precise SpectraStrobe control…*,
  TomasC, 12 April 2019](https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content)

MindPlace documentation and support:

- [MindPlace, *Kasina Quickstart Guide*](https://mindplacesupport.com/download/1244/)
- [MindPlace forum, *Strobe reference level* (2025 support referral)](https://mindplacesupport.com/forum/showthread.php?3737-Strobe-reference-level)

Project artefacts:

- [NAGLOED issue #31](https://github.com/leonvanbokhorst/nagloed/issues/31)
- `docs/v2/SPECTRASTROBE.md` §1.2/§7 (1:1 vs 1:4 conflict, open)
- `docs/v2/SAFETY-PRIVACY.md` R3 (25% per-carrier production ceiling)
- `experiments/spectrastrobe-poc/calibration/CALIBRATION.md` and
  `out/calibration-report.md` (41/41 PASS, 0.0 ms encoded gap)
- `experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md` (binding safety reference)
