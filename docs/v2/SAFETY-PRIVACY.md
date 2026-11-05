# Safety and privacy — light, flicker, visitors and data

> **Status:** research document for the v2 exploration (issue #14), **out of scope
> for v1.0.4**. Not an implementation proposal. This document binds the PoC and
> the hardware protocol in `docs/v2/SPECTRASTROBE.md`; it is neither medical nor
> legal advice.
>
> **Reading guide:** every claim is marked **[evidence]** (directly in a source),
> **[inference]** (substantiated, not directly confirmed) or **[open]** (cannot
> be established; requires measurement, expertise or a lawyer). Source links are
> in §8.

---

## 1. Why this document exists

NAGLOED v2 explores light as a second mode of expression alongside sound:
individual light grains, colour fields and movement in a Ganzfeld headset. Two
risks converge here that do not exist in v1.0.4:

1. **Flickering light close to and around the visual field**, through hardware we
   did not design for it and whose decoder behaves differently from a screen.
2. **Microphone capture of the environment**, including visitors and bystanders,
   in an installation context.

Both are conservatively bounded here. The artistic intent from issue #14 remains
guiding; this document only defines the limits within which that intent may be
carried out. Where artistic desire and safety conflict, safety wins, and the
artistic solution is redesigned.

---

## 2. Photosensitivity and flicker

### 2.1 What the sources say

- About 3% of people with epilepsy have photosensitive epilepsy; among children
  and young people that rises to about 5%, and it is diagnosed less often after
  the age of twenty. **[evidence]** —
  [Epilepsy Society, photosensitive epilepsy factsheet](https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf)
- Common trigger frequencies are between **3 and 30 flashes per second**; some
  people are sensitive up to 60 Hz; below 3 Hz it is uncommon. **[evidence]** —
  same factsheet.
- Of all colours, **saturated red** is the most provocative; that is why the WCAG
  has a separate red-flash threshold. **[evidence]** —
  [W3C, Understanding SC 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html)
- The WCAG threshold (level A) is: **no more than three flashes in any one
  second**, or staying below the general flash and red flash thresholds. A flash
  is a pair of opposing luminance changes of ≥ 10% of the maximum relative
  luminance, with the darker image below 0.80.
  **[evidence]** — W3C.
- Risk factors that increase the likelihood: the effect fills the entire visual
  field, a dark background, tiredness/stress, and patterns that change direction
  quickly. **[evidence]** — Epilepsy Society.
- UK TV rules restrict flashes to 3 Hz or less; the Health and Safety Executive
  advises a maximum of 4 Hz for public performances and clubs.
  **[evidence]** — Epilepsy Society.
- The encoder vendor itself warns that the visual output can produce
  stroboscopic light, that it is not intended for people with epilepsy or any
  other seizure disorder, and that a doctor should be consulted. **[evidence]** —
  [Lumasonic health & safety](https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md)
- The DeepVision headset is intended for **eyes open**, has a **higher maximum
  brightness** and diffuses the light into a colour field; SpectraStrobe content
  appears **much brighter** on DeepVision than on the standard Ganzframes.
  **[evidence]** —
  [Lumasonic devices](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md)
- The standard Ganzframes are intended for **eyes closed**. **[evidence]** —
  [Kasina Quickstart](https://mindplacesupport.com/download/1244/)
- The reference panning in the SpectraStrobe format is an L/R rhythm of
  10–50 Hz. It is not intended as visible light, but if it leaks into the colour
  channels it produces unintended flicker. **[evidence]** spec,
  **[inference]** that this can become visible as flicker.

### 2.2 What this means for NAGLOED

A Ganzfeld headset fills the visual field and sits close to the eyes. That
removes an important mitigation available on a screen: distance and a small
flash area. The WCAG thresholds are intended for screens; for a full visual
field they are a **lower bound**, not a safe upper bound.
**[inference]** — based on the WCAG explanation of the 10° field and the risk
factor "effect fills the visual field" from the Epilepsy Society factsheet.

NAGLOED therefore applies stricter rules than the WCAG minimum threshold. The
quantitative translation to the actual LED output (cd/m², flash duration,
spatial extent) has not been made; that is an **open** point and the reason every
hardware step in `SPECTRASTROBE.md` uses a ladder and an immediate stop.

---

## 3. Hard design rules for light

These rules apply to the encoder, the light engine and the choice of
preset/mode. They are not optional and cannot be disabled by a user without an
explicit, warned action.

| # | Rule |
|---|---|
| R1 | **No repeated flicker by default.** Colour and brightness changes are slow; default luminance modulation below 1 Hz. |
| R2 | **Never more than three flashes in any one second** (WCAG level), and no sustained modulation in 3–30 Hz. This is a hard encoder limit, not a UI recommendation. The prescribed 10–50 Hz panning of the **reference carrier** does not fall under this: it is not visible colour modulation, provided the reference does not leak into the colour channels. That leak is measurement point M5/M6 and a stop criterion. |
| R3 | **Brightness ceiling:** colour channels at most 25% of the reference **per colour channel**, default 6.25% (see `SPECTRASTROBE.md` §6). The ceiling takes precedence over artistic desire. The sum of the three colours in one ear can be higher; that sum is monitored separately. |
| R4 | **Smooth envelopes:** attack/release ≥ 20 ms hard, ≥ 50 ms for artistic events. A single, isolated 20 ms extinction is permitted; repeated fast modulation is not. Smooth envelopes do not make repetition in 3–30 Hz safe; R2 still applies. |
| R5 | **No jump from black to full.** Every light event starts and ends at an intermediate value; fade-in at the start, fade-out at the end. |
| R6 | **Red is extra cautious:** no saturated red flashes; red gets a lower ceiling than green/blue. |
| R7 | **The reference carrier is not an artistic channel.** The 10–50 Hz panning of the reference may not be used or amplified as a visible effect. |
| R8 | **Every session starts dark and low**, even if the previous session went well. No "restore" of the previous brightness without confirmation. |
| R9 | **Children and young people:** extra restraint; photosensitivity is more common in them. Supervision by an adult is a condition. |
| R10 | **Breaks and volume:** a maximum of about an hour continuously, then a break; warn about hearing damage with headphones, in line with the vendor warning. |
| R11 | **Emergency measure for discomfort:** taking the glasses off or covering one eye strongly reduces the effect; the operator must be able to instruct this. |
| R12 | **No "safe" claim.** We communicate "conservative" and "with immediate stop", never "safe for everyone". Where the word "safe" does appear, it concerns **signal integrity** ("signal-safe file"), never health. |

In addition: a **bounded audio-reactive mapping** is at most one optional mode,
never the default and never the conceptual model. The preferred relationship is
event/gesture/memory, not a continuous mapping of audio level to brightness.
Where a mapping is used, it has a ceiling, a rate limit and a dead zone (no light
in silence or at low levels), so that the installation does not flicker
continuously. **[technical solution]**

**Why the 20 ms extinction is allowed.** R4's "artistic" 50 ms is a precautionary
margin for repeated movements; R2 concerns repetition and 3–30 Hz, not a single
isolated transition. The one-off 20 ms extinction in *Eerste Gloed* is therefore
permitted, but remains an **assumption to be verified**: only once the M12
measurement on hardware shows that the decoder adds no extra luminance
modulation is it empirically supported. Without that measurement this is a
deliberate exception, not proven safety. The extinction is furthermore never
repeated within 3 seconds and the colour channels stay below the 25% ceiling.

---

## 4. Immediate stop and operation

### 4.1 Three levels

1. **Software panic (phase B).** An always-reachable stop that brings all
   carriers and colour gains to zero within 50 ms, click-free, without a "final
   flash". The button is visible in the GUI and has a keyboard shortcut; it is
   never hidden behind a menu or setting. This stop belongs to the realtime
   encoder in a later phase; the **offline PoC does not have it** and does not
   claim it. There the primary stop is human (hardware + glasses off).
2. **Hardware.** Kasina brightness to minimum, then power off; disconnect the
   glasses. The Kasina has separate controls for audio volume and LED
   brightness. **[evidence]** — Lumasonic Kasina doc
3. **Glasses off.** The simplest and fastest human action.

### 4.2 Stop requirements

- The stop also works while the audio engine is still processing blocks, if the
  host hangs or if the network fails. The stop therefore does not depend on the
  DAW's transport state. **[technical solution]**
- On signal loss or at the end of the piece, **the colours go to zero first**,
  then the reference. Reason: on sudden signal loss the decoder may hold the last
  brightness; that must not leave a persistent bright image. **[inference]** —
  still to be verified on hardware **[open]**.
- The stop is **tested before every session** and the result is recorded.
- There is one designated operator per session; that person does not watch
  inside the glasses and keeps hold of the stop. **[technical solution]**

### 4.3 Environment and preparation

- Room lit, no other flicker sources (spotlights, projectors, disco lighting) in
  the same visual field. **[inference]** based on the dark-background risk
  factor.
- No use in combination with other stroboscopic sources or in a busy club
  context without a separate risk assessment. **[open]**
- Clear verbal instruction beforehand: what you are about to see, that the light
  can be bright, that you stop immediately on discomfort, and that you may take
  the glasses off.
- No use by people who cannot or will not consent (sleeping, under the
  influence, children without supervision).
- **Hearing and level.** The carriers (18.2–19.7 kHz) are not guaranteed
  inaudible; that depends on age, hearing, transducers and playback level (U15).
  Play at a fixed, recorded level, not "as loud as needed to hear the program".
  Before the session, do a short carrier-only comfort listen (10 s) at the
  intended level. With tinnitus, hyperacusis, young ears or any doubt: lower
  levels or do not take part. This is a comfort and hearing-prevention measure,
  not a medical test.

---

## 5. No medical or wellness claims

- NAGLOED is an **artistic instrument**. It is not a medical device, not a
  therapy and not an instrument for measuring the brain or health.
- We make **no** statements about relaxation, sleep, concentration, anxiety,
  pain, "brainwave entrainment", neurological effects or wellbeing gains.
- The environment (in particular the hardware vendors) sometimes uses
  entrainment language. We do not adopt it in the UI, README, preset names or
  communication. References to the hardware are descriptive, not endorsing.
- Safety communication is risk reduction, not proof of effect. Words such as
  "safe", "heals", "calms" and "improves" are avoided.
- When in doubt about a formulation: describe what the system **does** (light,
  colour, timing), not what it **means for the user**.

**[technical solution]**

---

## 6. Visitors, consent and privacy

### 6.1 Starting point

In an installation NAGLOED can amplify or transform the environment. That
requires microphone capture. This is different from "hearing the environment":
natural hearing falls outside the software; from the moment the software
captures, privacy obligations apply. **[inference]**

Speech is personal data; capturing and storing visitor audio therefore falls
within the scope of the GDPR (purpose limitation, data minimisation, storage
limitation, transparency). Whether and how this is legally precise is
**[open]** and requires legal review, especially for a public installation and
for minors.

### 6.2 Consent and transparency

- Visible notice **before** the capture zone: that sound is being captured, for
  what purpose, how long it exists, and how to avoid it.
- Active consent from the participant using the installation; for children,
  consent from a supervising adult.
- A **mic-off mode** is always available: the installation then works with
  internal sources (noise, drones, previously loaded material). This is not a
  degradation of the concept; issue #14 explicitly mentions internal sources.
- The visitor can stop at any moment and request deletion.
- No capture without the visitor knowing, not even "for atmosphere" or for later
  promotion.

### 6.3 Bystanders

- Bystanders may unintentionally enter the microphone and cannot practically all
  consent. Measures: a clear zone, a directional microphone, notice, a short
  buffer, and an installation not aimed at conversations.
- Processing into grains does not automatically make speech unrecognisable or
  anonymous; fragments may return audibly. We therefore claim **no**
  anonymisation. **[inference]**
- If intelligible speech is a realistic risk: extra damping, a shorter buffer, or
  an "internal sources only" mode as the default. **[open]** what degree of
  abstraction is sufficient in practice.

### 6.4 Temporary storage of captured sound

- By default: **volatile memory only**. The capture ring (12 s in v1) is
  continuously overwritten; nothing is written to disk and nothing is
  transmitted. Freeze holds the buffer only as long as the user holds it.
- No automatic recording, no session logging with audio, no crash dump with
  audio.
- Export is possible only through a **deliberate, visible action** by the
  operator. In that case: store locally, document, and delete after the session
  (proposal: within 24 hours, unless the visitor explicitly agrees otherwise).
- There is a fixed, documented retention period and a fixed deletion routine;
  "temporary" without an end date is not a policy. **[technical solution]**
- Access to any recordings is limited to the operator; no shared folder, no
  cloud, no automatic backup.

### 6.5 Local-only processing

- Processing and playback are **local**. No network, no telemetry, no analytics,
  no accounts, no uploads by default.
- Logs contain no audio, no speech fragments and no personal data; error logs
  are stripped of paths and file names that could point to visitor material.
- A network connection exists only if a future mode genuinely requires it, and
  then solely for the minimal telegram below.

### 6.6 Distributed mode: data minimisation

`docs/v2/ARCHITECTURE-OPTIONS.md` (§2.D) describes a future peer model in which
rooms exchange only events, no audio. If that is ever built, the following
applies:

- **No audio over the network**, not even temporarily or as a "feature".
- Only the minimal telegram: id, kind, time, parameters, seed, cause — no names,
  no voice, no free text, no location of people.
- Timestamps coarse and relative; no continuous stream usable as a profile.
- No persistent history beyond the session; no logs that make visitors
  traceable.
- Only on a trusted local network; no internet exposure without a separate
  security and privacy review. **[open]**
- Participants can refuse the distributed mode without losing the core
  experience.

**[technical solution]**, largely taken from the architecture document mentioned
above.

### 6.7 Privacy design principles (summary)

1. **Data minimisation:** only what is needed for the sound; no identifying
   characteristics.
2. **Local-first:** processing, storage and playback on the machine itself.
3. **Volatile by default:** ring buffer in memory, no persistent recording.
4. **Transparency before capture:** notice and consent, no surprises.
5. **The visitor is always in control:** mic off, stop, delete.
6. **No reuse:** captured sound is not used for promotion, analysis, training or
   any purpose other than the session.
7. **Retention period with an end date** and a demonstrable deletion routine.
8. **Include bystanders** in the design, do not argue them away.
9. **No false sense of safety:** if abstraction does not guarantee
   anonymisation, we say so.
10. **Anything not needed does not exist:** no analytics, no accounts, no cloud.

---

## 7. Open questions

1. **[open]** What are the actual luminance (cd/m²) and flash duration of the LED
   output at the chosen Kasina setting and with DeepVision, and how do they
   relate to the WCAG thresholds? Without that measurement the ratio remains a
   relative safety margin.
2. **[open]** How does the decoder behave when the carriers stop suddenly (e.g.
   on a panic or power loss)? Does the last brightness persist? Determines
   whether the stop order (colours first, then reference) is sufficient.
3. **[open]** How do we verify objectively that the LED output contains no
   unintended 3–30 Hz modulation, even with complex envelopes? A photodiode plus
   analysis has been proposed but not yet carried out.
4. **[open]** What is the legal basis and is a DPIA required for a public
   installation with microphone capture? Who is the data controller, how long
   may material be kept, and how does a deletion request work?
5. **[open]** Can visitor sounds count as anonymous after grain processing?
   Working position: no, unless demonstrated. What is a defensible degree of
   abstraction?
6. **[open]** How do we organise first aid and incident handling? Training of
   operators according to official guidelines from a recognised epilepsy
   organisation; no self-invented medical instructions.
7. **[open]** Is a mandatory age limit or supervision required, and how do we
   communicate that without medical framing?
8. **[open]** Which signage and text work in a busy public space (language,
   legibility, children)?
9. **[open]** Do we include an emergency-stop witness in the installation (a
   second person watching), and if so, how without disturbing the experience?
10. **[open]** Can we ever claim "safe", or does it remain "conservatively
    designed"? Working position: the latter.

---

## 8. Sources

Safety and flicker:

- Epilepsy Society, *Photosensitive epilepsy* (factsheet, 2016):
  https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf
- W3C WAI, *Understanding Success Criterion 2.3.1: Three Flashes or Below
  Threshold* (WCAG 2.2):
  https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html
- ITU-R BT.1702 (reference framework for flash thresholds in broadcasting):
  https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1702-3-202311-I!!PDF-E.pdf
- Lumasonic, *Health and safety*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md
- Lumasonic, *Devices* (Ganzframes, DeepVision, Kasina/Limina):
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md
- MindPlace, *Kasina Quickstart Guide*:
  https://mindplacesupport.com/download/1244/

SpectraStrobe and hardware:

- Lumasonic, *SpectraStrobe format specification*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md
- Lumasonic, *Production and mixing*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md
- Lumasonic, *Kasina*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/08-kasina.md
- `docs/v2/SPECTRASTROBE.md` (measurement plan, tolerances, hardware protocol)

Privacy and framing:

- Regulation (EU) 2016/679 (GDPR), in particular the principles in Art. 5
  (purpose limitation, data minimisation, storage limitation) and the
  information obligations in Art. 12–14:
  https://eur-lex.europa.eu/eli/reg/2016/679/oj
- NAGLOED issue #14: `gh issue view 14 --repo leonvanbokhorst/nagloed`
- `docs/v2/ARCHITECTURE-OPTIONS.md` (§2.D distributed peer model)
- `docs/v2/EXPERIMENT-01.md`
