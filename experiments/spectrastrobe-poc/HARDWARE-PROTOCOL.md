# Hardware protocol — Eerste Gloed (SpectraStrobe PoC)

> **EXPERIMENT — not part of NAGLOED v1.0.4.**
> This protocol belongs to `experiments/spectrastrobe-poc/`. It is a
> conservative, human verification of the signal on a real MindPlace Kasina +
> DeepVision Ganzfeld. **No medical, therapeutic or wellness claims.** The goal
> is to establish whether the signal is recognised and whether the gesture does
> something artistically — nothing more.
>
> Binding: `docs/v2/SAFETY-PRIVACY.md` (R1–R12) and
> `docs/v2/SPECTRASTROBE.md` §4 (M11–M13). This document is the short,
> executable variant for *Eerste Gloed*.

## 1. Beforehand — safety and consent

- **No person with (suspected) photosensitivity or epilepsy in the room.**
  In doubt: do not proceed. Children/adolescents only with supervision and
  consent of an adult (R9).
- **Consent and verbal instruction beforehand:** what you are going to see,
  that the light can be bright, that you may always stop, and how to take the
  glasses off. Do not use with people who cannot or do not want to consent
  (asleep, under the influence, children without supervision).
- **One person operates, one person watches.** The operator does not watch
  inside the glasses and keeps the stop within reach.
- Room lit, no other flicker sources (spotlights, beamers, disco lights) in the
  same field of view.
- **Covering one eye (R11)** is a valid emergency measure and strongly reduces
  the effect; the operator must be able to instruct it.
- **Stop means within reach:** stop playback, Kasina brightness to minimum,
  plug/power, glasses off. Stopping is always allowed without explanation;
  "this is enough" is a valid reason.
- On discomfort, headache, dizziness, an unexplained flash or loss of
  detection: **stop immediately** and note it. That is information, not failure.
- **No software panic claim.** This offline WAV has **no encoder panic and no
  tested 50 ms software ramp**. The primary stop is human: stop playback +
  Kasina brightness low + glasses off. (A realtime panic belongs to phase B;
  see `docs/v2/SPECTRASTROBE.md` §6.)

## 2. Hearing and level — before the session

- The carriers (18.2–19.7 kHz) are **not guaranteed inaudible**; that depends
  on age, hearing, transducers and playback level (open assumption **U15**).
