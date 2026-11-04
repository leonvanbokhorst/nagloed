# NAGLOED 1.0.4 — release notes

Released 4 November 2026 · Master Lonn & Little Wan · macOS 12+ · AU / VST3 / Standalone

> *nagloed* (Dutch, n.) — the light that stays in the room after the source is gone.

## 1.0.4 — 4 November 2026

**Engineering follow-up release: no sound or behaviour changes.**

- `nagl_preview` now renders the documentation hero **deterministically** (fixed
  frame count, fixed dust RNG seed, wall-clock timer stopped, visualiser phases
  pinned), so `docs/ui-preview.png` is byte-reproducible for a given build and
  platform. CI renders it twice on its own runner and fails if the hashes differ.
- CI now runs the regression gates that were local-only: a headless host-mount
  lifecycle test (`--headless-host`, the CI-safe form of `--embedded`; it asserts
  the animation timer survives hide/show and drives fixed visual frames itself, so
  it does not depend on runner timer delivery), the 8-second `--soak` phosphor
  test (also frame-stepped), the deterministic-hero render comparison, and
  **pluginval strictness 10** on VST3 and AU.
- Docs/VALIDATION describe the deterministic snapshot and the expanded CI gate;
  version stamps refreshed.

Verified: 167 checks / 0 failures, `EMBEDDED PASS`, `SOAK PASS`, hero
byte-identical across runs, `auval -v aufx Nagl MLLW` PASS, pluginval SUCCESS on
VST3 and AU, demo unchanged (sha256 `de987eed…`).

## 1.0.3 — 4 November 2026

**Identity: NAGLOED is now a Master Lonn & Little Wan release.**

