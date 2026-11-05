# Geborgen Geluid — sharpened artistic vision

> **Status:** living research document accompanying the v2 exploration. This is
> not a v2 specification and not a release plan. NAGLOED 1.0.4 remains the
> stable reference; the existing DSP contracts, parameters and release branch
> remain untouched. The exploration was explicitly opened by Lonn; the
> guardrails from issue #14 still apply.

## 1. Where we come from

NAGLOED 1.0.4 is an instrument of residue: a capture buffer of twelve seconds,
Sintel grains (*sintel* — ember), Ruimte (room/space) and EAR. You play
something into it, the instrument holds on to it, breaks it into embers and
lets them glow on — *nagloed*, the afterglow that remains. *The sound after the
sound.* The instrument grew out of a musical practice of one-off takes,
modular, field recordings and space — not out of an audiovisual concept.

The audiovisual direction came later, as an artistic discovery. Issue #14
preserves that discovery: a world called *geborgen geluid* — sheltered sound —
after the child under the table at a family party. The voices around him are
familiar, strange and passing. The world goes on, but for a moment he is in it
differently.

This exploration builds on that and **sharpens the question**. Not: which
features can we add? But: **how do we turn NAGLOED into an environment in
which sound, light, machines and people can truly surprise one another?** That
question is already the core of issue #14; what is added here is not a new
question, but a sharper elaboration. Four things are genuinely new: the
dramaturgy of non-response (§8), memory and forgetting as compositional
material (§4.3), the attention budget instead of a duty cycle (§4.2), and the
choice to experiment offline first before the plug-in is touched. The rest is
ordering, deepening or repetition of what was already there.

## 2. The core in one movement

NAGLOED is not going to become an audiovisual instrument that translates sound
into light. It **wants** to become an environment with its own memory and its
own attention, in which sound and light can remember, imitate, miss, ignore
and forget one another. That is an intention and a research direction, not a
proven property of the current software.

The world is not completely closed. The real room remains audibly porous; the
system does not need to create an artificial always-on ambient passthrough.
Sometimes it transforms a fragment from that same environment. Sometimes
something sounds that has never been in the room. Sometimes nothing happens.

## 3. From synesthesia to kinship

The danger of audiovisual work is the colour-organ reflex: every bass a red
flash, every peak a white flash. That is illustration, not kinship. It
produces a demo of an effect, not a world.

We seek **family resemblance**: sound and light resemble each other because
they come from the same event, the same memory or the same room — and after
that they live their own lives. A light grain can cause a sound, a sound can
leave behind a colour field that outlives the sound, and two events can be kin
without coinciding.

Concretely, that means:

- **No frame synchrony.** A shared gesture may carry through immediately,
  delayed, probabilistically or not at all.
- **No fixed mapping.** Relationships are composed, not imposed as a function
  of RMS or FFT.
- **But recognisability.** If there is a relationship, a listener must be able
  to feel it — otherwise it is arbitrariness rather than kinship.
- **Two worlds, one family.** Light and sound are not two channels of the same
  signal. They are kin with a shared history.

## 4. Four compositional materials

NAGLOED 1.0.4 composes with sound, time and space. The next layer adds three
kinds of material to that, plus the human as the fourth.

### 4.1 Time: now, later, never

An event can carry through immediately, after a perceivable delay, at an
unpredictable moment, or never. Delay is not latency that has to be optimised
away; it is compositional material. The question is not only *how fast*, but
*when is a response still a response, and when does it become a memory?*

### 4.2 Attention: a budget, not a duty cycle

The system is not obliged to respond to every event. Restraint is not a
parameter that sets a fixed probability, but an attention budget: a bounded
number of interventions per unit of time, per room, per voice. Sometimes the
answer is a silence. Sometimes density is precisely the answer — a thick,
oppressive cloud as an expression of pressure or overwhelm. Contrast between
restraint and abundance is the composition.

### 4.3 Memory: remembering, distorting, forgetting

A fragment can return. Not as a loop, but as a memory: lowered, reversed,
shorter, further away, more colourful or, on the contrary, greyer than the
original. What is remembered is not the same as what happened. Forgetting is
as important as remembering: a fragment that slowly falls out of attention may
genuinely disappear. A memory that always comes back is a preset, not a
memory.

### 4.4 Presence: shared agency

Visitors, performers, instruments and agents can initiate, change, imitate or
ignore events. None of them has full control. The human is not a user driving
the system, and the system is not an animation entertaining the human. It is a
shared situation with multiple voices.

That also means: the system does not have to detect presence in order to
respond to it. A room can behave differently because someone is there, without
cameras or sensors. Presence can also be social and situational: an audience, a
performer, an open door, a late evening.

## 5. The room as co-player

The room is not decoration. In 1.0.4, Ruimte is already an engine of its own; in
this exploration the *physical* room becomes a co-player. A sound can travel
through a room, be imitated by another instance, change and return unexpectedly
somewhere else.

The real environment remains porous in the process: the listener still hears
the room with their own ears. NAGLOED does not need to mute the world to create
shelter. Sometimes the most beautiful event is a fragment that seems to come
from the real room and returns after the silence — but different.

