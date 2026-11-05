# Eerste Gloed — SpectraStrobe proof-of-concept

> **EXPERIMENT — NOT PART OF NAGLOED v1.0.4.**
> This is a standalone, offline experiment in `experiments/spectrastrobe-poc/`.
> It is **not** integrated into the plug-in, changes **no** `src/`, CMake,
> presets, README or build, and touches no DSP contract. It is safe to remove:
> deleting this directory is enough. No medical or wellness claims.

## Scope — what this is and is not

- **Is:** an **encoder + signal-integrity proof** for SpectraStrobe light in an
  audio WAV: four carriers, colour coupling, panning, band-limiting, headroom,
  a brightness ladder, a safety clamp and an offline measurement report.
- **Is not:** an event engine, a light scheduler, memory/recall or rate/feedback
  budgets. That is **phase 2** and the **A1 signal half** of the experiment.
  `score.json` is a declarative score that the renderer executes; there is no
  realtime system.
- **Is not:** hardware. Whether a Kasina recognises this and whether the
  Ganzfeld shows the intended colours is **not measured here** (M11–M13). That
  requires `HARDWARE-PROTOCOL.md`.
- **Is not:** a tested stop ramp. The offline WAV has **no encoder panic and no
  50 ms software ramp**; those belong to a later realtime encoder
  (`docs/v2/SPECTRASTROBE.md` §6, phase B). The primary stop here is human.

## Artistic premise

Issue #14 describes *Geborgen Geluid*: the sound of the room returning
transformed, and light and sound that are **related** without one illustrating
the other. `docs/v2/EXPERIMENT-01.md` translates that into four moments with
much space in between; the score follows that structure:

| Time | Event |
|---|---|
| 0–8 s | Room, almost nothing: only the reference carrier and one distant fragment |
| 8–14 s | Amber-coloured light grain starts left, drifts right, becomes slightly brighter |
| 14.0 s | Abrupt, isolated extinction in **20 ms** (R4) — no sound |
| 14.18 s | Answer: low impact, **180 ms later**, different decay (2.5 s), slightly right |
| 14–28 s | Diffuse amber-red field keeps breathing and fades; the light outlives the sound |
| 38 s | Weak light grain that fades by itself — **deliberately unanswered** |
| 46.0 s | Dimmer, slower, reversed echo of the first light |
| 46.4 s | Reversed and lowered fragment (memory) |
| 52–72 s | Fade-out: silence as the ending |

The 180 ms delay is an artistic choice, not a latency bug. Light and sound are
family, not a copy. The score is data (`score.json`); the renderer executes it
with a fixed seed. Envelopes follow R4/R5 from `docs/v2/SAFETY-PRIVACY.md`:
**attack/release ≥ 20 ms hard**, L1 has one isolated 20 ms extinction, L2 starts
with a 50 ms fade-in; no step from black to full.

## Safety

- **Conservative light.** Reference −30 dBFS peak amplitude. Colour channels
  are hard-clamped to **max 25% per carrier** of the reference peak (safety
  clamp in `render.py`, even if the score asks for more). The score uses 20%;
  the hardware ladder starts at **6.25%** (dim), then 12.5% (mid), then 20%.
- **Sum per ear.** 25% applies **per carrier** (R3). The sum of red+green+blue
  in one ear can be higher; it is reported separately (in this score about
  29.5% at the L1 peak). That is information, not an extra licence.
- **No intended luminance modulation in the colour channels.** Colour
  transitions are slow; there is one isolated 20 ms extinction. The **20 Hz
  reference panning is prescribed by the format** and remains an open
  assumption: that it does not leak as visible flicker into the colour channels
  is measurement target M5/M6 and a stop criterion, not a proven fact.
- **Immediate stop.** Stop playback, set Kasina brightness to minimum, glasses
  off or **cover one eye** (R11). One person operates, one person watches. No
  other flicker sources in the room.
- **Photosensitivity and consent.** No person with (suspected)
  photosensitivity in the room; children only with supervision and consent.
  Read `docs/v2/SAFETY-PRIVACY.md` before the session.
- **Hearing.** The carriers (18.2–19.7 kHz) are **not guaranteed inaudible**:
  that depends on age, hearing, transducers and playback level (open assumption
  **U15** in `docs/v2/TECHNICAL-UNCERTAINTIES.md`). Do a short comfort listening
  test at the intended level before the session; with tinnitus, hyperacusis or
  doubt: lower the level or do not take part.
- **Headroom.** Final peak well below −6 dBFS; no clipping, no dither.
- **No claims.** This is an artistic experiment, not therapy, not entrainment
  and not a medical device.

## Running

Requires: Python 3 (stdlib). `ffmpeg` is optional and is used only for the
advisory corruption matrix.

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

Or separately:

```bash
python3 render.py --score score.json --out-dir out
python3 verify.py  --score score.json --out-dir out
# optional: one exposure with a custom colour scale
python3 render.py --score score.json --out-dir out --colour-scale 0.3125   # 6.25% max
```

`run.sh` exits 0 when all core checks pass and writes to `out/`:

| File | Content |
|---|---|
| `spectrastrobe-48k-24bit.wav` | full mix, 48 kHz/24-bit — technical reference, **not verified on the Kasina** |
| `spectrastrobe-44k1-16bit.wav` | full mix, 44.1 kHz/16-bit — **signal-safe/recommended playback version** (no health claim) |
| `spectrastrobe-mid-44k1-16bit.wav` | brightness ladder: 12.5% max per carrier |
| `spectrastrobe-dim-44k1-16bit.wav` | brightness ladder: 6.25% max per carrier — **first hardware exposure** |
| `program-only-48k-24bit.wav`, `program-only-44k1-16bit.wav` | A/B without carriers |
| `events.csv` | events, levels, panning, relations |
| `timeline.svg` | light/audio/carrier timeline, derived from the score |
| `verify-report.json` / `.md` | measurement report with PASS/FAIL/INFO (M1–M10, clamp, ladder, M14) |
| `render-manifest.json` | version, sha256 of score and scripts, clamp info, exposures, output hashes |

`out/` is listed in `.gitignore` and is not committed.

## SpectraStrobe parameters (conservative)

| Parameter | Value |
|---|---|
| Reference | 18.200 Hz, **−30 dBFS peak amplitude** (binding M2), continuously present |
| Reference panning | sine, 20 Hz, depth 100%, offset 0%, per sample |
| Colours | red 18.700 Hz, green 19.200 Hz, blue 19.700 Hz |
| Colour coupling | direct channel amplitude, linear 0 dB centre panning; **clamp 25% per carrier** |
| Reference pan law | equal power (constant carrier energy) |
| Programme | synthesised, band-limited with an 8th-order Butterworth at 15.5 kHz |
| Order | process the programme completely → add carriers → nothing else |
| Peak | sample peak and true peak ≤ −6 dBFS/dBTP; no clipping, no dither |
| Exposures | dim 6.25% / mid 12.5% / full 20% (scale 0.3125 / 0.625 / 1.0) |

The 44.1 kHz variant is a **separate native render** of the same score, not a
sample-rate conversion of the master. SRC can damage the carriers close to
Nyquist; the corruption matrix measures that effect explicitly.

## What the test does and does not prove

**Does (offline, on this machine, CPython stdlib):**

- The four carriers sit at the correct frequency (±1 Hz), the reference at
  −30 dBFS peak amplitude, and the colour channels follow the score; the
  maximum colour coupling over the whole render is ≈20.0% (per carrier), with
  the sum per ear reported separately.
- The **safety clamp works adversarially**: a score with rgb = 0.90 is
  hard-clamped to 25% and the render logs it.
- The **brightness ladder** really ascends: dim ≈6.25%, mid ≈12.5%, full
  ≈20%, measured via probes; the reference stays equal in all variants.
- The 20 Hz panning is present per sample with the required depth; the
  reference is far below the threshold in the "off" phase.
- **Inter-carrier isolation** is measured with a dedicated probe (one colour at
  a time): inactive colour bands are far below −40 dB.
- There is no significant crosstalk, the programme does not leak into the
  control band, and 17–20 kHz is far below −60 dBFS.
- There is ample headroom, no clipping, and the render is deterministic
  (two renders byte-identical, manifest with script sha256s).
- The full M4/M7/M8 set also ran on the **44.1 kHz/16-bit** variant.
- The corruption matrix shows what resampling (neat/naive), lowpass, MP3
  (320/128 joint), AAC, gain, mono sum, L/R swap and rate mismatch do to the
  carriers — including the colour ratios.

**Does not:**

- Whether a real Kasina recognises the signal and whether the DeepVision
  Ganzfeld shows the intended colours/brightness (M11–M13). **Hardware is not
  available here.**
- Whether the Kasina can play the 48 kHz/24-bit master; that is not verified.
  Use the 44.1 kHz/16-bit variant for playback (16-bit PCM, no metadata, no
  dither) — signal-safe, no health claim.
- **Broadband v1 programme band-limiting**: the −60 dBFS requirement is
  demonstrated for the sparse, low-frequency synthesised material. A broadband
  test (white noise through the same 8th-order LPF) is **INFO** in the report:
  the filter alone reaches ≈−19 dB at 17 kHz, so the requirement is **not**
  demonstrated for broadband v1 material. No false PASS.
- Host/DAW behaviour, playback chains, Bluetooth, OS audio EQ and streaming
  outside the matrix.
- An event engine, memory or budget behaviour; and whether the delay feels like
  kinship rather than an error (perceptual question).
- A tested software stop/50 ms ramp; the primary stop is human.

## Sources

- `gh issue view 14 --repo leonvanbokhorst/nagloed` — artistic direction.
- `docs/v2/EXPERIMENT-01.md` — the composition *Eerste Gloed*.
- `docs/v2/SPECTRASTROBE.md` — signal model, measurement plan M1–M14, hard
  encoder requirements §6.
- `docs/v2/SAFETY-PRIVACY.md` — R1–R12 (envelope, ceiling and stop rules).
- `docs/v2/TECHNICAL-UNCERTAINTIES.md` — U15 (carrier audibility).
- Lumasonic SpectraStrobe specification and production/mixing notes:
  <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md>
  and <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md>.
- SpiraStrober (MIT, independent implementation):
  <https://github.com/Syluminose/SpiraStrober>.

Licence: GPL-3.0-or-later, same as NAGLOED.
