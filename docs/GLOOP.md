# Gloop — eight asynchronous gesture loopers

Gloop records the *movement* of a parameter knob and lets that gesture circulate
around the knob's live value. It is movement in sound, not faithful automation
playback: there is no shared musical clock, no transport sync, and no automatic
return to recording.

This document records the design of the first playable iteration (issue #33) and
the trade-offs behind it. Artistic acceptance is Lonn's playing/listening
feedback; automated tests only establish the mechanics below.

## What this iteration does

* Eight independent lanes. Each lane targets one parameter and has its own
  amount, speed, phase and lifetime. Lanes deliberately drift against each other.
* `REC` captures the target knob's movement relative to where the knob was when
  recording started. `REC` again ends the take and starts a seamless loop.
* `LOOP` repeats indefinitely. `FADE` withdraws that lane's influence over
  ~2.5 seconds and leaves the lane READY for another REC.
* Per-lane `AMOUNT` (0..1) and `SPEED` (0.25x..2x). Speed changes the playback
  time of the gesture and glides without restarting the phase.
* The target knob is the live centre. Moving it while a loop runs translates the
  orbit; the APVTS value is never overwritten by Gloop.
* The target knob keeps its **orange** live-value arc and pointer. Gloop's
  contribution is a separate **light-blue** arc that follows the current summed
  offset; when several lanes share a knob it is one combined display, not a
  pile of rings. Playback phase lives in the lane's mini-ring in the Gloop
  panel, never on the parameter knob. A separate Gloop panel shows all eight
  lanes.
* `RANDOMIZE` gently changes the depth and speed of lanes that are already
  moving, gliding over several seconds. It never starts an idle lane, never
  changes a target and never restarts a phase.
* A lane may be empty or motionless. Empty, motionless, `Off`, amount 0 and
  faded-out lanes contribute exactly zero modulation. Gloop never invents an
  oscillator or background wobble to fill a lane.

Out of scope for this iteration: more than one target per lane, a general
modulation matrix, per-lane smoothing curves, tempo sync and audio-rate
recording of the knob.

## Architecture

```
  APVTS target/amount/speed ──┐
  GloopControl mode atomics ──┤
                              v
  processor (message thread)  ── 100 Hz recorder ── gesture (int16-quantised)
        │                                              │
        │  SPSC queue (gesture hand-off)               │ state XML blob
        v                                              v
  EngineDSP (audio thread)  GloopEngine  ── offsets per target ── existing
        │                                   control-rate frame    macros/LFO
        v
  VisualState::gloop  ──  target knob rings + Gloop panel
```

### Gesture representation

A gesture is a fixed 100 Hz sequence of **normalised offsets** ([-1, 1]) from the
target knob's value at the start of the take, plus its duration. Offsets live in
the target parameter's own normalisable range, so a cutoff sweep keeps its
musical shape when the live centre moves: the engine converts the current value
to normalised space, adds the offset and converts back through the same skew.

A 30-second cap keeps the engine memory bounded (8 lanes x 30 s x 4 B = 96 KB)
and the saved state compact. Offsets are quantised to int16 on save.

### Recording (message thread)

The processor owns a 100 Hz `juce::Timer`. On a `REC` request it locks the
target, records the target APVTS value relative to the start centre, and on
`REC` again (or when the buffer fills) finalises the take and publishes it to
the audio thread through a lock-free SPSC queue. This keeps the audio thread
free of APVTS reads and large copies, and makes session persistence
straightforward (the processor owns the authoritative take).

Limitation: capture resolution depends on the host's message-thread timer. The
100 Hz grid and the output smoothing make this musical for knob gestures; it is
not a sample-accurate automation recorder.

### Playback (audio thread)

Each lane keeps its own phase. Phase advances by

```
dt * smoothedSpeed * randomSpeed / duration
```

so changing speed glides in phase without a restart. Samples are interpolated
linearly; the loop boundary crossfades over a short window so a take whose start
and end do not match still wraps gracefully. The resulting offset is smoothed by
a one-pole whose time constant comes from the global `SMOOTH` control
(~8 ms at 0, ~260 ms at 1; default ~40 ms).

The lane's effective contribution is

```
gestureOffset * amount * fadeGain * randomAmountScale
```

Lanes targeting the same parameter are summed and passed through a bounded
soft limiter, then applied to the engine's control value with
`gloopApplyOffset()`. There is no last-writer-wins and no writing back to the
APVTS, so the human knob always remains in charge.

### Modes

| Mode | Audio contribution | Next |
|---|---|---|
| `Idle` | zero | `Record` on REC, or silently READY with a stored take |
| `Record` | zero (capturing) | `Loop` on the next press, or `Idle` on cancel |
| `Loop` | looping gesture | `Fade` on FADE |
| `Fade` | looping gesture x fading gain | `Idle` when the gain reaches 0 |

Modes are performance state, not host parameters: they are not automatable,
they are persisted with the session, and REC/FADE never restart automatically on
load. A completed FADE is reported through `VisualState` and settles to IDLE on
the message thread.

### Randomize

A Randomize press increments an epoch counter. On the audio thread, lanes that
are already `Loop` or `Fade` with a gesture glide new internal amount and speed
scales over ~3–4 seconds. Target and mode are untouched; idle lanes stay silent;
phase is never reset. The user's amount/speed knobs remain the base values, so
Randomize is a temporary, reversible-feeling transform rather than a preset
explosion.

### Persistence

`getStateInformation` adds a versioned, base64-encoded `gloopState` root
property when at least one lane has a take. It contains per-lane mode
(`Idle`/`Loop` only), duration and int16 offsets. REC and FADE are transient and
are saved as `Idle`; a project saved mid-fade restores the take as READY, not as
a resumed fade. Sessions without Gloop takes have no extra state. Existing
factory presets are untouched: they contain no Gloop data, so loading one leaves
the current Gloop targets/amounts/speeds and takes alone (the preset's own
parameters change, and the orbits continue around the new values). Loading
**Init** resets every parameter, including Gloop, to its default; that silences
the lanes but does not delete the takes.

## Trade-offs presented for review

1. **Gloop is an additional movement layer in this iteration, not a replacement
   for the Gesture macros or the LFO.** Replacing them now would break existing
   presets, saved sessions and host automation, and the new layer has not yet
   had musical feedback. Keeping them intact lets Lonn compare. The likely
   migration path (after acceptance) is to map the five macro destinations onto
   recorded Gloop lanes, keep the macros as preset-level controls, and retire
   the LFO destinations only with a preset migration/versioning step. No
   existing feature is removed or silently rewritten here.
2. **Eight lanes are exposed in the first playable build.** The data model is a
   flat array, so eight lanes cost little more than one; the panel uses one row
   per lane. The single-lane experience is still the primary demonstration.
3. **One target per lane.** This covers the musical use case and keeps the UI
   legible. A modulation matrix is deliberately not attempted.
4. **Message-thread capture at 100 Hz.** It reads the same APVTS values the user
   is moving, keeps the audio thread clean and makes persistence simple. The
   cost is timer jitter; smoothing and interpolation absorb it. Audio-thread
   capture would be more accurate but would need a second hand-off just to save
   the take.
5. **FADE is a real 2.5-second withdrawal, not an immediate stop.** This is the
   artistic contract; an immediate stop would click and would defeat the
   "ready for another REC" gesture.
6. **Bounded summing, not a mixer.** Multiple lanes on one parameter add and
   soft-limit. This is musically intelligible and bounded; a per-lane weight
   matrix is future work.

## How to play it

1. Build the plugin or standalone (`./scripts/build.sh NAGLOED`).
2. Open the Gloop panel with the `GLOOP` button over the grain field.
3. In a lane, choose a target (for example `Cutoff`), press `REC`, and move that
   target knob for a few seconds. Press the same button (`STOP`) to loop it; the
   button then reads `LOOP`.
4. Add more lanes on different targets or the same target, with different
   lengths. Leave them unsynchronised.
5. Move a target knob while a loop runs: the ring translates around the live
   value. Change `AMT` and `SPD`; speed glides without restarting.
6. Click `LOOP` on a lane to start its `FADE…`; the lane becomes READY.
7. Press `RANDOMIZE` to let the moving lanes drift to new depths/speeds.

The standalone is the quickest way to play; the AU/VST3 behave the same inside a
host. `docs/gloop-preview.png` is a deterministic layout preview with an injected
synthetic lane frame (the preview tool labels it as such); it shows the panel and
the knob rings, not a recorded performance.

## Verification

Automated tests cover: silent/motionless/Off/amount-zero lanes; gesture playback
and amount scaling; 0.25x/1x/2x phase rates and continuous speed changes; fade
completion; seam continuity; live-centre translation around two base values;
bounded summing of two lanes; gentle Randomize and untouched idle lanes; visual
state; allocation-free processing; and session state round-trip. The UI
parameter-coverage check ensures every Gloop parameter is attached. These
establish mechanics, not artistic success.

Not established by tests: whether the gesture feels musical on Lonn's material,
whether the default smoothing is right for his playing, and whether eight lanes
are useful in practice. Those require playing/listening feedback.