- **Vendor** changed from "UnlikeNoise Labs" to **Master Lonn & Little Wan**
  (Leon van Bokhorst / Master Lonn with Little Wan / Little Padawan). It shows in
  hosts, `Info.plist`, docs and the top bar ("MASTER LONN & LITTLE WAN ·
  EINDHOVEN NL").
- **AU/VST3 identity changed**: manufacturer code `MLLW` (was `UnlN`), subtype
  `Nagl`, bundle id `com.masterlonn.littlewan.nagloed`. The VST3 class UID derives
  from the same code, so **old instances will not resolve**: sessions saved with
  1.0.0–1.0.2 need NAGLOED to be re-instantiated once. Presets and the state format
  are unchanged.
- **UnlikeNoise is retired.** It was Leon's solo alias for years; NAGLOED belongs
  to the Master Lonn & Little Wan practice. The alias appears only as history.
- **EAR**: the analogue stage keeps its name (the ear that listens). It is **not**
  affiliated with, endorsed by, or connected to Jurgen Winkel's EAR label. Brand
  book and manual now state this explicitly.
- **Modulab Eindhoven**: Leon was part of its early team but has not been involved
  for over eight years. It is described as scene and friendship, not a current
  affiliation — same for the rest of the Eindhoven/Helmond underground.
- Releases 1.0.0–1.0.2 shipped under the old vendor name and manufacturer code and
  are kept here as the historical record.

Everything from 1.0.2 (grain-field rendering, LFO dot, host-mount and phosphor
regression harnesses) is included. Verified: 167 checks / 0 failures,
`EMBEDDED PASS`, `SOAK PASS`, `auval -v aufx Nagl MLLW` PASS, pluginval strictness
10 SUCCESS on VST3 and AU, demo unchanged (sha256 `de987eed…`).

## 1.0.2 — 4 November 2026

**Fix: grain field rendering (black/white stutter, white ghosts, clipped embers).**

Two rendering bugs in the phosphor field shipped in 1.0.0/1.0.1:

- The phosphor decay used a bit-trick that also subtracted from the **alpha**
  channel. The phosphor screen is an opaque premultiplied ARGB image, so decaying
  its alpha made the dark base drift and the field itself translucent: a washed
  grey/white rectangle that flickered as splats reset alpha.
- Independently, per-frame additive splats drove the bright channels to 255 within
  a few frames, turning stationary grains into hard white discs.

Fixes: RGB-only decay with the alpha byte pinned opaque; a tight per-splat colour
ceiling (a little brighter than the ember colour) so repeated splats stay warm
instead of clipping; grain/dust energies re-tuned so the cloud keeps its density;
the pitch colour ramp capped at a warm gold rather than near-white pale ember.

Also: the LFO phase dot now traverses the **full** scope width and samples the
displayed two-cycle waveform (it previously turned around at the middle).

Regression coverage: `nagl_preview --soak` (8 s run; asserts opaque phosphor, no
white wash, sane luminance, still animating) and the 1.0.1 `--embedded` host-mount
test; both run in `./scripts/validate.sh`. `SOAK FAIL`/`alphaOk=0` on the old decay,
`SOAK PASS` now.

Verified: 167 checks / 0 failures, `EMBEDDED PASS`, `SOAK PASS`, `auval` PASS,
pluginval strictness 10 SUCCESS on VST3 and AU, demo unchanged
(sha256 `de987eed…`, peak −2.13 dBFS).

## 1.0.1 — 4 November 2026

**Fix: visualisers could freeze in hosts.**

In 1.0.0, mounting the editor in Ableton Live or REAPER could leave the 60 Hz
animation clock stopped: the editor stopped its timer in `visibilityChanged()`
when a host reparented the view, and some hosts never deliver a matching
visibility event afterwards. The knobs kept working (they repaint directly) while
the grain field, filter curve, LFO scope, VU meters and buffer/scan readouts sat
frozen — until a window switch forced a full repaint.

- The animation timer now stays alive for the editor's lifetime; the callback is a
  cheap no-op while hidden, and is restarted defensively from
  `parentHierarchyChanged()` and `visibilityChanged()`.
- Every parameter change — mouse or host automation — now schedules an immediate
  async visual refresh through the APVTS listener, on top of the timer.
- New regression harness: `nagl_preview --embedded` mounts the editor as a child
  of a peer (the way hosts do) and fails if the filter curve, LFO scope or grain
  field do not repaint. `./scripts/validate.sh` runs it.

Verified: 167 checks / 0 failures, `auval` PASS, pluginval strictness 10 SUCCESS
on VST3 and AU, embedded refresh PASS, demo unchanged
(sha256 `de987eed…`, peak −2.13 dBFS).

## 1.0.0 — 3 November 2026

NAGLOED 1.0.0 is the first release: a granular effect and instrument built around
a twelve-second capture buffer, a 256-grain engine, a space, and an analog output
stage. It is the studio version of a one-take habit — record the moment, freeze
it, and play what remains.

## What's included

- **Sintel**, the grain engine: capture buffer, grain scheduling, density, grain
  length, spray, scan, pitch, spread, reverse, jitter, shape, width, feedback,
  focus, freeze, tuning modes and source modes.
- **Ruimte**, the space: Space, Size, Damp, Mod.
- **EAR**, the analog section: Drive, Bias, Tone, Cutoff, Reso, Drift, Wow, Tape,
  LoFi, Hiss, In, Out, Mix.
- Five gesture macros (Glow, Drift, Grain, Space, Shimmer), one LFO with six
  shapes and six destinations plus Off, and Auto-motion.
- 45 host-automatable parameters in total, with permanent parameter IDs.
- Freeze workflow: the buffer stops recording, the grains keep playing.
- Resizable editor with a live grain cloud, default 1180 × 680.
- AU, VST3 and Standalone builds for macOS 12 or later; mono and stereo plug-in
  configurations.
- Factory preset library: 24 presets across eight categories plus Init, exposed
  as plug-in programs. See [the preset guide](PRESET_GUIDE.md).
- Full session state recall, including the current program.
- Documentation: brand and design language, manual, architecture and these
  release notes.
- An 84-second demo performance rendered through the full engine
  (`Demo/Nagloed-Demo.wav`): 48 kHz / 24-bit stereo, peak −2.1 dBFS, no clipping,
  byte-reproducible from `nagl_render`. A spectrogram of the render is in
  `docs/demo-spectrogram.png`.
- Validation for 1.0: 167 automated checks, `auval` PASS, pluginval strictness 10
  PASS on VST3 and AU, 0 audio-thread allocations, ~2% of one core at 48 kHz /
  128 samples with a 64-grain cloud. Full evidence in `docs/VALIDATION.md`.

## Presets

- **Init** (program 0) is the documented default state: a clean starting point,
  not silence.
- The 24 factory presets are grouped into eight categories and embedded into the
  binary at build time from `presets/*.json`; they appear in the host's program
  list. The preset guide describes every patch: [PRESET_GUIDE.md](PRESET_GUIDE.md).
- Loading a preset replaces all parameter values. Saved sessions and host presets
  recall the current program and parameter state.

## Formats

| Format | Artefact | Notes |
|---|---|---|
| AU | `NAGLOED.component` | Type `aufx`, subtype `Nagl`, manufacturer `MLLW`. Validated with `auval`. |
| VST3 | `NAGLOED.vst3` | Category Fx. |
| Standalone | `NAGLOED.app` | Live processor for an audio input; configure the device in the app's audio settings. |

Minimum macOS version: **12.0**. Built with JUCE 9.0.5. Mono and stereo plug-in
configurations are supported; the plug-in is an effect, not a virtual instrument.
No MIDI or MPE input is accepted in this release.

## Validation

- Unit and smoke tests: PASS.
- Apple AU validation: PASS — `auval -v aufx Nagl MLLW`.
- pluginval strictness 10: PASS on both VST3 and AU, including state
  save/restore (verified across several random seeds; a state-recall bug that
  only appeared under specific seeds was found by this test and fixed before
  release).
- Offline render through the real processor: PASS — `nagl_render` produces
  `Demo/Nagloed-Demo.wav`.
- GitHub Actions CI (macOS): builds the AU/VST3/Standalone bundles, runs the
  suite and validates the AU on every `v*` tag (and on demand).

Rebuild and re-validate locally with `./scripts/validate.sh`.

## Known limitations and intentional character

- **No MIDI or MPE in 1.0.** NAGLOED is an audio effect; expression comes from the
  five macros, the LFO, Auto drift and the grain-field gestures. The Pressure,
  Slide and Expression parameters are automatable and preset-addressable, but
  they cannot be played from a controller in this release.
- **macOS 12+ only.** AU, VST3 and Standalone; no Windows, Linux or AAX build.
- **Live capture only.** There is no audio-file loading or sampling: the material
  is whatever you play into the twelve-second capture buffer, or the internal
  Noise/Drone sources. The grain pool is capped at 256 voices.
- **Mono/stereo only.** No surround or multichannel configurations.
- **Factory presets only.** There is no user preset file browser in 1.0; save your
  own variations with the host's preset, rack or track-preset system. The
  plug-in state itself is saved with the session.
- **CPU scales with the grain field.** Density x grain length is the main cost;
  freezing does not reduce it. The engine ceiling is 256 grains.
- **Tape transport affects the dry path.** At very low `Mix` with high `Wow`, the
  dry signal can waver slightly. This is deliberate analogue behaviour.
- **Feedback is overlap-compensated.** Sparse clouds use the full feedback amount;
  dense clouds get a scaled amount so the loop always decays once the source
  stops. High feedback regenerates for seconds, never forever.
- **PDC is a few samples short of the full wet group delay** (~0.05 ms at
  48 kHz; inaudible). Documented in the validation report.
- **Ad-hoc signed builds.** Release zips are signed ad hoc, not notarised. On
  first use macOS may quarantine them: right-click -> Open once, or run
  `xattr -dr com.apple.quarantine <bundle>` before copying to the plug-in
  folder, then rescan in your host. The standalone app may also need approval in
  System Settings -> Privacy & Security.

## Compatibility

- macOS 12 or later, Intel or Apple silicon (built from source for the host
  machine).
- Ableton Live and REAPER are the reference hosts for 1.0.0. NAGLOED also runs in
  the standalone app.
- Host notes, automation guidance and troubleshooting are in
  [the manual](MANUAL.md).

## Upgrading and compatibility policy

1.0.0 is the first release, so there is no migration path yet. Parameter IDs are
permanent API: automation, presets and saved sessions address parameters by ID,
and future releases will not rename them.

## Roadmap

Intent, not a delivery promise:

- MIDI/MPE expression, using the existing Pressure, Slide and Expression
  parameters.
- User preset management (save, load, organise) alongside the factory library.
- Further grain-engine and Ruimte refinements based on release feedback.
- More tuning systems and scale options.
- Windows and Linux builds once the platform work is scheduled.

## Credits

Designed and directed by Master Lonn (Leon van Bokhorst) with Little Wan, Helmond/
Eindhoven, NL. Made by Master Lonn & Little Wan with JUCE 9. UnlikeNoise is a
retired solo alias. Scene gratitude: Modulab Eindhoven, SONICrider, and Jurgen
Winkel's EAR label — inspiration and friendship, no affiliation. NAGLOED 1.0 was developed in a small, human-directed, AI-assisted
process; see the transparency note in [README.md](../README.md).

## Documentation

- [Manual](MANUAL.md)
- [Brand and design language](BRAND.md)
- [Architecture](ARCHITECTURE.md)
- [README](../README.md)
