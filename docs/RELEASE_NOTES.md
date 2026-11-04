# NAGLOED 1.0.1 — release notes

Released 4 November 2026 · UnlikeNoise Labs · macOS 12+ · AU / VST3 / Standalone

> *nagloed* (Dutch, n.) — the light that stays in the room after the source is gone.

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
| AU | `NAGLOED.component` | Type `aufx`, subtype `Nagl`, manufacturer `UnlN`. Validated with `auval`. |
| VST3 | `NAGLOED.vst3` | Category Fx. |
| Standalone | `NAGLOED.app` | Live processor for an audio input; configure the device in the app's audio settings. |

Minimum macOS version: **12.0**. Built with JUCE 9.0.5. Mono and stereo plug-in
configurations are supported; the plug-in is an effect, not a virtual instrument.
No MIDI or MPE input is accepted in this release.

## Validation

- Unit and smoke tests: PASS.
- Apple AU validation: PASS — `auval -v aufx Nagl UnlN`.
- pluginval strictness 10: PASS on both VST3 and AU, including state
  save/restore (verified across several random seeds; a state-recall bug that
  only appeared under specific seeds was found by this test and fixed before
  release).
- Offline render through the real processor: PASS — `nagl_render` produces
  `Demo/Nagloed-Demo.wav`.
- GitHub Actions CI (macOS): green on the release branch (run `41698856335`).

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

Designed and directed by Leon van Bokhorst / UnlikeNoise, Helmond, NL. Made by
UnlikeNoise Labs with JUCE 9. Scene credits: Modulab Eindhoven, SONICrider and the
EAR label. NAGLOED 1.0 was developed in a small, human-directed, AI-assisted
process; see the transparency note in [README.md](../README.md).

## Documentation

- [Manual](MANUAL.md)
- [Brand and design language](BRAND.md)
- [Architecture](ARCHITECTURE.md)
- [README](../README.md)