- Choose **one fixed, noted playback level** (not "as loud as needed to hear
  the programme"). Note it on the form.
- **Comfort listening test (10 s, carriers only)** at the intended level: play
  `spectrastrobe-dim-44k1-16bit.wav` or the first seconds of the master, and
  ask about audibility, sharpness or discomfort. With tinnitus, hyperacusis,
  young ears or doubt: lower the level or do not take part. This is a comfort
  and hearing-prevention measure, not a medical test.

## 3. Material

- Mac with this directory; `./run.sh` must be green (exit code 0) before the
  session.
- Copy to a microSD card, **without metadata**, short file names:
  - `spectrastrobe-dim-44k1-16bit.wav` — **6.25% max**: first exposure
  - `spectrastrobe-mid-44k1-16bit.wav` — **12.5% max**
  - `spectrastrobe-44k1-16bit.wav` — **20% max** (full score)
  - `program-only-44k1-16bit.wav` — A/B without carriers
  - `spectrastrobe-48k-24bit.wav` — **not verified on the Kasina**; only if the
    firmware can handle 24-bit/48 kHz (unknown, open question 8 in
    `docs/v2/SPECTRASTROBE.md`)
- Kasina with current firmware. LED brightness at a **fixed, noted position** —
  not "max". Audio and LED brightness are separate controls.
- Standard Ganzframes (eyes closed, first steps) and DeepVision Ganzframes
  (eyes open, diffuser, brighter). DeepVision only after a comfortable ladder.

## 4. Conservative brightness ladder (do not skip)

Every step: **10–15 s**, then 5 s dark. In doubt: back to 0 and stop.

| Step | File | Max colour | Goal |
|---|---|---|---|
| T0 | `spectrastrobe-dim-44k1-16bit.wav` | 6.25% | **Detection**: the Kasina display must show `Mode SpectraStr/AS` with SpectraStr active. If needed, play only the first seconds (reference only). No SpectraStr? Stop: format/panning problem. |
| T1 | `spectrastrobe-dim-44k1-16bit.wav` | 6.25% | **First exposure**: first 15 s (light grain + 20 ms extinction + 180 ms impact) at low Kasina brightness. |
| T2 | `spectrastrobe-mid-44k1-16bit.wav` | 12.5% | **Middle**: full 72 s at the same Kasina setting. |
| T3 | `spectrastrobe-44k1-16bit.wav` | 20% | **Full**: full 72 s at the same Kasina setting. |
| T4 | `spectrastrobe-48k-24bit.wav` | 20% | **48 kHz/24-bit**: only if T0–T3 were comfortable and the firmware supports the format. A difference in detection/brightness is a finding. |
| T5 | `spectrastrobe-44k1-16bit.wav` | 20% | **DeepVision, eyes open**, at **lower** Kasina brightness than T3 (DeepVision feels much brighter). Only after T0–T4. |
| T6 | `program-only-44k1-16bit.wav` | n/a | **A/B without Ganzfeld**: listen only. Does the sound stand on its own? Does the silence carry? |

Then optionally: **watch a second time** (knowing what is coming) to hear
whether recognition changes the sense of kinship.

## 5. Observation form

Fill this in during or immediately after each step. Short is fine.

| Question | T0 | T1 | T2 | T3 | T4 | T5 | T6 |
|---|---|---|---|---|---|---|---|
| Detection (SpectraStr yes/no) | | | | | | | n/a |
| Visible colour(s) | | | | | | | n/a |
| Unwanted channels / flicker | | | | | | | n/a |
| Left→right movement recognisable | | | | | | | n/a |
| Abrupt extinction (20 ms) seen | | | | | | | n/a |
| Impact experienced as an answer | | | | | | | n/a |
| Silence 28–46 s carried | | | | | | | |
| Unanswered grain at 38 s noticed | | | | | | | n/a |
| Late echo recognisable | | | | | | | n/a |
| Covering one eye tested (R11) | | | | | | | n/a |
| Comfort (1–5) | | | | | | | |
| Notes | | | | | | | |

Context to record: firmware version, Kasina brightness setting, playback level,
sample rate, Ganzframe type, order of the steps, date. Without that context the
observation is not reproducible.

Five perceptual questions (from `docs/v2/EXPERIMENT-01.md` §5), afterwards:

1. Did light and sound feel related to you, or did they seem like two separate
   things?
2. Did you notice that they were not synchronised — did that bother you, or did
   it make the relationship more interesting?
3. What did the silence between 28 and 46 seconds do?
4. Did you notice that the light grain at 38 seconds received no answer? Did
   that feel like intention or like something missing?
5. What stayed with you: the light, the sound, the silence, or nothing?

## 6. Stop and acceptance criteria

- **Continue** only if T0 gives detection and T1–T3 pass without unwanted
  channels, flicker or discomfort. T4 (48k) and T5 (DeepVision) only afterwards.
- **Stop and report** on: no detection, visible flicker outside the intended
  colour transition, discomfort/headache, or an unexplained flash.
- **Never** raise brightness to mask a detection problem. First signal
  integrity (the offline check), then hardware.
- **No conclusions about therapy, relaxation or health.** Only: did detection
  work, what did the light look like, how did the gesture feel.

## 7. Technical check after the session

- M11: 120 s of uninterrupted SpectraStr detection on the Kasina display
  (repeat with dim/mid/full and note any dropouts).
- M12 (optional, photodiode + audio interface): no periodic luminance
  modulation at 3–30 Hz with ≥ 10% depth; brightness monotonic with the ladder.
  Without a photodiode: visual only, explicitly noted as "not objective".
- M13: stop latency — measure how quickly the carriers/LEDs are off after the
  **human** stop action (stop playback + Kasina brightness low). There is no
  software panic to measure; state that explicitly.
- Compare the observations with `out/verify-report.md` (offline figures) and
  with the corruption matrix (what survives playback/transport).

## 8. If the hardware recognises nothing

That is a **signal-integrity or detection problem**, not an artistic failure.
Check in this order:

1. Is the correct WAV playing (not a 44.1 file at 48 kHz or vice versa)?
2. Is the reference really continuously present (the master has a lead-in of
   ≥ 1 s)?
3. Is the playback chain clean: no EQ, normalisation, spatial audio,
   loudness processing, Bluetooth or lossy compression?
4. Has the WAV not been through a DAW/limiter after the render?
5. Repeat the offline check (`./run.sh`) and the corruption matrix.

Only then try again, with the same conservative brightness.
