# NAGLOED 1.0.5 — validation report

Everything below was produced on the **NAGLOED 1.0.5 release candidate
(5 November 2026)** on the build machine described here. The point is not a
badge: it is that the numbers can be reproduced with the commands given.

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

AU identity: type `aufx`, subtype `Nagl`, manufacturer `MLLW`. The build copies
AU and VST3 into `~/Library/Audio/Plug-Ins/{Components,VST3}`.

Version metadata in the 1.0.5 bundles: `CFBundleShortVersionString` and
`CFBundleVersion` are `1.0.5` for AU and VST3 (`PROJECT_VERSION`, rendered from
`CMakeLists.txt` and `src/core/Version.h`). AU/VST3 plug-in identity and the state
format are unchanged from 1.0.4, so existing sessions load normally.

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
auval -v aufx Nagl MLLW
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
the engine audio, so the snapshot shows the live grain field. The wall-clock timer
is stopped and the frame is advanced deterministically while audio is fed in fixed
chunks (`advanceForPreview(0)` followed by 40 × (0.1 s chord + six 1/60 s
frames)), with the dust generator's fixed seed and pinned visualiser phases. The
PNG is therefore **byte-reproducible for a given build and platform**; CI renders
it twice on the runner and compares hashes.

### Grain-field phosphor soak

```
./build/nagl_preview_artefacts/Release/nagl_preview --soak
```

Runs the editor for eight seconds **with audio flowing** (so grains actually move),
stepping frames deterministically instead of relying on message-loop timer delivery
(audio arrives in 0.1 s chunks, six frames each, to keep grain overlap at the live
soak's density). It asserts the phosphor buffer stayed opaque (`alphaOk`, checked
before alpha is pinned), no near-white wash, and that clipped-ember pixels stay
under 0.02% of the field while the field keeps animating. This is the regression
test for the grain-field corruption: the decay's bit-trick subtracted from the alpha
byte as well as the colour, so the opaque phosphor screen became translucent and
the field washed out; independently, additive splats clipped bright channels to
white discs. It prints `SOAK PASS`; reverting the decay prints `alphaOk=0` /
`SOAK FAIL`, and the loose colour cap prints `clippedRatio=0.00038` / `SOAK FAIL`.

### Grain-field motion QA

```
./build/nagl_preview_artefacts/Release/nagl_preview --motion
```