## 6. Light as material, not as display

In this exploration, light is not a visualisation of audio. A light grain has
its own colour (RGB), envelope/brightness, duration, position (left/right, or
two eyes), movement, persistence and timing. It can arise in response to sound,
but then live on independently.

SpectraStrobe, Kasina and DeepVision Ganzfeld are the first concrete
experimentation environment: an output medium, not the conceptual model. The
Ganzfeld turns light into a diffuse field of colour instead of an image of
individual LEDs. That suits *geborgen*: no screen, no pixel, but a room that
changes colour.

The inversion matters: a light event may also cause a sound. A light grain that
shoots away to the left and fades out sharply can make audible a low impact, a
scrape or a released fragment from a capture buffer. The causal direction is
not a given; it is part of the composition.

## 7. The human scale

*Geborgen* means warm and sheltered, not clinical and not therapeutic; here the
word is an artistic and social quality, not a health claim. No wellness,
meditation or entrainment claims are made. There is no medical promise and no
neurological claim. What there is, is an artistic situation.

Care comes with that:

- **Photosensitivity.** Conservative light intensity, slow default, clear
  warning, immediate stop/shutdown.
- **Consent and privacy.** Anyone contributing sound must know what is
  captured, how long it stays and what happens to it.
- **Temporariness.** Captured sound is in principle fleeting. Keeping it is a
  deliberate choice, not a default.
- **Local-first.** No cloud, no silent upload, no profiling.
- **Bystanders.** In a public or shared space, not everyone is a participant;
  the system must also be able to fall silent.

## 8. Contrast as structure

The principles from the assignment are not a list of effects but a
compositional space: high/low, near/far, soft/overdriven, still/moving, slow
evolution/sudden change. Contrast is only musical if it moves towards something
and if the silence afterwards is as important as the event.

That is why a second movement is conceivable that is not in the original
assignment: **the dramaturgy of non-response**. A light grain that receives no
sound. A sound that receives no light. A long silence that does not feel like
an error. If the system always answers, there is no kinship left — only a
mechanism.

## 9. What this explicitly is not

- No colour organ and no VJ tool.
- No one-to-one audio-reactive visualiser.
- No synesthesia machine that couples colour to frequency.
- No always-on ambient passthrough.
- No therapy, meditation or entrainment product.
- No AI inference in the realtime audio pipeline.
- No product roadmap before something musically worthwhile has been discovered.
- No change to NAGLOED 1.0.4, its DSP contracts or its release branch.

## 10. Artistic success criteria

These criteria are a **compass, not a measuring stick**. They are deliberately
qualitative and must be operationalised per experiment into observable
questions, conditions and decision rules (see
[EXPERIMENT-01.md](EXPERIMENT-01.md) §5). Without that translation they are
directional, not testable.

1. **The moment of forgetfulness.** For a moment you no longer notice whether
   the light belongs to the sound or the other way around.
2. **A silence that feels right.** Nothing happens, and that feels like
   composition, not like a malfunction.
3. **A recognisable return.** Something you know comes back, but you cannot
   place it exactly.
4. **Contrast that works musically.** Density or emptiness is a choice with
   weight, not a parameter that happens to be set that way. (Not tested by
   *Eerste Gloed*, which is deliberately sparse; requires a second, denser
   variant.)
5. **Shared agency.** A performer or visitor can cause something without the
   system being obliged to answer. (Not tested by *Eerste Gloed*, which is a
   written score; requires a later, interactive set-up.)
6. **No demo feeling.** Afterwards you do not think "nice effect" but "what was
   that".

## 11. Open artistic questions

These questions are deliberately still unanswered. They steer the experiments.

- Is kinship stronger when light and sound come from the same event, or when
  they are each other's cause?
- How long may a memory last before it becomes sentimental?
- How much silence can an installation, a concert, a living room bear? Is that
  context-dependent?
- Can light have a voice of its own that cannot be traced back to sound — and is
  that desirable, or does it then become decoration?
- What does presence mean if the system does not detect people? Is presence a
  social agreement rather than a sensor?
- When does a non-reaction feel like intent and when like a bug?
- How do sound, light and room relate to the human as the fourth voice, without
  the human controlling everything?

## 12. What this vision asks of technology

Not more features, but a few hard properties:

- An **event/memory model** that can represent delay, probability, repetition,
  forgetting and silence — bounded, deterministic where necessary, and without
  making the realtime DSP dependent on AI.
- A **standalone light world** that can live apart from the audio and yet
  remain kin.
- A **network and room layer** that can later connect multiple instances and
  rooms, without local autonomy disappearing.
- **Signal integrity and safety** as first-class requirements, not as an
  afterthought.
- **Consent, privacy and temporariness** as design material.

The technical elaboration is in
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md),
[TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md),
[SPECTRASTROBE.md](SPECTRASTROBE.md) and
[SAFETY-PRIVACY.md](SAFETY-PRIVACY.md). The first experiment is in
[EXPERIMENT-01.md](EXPERIMENT-01.md).
