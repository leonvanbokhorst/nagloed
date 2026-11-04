# NAGLOED 1.0 — validation report

Everything below was produced on the release candidate on the build machine
described here. The point is not a badge: it is that the numbers can be
reproduced with the commands given.

## Environment

| | |
|---|---|
| Machine | MacBook (Apple silicon, arm64), macOS 26.6.2 |
| Toolchain | Apple Command Line Tools, clang 21.0.0, CMake 4.0.0 |
| JUCE | 9.0.5 |
| pluginval | 1.0.4 |
| Build type | Release (`-DCMAKE_BUILD_TYPE=Release`), UNIX Makefiles |

JUCE is the only third-party dependency. `./scripts/fetch-juce.sh` downloads the
pinned version into `.deps/JUCE`; the build never fetches at compile time.

## Artefacts

```
build/NAGLOED_artefacts/Release/AU/NAGLOED.component
build/NAGLOED_artefacts/Release/VST3/NAGLOED.vst3
build/NAGLOED_artefacts/Release/Standalone/NAGLOED.app
```

AU identity: type `aufx`, subtype `Nagl`, manufacturer `UnlN`. The build copies
AU and VST3 into `~/Library/Audio/Plug-Ins/{Components,VST3}`.

## Automated checks

### Unit + DSP suite

```
./build/nagl_tests_artefacts/Release/nagl_tests
```

- 167 checks, 0 failures, ~3.5 s.
- Covers: parameter layout and display strings; preset loading; state
  round-trip; silence in = silence out; finite output under 32 hostile random
  parameter configurations; freeze semantics; grain density accuracy; tuning
  quantisation at ±48 semitones; dry/wet latency alignment; focus mapping;
  feedback decay across density/window shapes; state recall (save, change,
  restore, verify — the pluginval regression); mono fold; block-size invariance
  (128 / 512 / 4096 samples, including 4096-sample blocks on a 128-sample
  prepare); visual-state ranges; 30-second render; CPU timing.
- Audio-thread allocation watch (global `operator new`): **0 allocations** in
  `process()`, `setParams()` and `getVisual()`.
- Measured CPU: **≈ 1.9–2.2 % of one core** at 48 kHz / 128 samples / stereo
  with a 64-grain cloud plus reverb and analog stage; ~5.6 % at 200 grains.
- Reported latency: **51 samples** at 48 kHz (oversampler + mean tape delay);
  the dry path is aligned to it, and the host compensates via `setLatencySamples`.

### Apple AU validation

```
auval -v aufx Nagl UnlN
```

```
* * PASS
AU VALIDATION SUCCEEDED.
```

All Apple test groups pass: open/initialize, render tests across sample rates
(11.025–192 kHz) and frame sizes (64–4096), channel configurations, connection
semantics, bad-max-frames rejection, parameter setting/scheduling/ramping, MIDI.

### pluginval — strictness 10

```
pluginval --strictness-level 10 --validate build/NAGLOED_artefacts/Release/VST3/NAGLOED.vst3
pluginval --strictness-level 10 --validate build/NAGLOED_artefacts/Release/AU/NAGLOED.component
```

```
SUCCESS
```

Both formats pass at maximum strictness: scanning, bus layout changes, parameter
fuzzing, state save/restore, editor open/close, audio processing. VST3 was run
with several random seeds (`0x3eafa08`, `0x23024d1`, `0x1234567`, `0xabcdef`,
`0x99`) because pluginval's state test is seed-dependent; AU was re-checked with
`0x3eafa08` and `0x99`. The suite also
asserts parameter recall explicitly, because pluginval's state test found a real
bug during release review: `getStateInformation` used `apvts.state.createCopy()`
without flushing the APVTS parameter timer, so a save immediately after a change
could serialise stale values. It now uses `apvts.copyState()`, and the regression
is covered by `nagl_tests` (167 checks now).

### Offline render through the real processor

```
./build/nagl_render_artefacts/Release/nagl_render
```

