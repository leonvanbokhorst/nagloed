# Experiment 01 — Eerste Gloed

> **Status:** proposal for one small, separate experiment. Not a v2 feature, no
> integration into the plug-in, no change to NAGLOED 1.0.4. The experiment is
> entirely separate from the release and can safely be removed. It is first of
> all a **material exploration**: it tests whether a *written* kinship between
> one light gesture and one audio gesture is perceptible. It does not test
> whether a system can refuse, remember or forget on its own.

## 1. The question

Can **one shared gesture** — a light grain that fades out and an auditory
response that deliberately does not coincide with it — create a felt kinship
without frame synchrony? And is a deliberate **non-response** to a later light
grain perceivable as composition rather than as absence or error?

Two sub-questions, which must be answered separately:

1. **Kinship:** does a listener/viewer experience a relationship between the
   light gesture and the audio impact 180 ms later, without the two running in
   sync?
2. **Non-response:** does a listener/viewer notice that the 38 s light grain
   receives no answer, and do they read that as intent, as absence or as
   nothing?

## 2. What this experiment is and is not

**Is:** an offline, reproducible composition with a declarative score, its own
renderer, a SpectraStrobe encoder and a verification report. The relationships
are **written**, not emergent: the composer places the events, the renderer
executes them. That is deliberate — the first question is whether the
*experience* of kinship works, not whether an agent takes decisions on its
own.

**Is not:** proof of system autonomy, attention or memory. The "memory" at 46 s
is a synthesised transformation of the impact at 14.18 s; v1's 12 s capture
ring cannot contain that material. If this experiment succeeds musically, the
next question is precisely whether that kind of memory outside the ring is
necessary — a design question, not a settled matter. "One shared gesture" here
means: **one pair** (light extinction + audio impact), not continuous coupling.

## 3. The composition (72 seconds)

Working title: **Eerste Gloed** (first glow). Four moments, with plenty of
space between them.

| Time | Event | Light | Sound | Relationship |
|---|---|---|---|---|
| 0–8 s | Room, almost nothing | No light (reference carrier only) | Very sparse room image, one distant fragment | None |
| 8–14 s | Light grain | Amber grain starts on the left, drifts slowly to the right, becomes slightly brighter | Silent | Light only |
| 14.0 s | Extinction | Light fades out in **20 ms** (R4 minimum) | Silent | — |
| 14.18 s | Response | No new light | Low impact, 180 ms later, different decay (2.5 s), slightly to the right | Kinship, not synchrony |
| 14–28 s | Lingering | Diffuse amber-red field keeps breathing slowly and fades out | The impact is already gone | Light outlives sound |
| 28–46 s | Restraint | One weak light grain at 38 s, fades out by itself | Silent | **Deliberately unanswered** |
| 46–52 s | Memory | Dimmer, slower echo of the first light, different timing | Reversed and lowered fragment, 46.4 s | Recognisable, displaced |
| 52–72 s | Fading out | Slow fade, no final chord | Room falls silent | Silence as ending |

The 180 ms delay is an **artistic choice**, not a latency bug. The audio
envelope does not resemble the light envelope: the light fades out in 20 ms,
the sound has a long decay. They are family, not a copy. The extinction was
extended from 8 ms to 20 ms so that it respects hard safety rule R4; 20 ms is
still perceivable as abrupt. The start of the second light field gets a 50 ms
fade-in (R5: no jump from black to full).

## 4. Technical implementation

1. **Score as data.** `score.json` describes events, colours, panning,
   envelopes, delays and silences. The renderer executes the score with a fixed
   seed; a render is reproducible.
2. **Programme audio.** Sparse, synthesised "found" material, band-limited
   below 15.5 kHz. No real voices, no field recording of people: in this first
   experiment privacy is not a variable. (For later experiments with real
   environments, [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) applies.)
3. **SpectraStrobe encoding.** The reference carrier (18.2 kHz) runs
   continuously with a 20 Hz stereo pan LFO. The colour channels (18.7 / 19.2 /
   19.7 kHz) follow the light grains. The carriers are added **after** the
   audio processing; the programme is band-limited in advance.
