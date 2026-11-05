# NAGLOED architecture

```
              +-----------------------------+
  audio in -> |  Sintel  granular engine    | -> Ruimte FDN -> EAR analog -> out
              |  capture buffer + grains    |                    chain
              +--------------+--------------+
                             | VisualState (lock-free, 1/block)
                             v
              +-----------------------------+
              |  Editor (src/ui)            |  grain cloud, VU, knobs
              +-----------------------------+
```

## Layers

| Layer | Path | Owner | Rules |
|---|---|---|---|
| Contracts | `src/core/` | Lead | Frozen public types: parameter ids/ranges, `EngineParams`, `VisualState`, `PresetAccess`. Change only by agreement. |
| DSP | `src/dsp/` | DSP workstream | `Engine` (pimpl) + `EngineDSP` implementation. Audio-thread safe, allocation-free. |
| UI | `src/ui/` | UI workstream | Pure JUCE components. Talks to audio only through `VisualSource` + `APVTS`. |
| Plugin glue | `src/plugin/` | Lead | `NagloedProcessor`: APVTS, bus layout, state, presets, program list. |
| Presets | `presets/*.json` | Preset workstream | Embedded at configure/build time by `scripts/gen_presets.py`. |
| Tools | `tools/` | Lead + preset workstream | `nagl-tests`, `nagl-render`, `nagl-preview` compile the *same* sources as the plugin. |

## Contract highlights

* `Engine::setParams()` / `process()` / `getVisual()`: audio thread only, no
  allocation, locking, throwing or file/network access.
* `Processor::copyVisual()` publishes `VisualState` through
  `SnapshotChannel<VisualState>` (single writer / single reader).
* GUI never includes `src/dsp/`; DSP never includes `src/ui/`.
* Parameter ids in `src/core/ParamIDs.h` are permanent API.
* `VisualState::grains` is a *display* copy: the engine may emit a subset.
* Gloop gestures are recorded on the message thread and handed to the audio
  thread through a preallocated SPSC queue (`Engine::publishGloopGesture`).
  The audio thread never reads the APVTS or allocates; the processor owns the
  authoritative takes for session persistence. See `docs/GLOOP.md`.

## Build & tools

```bash
./scripts/fetch-juce.sh          # once, into .deps/JUCE
./scripts/build.sh               # everything
./scripts/build.sh NAGLOED       # just the plugin
./scripts/build.sh nagl_tests    # just the tests
./scripts/validate.sh            # tests + AU validation
```

Artifacts land in `build/NAGLOED_artefacts/Release/` and are copied to
`~/Library/Audio/Plug-Ins/{Components,VST3}` by the build.
