# NAGLOED manual

**NAGLOED 1.0.0** · UnlikeNoise Labs · AU / VST3 / Standalone · macOS 12 or later

> *nagloed* (Dutch, n.) — the light that stays in the room after the source is gone.

This manual documents NAGLOED's controls as implemented in 1.0.0. Control
names, ranges, defaults and parameter IDs are taken from the same source of
truth the plug-in uses (`src/core/Parameters.h`), so what is written here is what
the host sees.

## What it is

NAGLOED is an audio effect that granulates live input, and a granular instrument
that can play an internal source. It records what you feed it into a capture
buffer, reads that material as short grains, and lets the grains glow in a room
before an analog-style output stage colours them. It does not load audio files
and it does not respond to MIDI notes; it is an insert effect with a strong
opinion about what happens after the source stops.

Signal order: input trim → capture buffer → Sintel → Ruimte → EAR → output trim
→ dry/wet mix.

## Who this is for

Experimental musicians, modular players, sound artists, field recordists and
one-take recorders; also sound designers who need texture that behaves like
material. The instrument expects you to play it. There is a factory library, but
no single preset will make a finished track for you.

## Quick start

1. Insert NAGLOED on an audio track that already has sound on it (a synth, a
   voice, a recording), or use the standalone app with an audio input.
2. Play the source and watch the capture buffer fill. Up to twelve seconds are
   held.
3. With **Source** set to *Input*, raise **Density** until the grains become
   audible. Start with **Grain** around 80 ms.
4. Move **Spray**, **Focus** and **Scan** while the source plays. These three
   decide where the grains come from and how they move.
5. Turn **Freeze** on. The capture buffer stops recording new input; the grains
   keep reading what is there. Move **Scan** and **Pitch** to play the afterglow.
6. Add **Ruimte** (Space, Size, Damp) and EAR colour (Drive, Cutoff, Tape, Mix)
   to taste.

If you want a starting point rather than Init, load a factory preset from the
host's program list — see [Presets](#presets).

## Signal flow

```
 live input ─► In ─► capture buffer (12 s) ─► Sintel ─► Ruimte ─► EAR ─► Out ─► Mix ─► main output
                       ▲    │
                       │    └── Freeze stops recording new input into the buffer
                       └─────── Feed: grain feedback returns grain output to the buffer
```

- **Source = Input** records the live input into the capture buffer and reads it
  as grains.
- **Source = Noise** feeds Sintel from an internal noise source.
- **Source = Drone** feeds Sintel from an internal drone source.
- **Freeze** stops the buffer recording. Grains keep reading the material that is
  already there. It does not stop the output.
- **In**, **Out** and **Mix** live in the EAR section but act on the whole chain.

The engine supports mono and stereo plug-in configurations. It reports an
eight-second tail to the host so that a frozen buffer or a long Ruimte tail can
ring out when the host stops.

## Sintel — the grain engine