| Property | Value |
|---|---|
| File | `Demo/Nagloed-Demo.wav` |
| Length | 84.000 s |
| Format | 48 kHz, 24-bit, stereo |
| Peak | 0.783 (−2.13 dBFS), true peak −2.0 dBTP |
| Full-scale samples | 0; all samples finite |
| Section RMS (sox left-channel windows: 0–4, 4–15, 15–30, 30–36, 36–45, 45–57, 57–69, 69–80, 80–84 s) | intro −23.3 / grains −18.3 / freeze −17.4 / dust −17.5 / rhythm −18.2 / drone −8.4 / space −7.8 / return −12.5 / fade −27.6 dBFS |
| SHA-256 | `de987eed2bd4fde4f0a367ab6f0a249133342b0a42488bdf74db17da69f721c2` |
| Determinism | byte-identical across renders |

`nagl_render` feeds a deterministic, stateless performance score
(`tools/DemoScore.cpp`) through `NagloedProcessor` — the same code path the
plug-in uses in a host.

### Offscreen editor snapshot

```
./build/nagl_preview_artefacts/Release/nagl_preview
```

Renders the real `AudioProcessorEditor` offscreen to `docs/ui-preview.png`
(2360 × 1360, i.e. 1180 × 680 at 2×) after loading a factory preset and feeding
the engine audio, so the snapshot shows the live grain field.

### Grain-field phosphor soak

```
./build/nagl_preview_artefacts/Release/nagl_preview --soak
```

Runs the editor for eight seconds **with audio flowing** (so grains actually move)
and asserts the phosphor buffer stayed opaque (`alphaOk`, checked before alpha is
pinned), no near-white wash, and that clipped-ember pixels stay under 0.02% of the
field while the field keeps animating. This is the regression test for the grain-field
corruption: the decay's bit-trick subtracted from the alpha byte as well as the
colour, so the opaque phosphor screen became translucent and the field washed out;
independently, additive splats clipped bright channels to white discs. It prints
`SOAK PASS`; the loose cap prints `SOAK FAIL`.

### Editor refresh in a host-style mount

```
./build/nagl_preview_artefacts/Release/nagl_preview --embedded
```

Mounts the editor as a child of a peer component (the way hosts mount plugin
views), changes cutoff and LFO shape, and fails unless the EAR curve, LFO scope
and grain field all repaint. This is the regression test for the 1.0.0 frozen
visualiser bug; a passing run prints `EMBEDDED PASS`.

One command reproduces everything above, provided pluginval is present in
`.deps/pluginval` (the pluginval stage is optional and skipped otherwise):

```
./scripts/validate.sh
```

GitHub Actions CI (`.github/workflows/ci.yml`) builds the AU/VST3/Standalone
targets, runs the suite and validates the AU; run `41804122764` on the release
branch is green.

## Manual checks

- Plug-in state save/load round-trips through the host (asserted in the suite:
  parameter values changed, saved, changed again, restored and checked; also
  exercised by pluginval's state-restore test).
- Editor opens and closes cleanly at 960 × 560, 1180 × 680 and 2400 × 1500 at
  1×, 2× and 3× scale; no clipped panels or text.
- Resizing while audio runs does not interrupt processing or leak timers.
- No host-specific crashes during the pluginval and auval runs.

## Known non-blocking observations

- **PDC is 2.6–3.4 samples short of the full wet group delay** (the residual is
  the ladder filter's group delay, not the tape delay). Below 0.1 ms; inaudible.
  Measured during review; the suite asserts dry/wet alignment at the reported
  latency (50/50 gain −0.20 dB) rather than the residual.
- **Dry path follows tape wow.** At very low `Mix` with high `Wow` the dry
  signal can waver slightly — intentional analog transport behaviour.
- **Dense clouds get scaled feedback** by design (worst-case overlap
  compensation) so the loop always decays once the source stops; sparse clouds
  use the full feedback knob.
- **Focused-grain position at maximum Focus with default Spray** averages 0.94
  rather than 1.0; the spray window is clamped at the newest edge on purpose.
  The suite asserts 0.004 / 0.500 / 0.996 at Focus 0 / 0.5 / 1 without spray.

## Not covered by automation

- Loading inside a specific DAW (Ableton Live 12, REAPER) was left as a manual
  smoke step: the binaries are installed to the standard user plug-in folders,
  and both formats are already validated by the host-agnostic validators above.
- Subjective sound quality. The numbers prove stability, range and CPU, not
  taste; the demo and the preset library are the listening evidence.
