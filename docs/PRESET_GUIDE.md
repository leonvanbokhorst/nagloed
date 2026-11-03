# NAGLOED — Preset Guide

NAGLOED ships with **24 factory presets** (plus the Init patch) across eight
categories: Textures, Freeze, Pads, Dust & Noise, Rhythmic, Drone/Synth, Broken
and Space.

Every preset is a *complete snapshot*: all 45 parameters are written explicitly,
so switching presets never inherits a stray value from the one before. The
values are in real-world units — grains per second, milliseconds, hertz, dBFS,
semitones and 0–1 for percentages — exactly as shown in the editor.

The presets are designed to be played, not just loaded. The five macro knobs
(Glow, Drift, Grain, Space, Shimmer), the LFO and the source modes are part of
every patch. Where a preset has an obvious gesture, it is listed below.

| # | Preset | Category | Character | Source material & gesture |
|---|--------|----------|-----------|----------------------------|
| 01 | **Nachtgloed** | Textures | Slow, warm amber grain bed that thickens without harshness | Long synth or bowed notes. Ride **Glow** from 40 → 80%; nudge **Space** for a bigger room. |
| 02 | **Veldopname** | Textures | Wide field-recording haze with tape hiss and slow auto-pan | Field recordings, breath, rain. Leave it running; ride **Drift** to loosen the pan. |
| 03 | **Klankstof** | Textures | Fine sound dust — a held note atomised into particles | Dense chords, metal hits. Open **Grain** then **Glow** slowly; play with Spray. |
| 04 | **Strijp Sirene** | Freeze | Frozen interval siren bent by a slow triangle LFO | Any sustained chord. Freeze, then let the LFO wail; ride **Shimmer** for the octave edge. |
| 05 | **Vlokken** | Freeze | Sparse, long reverse flakes drifting in a huge room | One soft note, then silence. Let it snow; **Space** and **Drift** stretch the drift. |
| 06 | **Doorzichtig** | Freeze | Glass-clear frozen shimmer, 19 semitones up | Soft pad or voice. Freeze and play **Shimmer** like a bell; keep playing quiet single notes. |
| 07 | **Wolkenkaart** | Pads | Shimmering minor cloud-pad an octave up | Held triads. Ride **Shimmer** and **Space** until the room becomes weather. |
| 08 | **Zomernacht** | Pads | Warm minor night-pad with tape wow and a lazy filter LFO | Slow dyads. Ride **Drift** to swell the tape; **Glow** to pull the cloud forward. |
| 09 | **Schemerschijn** | Pads | Dorian feedback bloom with an S&H-nudged focus | Held chords, then feed it back. Hold and push **Glow** past 70%. |
| 10 | **Ruisvlek** | Dust & Noise | Pure-noise stain, high density, resonant filter | No input needed (Noise source). Sweep Cutoff; **Grain** controls the hiss density. |
| 11 | **Tape Ember** | Dust & Noise | Worn tape ember: heavy wow, LoFi grit, hissing feedback | Short sustained notes, radio speech, guitar. Stir **Drift** to wobble the transport. |
| 12 | **Onweer** | Dust & Noise | Thunder rolling under a dark resonant filter | Low notes or a kick. Ride **Space** for the rain tail; **Drift** for the wind. |
| 13 | **Korrelregen** | Rhythmic | Grain rain — forward scan chopped by a square LFO | Steady arpeggios or drum loops. **Grain** tightens/loosens the stutter; play with Scan. |
| 14 | **Modulab 2AM** | Rhythmic | Self-running modular clockwork, no input required | Leave the input silent (Drone source). Conduct with **Grain** and **Space**. |
| 15 | **Nikkel** | Rhythmic | Struck-nickel ticks through a screaming resonance | Staccato notes, rim clicks. Play short; ride **Glow** for the ring. |
| 16 | **Sintel Drone** | Drone/Synth | Dark self-oscillating grain drone an octave down | No input needed. Play the macros as a slow synth: **Glow**, **Drift**, **Space**. |
| 17 | **Nazomer** | Drone/Synth | Late-summer warm drone with heavy tape | One long chord, then leave it. Ride **Glow** and **Space**; use **Drift** sparingly. |
| 18 | **Diepgang** | Drone/Synth | Sub-only depth: half-second grains, two octaves down | One low note per bar. **Space** builds pressure; keep Glow low until the end. |
| 19 | **Halflicht** | Broken | Warped half-broken loop that never resolves | A phrase, then stop playing. Ride **Drift** to break it further; **Glow** decays wrong. |
| 20 | **Prison Songs** | Broken | Brittle, band-limited, Phrygian and boxed-in mono | One mournful line. The buffer repeats it back; **Drift** wears the tape. |
| 21 | **Brakke Grond** | Broken | Brackish noise dragged through a broken tape filter | No input needed. Slowly ride **Drift**; **Grain** stirs the sediment. |
| 22 | **Nagalm** | Space | Five-grain bell in an endless room | One note every few seconds. **Space** and **Shimmer** are the whole performance. |
| 23 | **Helmond Haze** | Space | Washy industrial haze, heavily modulated space | Held chords, metal, radio. Push **Space** until the room dissolves. |
| 24 | **EAR Room** | Space | Driven, warm, close room — the EAR circuit's own voice | Dry chords, drum machines. Use Drive as the gesture; **Glow** adds the afterglow. |