4. **Conservative brightness and safety limiting.** Reference −30 dBFS, colour
   channels at most 25% of that per carrier (score: 20%). The encoder clamps a
   score that asks for more, validates the minimum attack/release (≥ 20 ms),
   rejects steps and limits the number of brightness transitions per second.
   For the hardware ladder there are additional variants: **dim** (max 6.25%)
   and **mid** (12.5%). The peak of the entire mix stays below −6 dBFS.
5. **Verification in advance.** The PoC checks carrier frequencies, relative
   amplitudes, isolation between the colours, the 20 Hz pan modulation, the
   band limiting, the headroom, the safety clamp, the envelopes and the
   deterministic re-render. The binding tolerances are in
   [SPECTRASTROBE.md](SPECTRASTROBE.md) §3; this document does not state any
   separate, looser criteria. Latest run: **52/52 core checks PASS, 1 INFO, 0
   FAIL**; the INFO is the honest broadband probe for M7 (the −60 dBFS
   requirement is not demonstrated for broadband v1 material). Only after that
   does anything go to the hardware. See
   `experiments/spectrastrobe-poc/out/verify-report.md`.

## 5. What we measure

### Technical (objective, offline)

The binding measurements M1–M10 and the advisory corruption matrix M14 are in
[SPECTRASTROBE.md](SPECTRASTROBE.md) §3. The output and the latest report are
in `experiments/spectrastrobe-poc/out/verify-report.md`. The PoC proves
signal integrity on this machine; hardware detection (M11–M13) has not
been measured.

### Perceptual — phased, with an explicit distinction

The perceptual test has two phases. Phase 0 is an author test and provides **no
evidence**. Phase 1 is a pre-registered test with independent participants.

#### Phase 0 — author test (Lonn, exploratory)

Lonn watches/listens once, without needing to know the timing in advance, and
afterwards notes in their own words what stood out. Only then come the targeted
questions. This is material exploration: one person, no blinding, no evidence.
The outcome may adjust the composition, but may not carry the conclusion.

#### Phase 1 — pre-registered test (independent participants)

This phase is **only carried out once hardware detection is green** and the
control versions exist. It is fixed in advance:

- **Participants:** at least 5, not the author, no family or direct colleagues.
  Voluntary, informed, no (suspected) photosensitivity; hearing level and age
  recorded. Children only with supervision and only at a lower brightness.
- **Conditions, in randomised order and blind for the participant:** A = the
  score as it is; B = the same score with a 0 ms delay (A1 at 14.00 s); C = the
  same score in which the 38 s light grain does receive an answer; D =
  programme-only, without carriers/light. B and C are **control versions** that
  still have to be rendered; without those controls phase 1 is not valid and
  will not be carried out.
- **Procedure:** first an open question ("What did you notice?"), then the
  targeted items. The participant receives no advance explanation about the
  38 s event or about the expected relationship. The observer records verbatim.
- **Analysis:** for each condition and each question, answers are independently
  coded by two coders. At least one coder is **blind to the condition** and
  codes anonymised fragments; Lonn is the author and does not code alone. The
  coding and the decision rules are fixed before the session. No statistical
  claim at N=5; rather a clear direction and a list of counterexamples.

#### The questions (open first, then targeted)

1. "What did you notice?" (open, without revealing the design)
2. "Did you experience a connection between what you saw and what you heard?
   Describe it in your own words." (open; not a dichotomy)
3. "Was there a moment when you expected something that did not come? What did
   that do?" (open)
4. "How did you experience the long silence?" (open)
5. "What stayed with you?" (open)
6. Only then: "The light grain around 38 seconds received no answer. Did you
   notice that? If so: how did it feel?" (targeted; the design is only now
   revealed)
7. "Would you want to show this to someone else? Why/why not?" (open)

#### Decision rules (proposal, to be fixed in advance)