Sintel is the heart of NAGLOED. It writes input into the capture buffer and
schedules grains from that buffer at a rate you set. Up to 256 grains can be
alive at once; the buffer holds up to twelve seconds.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Density** (`grain.density`) | 0.5–200 g/s | 30.0 g/s | How many grains are launched per second. Low values make separate embers; high values fuse into a cloud. |
| **Grain** (`grain.size`) | 5–500 ms | 80 ms | Length of one grain. Short grains are dust and texture; long grains are pads and smears. |
| **Spray** (`grain.spray`) | 0–100% | 25% | Scatters the read position of each grain around Focus. At 0% grains are orderly; high values turn the buffer into a cloud. |
| **Scan** (`grain.scan`) | −100%–+100% | 0% | Speed and direction of the read position through the capture buffer. At 0% the read position holds; positive scans forward, negative backward. |
| **Pitch** (`grain.pitch`) | −24–+24 st | 0 st | Transposes the grains in semitones. |
| **Spread** (`grain.spread`) | 0–24 st | 0 st | Per-grain random pitch spread. At 0 every grain has the same pitch; higher values detune the cloud. |
| **Reverse** (`grain.reverse`) | 0–100% | 0% | Probability that a grain plays backwards. |
| **Jitter** (`grain.jitter`) | 0–100% | 30% | Randomises grain timing. Low values are precise; high values are loose and human. |
| **Shape** (`grain.shape`) | 0–100% | 15% | Grain window from smooth Hann to a sharper exponential decay. Soft at the low end, more percussive at the high end. |
| **Width** (`grain.width`) | 0–100% | 50% | Stereo width of the grain field. 0% is centred, 100% is wide. |
| **Feed** (`grain.feedback`) | 0–95% | 0% | Returns a portion of the grain output to the capture buffer. At 0 the buffer holds only recorded input; high settings thicken and can build. |
| **Focus** (`grain.focus`) | 0–100% | 50% | Positions the centre of the grain read area inside the capture buffer. Spray scatters around this point. |
| **Freeze** (`grain.freeze`) | Off / On | Off | Stops recording new input into the capture buffer. Grains keep reading what is there. |
| **Tune** (`grain.tune`) | Free, Chromatic, Minor, Major, Dorian, Phrygian, Pentatonic, Whole-tone | Free | Quantises grain pitch to a scale. See [Tuning and scales](#tuning-and-scales). |
| **Source** (`grain.source`) | Input, Noise, Drone | Input | Selects what feeds Sintel: the live capture buffer or an internal noise/drone source. |

### Notes on playing Sintel

- **Density × Grain length** is the main texture control. Low density with long
  grains gives isolated pads; high density with short grains gives dust.
- **Spray and Jitter together** make the difference between a machine and a
  room. Spray moves the source position; Jitter moves the time.
- **Scan at 0%** is a held position, not a stopped buffer. The buffer still
  records unless Freeze is on.
- **Feed** is a feedback path, so it can grow. Raise it slowly and keep an eye on
  output level; the design limit is 95%, not 100%, for a reason.

## Ruimte — the space

Ruimte is the room the grains glow in. It is a reverb-style space, not a clean
digital plate; it is meant to sit behind the grains and move a little.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Space** (`space.mix`) | 0–100% | 35% | How much Ruimte you hear against the direct grain signal. |
| **Size** (`space.size`) | 0–100% | 60% | Size of the room. Low values are close and dry; high values are long and open. |
| **Damp** (`space.damp`) | 0–100% | 40% | High-frequency damping of the tail. Higher values make the room darker. |
| **Mod** (`space.mod`) | 0–100% | 30% | Movement inside the room. Higher values make the tail breathe instead of sitting still. |

## EAR — the analog section

EAR is the output stage: drive, tone shaping, tape behaviour, noise and the final
trim. It is the part that makes grains sound like they ran through a small analog
console rather than a plug-in. The settings are deliberately conservative at
Init; push them when you want the instrument to sound older than it is.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Drive** (`ear.drive`) | 0–30 dB | +6.0 dB | Gain into the analog stage. More drive means more colour and compression; at 0 dB the stage is close to clean. |
| **Bias** (`ear.bias`) | 0–100% | 15% | Asymmetry of the drive stage. Small amounts thicken; high amounts get rough and asymmetric. |
| **Tone** (`ear.tone`) | 0–100% | 50% | Overall brightness of the analog colour, from dark to bright. |
| **Cutoff** (`ear.cutoff`) | 20 Hz–20 kHz | 9.0 kHz | Low-pass cutoff of the EAR stage. |
| **Reso** (`ear.reso`) | 0–100% | 15% | Resonance at the cutoff. |
| **Drift** (`ear.drift`) | 0–100% | 25% | Slow random instability: the feeling of analog circuits that never quite settle. |
| **Wow** (`ear.wow`) | 0–100% | 20% | Cyclic pitch wobble, in the tape-wow family. |
| **Tape** (`ear.tape`) | 0–100% | 35% | Amount of tape character: gentle compression, softening, saturation. |
| **LoFi** (`ear.lofi`) | 0–100% | 0% | Digital degradation. 0% is clean; higher values add sample-rate/bit reduction. |
| **Hiss** (`ear.hiss`) | 0–100% | 6% | Analog noise floor. Keep it low; it is a texture, not a feature. |
| **In** (`ear.input`) | −24–+24 dB | 0.0 dB | Input trim at the start of the chain. |
| **Out** (`ear.output`) | −24–+6 dB | 0.0 dB | Output trim at the end of the chain. |
| **Mix** (`ear.mix`) | 0–100% | 65% | Dry/wet balance between the input and the fully processed NAGLOED signal. |

Two labels repeat across sections: **Drift** exists in both EAR and the gesture
macros, and **Space** is both the Ruimte mix control and a macro. The parameter
ID is always unique; in this manual, a control is named by its section
(`EAR → Drift`, `Macro → Space`).

## Gesture macros

The five macros are performance gestures, not a modulation matrix. Each one moves
several internal parameters at once, and what it does depends on where the direct
controls already are. They are designed to be moved while the sound is playing,
and they are stored in presets and saved sessions like any other parameter.

| Macro | Range | Default | Gesture |
|---|---|---|---|
| **Glow** (`macro.glow`) | 0–100% | 30% | Overall heat and presence of the grain field. Raise to bring the cloud forward; lower to let the source through. |
| **Drift** (`macro.drift`) | 0–100% | 30% | Total instability: timing, pitch and analog wobble. Raise for a sound that refuses to sit still. |
| **Grain** (`macro.grain`) | 0–100% | 50% | Grain character, from fine dust to coarse embers. |
| **Space** (`macro.space`) | 0–100% | 40% | Overall distance and room. Raise to step back from the instrument. |
| **Shimmer** (`macro.shimmer`) | 0–100% | 20% | High, glassy movement in the cloud. Use sparingly; it is the brightest gesture. |

Practical gesture: hold **Freeze** with one hand and move **Scan**, **Pitch** or
**Shimmer** with the other. That is the core performance move of the instrument.

## Freeze workflow

Freeze is the central workflow of NAGLOED. It stops the capture buffer from
recording new input; it does not stop the grains.

1. **Feed the buffer.** With Source = Input and Freeze off, play the material you
   want to keep. Up to twelve seconds are held. If you plan to work for a while,
   re-feed the buffer with a fresh phrase before freezing.
2. **Choose the moment.** Engage Freeze while the material you want is in the
   buffer. The buffer keeps exactly what it has; new input is ignored.
3. **Play the remains.** Move **Scan** to travel through the held material, or
   leave it at 0% for a static room. **Focus** and **Spray** choose where grains
   come from. **Pitch**, **Spread** and **Tune** choose what they become.
4. **Let it evolve.** Raise **Auto**, set a slow **LFO**, or move a macro by hand.
   Frozen material is where the long, evolving end of NAGLOED lives.
5. **Unfreeze when you want new material.** Recording resumes immediately and the
   buffer starts taking on the new input; it does not erase everything at once,
   but the old material is gradually replaced.

### Freeze recipes

**Freeze a choir.** Feed a sustained vocal or a chord for two to four seconds.
Set Density around 35 g/s, Grain around 120 ms, Spray around 30%, Focus 50%,
Space around 45%, EAR Mix around 70%. Engage Freeze on the warmest part of the
chord, then move Scan very slowly and add Pitch +7 st for the second pass. Listen
for the vowel turning into a room rather than a loop.

**Dust a field recording.** Feed a recording with transients — rain, birds, a
street. Set Density around 60 g/s, Grain 25 ms, Spray 70%, Jitter 50%, Shape 70%,
Width 80%, Freeze on. Add LoFi 20% and Hiss 10%. Keep Ruimte short (Size 30%,
Damp 60%). The result should be recognisably the place, not recognisably the
recording.

**Drone self-oscillation.** Set Source = Drone, Density 90 g/s, Grain 300 ms,
Spray 80%, Jitter 70%, Feed 80%, Ruimte Size 80%, Ruimte Mod 60%, EAR Drive
15 dB. Raise Feed in small amounts until the feedback starts to bloom, then leave
it. This is the instrument playing itself; use the macro **Drift** to keep it
unstable.

**One-take coda.** Freeze a phrase at the end of a one-take performance, fade the
dry input in the host, and let the frozen grains carry the last thirty seconds.
Use Scan at 0%, Grain long (200–400 ms) and Auto around 30%.

## Tuning and scales

**Tune** quantises grain pitch to a scale. It is not a keyboard lock: NAGLOED
accepts no MIDI notes, so there is nothing to play chromatically. It is a way of
keeping a grain cloud in a harmonic world.

| Tune | Result |
|---|---|
| **Free** | No quantisation. Grains use Pitch and Spread exactly as set. |
| **Chromatic** | Grains snap to semitones. |
| **Minor** | Grains fold onto a minor scale. |
| **Major** | Grains fold onto a major scale. |
| **Dorian** | Grains fold onto a Dorian mode. |
| **Phrygian** | Grains fold onto a Phrygian mode. |
| **Pentatonic** | Grains fold onto a pentatonic scale. |
| **Whole-tone** | Grains fold onto a whole-tone scale. |

Start with Free while you are learning the instrument. Switch to a scale when you
want a frozen buffer to hold together as harmony rather than as texture. Pitch
and Spread still apply before quantisation, so you can move the whole cloud with
Pitch and keep it in scale.

## Modulation

### LFO

One LFO, one destination. It is intended for slow movement, not for rhythmic
tremolo.

| Control | Range | Default | What it does |
|---|---|---|---|
| **Rate** (`mod.lfoRate`) | 0.01–20 Hz | 0.15 Hz | LFO speed. The low end is the useful end. |
| **Shape** (`mod.lfoShape`) | Sine, Triangle, Saw Up, Saw Down, Square, S&H | Sine | LFO waveform. S&H gives stepped random movement. |
| **Dest** (`mod.lfoDest`) | Off, Focus, Pitch, Cutoff, Pan, Space, Density | Off | Which control the LFO moves. Off leaves it disconnected. |
| **Amount** (`mod.lfoAmount`) | −100%–+100% | 0% | Depth and polarity of the modulation around the current value. |

### Auto-motion

| Control | Range | Default | What it does |
|---|---|---|---|
| **Auto** (`mod.auto`) | 0–100% | 15% | Amount of slow automatic motion the engine adds on its own. At 0% a frozen buffer is static until you move something; raise it for unattended evolution. |

### Expression parameters

NAGLOED 1.0.0 does not accept MIDI or MPE input, so these parameters cannot be
played from a controller. They are part of the permanent parameter surface for
automation and preset authoring, and are reserved for the MPE update.

| Control | Range | Default | Destination options |
|---|---|---|---|
| **Pressure** (`mod.pressure`) | 6 choices | Density | Density, Pitch, Cutoff, Drive, Space, Spread |
| **Slide** (`mod.slide`) | 4 choices | Pitch | Pitch, Focus, Cutoff, Pan |
| **Expression** (`mod.expression`) | 0–100% | 100% | Amount of expression response |

## Presets

- **Init** is always program 0: the documented defaults exactly (Density
  30 g/s, Space 35%, EAR Mix 65%, and so on). It is a clean starting point, not
  silence.
- The factory library is exposed through the host's program list. Loading a preset
  replaces every parameter value, so move the controls afterwards.
- NAGLOED 1.0.0 does not write user preset files. Save your own variations in
  your host's preset, rack or track-preset system; the plug-in state is saved in
  the session.
- The preset guide ships with the release: `docs/PRESET_GUIDE.md`.

## CPU, latency and sample rate

- The grain pool tops out at 256 simultaneous grains and the capture buffer at
  twelve seconds. Both limits are part of the instrument.
- **Density × Grain length** is the main CPU cost; **Feed** and long **Ruimte**
  tails add to it. Freezing does not reduce CPU because the grains keep running.
- The capture buffer is written in real time, so granulation needs no look-ahead
  delay. The plugin reports its processing latency to the host and an eight-second
  tail so that frozen or reverberant material can ring out after transport stops.
- All processing runs at the host's sample rate. Test at your working rate and
  buffer size; CPU cost scales with sample rate like any plug-in.

## Automation

- Every control is a host-automatable parameter with a permanent ID (listed in
  the tables above). Automation writes real-world units: g/s, ms, %, dB, Hz,
  semitones.
- Parameters are read once per audio block, so automation is applied at block
  boundaries. Slow, musical automation works well; very fast stepped automation
  will step.
- **Freeze** is a two-state parameter. Automate it as a clean step between
  phrases rather than a ramp.
- **Tune**, **Source**, **Shape** (LFO) and **Dest** are choice parameters. Change
  them between phrases, not mid-grain, if you want a clean transition.
- For performance, automate the five macros or use your host's MIDI learn on
  them; they are designed for large gestures rather than exact values.

## Host notes

### AU, VST3 and Standalone

- **AU** — component `NAGLOED.component`, type `aufx`, subtype `Nagl`, manufacturer
  `UnlN`. Validate with `auval -v aufx Nagl UnlN`.
- **VST3** — `NAGLOED.vst3`, category Fx.
- **Standalone** — runs as a live processor for a microphone or instrument input.
  Configure the audio device in the standalone app's audio settings.
- Mono and stereo plug-in configurations are supported. NAGLOED is an effect,
  not a virtual instrument (`IS_SYNTH` is false); it will appear in effect folders,
  not instrument folders.

### Ableton Live

NAGLOED works as an AU or VST3 insert on an audio track, a return or a group.
Live's own **Freeze** renders the track and is unrelated to NAGLOED's Freeze,
which holds the capture buffer while the track keeps playing. For parallel
treatment, put NAGLOED on a return and send material to it; for one-take
processing, keep it directly on the track and automate the macros.

### REAPER

VST3 is the preferred format; AU also works. NAGLOED can be used on a track or an
item. REAPER's parameter modulation can drive the macros and the LFO-destination
controls for slower, hands-off movement. Remember that automating **Freeze** uses
REAPER's automation, not a MIDI note.

### Preset recall

Presets are loaded through the host's program list. The current program number
and all parameter values are saved with the session and restored on reload.

## Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| No output at all | Mix at 0%, no input reaching the insert, or Freeze on with an empty capture buffer | Raise Mix, check the host's input routing, and feed audio with Freeze off before freezing. |
| Output is very quiet | In/Out trims low; Mix low; Space high and dry grain level low | Raise In/Out, check Mix, raise Density. |
| Grains are inaudible | Density near minimum; Grain too short for the material; Spray/Focus off the material | Raise Density first, then Grain length, then move Focus/Scan. |
| The sound is static | Scan at 0%, Auto at 0%, LFO off | Move Scan or raise Auto/LFO Amount. A static frozen room is a valid sound, but it is a choice. |
| Feedback gets loud or runaway | Feed high and output level not managed | Lower Feed first, then Drive/In; keep Out within headroom. |
| CPU spikes | High Density, long Grain, high Feed, long Ruimte tail | Reduce Density and Grain length; lower Ruimte Size; Freeze does not help CPU. |
| Clicks when automating | Choice parameters or Freeze changing mid-grain | Automate between phrases, not inside a grain or a freeze. |
| Plug-in not listed in the host | Not installed in the scanned plug-in folder, or host cache stale | Build/copy to `~/Library/Audio/Plug-Ins/{Components,VST3}`, rescan, restart the host. |
| AU fails validation | Stale installed component, or build not copied | Run `./scripts/validate.sh`; check `auval -v aufx Nagl UnlN`. |
| No response to MIDI | By design in 1.0.0: no MIDI or MPE input | Use host automation, the macro knobs or the editor. |
| Only Init preset appears | Source tree built without `presets/*.json` | Build from the release tree; presets are embedded at build time. |
| Mono track sounds odd | Host routing, not the plug-in | The plug-in supports mono and stereo; check the track's input configuration. |

## Reference

- Parameter IDs are permanent. Automation, presets and saved sessions address
  parameters by ID, so IDs are never renamed in a release.
- Version: 1.0.0 · Manufacturer: UnlikeNoise Labs · Bundle:
  `com.unlikenoise.nagloed`.
- Design notes and palette: [BRAND.md](BRAND.md).
- Architecture and build internals: [ARCHITECTURE.md](ARCHITECTURE.md).
- Release history: [RELEASE_NOTES.md](RELEASE_NOTES.md).