## Freeze tips

The three **Freeze** presets (Strijp Sirene, Vlokken, Doorzichtig) open with
`Freeze` engaged. The capture buffer starts empty, so do this:

1. Bypass the freeze for a moment — click **Freeze** off — while your source
   plays a chord or a note through the plugin.
2. Click **Freeze** back on. The cloud now holds that moment.
3. Play **Scan**, **Pitch**/Shimmer and **Spray** to move inside the frozen
   material.

With the **Drone** or **Noise** source (`grain.source`) the freeze state is
irrelevant — those presets generate their own material and sound immediately.

## How to audition

1. **Load the plugin on a stereo track** and put a simple source in front of it:
   a soft synth chord, a field recording, or a drum loop. A dry piano or voice
   works well because it leaves space for the grains.
2. **Set the input level** so the dry signal peaks around −12 dBFS. The presets
   are gain-staged for that; they never rely on clipping.
3. **Step through the presets** with the DAW program list. Each name is ≤ 24
   characters so it fits a program list; the category tells you what to play.
4. **Play the macros.** Every preset has a musical macro or LFO move described
   in the table above. The macros are the performance surface: small moves
   (5–15%) over several seconds are more interesting than a full sweep.
5. **For Drone/Synth, Dust & Noise and Broken**, try removing the input
   entirely (or mute the track): the internal source keeps playing.
6. **Check the demo.** `Demo/Nagloed-Demo.wav` is an 84-second performance
   rendered through the plugin with `nagl-render`; it moves through every
   category's territory: dry tap-in → frozen chord → shimmer octave → dust →
   drone dive → open space → return. Use it as a listening reference for how
   far the macros can be pushed.

### Quick macro glossary

| Macro | What it does | Try it |
|-------|--------------|--------|
| **Glow** | Grain cloud density/size warmth | Slow 40 → 80% builds |
| **Drift** | Pitch/timing instability and tape drift | Small wobbles; large = broken |
| **Grain** | Grain rate/character (the engine's core) | Breathe it 30 → 70% |
| **Space** | Ruimte reverb depth | Open at the end of a phrase |
| **Shimmer** | Octave-up shimmering layer | Bring in for the last bar |

## Rendering the demo

From the repository root:

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DNAGLOED_JUCE_DIR="$PWD/.deps/JUCE"
cmake --build build -j10 --target nagl_render
./build/nagl_render_artefacts/Release/nagl_render   # writes Demo/Nagloed-Demo.wav
```

The score (`tools/DemoScore.cpp`) is deterministic and stateless: it renders in
about a second and always produces the same file.
