# NAGLOED — Preset Guide

NAGLOED ships with **24 factory presets** (plus the Init patch) across eight
categories: Textures, Freeze, Pads, Dust & Noise, Rhythmic, Drone/Synth, Broken
and Space.

Every preset is a *complete snapshot*: all 45 parameters are written explicitly,
so switching presets never inherits a stray value from the one before. The
values are in real-world units — grains per second, milliseconds, hertz, dB of
gain, semitones and 0–1 for percentages — exactly as shown in the editor.

The presets are designed to be played, not just loaded. The five macro knobs
(Glow, Drift, Grain, Space, Shimmer), the LFO and the source modes are part of
every patch. Where a preset has an obvious gesture, it is listed below.

| # | Preset | Category | Character | Source material & gesture |
|---|--------|----------|-----------|----------------------------|
| 01 | **Nachtgloed** | Textures | Slow, warm amber grain bed that thickens without harshness | Long synth or bowed notes. Push **Glow** from its 75% start toward 95%; nudge **Space** for a bigger room. |
| 02 | **Veldopname** | Textures | Wide field-recording haze with tape hiss and slow auto-pan | Field recordings, breath, rain. Ride **Drift** to loosen the pan; **Space** lifts the room. |
| 03 | **Klankstof** | Textures | Fine sound dust — a held note atomised into particles | Dense chords, metal hits. Open **Grain** then **Glow** slowly; play with Spray. |
| 04 | **Strijp Sirene** | Freeze | Frozen interval siren bent by a slow triangle LFO | Any sustained chord. Freeze it, then let the LFO wail; ride **Shimmer** for the octave edge. |
| 05 | **Vlokken** | Freeze | Sparse, long reverse flakes drifting in a huge room | One soft note, then silence. Let it snow; **Space** and **Drift** stretch the drift. |
| 06 | **Doorzichtig** | Freeze | Glass-clear frozen shimmer, an octave and a fifth up | Soft pad or voice. Freeze it and play **Shimmer** like a bell; keep feeding quiet single notes. |
| 07 | **Wolkenkaart** | Pads | Shimmering minor cloud-pad an octave up | Held triads. Ride **Shimmer** and **Space** until the room becomes weather. |
| 08 | **Zomernacht** | Pads | Warm minor night-pad with tape wow and a lazy filter LFO | Slow dyads. Ride **Drift** to swell the tape; **Glow** to pull the cloud forward. |
| 09 | **Schemerschijn** | Pads | Dorian feedback bloom with an S&H-nudged focus | Held chords, then feed it back. Hold and push **Glow** past 70%. |
| 10 | **Ruisvlek** | Dust & Noise | Pure-noise stain, high density, resonant filter | No input needed (Noise source). Sweep Cutoff; **Grain** controls the hiss density. |
| 11 | **Tape Ember** | Dust & Noise | Worn tape ember: heavy wow, LoFi grit, hissing feedback | Short sustained notes, radio speech, guitar. Stir **Drift** to wobble the transport. |
| 12 | **Onweer** | Dust & Noise | Thunder rolling under a dark resonant filter | No input needed (Noise source). Ride **Space** for the rain tail; **Drift** for the wind. |
| 13 | **Korrelregen** | Rhythmic | Grain rain — forward scan chopped by a square LFO | Steady arpeggios or drum loops. **Grain** tightens/loosens the stutter; play with Scan. |
| 14 | **Modulab 2AM** | Rhythmic | Self-running modular clockwork, no input required | No input needed (Drone source). Conduct with **Grain** and **Space**. |
| 15 | **Nikkel** | Rhythmic | Struck-nickel ticks through a screaming resonance | Staccato notes, rim clicks. Play short; ride **Glow** for the ring. |
| 16 | **Sintel Drone** | Drone/Synth | Dark self-oscillating grain drone an octave down | No input needed (Drone source). Play the macros as a slow synth: **Glow**, **Drift**, **Space**. |
| 17 | **Nazomer** | Drone/Synth | Late-summer warm drone with heavy tape | No input needed (Drone source). Ride **Glow** and **Space**; use **Drift** sparingly. |
| 18 | **Diepgang** | Drone/Synth | Sub-only depth: half-second grains, two octaves down | No input needed (Drone source). **Space** builds pressure; keep Glow low until the end. |
| 19 | **Halflicht** | Broken | Warped half-broken loop that never resolves | A phrase, then stop playing. Ride **Drift** to break it further; **Glow** decays wrong. |
| 20 | **Prison Songs** | Broken | Brittle, band-limited, Phrygian and narrow | One mournful line. The buffer repeats it back; **Drift** wears the tape. |
| 21 | **Brakke Grond** | Broken | Brackish noise dragged through a broken tape filter | No input needed (Noise source). Slowly ride **Drift**; **Grain** stirs the sediment. |
| 22 | **Nagalm** | Space | Five-grain bell in an endless room | One note every few seconds. **Space** and **Shimmer** are the whole performance. |
| 23 | **Helmond Haze** | Space | Washy industrial haze, heavily modulated space | Held chords, metal, radio. Push **Space** until the room dissolves. |
| 24 | **EAR Room** | Space | Driven, warm, close room — the EAR circuit's own voice | Dry chords, drum machines. Use Drive as the gesture; **Glow** adds the afterglow. |

