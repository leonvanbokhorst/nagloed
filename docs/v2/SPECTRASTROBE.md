# SpectraStrobe — signal model, signal integrity and hardware verification

> **Status:** research document for the v2 exploration (issue #14), **out of scope
> for v1.0.4**. Not an implementation proposal for `src/`, presets or release.
> This document is the authority for the measurement plan and hardware protocol
> referenced by `docs/v2/EXPERIMENT-01.md` and `docs/v2/ARCHITECTURE-OPTIONS.md`.
>
> **Reading guide:** every claim is marked **[evidence]** (directly in a primary
> source or in code), **[inference]** (technically substantiated, not directly
> confirmed) or **[open]** (cannot be established with the current sources; must
> be measured). Source links are in §8.

---

## 1. Signal model

### 1.1 Carriers

SpectraStrobe encodes light control as four high-frequency sines in an ordinary
stereo audio mix. The playback hardware (Kasina/Limina) filters those bands out
during playback and routes them to the LED glasses. **[evidence]** —
[Lumasonic format spec](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[production/mix notes](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md)

| Carrier | Frequency | Function |
|---|---|---|
| Reference | 18,200 Hz | format detection; brightness scale |
| Red | 18,700 Hz | amplitude → brightness of the red LEDs |
| Green | 19,200 Hz | amplitude → brightness of the green LEDs (also the AudioStrobe carrier) |
| Blue | 19,700 Hz | amplitude → brightness of the blue LEDs |

- Each colour channel is a **stereo track** with independent L/R amplitude; the
  Kasina uses this to drive both eyes separately. That gives 3 × 2 = 6 light
  channels. The Kasina manual explicitly mentions "six light control signals
  instead of two". **[evidence]** — [Kasina Quickstart](https://mindplacesupport.com/download/1244/)
- The reference carries **no** brightness information; it marks the signal as
  SpectraStrobe and provides the amplitude scale. **[evidence]** — Lumasonic spec.
- The reference must **alternate hard left/right** with a panning LFO of
  10–50 cycles per second; the prescribed form is a **sine**, 20 Hz, offset 0%,
  depth 100%. The spec also permits square/pulse, but that adds more audible
  artefacts; the decoder is "somewhat forgiving". **[evidence]** —
  Lumasonic spec.
- The 500 Hz spacing between the carriers is the room within which the decoder
  separates the bands. **[inference]** — the spec gives no filter parameters; the
  spacing and the recommended steep bandpass filters make this the obvious
  reason.

### 1.2 Brightness, panning and reference

- Brightness of a colour channel = **amplitude of the colour channel relative to
  the reference amplitude**. Volume -INF = channel off; at reference level =
  fully on. Panning the colour track determines the left/right balance of that
  channel. **[evidence]** — Lumasonic production doc.
- There is a **source conflict** about the exact ratio at 100% brightness:
  - the production-doc table says: colour track at reference level = full
    brightness (so 1:1);
  - a forum reply from a Lumasonic developer states "RGB at -24 dB = 100%
    brightness" and "reference = 1/4 of the colour tones" (so 1:4).
  **[open]** — [MindPlace forum thread](https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content)
- Our PoC does not have to resolve this conflict: with a **maximum brightness
  ratio of 25% of the reference**, brightness remains conservative under both
  interpretations (25% at 1:1, 6.25% at 1:4). See §7.
- The reference panning is also one of the safety risks: in the raw carrier the
  LFO produces a left/right rhythm of 10–50 per second. That is not visible as
  colour brightness, but it must not leak into the colour channels. See
  `docs/v2/SAFETY-PRIVACY.md`.

### 1.3 Facts for 44.1 and 48 kHz

| Quantity | 44,100 Hz | 48,000 Hz |
|---|---|---|
| Samples per cycle @ 18.2 kHz | 2.423 | 2.637 |
| Samples per cycle @ 19.7 kHz | 2.239 | 2.437 |
| 19.7 kHz as a share of Nyquist | 89.3% | 82.1% |
| 18.2 kHz as a share of Nyquist | 82.5% | 75.8% |

- At 44.1 kHz the carriers sit extremely close to Nyquist (19.7 kHz at 89% of
  22.05 kHz). **[evidence]** — arithmetic from the spec frequencies.
- Any sample-rate conversion, any filter that starts rolling off above ~17 kHz
  and any lossy codec therefore directly affects the carriers in amplitude or
  phase. **[inference]**
- If a 44.1 kHz file is played at 48 kHz without the correct rate, all carriers
  shift by +8.84%: 18.2 kHz becomes ~19.8 kHz, in the middle of the
  blue/reference detection region. **[inference]** — simple frequency-scaling
  calculation.
- Both 44.1 and 48 kHz are permitted; 16-bit PCM WAV is the recommended
  carrier. **[evidence]** —
  [Lumasonic file formats](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/04-file-formats.md),
  [render output](https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/07-rendering-output.md)

### 1.4 Headroom: why four stereo tracks cost space

- The four carriers behave as **four stereo tracks, each between -32 and
  -24 dBFS when fully on**. **[evidence]** — Lumasonic production doc.
- Fully on means: reference plus three colours, all four at a maximum of
  -24 dBFS. Summed coherently that is 4 × 0.0631 FS ≈ 0.252 FS ≈ **-12 dBFS**;
  at -32 dBFS the sum is ≈ -20 dBFS. **[evidence]** — amplitude arithmetic.
  The carriers therefore consume 12–20 dB of the available peak space before
  any music is added.
- In the PoC's practice (reference -30 dBFS, colours ≤ 25% of it) the carrier
  tail is much smaller: reference 0.0316 FS plus three colours of
  0.0079 FS ≈ 0.055 FS ≈ **-25 dBFS** coherent worst case. **[inference]** —
  arithmetic; the ratio used is a design choice, not a spec value.
- The rest of the mix must therefore stay below a **total true peak of -6 dBFS**,
  including the carriers. Without that margin clipping not only sounds bad: the
  intermodulation and harmonic products fold back into the 18–20 kHz band and
  are decoded as false brightness. **[inference]** — see §3.
- 16-bit quantisation is not a problem at these levels: -30 dBFS is over
  1,000 LSB. Dithering must be off, though, because noise shaping pushes noise
  straight into the control band. **[inference]**; Lumasonic recommends
  dithering off **[evidence]** — render output.

---

## 2. Why this is a signal-integrity problem, not an encoding problem

The encoder can be perfect and the playback chain can still corrupt the signal.
The order is always: **process the audio completely first, only then add
carriers, and nothing afterwards**. All threats below affect either the carrier
frequency, the relative amplitude, the L/R phase or the band space.

### 2.1 Threat matrix

| Threat | Mechanism | Effect on light | Status |
|---|---|---|---|
| Resampling 44.1 ↔ 48 kHz | few samples/cycle; filters at Nyquist attenuate 19.7 kHz and create sidebands | weaker or dropped channels; loss of detection | [inference] |
| Wrong sample rate at playback | all frequencies scale (+8.84% at 44.1→48) | decoder recognises nothing or the wrong channel | [inference] |
| DAW low-pass/EQ on master | roll-off from 13–17 kHz, exactly per Lumasonic advice for the *program* | carriers attenuated; colour ratios skewed | [evidence] advice + [inference] effect |
| Saturation/limiters/clippers after the encoder | harmonics and intermodulation; gain pumping | false brightness, flicker, change in colour ratio | [inference] |
| Lossy codecs (MP3/AAC, Bluetooth) | high-frequency cut around 19–20 kHz, psychoacoustics, joint stereo | 19.7 kHz disappears or L/R phase is distorted | [inference]; MP3 320 kbps CBR stereo is recommended by Lumasonic [evidence] |
| OS audio EQ/spatial audio/normalisation | system-wide EQ, spatial audio, loudness processing | unpredictable colour and brightness errors | [evidence] warning in Lumasonic doc |
| Mono summing | adding L+R cancels the reference panning; 19.2 kHz remains | detection fails or falls back to AudioStrobe | [inference] |
| Main/bus routing | encoder on a subgroup that then passes through the master chain (master EQ, glue compressor, limiter, "high cut"); or output to a mono bus / wrong L/R | detection loss, colour distortion or swapped eyes | [inference] |
| Dither/noise shaping on 16-bit export | noise is shaped towards 15–20 kHz | raised noise floor in the control bands | [inference]; dithering off is [evidence] |
| Old Kasina firmware + WAV metadata | firmware played metadata as audio at the end | loud noise burst + bright flash at the end of the file | [evidence] — forum thread (2019, fixed later) |
| USB audio driver | driver glitches, sample mismatch | clicks that decode as flashes; temporary dropouts | [evidence] — forum thread |
| Loud program content relative to the carriers | decoder loses the reference or saturates | LEDs drop out or colour becomes unstable | [evidence] — forum thread (empirical, one user) |

### 2.2 Consequences for the architecture

- **Never** pass carriers through NAGLOED's own low-pass, saturation, LoFi,
  time-stretch or pitch-shift; those sit in the EAR chain. [inference based on
  issue #14 and the signal physics]
- Add carriers after all non-linear processing, in a separate
  (post-processing) step or offline render. [technical solution]
- Prepare the program before summing: taper from 13 kHz, steep low-pass at
  16–17 kHz. That is exactly the Lumasonic advice. [evidence]
- Final responsibility stays with the export: one verification report per file,
  no assumptions about the host. [inference]
- Explicitly check whether the main/master channel applies any processing after
  the encoder (high cut, limiter, loudness normalisation). If so, that is the
  failure point, not the encoder. [inference]

---

## 3. Measurement plan and tolerances

This table is binding for the PoC verification. Measurements are made **offline
on the exported WAV** (and, where stated, on the photodiode recording), not on
intermediate results in the DAW. All tolerances are provisional: the Kasina
specification publishes no decoder passband, so these are design targets to be
validated on hardware. [open]

| # | Measurement | Method | Target / tolerance |
|---|---|---|---|
| M1 | Carrier frequencies | FFT ≥ 65,536 points, Hann, parabolic interpolation | 18,200 / 18,700 / 19,200 / 19,700 Hz, **±1 Hz** |
| M2 | Absolute reference | **peak amplitude** of the 18.2 kHz band (sample peak of the complex envelope); band RMS is reported separately | **-30.0 dBFS ± 0.5 dB**, constant across the piece |
| M3 | Relative channel amplitude | RMS per colour band relative to the reference | follows the score within **±0.5 dB**; max **25%** (-12.04 dB) |
| M4 | L/R balance and isolation | RMS L and R per colour band | at centre **±0.5 dB**; when hard-panned isolation **≥ 40 dB** |
| M5 | Reference pan modulation | envelope of 18.2 kHz in L and R | LFO **20 Hz ± 0.5 Hz**, depth **≥ 99%** (out-of-phase ≥ 30 dB down), no residual carrier in the "off" phase |
| M6 | Inter-carrier isolation | render **one colour at a time**; measure the energy at the other carrier frequencies (not the sidebands of the carrier itself) | **≤ -40 dB** relative to the carrier itself |
| M7 | Program leakage in the control band | program-only render, FFT 17–20 kHz; **plus a broadband probe** (noise through the same LPF) | **≤ -60 dBFS** in every band for the actual mix; the broadband probe reports the measured attenuation honestly as INFO/EXPECTED-DEGRADED, because an 8th-order LPF does not automatically reach -60 dBFS at 17 kHz |
| M8 | Noise floor with muted colours | FFT of the 18.7/19.2/19.7 kHz band without a colour signal | **≤ -80 dBFS** |
| M9 | Peak / true peak | 4× oversampled true peak, sample peak | **≤ -6 dBTP**, no clipping, no non-finite samples |
| M10 | Determinism | two renders, same seed/platform/rate | **byte-identical** |
| M11 | Detection on hardware | Kasina display + visual | **120 s** uninterrupted SpectraStr detection |
| M12 | LED output (objective) | photodiode + audio interface, FFT/autocorrelation | no periodic luminance modulation at **3–30 Hz with ≥ 10% depth**; brightness monotonic with the ladder |
| M13 | Stop latency | measured audio signal + photodiode | carriers fully off within **50 ms** after the stop action |
| M14 | Corruption matrix | deliberately corrupted variants (§4.4), including colour channels (frequency and amplitude) | each case documented as pass/expected-degraded/info, not "probably fine"; advisory unless `--strict-corruption` |

In addition: one **program-only A/B WAV** (without carriers) accompanies every
render, so that M7 and a subjective A/B are always possible. This matches the
output of `docs/v2/EXPERIMENT-01.md`.

---

## 4. Hardware verification protocol

### 4.1 Setup

- Mac with the PoC renderer; **no** audio interface in the signal chain unless
  explicitly as a test variant.
- MindPlace Kasina with current firmware (the 2019 WAV-metadata bug is fixed in
  later firmware; verify this). [evidence] — forum thread
- microSD card with 16-bit PCM WAVs, 44.1 and 48 kHz, **without metadata**, simple
  file names.
- Standard Ganzframes (intended for eyes closed) and DeepVision Ganzframes
  (intended for eyes open, higher maximum brightness, diffuse colour field).
  [evidence] — Lumasonic device doc
- Kasina LED brightness at a **fixed, recorded setting** (not "max"); audio and
  LED brightness are separate controls. [evidence] — Lumasonic Kasina doc
- Optional but strongly recommended: photodiode (e.g. BPW34) + audio interface
  to capture the LED output objectively (M12, M13).
- **Stop means within reach:** software panic, Kasina brightness to minimum,
  plug/power, glasses off. One person operates, one person watches.

### 4.2 Order (do not skip, do not parallelise)

1. **Safety first.** Read `docs/v2/SAFETY-PRIVACY.md`. No person with (suspected)
   photosensitivity in the room. Room lit, no other flicker sources. First steps
   with the **standard Ganzframes, eyes closed**, Kasina brightness low.
2. **T0 — static detection.** 60 s reference-only, all colours at 0%. Kasina must
   show `Mode SpectraStr/AS` with SpectraStr active. If not: stop, this is a
   format/panning error, not an artistic problem.
3. **T1 — brightness ladder 44.1 kHz.** One colour at a time (R, G, B), then
   white; relative amplitude 0, 1/64, 1/32, 1/16, 1/8, 1/4 of the reference;
   10–15 s per step, ascending, 1 s fades, 5 s dark in between. Record per step:
   detection, visible colour, unwanted channels, flicker, comfort. On doubt or
   discomfort: immediately back to 0 and stop. For *Eerste Gloed* the PoC
   provides two extra variants: **dim** (max 6.25% = 1/16) and **mid**
   (12.5% = 1/8); the full score (20% ≈ 1/5) is the third step.
4. **T2 — same at 48 kHz.** Same ladder, same Kasina settings. A difference in
   behaviour between 44.1 and 48 kHz is a finding, not a detail.
5. **T3 — program + carriers.** Mix with program at -24, -18, -12 and -6 dBTP;
   find the point where detection drops out (forum experience: loud
   low-frequency content can make the decoder drop out). This calibrates the
   safe program/carrier margin.
6. **T4 — DeepVision, eyes open.** Only if T1–T3 were comfortable and stable:
   the same ladder at **lower** Kasina brightness, because DeepVision appears
   much brighter. [evidence] — Lumasonic device doc
7. **T5 — dynamics and stop.** 1 s fade-in/out, 50 ms fade, hard cut. Test the
   **human stop** (stop playback, Kasina brightness to minimum, glasses off); a
   real encoder panic of ≤ 50 ms belongs to realtime phase B and is not
   available in an offline WAV. Measure M13 only if the photodiode is set up;
   otherwise this is an observation, not a measurement.
8. **T6 — objective LED measurement.** Photodiode recording at 25% maximum: M12.
9. **T7 — corruption matrix** (§4.4), only at low brightness and with the
   glasses disconnected if needed; observe the Kasina display as the primary
   indicator.

Only after a fully green T0–T7 is there a hardware basis for the artistic
experiment. [technical solution]

### 4.3 Acceptance and stop criteria

- **Continue** only if M11, M12 and M13 pass and the whole ladder runs without
  unwanted channels or flicker.
- **Stop and report** on: loss of detection, visible flicker outside the
  intended colour transition, discomfort, headache, or an unexplained flash.
  This is information, not a failure; document cause and circumstances.
- **Never** raise brightness to mask a detection problem. Fix signal integrity
  first (M1–M10), then hardware.

### 4.4 Corruption matrix (deliberate)

| Variant | Expectation | Purpose |
|---|---|---|
| 48 → 44.1 kHz clean SRC | carriers intact, slight HF attenuation | determine the limit of SRC |
| 48 → 44.1 kHz with cheap/naive SRC | distortion, loss of detection | make the failure mode visible |
| 44.1 file played at 48 | frequencies +8.84%, detection fails | prove rate mismatch |
| Mono summing | reference panning gone | demonstrate fallback to AudioStrobe |
| L/R swapped | detection possible, eyes swapped | record tolerance |
| MP3 320 kbps CBR stereo | intact | validate the production path |
| MP3 128 kbps / joint stereo | audible and visible loss | document the lower bound |
| Reference LFO 10 and 50 Hz | detection within spec range | record the range |
| Reference LFO 5 Hz | detection fails | confirm the lower bound |
| Square/pulse panning instead of sine | detection OK, more artefacts | substantiate the waveform choice |

---

## 5. Failure modes

| Failure mode | Observable | Likely cause | Response |
|---|---|---|---|
| No SpectraStrobe detection | Kasina shows AS or ColorOrgan | reference missing, LFO too slow, mono summing, rate mismatch | work through M5 and §2.1 |
| Colour leaks on silent channels | red/green LEDs dim or flicker without a command | crosstalk, clipping, noise floor, old firmware | M6/M8, carrier level, firmware |
| LEDs drop out on loud music | light goes dark on bass/dynamics | insufficient reference relative to the program; decoder loses the reference | T3; program/carrier margin |
| Bright flash at end of file | noise burst + flash | WAV metadata on old firmware | strip metadata, update firmware |
| Unintended brightness jumps | light "pumps" with the music | limiter/saturation/OS processing after the encoder | §2.1, clean up the chain |
| Wrong colour or eye | red/green or L/R swapped | bus routing, channel order | M4, routing test |
| Slow detection at start | no light in the first seconds | reference starts too late or too quiet | ≥ 1 s reference lead-in |
| Clicks/flashes over USB | irregular bursts | USB driver, sample mismatch | microSD baseline, driver reset |
| Light too weak at 25% | Ganzfeld stays dark | Kasina brightness or LED calibration | raise hardware brightness, not the carriers |

---

## 6. Encoder requirements for the PoC

These requirements are the implementation brief for
`experiments/spectrastrobe-poc/`. They are conservative: they may limit
artistic brightness, never safety or detection. [technical solution]

**Format and numbers**

- Sample rates: **48,000 Hz** (master, 24-bit float internally, 24-bit PCM
  export) and **44,100 Hz** (16-bit PCM variant). Only these two.
- Reference: **18,200 Hz**, fixed **-30.0 dBFS** (peak amplitude), present for
  the duration of the piece; ≥ 1 s lead-in; fades out together with the colours
  on stop.
- Reference panning: **sine, 20.0 Hz, depth 100%**, offset 0%, computed per
  sample (no block-rate LFO), phase-continuous. The range 10–50 Hz is
  permitted; default 20 Hz.
- Colours: **18,700 / 19,200 / 19,700 Hz**, tolerance **±1 Hz**.
- Brightness ratio: amplitude relative to the reference, **maximum 25%**
  (-12.04 dB) **per colour channel**, **default 6.25%** (-24.08 dB). Per ear
  separately, as a direct channel amplitude; **no -3 dB centre-pan law** (that
  would halve the decoded brightness). [inference] The 25% is a ceiling per
  channel, not an overall ceiling: the sum of red + green + blue in one ear can
  be higher (in *Eerste Gloed* 29.5% at the L1 peak). The PoC therefore also
  monitors that sum and keeps the total true peak ≤ -6 dBTP.
- Sum of all carriers: no clipping; total **true peak ≤ -6 dBTP** (measured 4×
  oversampled), sample peak ≤ -6 dBFS, no non-finite samples.

**Program and order**

- Process the program completely first, then sum the carriers; **nothing** after
  the encoder: no EQ, saturation, limiter or normalisation.
- Program low-pass: taper from **13 kHz**, steep (≥ 48 dB/oct) at **16 kHz**;
  measured energy 17–20 kHz **≤ -60 dBFS** (M7).
- Choose the program level on the total true peak, not on listening experience.

**Export**

- WAV, PCM, stereo, **no metadata**, **no dither/noise shaping**.
- File name and report include rate and version; every render comes with a
  program-only A/B WAV and the verification report (M1–M10, M14).

**Timing and stop**

- Envelopes per colour/per ear: smooth attacks/releases, **≥ 20 ms**, no steps
  or discontinuities. The PoC validates this and fails hard on a score that
  exceeds the limit.
- Stop/panic: carriers and reference fully off within **50 ms**, click-free
  (short ramp ≤ 5 ms), then silence. This is a requirement for the **realtime
  encoder in phase B**; the offline PoC has no panic stop. There the primary
  stop is human: stop playback, Kasina brightness to minimum, glasses off. That
  human stop is not a 50 ms ramp and is not measured as one.
- Duration and start: reference first, then colours; no detection-dependent
  startup tricks.

**Realtime (only if this later goes into the plug-in)**

- No allocation, locks, file or network operations in the audio path; fixed
  buffers; double-precision phase accumulator; deterministic.
- The encoder is a separate step *after* the existing DSP chain, never inside
  it. [technical solution]

**Safety**

- The encoder limits itself, even if the artistic score asks for more: the
  colour ceiling (25% per colour channel), the minimum attack/release (≥ 20 ms),
  a check for sudden steps and an upper bound on the number of brightness
  transitions per second. The PoC implements this limiting and fails hard on an
  unsafe score; a score that asks for more is clamped. The reference panning
  (20 Hz, 100% depth) is a format requirement and not intended luminance
  modulation; leakage into the colour channels is a measurement point (M5/M6)
  and a stop criterion.
- A real panic stop of ≤ 50 ms belongs to phase B (realtime); the offline PoC
  does not claim it.

---

## 7. Open questions

1. **[open]** What are the exact decoder passband and amplitude characteristic
   of the Kasina firmware? Public sources only mention "steep bandpass filters".
2. **[open]** Is 100% brightness 1:1 or 1:4 relative to the reference? (§1.2.)
   Affects absolute calibration, not the safe upper bound.
3. **[open]** How much tolerance does the decoder have for LFO frequency,
   waveform and duty cycle? The forum thread suggests "some latitude"; a number
   is missing.
4. **[open]** Does the Kasina resample internally between 44.1 and 48 kHz, and
   with which filter? Determines whether the 44.1 variant is really different
   from the 48 variant.
5. **[open]** What do MP3 320 kbps and AAC do to 19.7 kHz and to the L/R phase
   of the reference? Answerable only by measurement.
6. **[open]** How does DeepVision brightness compare quantitatively to the
   standard Ganzframes? Lumasonic says "much brighter"; a number is missing.
7. **[open]** What is the minimum perceptible brightness at a 25% brightness
   ratio with the Kasina at minimum? If that is too low, the artistic range is
   too narrow and the ratio must go up — with a safety check.
8. **[open]** Does the Kasina support 24-bit WAV? Lumasonic recommends 16-bit;
   our master is 24-bit. The 16-bit variant is the safe playback version.
9. **[open]** Can a VST3 host deliver a clean, unprocessed stereo bus that
   reaches the Kasina without resampling or processing? Issue #14 asks this; the
   answer differs per host and must be measured.
10. **[open]** What is the latency from sample to LED, and is it stable enough to
    make light and sound feel "related"? (An artistic question, but technically
    measurable.)

---

## 8. Sources

Primary (specification and manufacturer documentation):

- Lumasonic, *SpectraStrobe format specification*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md
- Lumasonic, *Production and mixing*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md
- Lumasonic, *Devices (Kasina/Limina, Ganzframes, DeepVision)*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md
- Lumasonic, *File formats*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/04-file-formats.md
- Lumasonic, *Kasina*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/08-kasina.md
- Lumasonic, *Rendering output*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/07-rendering-output.md
- Lumasonic, *Health and safety*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md
- MindPlace, *Kasina Quickstart Guide*:
  https://mindplacesupport.com/download/1244/
- MindPlace Support Forum, *Recommendations on maintaining precise
  SpectraStrobe control for given audio content?* (2019):
  https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content

Secondary / independent implementation:

- SpiraStrober (MIT, browser-based SpectraStrobe/AudioStrobe generator):
  https://github.com/Syluminose/SpiraStrober
  - code inspection: carriers 18,700/19,200/19,700/18,200 Hz, sample rate
    44,100 Hz hardcoded, 20 Hz sine panning of the reference, strong relative
    attenuation of the colour channels, WAV export. Details from minified code
    are **inference**, not specification.

Artistic/architectural framing:

- NAGLOED issue #14: `gh issue view 14 --repo leonvanbokhorst/nagloed`
- `docs/v2/EXPERIMENT-01.md`, `docs/v2/ARCHITECTURE-OPTIONS.md`,
  `docs/v2/SAFETY-PRIVACY.md`