- **Kinship:** A is described as "kin" more often than B (0 ms) and D
  (programme-only). If A does not rise above B, the delay adds nothing
  artistically; if A does not rise above D, the light adds nothing.
- **Non-response:** at least 2 of the 5 participants mention the missing answer
  at question 3 or 6. If nobody notices it, the non-response condition for this
  run has **not succeeded**; that is an outcome, not a success. If nobody reads
  it as intent and more than half read it as error/absence, the dramaturgy has
  not succeeded.
- **Silence:** the silence must not be described as "malfunction" or
  "emptiness" by more than half. Otherwise the restraint is too long or too
  unclear.
- **No demo feeling:** the question "what stayed with you" does not produce an
  answer consisting solely of effect words ("nice light", "fat reverb"). This is
  a qualitative check, not statistics.

These rules are stricter than "feels good": they can also **fail**. That is the
intention.

## 6. Hardware protocol and safety

The conservative protocol is in
`experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md` and the binding
requirements in [SPECTRASTROBE.md](SPECTRASTROBE.md) §4 and
[SAFETY-PRIVACY.md](SAFETY-PRIVACY.md). In short: dark and low first, Kasina +
standard Ganzfeld frames, 44.1 kHz/16-bit, brightness ladder **dim (6.25%) →
mid (12.5%) → full (20%)**, then only afterwards 48 kHz and DeepVision. The
primary stop is human: stop playback, turn Kasina brightness to minimum, take
off the glasses, or cover one eye. The offline PoC does not have a proven 50 ms
software panic and does not claim one. One person operates, one person watches;
no other flicker sources; no people with (suspected) photosensitivity; hearing
level and consent in advance; no medical or wellness claims.

## 7. Success and failure signals

**Success (after phase 1, not after phase 0):**

- the delay does not feel like an error, and A is described as kin more often
  than B and D;
- the silence is not experienced as a malfunction by the majority;
- the late return is recognisable without being immediately placeable;
- the non-response is noticed by at least 2 of the 5 and read as intent more
  often than as absence;
- the listener describes something other than "an effect".

**Failure (useful information):**

- light and sound feel like two separate demonstrations;
- the delay feels like an error or like arbitrariness;
- the light is too bright, too fast, or the carriers are audible/irritating;
- the audio is too full and the silence cannot carry;
- the brightness is so conservative that the Ganzfeld shows nothing (then it is
  a hardware/brightness problem, not an artistic one);
- nobody notices the non-response, or the majority reads it as a bug;
- the hardware does not recognise the signal (a signal integrity or
  detection problem, not an artistic one).

## 8. What this experiment does not prove

- No realtime plug-in feasibility; this is an offline render.
- No host routing or DAW behaviour; that is a separate technical question.
- No distributed operation, no network, no multiple rooms.
- No AI component.
- No system autonomy or emergent memory; the relationships are written.
- No statement about the definitive architecture of v2.
- No general statement about "audio-visual kinship"; only about this one small
  situation.

## 9. Execution and evidence

The executable form is in `experiments/spectrastrobe-poc/`:

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

That writes to `out/`: the 48 kHz/24-bit mix, the 44.1 kHz/16-bit variant, the
dim and mid variants, a programme-only A/B version, `events.csv`,
`timeline.svg` and a verification report. The hardware protocol is in
`HARDWARE-PROTOCOL.md`.

## 10. Follow-up — only if it does something musically

No v2 plan and no backlog. If *Eerste Gloed* touches something, two small,
separate follow-up questions are conceivable:

1. **Memory and decay** (idea #2 from [IDEAS.md](IDEAS.md)): one fragment that
   returns three times and distorts more each time, with a light that does not
   move with it but changes with it. Question: when does a memory become
   sentimental?
2. **The art of ignoring** (idea #3): ten light events, eight ignored versus
   four ignored, with a pre-registered paired test. Question: does restraint
   feel like intent or like absence?

Only after that, and only with Lonn's explicit permission, is a shared
architecture on the agenda. Not before.