## Freeze tips

The three **Freeze** presets (Strijp Sirene, Vlokken, Doorzichtig) use the
**Input** source, open with `Freeze` engaged, and the capture buffer starts
empty — so do this:

1. Bypass the freeze for a moment — click **Freeze** off — while your source
   plays a chord or a note through the plugin.
2. Click **Freeze** back on. The cloud now holds that moment.
3. Play **Scan**, **Pitch**/Shimmer and **Spray** to move inside the frozen
   material.

Presets that use the **Drone** or **Noise** source (`grain.source`) generate
their own material and sound immediately, whatever the freeze state.

## How to audition

1. **Load the plugin on a stereo track** and put a simple source in front of it:
   a soft synth chord, a field recording, or a drum loop. A dry piano or voice
   works well because it leaves space for the grains.
2. **Set the input level** so the dry signal peaks around −12 dBFS. The presets
   are gain-staged for that; they never rely on clipping.
3. **Step through the presets** with the DAW program list. Each name is ≤ 24
   characters so it fits a program list; the category tells you what to play.
   When you land on a **Freeze** preset (04–06), the capture buffer is still
   empty; use the Freeze tips above to load it.
4. **Play the macros.** Every preset has a musical macro or LFO move described
   in the table above. The macros are the performance surface: small moves
   (5–15%) over several seconds are more interesting than a full sweep.
5. **Self-generating presets** — Ruisvlek, Onweer, Modulab 2AM, Sintel Drone,
   Nazomer, Diepgang and Brakke Grond — use the internal Noise/Drone source, so
   they keep playing with the input muted. Tape Ember, Halflicht and Prison
   Songs are input-driven on purpose: their `ear.mix` keeps the dry source
   audible, and they want you to play into them.
6. **Check the demo.** `Demo/Nagloed-Demo.wav` is an 84-second performance
   rendered through the full engine with `nagl_render` (48 kHz / 24-bit stereo,
   peaking around −2.1 dBFS); the score moves through dry tap-in → frozen chord
   → shimmer octave → dust → drone dive → open space → return, and all five
   macros are automated. Use it as a reference for the arc.

### Quick macro glossary

These are the mappings the engine applies on top of the value set in a preset,
so a macro always reads as a "more" gesture rather than a second parameter.

| Macro | What it does | Try it |
|-------|--------------|--------|
| **Glow** | Warm old-tape character: +8 dB drive, cutoff down ~1.2 octaves, more hiss, darker tone | Slow 40 → 80% builds |
| **Drift** | Analog instability: extra ear drift, wow, tape and pitch wobble | Small wobbles; large = broken |
| **Grain** | Cloud shape: density ×2^(±1.2 octaves), plus spray, width and spread | Breathe it 30 → 70% |
| **Space** | Ruimte depth: mix +0.35, size +0.25, modulation +0.30 | Open at the end of a phrase |
| **Shimmer** | Chance each grain jumps up an octave, plus extra space | Bring in for the last bar |

## Rendering the demo

From the repository root:

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DNAGLOED_JUCE_DIR="$PWD/.deps/JUCE"
cmake --build build -j10 --target nagl_render
./build/nagl_render_artefacts/Release/nagl_render   # writes Demo/Nagloed-Demo.wav
```

The score (`tools/DemoScore.cpp`) is deterministic and stateless: it renders in
about a second and is reproducible on the same build and platform.
