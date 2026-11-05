# NAGLOED — brand and design language

Version 1.0.5 · Master Lonn & Little Wan

## The sentence

**NAGLOED** *(Dutch, noun)* — the light that stays in the room after the source is gone.

The tagline is **"the sound after the sound"**. The product is a granular
effect and instrument for macOS: it captures live audio, breaks it into grains
and lets the remains glow. The name is final; the tagline is not a slogan to be
rotated.

## The name

Pronounced **NAH-khloot**. The Dutch *g* is not a hard English *g*; it is the
dry, throaty sound in *loch* with the lips already moving towards *loot*. There
is no exact English sound for it, which is the point. Say it once, then say it
properly.

*nagloed* is a compound: *na* (after) + *gloed* (glow, ember, heat). It is the
word for the light that remains on a wall after the lamp is switched off, the
red in the stove after the fire has died. English calls it an afterglow, which
is accurate but too comfortable. The word keeps the small ache in it.

Usage:

- **NAGLOED** in capitals is the product mark: titles, labels, the plug-in name.
- *nagloed* in italics or plain lowercase is the Dutch word and the idea:
  *the nagloed of a chord*, *a nagloed session*.
- Never "Nagloed", never "NaGloed", never "the NAGLOED".

## Why afterglow is the right metaphor for granular sound

Granulation is usually explained as an effect: a source goes in, something
strange comes out. That is not what this instrument is about.

A short sound has no time to be itself. It arrives, it is loud, it is gone. A
grain engine is a way of refusing the disappearance: hold the material in a
buffer, cut it into embers, and place those embers back into the room at a rate
your ear reads as a texture rather than a rhythm. What you hear is not the
source repeated. It is the source *remaining*.

That is nagloed. The capture buffer is the room. The grains are embers. Freeze
is the moment you decide the room is warm enough and stop letting new air in.
Sintel makes the embers, Ruimte gives them a room, EAR colours them like analog
tape rather than glass. The instrument is about residue, not origin.

It also sets the limits honestly: embers are finite. The buffer holds twelve
seconds. The grain pool tops out at 256 voices. Those limits are part of the
sound — a small room with a fire in it, not an infinite cloud generator.

## The lineage

NAGLOED is built for and with **Master Lonn** (Leon van Bokhorst) and **Little Wan**
(Little Padawan): a duo working from Helmond/Eindhoven with modular synths, analog
gear, one-take live recordings, field recordings and sound art. Leon performed for
many years as **UnlikeNoise**; that solo alias is retired, and NAGLOED belongs to
the Master Lonn & Little Wan practice.

Leon was part of the early team around **Modulab Eindhoven**, the modular/DIY event
platform, but has not been involved for over eight years. The friendship, the
Modulab idea (sound as building blocks, patched by hand) and the wider Eindhoven
underground remain a deep influence; there is no current affiliation.