Deterministic PASS/FAIL checks for the Sintel particle layer (issue #32), using
only the public `GrainCloud` QA hooks and a fixed 60 fps dt. It asserts that
embers actually move while the engine plays (mean speed, nearest-neighbour
displacement, matching continuity), that the **rendered field pixels** change
frame to frame (a render guard against the macOS native-image freeze, independent
of particle state), that the ember population **decays from the dense playing
peak** when digital silence follows on the same pipeline (no state reset), that
dense material has clearly more matched embers than sparse, that two fresh
editor pipelines produce identical frame hashes, that dense-phase tick + paint
cost stays inside one 60 Hz frame, and that particle state stays finite,
in-field and resize-safe. It prints `MOTION PASS`; the baseline renderer prints
`MOTION SKIP` because the hooks did not exist yet.

Measured on the 1.0.5 release build (dense phase, preset 07, 1180 × 680):

```
[PASS] motion      meanSpeed=0.1124 fw/s  medianNN=1.09 px/frame  continuity=1.00
                   meanTrack=0.16 s  matched=37.5
[PASS] render      dense field frames: 60 distinct (floor 54), 59/59 consecutive differ
[PASS] silence     dense peak live=174 afterglow=138 proxy=1.000;
                   after 2.33 s silence: live=1, afterglow=1, matched=0, proxy=0.001
[PASS] density     dense matched=37.0 vs sparse 1.1 = 32.63x
[PASS] cost        tick mean 1.76 ms / p95 1.94 ms, paint 1.70 ms,
                   tick+paint 3.46 ms (caps 8 / 20 / 16.7 ms)
[PASS] determinism runA = runB = baf8b4e3c0e843fb
[PASS] stability   finite, in-field, 900×520 / 1400×800 / 1180×680
MOTION PASS
```

Timing varies by a few hundredths of a millisecond between runs; frame content is
deterministic. The render check exists because the field was frozen on macOS in
1.0.2–1.0.4 (see [SINTEL-MOTION.md](SINTEL-MOTION.md)); reverting the
`SoftwareImageType` fix makes it fail.

### Deterministic before/after frame capture

```
./build/nagl_preview_artefacts/Release/nagl_preview --frames /tmp/issue32-after
```

Runs the fixed 8-second evidence scenario (dense → sparse → near silence →
return) one audio chunk and one visual frame at a time, writes field/UI frames,
2× stills and a per-frame `metrics.csv` (mean luminance, bright-pixel centroid,
frame delta, coverage). The same harness runs on the old renderer, which is how
the issue #32 before/after evidence was produced; see
[issue-32/evidence/EVIDENCE.md](issue-32/evidence/EVIDENCE.md).

### Editor refresh in a host-style mount

```
./build/nagl_preview_artefacts/Release/nagl_preview --embedded
```

Mounts the editor as a child of a peer component (the way hosts mount plugin
views), changes cutoff and LFO shape, and fails unless the EAR curve, LFO scope
and grain field all repaint. This is the regression test for the 1.0.0 frozen
visualiser bug; a passing run prints `EMBEDDED PASS`. It needs a desktop window
server, so it stays a local/manual check.

GitHub's macOS runners have no window server, so CI runs the CI-safe variant:

```
./build/nagl_preview_artefacts/Release/nagl_preview --headless-host
```

It performs the lifecycle a DAW performs on the editor (hide/show while
`isShowing()` stays false because there is no peer), then asserts the animation
timer is still running and drives a fixed number of visual frames to confirm the
cloud, curve and scope repaint. Both checks are independent of message-loop timer
delivery, so they are stable on headless runners. It prints `HEADLESS-HOST PASS`;
restoring the 1.0.0 timer logic makes it print `HEADLESS-HOST FAIL`
(`timerRunning=0`).

One command reproduces everything above, provided pluginval is present in
`.deps/pluginval` (the pluginval stage is optional and skipped otherwise):

```
./scripts/validate.sh
```

GitHub Actions CI (`.github/workflows/ci.yml`) builds the AU/VST3/Standalone
targets, runs the suite, validates the AU, runs the CI-safe `--headless-host`,
`--soak` and `--motion` regressions (the windowed `--embedded` variant needs a desktop window
server and stays local), checks the hero is byte-identical across two renders on
the runner, and runs pluginval strictness 10 on VST3 and AU; it runs on every `v*`
tag (and on demand). A tag run is not cited by number here because it changes with
every release.

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
- **Retina paint cost is estimated, not profiled at the cap.** The phosphor is
  software-backed now (necessary for correct animation on macOS), so each paint
  builds a CGImage from the buffer. Measured paint ~1.5–1.7 ms at 1180 × 680;
  the phosphor is capped at 4.5 MP, which projects to roughly ~2 ms. A profile on
  a large Retina window remains a sensible follow-up, not a 1.0.5 blocker.
- **High-density worst case is bounded, not exercised.** The motion QA dense
  phase reaches 37 matched / 174 live particles. The engine itself bounds
  concurrency (density ≤ 200 g/s, grain size ≤ 500 ms ⇒ ≈100 voices) and the
  particle pool is a hard 360; worst-case matcher cost is ≤ 256 × 360 score
  comparisons, and rendering is bounded by three core + two trail splats per
  particle. No unbounded path was found; a dedicated max-density soak is a
  possible follow-up.

## Not covered by automation

- Loading inside a specific DAW (Ableton Live 12, REAPER) was left as a manual
  smoke step: the binaries are installed to the standard user plug-in folders,
  and both formats are already validated by the host-agnostic validators above.
- Subjective sound quality. The numbers prove stability, range and CPU, not
  taste; the demo and the preset library are the listening evidence.
- Subjective motion feel. The owner accepted the living Sintel grain field for
  issue #32 on 5 November 2026; the automated checks prove motion, render
  integrity, silence, density contrast, determinism and cost — not whether it
  *feels* alive.

## Development branch: Gloop (issue #33)

The Gloop feature branch adds eight independent gesture loopers. Its automated
evidence, run on the branch before hand-over:

- `nagl_tests`: **767 checks, 0 failures** (~4 s), including the existing DSP
  suite, 15 Gloop engine suites and the processor/recorder suites.
- Gloop engine evidence: silence for empty/Off/motionless/amount-0 lanes; amount
  linearity; speed 0.25x/2x ratio 8.00 and continuous mid-play speed changes;
  monotonic 2.5 s fade to Idle; seam wrap max per-frame step 0.0054 on a
  discontinuous take; live-centre mapping around two knob values; bounded
  two-lane summing (0.9 + 0.9 -> 1.24, max step 0.0036); gradual Randomize
  (max per-frame amount step 1.8e-4) with idle lanes untouched; SPSC hand-off
  full/empty/retry; **0 audio-thread allocations** over 512 blocks with fades,
  mode changes and Randomize; **~2.2 % of one core** at 48 kHz / 128 samples /
  stereo with all eight lanes playing a 64-grain cloud (pre-Gloop baseline in
  the same run: 1.93 %).
- Processor evidence: record/finalise, target-Off settling, mode transitions,
  and session state round-trip (including a restore before `prepareToPlay`).
- `nagl_preview --gloop` writes `docs/gloop-preview.png`; `--soak`,
  `--headless-host` and `--motion` remain PASS with unchanged determinism
  (`MOTION-HASH baf8b4e3c0e843fb`).

Not run locally on this branch: the AU/VST3 install step, `auval` and
pluginval. The source, shared library and all tools link and run; the sandbox
cannot write `~/Library/Audio/Plug-Ins`, so the bundle copy target fails after
linking. CI (and the owner's normal build) runs those validators.

Artistic acceptance is Lonn's playing/listening feedback, not a claim from these
checks.