One-take sessions with **SONICrider** (Jurgen Winkel) shaped the habit: modular and
analog gear captured as it happened, no undo — *Unlike* (2017) and *Connected*
(2017), made when Leon still recorded as UnlikeNoise.
([SONICrider](https://sonicrider.nl/))

The broader sound world includes **EAR**, Jurgen Winkel's label for non-commercial
experimental and ambient electronic music under the phrase *"Emotional Analog
Resonance"*. NAGLOED's analogue stage is also called **EAR**, after the ear that
listens. That is a shared word, not a connection: NAGLOED is **not affiliated
with, endorsed by, or an official product of** the EAR label. The label is simply
part of the scene we love.

And it includes reimagining other people's remains. For **Cities and Memory**'s
*Prison Songs* project — 1950s recordings from the Mississippi State Penitentiary,
recomposed by sound artists around the world — Leon (then recording as UnlikeNoise)
made *Frozen Prisonaire* (2022, 4:03).
([MPDS Audio Archive entry](https://www.mpdsaudioarchive.org/en/suoni/08b-frozen-prisonaire-unlikenoise-reimagined-compositions-leon-van-bokhorst/))

The through-line is one-take thinking: the take is the material, the mistakes are
the material, the room is audible, and what survives the session is the point.
NAGLOED is the studio version of that habit.

A note on what this is not: NAGLOED is not an official EAR product and not a
reissue of anyone's instrument. The people above are credited because they shaped
the ear this instrument was tuned for, not because they endorse a product.

## Naming logic: Sintel, Ruimte, EAR

The three engines are Dutch or Dutch-adjacent words with one meaning each. They
are not acronyms, and they should not be translated in the interface.

| Name | Meaning | Role | Why |
|---|---|---|---|
| **Sintel** | *sintel* — cinder, ember, the solid thing left after burning | Grain engine: capture buffer, grain scheduling, pitch and spray | Grains are embers: small, hot, finite, and best in numbers. A spark flies; an ember stays. |
| **Ruimte** | *ruimte* — space, room, room to move | Space/reverb section | The room the embers glow in. Not a cathedral; a room with a history. |
| **EAR** | the ear (the hearing organ) | Analog character section: drive, bias, filtering, drift, wow, tape, lo-fi, hiss | The part that listens back. Not an acronym, and not a reference to the EAR label; see the lineage note above. |

## Who this is for

Experimental musicians, modular players, sound artists, field recordists,
improvisers and one-take recorders; also film, game and installation sound
people who need texture that behaves like material rather than like a preset.
People who are comfortable with a capture buffer, an LFO destination list and a
manual page of parameters.

It is not for the listener who wants a finished sound in one click. There is an
Init preset and a factory library, but the instrument expects to be played.

## The visual language: a warm room at night

The interface is a dark room with a fire in it. Almost everything is near-black;
light is signal, not decoration. Amber means grain activity. Copper means analog
— the EAR section, the parts with heat and tolerance. Pale ember is for text.
Mint appears rarely, because anything that appears rarely is read as a warning
or a measurement, and that is exactly what it is for.

The grain cloud is the primary image of the instrument. It should show real
behaviour: density, position, age, pitch, stereo spread, brightness. If a visual
does not correspond to something the audio engine is doing, it is decoration
and should not be there.

### Palette

The editor is built from a small set of colours defined in `src/ui/Theme.h`. The
page and plate colours are gradients, not flat fills; the table lists the top and
bottom stops.

| Hex | Name | Use |
|---|---|---|
| `#0B0E14 → #06080C` | Page | The editor background, top to bottom. |
| `#141922 → #0B0F16` | Plate | Panel plates and raised surfaces, top to bottom. |
| `#080B10 → #05070A` | Well | Inset wells and the deepest recesses, top to bottom. |
| `#28303E` | Edge | Panel hairlines and boundaries. |
| `#4A3826` | Edge hot | Warm edge highlight on active panels. |
| `#FF6B1A` | Amber ember | Primary accent: grain cloud energy, active/hot states, focus rings. Never body text. |
| `#9C3D08` | Ember deep | Cooled ember: shadowed heat, secondary grain trails. |
| `#FFB347` | Gold | Secondary light: grain heads, peak indicators, the second step of heat. |
| `#FFE9D6` | Pale ember | Warm white: headlines, selected labels, the product wordmark. |
| `#7CE8C7` | Phosphor mint | Modulation, LFO, measurement, tuning lock. Sparingly: one element per view. |
| `#B87333` | Copper | EAR / analog section accents, hardware metal, tape path. Not a general accent. |
| `#6F4622` | Copper dim | Secondary copper: hardware shadows, inactive analog controls. |
| `#E7E2DA` | Text | Body text on dark. A paper white, never pure `#FFFFFF`. |
| `#8A8F98` | Muted | Secondary labels, units, inactive text. |
| `#565D69` | Dim | Tertiary labels and backgrounds behind text. |
| `#FF4D6D` | Warning | Overload, clipping and fault states. Use only for real warnings. |
| `#2B3240` / `#1C222D` | Line / line soft | Dividers; the soft variant for quiet separators. |

Rules:

- Never put amber and mint in equal amounts. Amber is the instrument; mint is a
  gauge.
- Do not use mint for decoration, success states or "on" indicators. It means
  modulation or measurement.
- Do not use copper outside the EAR section. Copper is a material reference, not
  a brand colour.
- Keep surfaces matte. No glossy highlights, no glass panels, no neon glow. The
  only glow is the grain field and the amber the grain field leaves behind.
- Text on the page (`#0B0E14`–`#06080C`) must be `#E7E2DA` or `#FFE9D6`; use
  `#8A8F98` for secondary labels. Check contrast before shipping a new label
  colour.
- Warning red is reserved for something actually wrong. It is not a mood.

### Typography

The editor draws with the macOS system type stack resolved in
`src/ui/Theme.cpp` — Avenir Next Condensed for display, Avenir Next or
Helvetica Neue for body text, Menlo for numeric micro-labels — so no font files
ship and nothing needs licensing. For print, or for a future build that bundles
type, the brand recommends these open-licensed equivalents.

| Role | Shipped UI (system) | Recommended alternative | Rules |
|---|---|---|---|
| Display / wordmark | Avenir Next Condensed → Avenir Next → Helvetica Neue | Inter Tight (OFL) | NAGLOED always uppercase, tracking approx. +0.08em, pale ember on near-black. Never outline, emboss or shadow; one subtle ember gradient in the wordmark is allowed (see shipped decisions below). |
| UI labels | Avenir Next → Helvetica Neue | Inter (OFL) | Sentence case, 11–13 px at 100% scale. Short nouns. Tabular numerals for values. |
| Body / documentation | System sans (Helvetica Neue, `-apple-system`) | Inter (OFL) | 15–17 px, generous leading (1.5–1.6). |
| Numeric / IDs / debug | Menlo → SF Mono → Monaco | IBM Plex Mono (OFL) | Only for parameter IDs, test output and technical annotation. |

Casing: product and section names are capitalised as written — **NAGLOED**,
**Sintel**, **Ruimte**, **EAR**. Ordinary controls are sentence case — *Density*,
*Grain*, *Freeze*. Do not set whole sentences in capitals. Do not letterspace
lowercase Dutch words; *nagloed* is one word, not a logo exercise.

### Do and don't

Do:

- Leave a lot of dark. The room is part of the instrument.
- Let heat encode data: more grains, more amber; older grains, dimmer.
- Keep geometry quiet: one panel radius (about 12 px), one hairline weight,
  one level of shadow at most.
- Move at the speed of audio. If the sound is still, the picture is still.
- Respect flicker limits: nothing above roughly 3 Hz, no strobe.

Don't:

- No purple/teal synthwave gradients, lens flares, decorative bokeh or fake
  chrome. (The grain field's embers are data, not bokeh: they must track
  `VisualState`.)
- No bitmap textures, VHS overlays, paper grain, distressed type. The grit is in
  the audio path, not the pixels.
- No emoji, no icon soup, no badge clutter, no "revolutionary".
- No literal pictures of modular synths and no depiction of real hardware.
- No pure black (`#000000`) fields and no pure white body text.
- No decorative idle animation. Before the first grain the field may show a
  cold, pre-warmed ember bed that represents the empty engine; it must never
  suggest audio that is not there.

### Shipped interface decisions (1.0)

The 1.0 editor resolved a few open questions; these are deliberate and final for
this release:

- **Tagline** — "THE SOUND AFTER THE SOUND" in the top bar, with the credit line
  "MASTER LONN & LITTLE WAN · EINDHOVEN NL" and "MASTER LONN & LITTLE WAN" in the
  footer (the retired UnlikeNoise credit line is superseded as of 1.0.3).
- **Idle ember field** — a deterministic, pre-warmed dust field is allowed before
  audio arrives (see the don't list); it is not decorative animation.
- **Wordmark** — pale ember with one subtle ember gradient; no outline, emboss or
  shadow.
- **Phosphor base** — `#05070A`, not `#000000`.
- **LIVE indicator** — amber (`#FF6B1A`), not mint. Mint stays reserved for the
  LFO and modulation.
- **Copper** — used only in the EAR section, as a material reference.
- **Micro-labels** — 10 px uppercase mono is the floor at 100% scale; never
  smaller.

## Tone of voice

Precise, tactile, unsentimental, with a little Dutch dry humour. Write like
someone who has spent a day patching and has no patience for a sales deck.

Principles:

1. **Say what the control does, then what it is for.** One sentence each.
2. **Prefer the concrete noun.** Grain, buffer, freeze, ember, room, tape, hiss.
3. **Admit the limit.** "Twelve seconds" is more interesting than "unlimited".
4. **No hype.** No *unleash*, *revolutionary*, *next-generation*, *sonic
   journey*, *game-changing*, *infinitely creative*. If the instrument is good,
   the sentence does not need to perspire.
5. **Dry is funny enough.** Understatement lands; exclamation marks do not.
6. **English text, Dutch concepts.** Keep *nagloed*, *Sintel*, *Ruimte*, *EAR*
   untranslated and unexplained after their first definition.

| Don't write | Write instead |
|---|---|
| "Unleash endless granular possibilities!" | "Capture up to twelve seconds. Freeze it. Play what remains." |
| "Experience a revolutionary sonic journey." | "Feed it a chord. Hold the room." |
| "The ultimate granular solution for modern producers." | "A granular effect and instrument for people who record in one take." |
| "Its innovative engine will transform your sound." | "Sintel turns the capture buffer into grains; Ruimte gives them a room." |
| "Freeze your audio into infinity." | "Freeze holds what is in the buffer. It adds nothing." |

### Terminology

Use these words consistently; they are the vocabulary of the instrument.

| Term | Meaning | Do not use |
|---|---|---|
| *grain* | One short windowed piece of the capture buffer | *particle*, *slice*, *chunk* |
| *capture buffer* | The 12-second ring of recorded input | *sample buffer*, *loop buffer*, *tape* |
| *freeze* | Stop recording new input into the capture buffer; grains keep reading | *hold* (except conversationally), *lock*, *stutter* |
| *afterglow* | The remaining light/sound after the source stops | *residue*, *tail* (tail is a host term) |
| *gesture* | One deliberate performance move — a macro, a freeze, a scan sweep | *automation* (that is host automation) |
| *Sintel*, *Ruimte*, *EAR* | Engine names, always capitalised as shown | translations, acronym expansion in UI |

## Product descriptions

### 60 words (store listing, one paragraph)

NAGLOED is a granular instrument and effect for macOS, built for people who
treat sound as material. Feed it a synth, a voice or a field recording; it holds
up to twelve seconds in its capture buffer, dissolves the sound into grains and
lets the remains glow. Sintel grains, Ruimte space, EAR analog colour. Freeze
the moment. Play the afterglow.

### 200 words (press / PR)

NAGLOED is a granular instrument and effect for macOS, made by Master Lonn & Little
Wan
for sound artist Leon van Bokhorst. It is built around a simple idea: the most
interesting part of any sound is often what happens after the source stops. Feed
NAGLOED a synth, a voice, a bowed string or a field recording and it holds up to
twelve seconds in a capture buffer, then reads it as grains — small, windowed
embers of sound. The Sintel engine schedules up to 256 of them, with control over
density, grain length, spray, scan, pitch, spread, reverse, jitter, window shape,
stereo width, feedback, focus and tuning. Ruimte puts the grains in a room. EAR
colours them like analog gear: drive, bias, filtering, drift, wow, tape, lo-fi
and hiss, with input and output trim and a dry/wet mix. Freeze is the central
gesture: record a moment, freeze the buffer, and keep playing what is left while
new sound no longer enters. A factory preset library, five performance macros, an
LFO and auto-motion cover the long, evolving end of the instrument. NAGLOED is an
AU, VST3 and standalone plug-in for macOS 12 or later. Free software, licensed
under GPL-3.0-or-later, made in Helmond and Eindhoven.

## Sources and credits

Brand research, all verified November 2026:

- [UnlikeNoise biography — Last.fm](https://www.last.fm/music/UnlikeNoise/+wiki) (historical alias)
- [Modulab Eindhoven — SONICrider](https://sonicrider.nl/modulab-eindhoven/)
- [EAR label — SONICrider](https://sonicrider.nl/ear/)
- [SONICrider sessions](https://sonicrider.nl/) (recorded when Leon performed as UnlikeNoise)
- [Frozen Prisonaire — MPDS Audio Archive](https://www.mpdsaudioarchive.org/en/suoni/08b-frozen-prisonaire-unlikenoise-reimagined-compositions-leon-van-bokhorst/)

NAGLOED is designed by Master Lonn (Leon van Bokhorst) with Little Wan, and made by
Master Lonn & Little Wan. Product name, engines and palette are NAGLOED 1.0.5 brand
assets; the software itself is
GPL-3.0-or-later. See [the manual](MANUAL.md) for how the instrument works.
