# NAGLOED v2 — ideas beyond the brief

Artistic research material for issue #14. This file is additive and non-binding: every
thought here is a **hypothesis or an invitation**, not a requirement, not a specification and
not v1 scope. Nothing here asks for implementation, a contract change or a release criterion.
The issue's guardrails apply in full (no one-to-one mapping, no colour organ, no therapy or
meditation claims, no claims about neurological effects).

The ideas below do not repeat the sketches in the issue. Where an idea touches an existing
sketch, the addition is the concrete dramaturgy, the memory or the social form — not the
concept. The starting point remains Master Lonn's musical practice: one-take recordings,
modular, field recordings, noise and space, and the image of the child under the table at a
family gathering.

How to read each idea: idea, why it fits, artistic effect, rough technical shape (one
paragraph, no spec), smallest conceivable test, risk/uncertainty.

---

## 1. The offstage composer

*Hypothesis, not a requirement.*

- **Idea** — The piece is not conceived in the room. A composer (script or AI) works offline,
  outside the installation, and delivers a score: events, rests, refusals and rules for what
  may deviate. On the floor, only the score and the room perform. No model, no inference, no
  network in the chain.
- **Why it fits** — A one-take recording is also a score that writes itself while it sounds.
  The installation then becomes the performance of something that already existed, like the
  conversation at the party that was already under way before the child crawled underneath
  it.
- **Artistic effect** — The room does not feel like a machine that reacts but like someone
  performing something. Visitors can suspect an author without ever seeing one.
- **Rough technical shape** — A score as text/JSON with a small event vocabulary (sound,
  light, rest, refuse, remember, forget), time in seconds, a seed and a validation step. A
  compiler outside the plug-in turns a short brief into a score, renders it deterministically
  to audio and an event trace, and keeps both. The installation only reads a pre-validated,
  read-only score; iteration happens offline (brief → score → render → listening), not during
  the performance.
- **Smallest test** — Three short briefs ("three minutes, one voice, do nothing"; "a room
  emptying"; "two answers that arrive too late"), two renders per score with the same seed,
  byte comparison. Then two listeners blindly match the scores to the briefs. If they succeed,
  there is a signature; if they fail, it is noise.
- **Risk/uncertainty** — A score that fixes too much becomes dead music; one that fixes too
  little is chance again. And "AI" quickly becomes a sticker: the composer must have an
  observable voice or stay away entirely.

## 2. The book of forgetting (*vergeetboek*)

*Hypothesis, not a requirement.*

- **Idea** — Forgetting is not missing data but an active process with rules. Every recorded
  fragment gets an age and a count of how many times it has been retrieved. Every return costs
  detail; some fragments disappear from the index for good. The forgetting curve is a
  composition parameter, not a bug.
- **Why it fits** — Geborgen Geluid is porous and temporary; the child knows that the party
  will end. *nagloed* is what remains, but not everything remains, and what remains is no
  longer the same.
- **Artistic effect** — You hear a memory that mutilates itself. After a few returns it is a
  different place. That is moving without becoming sentimental; the audience can almost point
  at the loss.
- **Rough technical shape** — Per capture segment a memory record (id, first heard, recall
  count, last recall, decay state). Each recall applies a cumulative degradation factor
  (bandwidth, short dropouts, quantisation, pitch drift). A deterministic drop rule removes
  some of the records permanently after a number of recalls or after elapsed time. Everything
  lives in the score and the state, nothing in a random generator per playback; an offline
  render can reproduce it with the same seed.
- **Smallest test** — One eight-second fragment, six recalls in two minutes. Also render the
  sixth recall as a "first recall" (control) and ask three listeners which of the two is the
  old one. Expectation: the degraded version sounds older, but not broken.
- **Risk/uncertainty** — Degradation becomes an effect (LoFi) instead of a memory. The
  difference must lie in the structure — which fragments, when, and what never returns — not
  in the sound.

## 3. The art of ignoring (*de kunst van het negeren*)

*Hypothesis, not a requirement.*

- **Idea** — The system gets the right to refuse. Every event can remain explicitly
  unanswered; refusals are tracked. An ignored light question can later return as an auditory
  shadow, or never. Not responding is a move with memory, not dead probability.
- **Why it fits** — The child under the table hears voices that are not talking to him; the
  world does not respond to him, and precisely that is what makes it safe. The issue says
  "intervening less often than not"; this turns that into a dramaturgy instead of a setting.
- **Artistic effect** — Tension without an event. The audience starts waiting, and the waiting
  becomes the experience. A late response feels like recognition.
- **Rough technical shape** — An event bus with "open questions" per domain. A light event can
  leave an auditory request with a shelf life; the mixer decides not per event but per phrase
  ("we are not answering this minute"). Refused events go into a drawer with an expiry; the
  score can dig them up later or erase them. Everything bounded, no unbounded growth, no
  feedback loop.
- **Smallest test** — Three minutes with ten light events, eight of them ignored; one variant
  with four ignored. Ask listeners which version feels more like "waiting" and which more
  "full". Expectation: eight out of ten gives more tension but also bores sooner.
- **Risk/uncertainty** — The audience does not notice the difference and reads it as broken;
  or the drawer becomes a gimmick. Needed: someone who knows the score and can check whether
  the refusal matches what you hear.

## 4. Counterpoint between the senses

*Hypothesis, not a requirement.*

- **Idea** — Light and sound are two voices in a counterpoint, not a pair. Rules: when one
  speaks, the other rests; they follow each other's motif but not each other's timing; a canon
  across the senses, where light begins and sound follows a phrase later with different
  intervals.
- **Why it fits** — The premise says: related, but not a literal illustration. Counterpoint
  is the musical form of kinship without synchrony. The child under the table also hears the
  room as two layers that do not parrot each other.
- **Artistic effect** — A conversation between two media that know each other but do not
  imitate each other. The space becomes polyphonic; you feel that something has been composed
  rather than coupled.
- **Rough technical shape** — A small rule system with two voices, each with a motif
  (intervals, colour progression, envelope shape) and a transformation per phrase (delay,
  inversion, retrograde). A shared clock for phrases only, not for samples. The score notates
  the voices and the rules; the engine performs them. No per-sample coupling, no mapping
  table.
- **Smallest test** — Sixty seconds of canon: light begins with a motif of three gestures,
  sound follows four seconds later with the same rhythmic shape but a different pitch, then
  inversion. Ask listeners whether they hear a connection without being able to say what it
  is. Nobody hears anything: too subtle. Everyone says "sync": too literal.
- **Risk/uncertainty** — Light has no pitch; the rules can feel arbitrary. And an exercise is
  not yet a piece: the counterpoint must audibly go somewhere.

## 5. The lamp gets bored (*de lamp verveelt zich*)

*Hypothesis, not a requirement.*

- **Idea** — The light rig is a performer with a state of its own: restless, content, bored,
  attentive. That state comes from history — how long has nothing happened, how much has been
  ignored — not from a random generator. A bored lamp initiates a gesture of its own; a
  content lamp allows silence.
- **Why it fits** — The installation is a presence in the room, not a projection of sound. The
  child under the table also knows the lamp that comes on when nobody is talking.
- **Artistic effect** — The light feels wilful. Visitors start "reading" the light and get the
  feeling that the room has an opinion of them: not reacting, but present.
- **Rough technical shape** — A small state machine with four or five states, slow transitions
  and a drive that grows with silence. Initiative only if the state allows it and the
  attention budget permits it. All decisions in the score and the state, reproducible with a
  seed; no model, only rules and thresholds.
- **Smallest test** — Ten minutes without audio input. Watch whether the lamp makes five to
  eight gestures of its own that differ from one another. Ask three people afterwards to
  retell the order: if they can reconstruct a story without it feeling random, there is agency
  in it. Do not claim it is alive.
- **Risk/uncertainty** — Anthropomorphism turns saccharine, and "boredom" is hard to
  distinguish from randomness. The state must have consequences — a bored lamp makes shorter,
  brighter gestures; a content lamp slow ones — otherwise it is decoration.

## 6. Weather and event (*weer en gebeurtenis*)

*Hypothesis, not a requirement.*

- **Idea** — Two timescales: a slow "weather" (density, damping, brightness, pressure) and
  sudden events. The weather changes not the sound of an event but its meaning: the same
  impact sounds different in fog than in clear light. Weather is dramaturgy, not an LFO.
- **Why it fits** — The practice is one-take, modular, space; a room has a mood that shifts
  slowly. The child under the table feels the mood of the party sooner than the words.
- **Artistic effect** — Listeners get the feeling that a day passes, even though the piece is
  short. Events feel meaningful because they fall into a state.
- **Rough technical shape** — A weather state with three or four slow dimensions that moves by
  rules and influences events only through thresholds: which events are allowed, which
  colours, which space, which density. The events themselves come from the score; the weather
  is exported as a second track so you can inspect it separately.
- **Smallest test** — The same five events in two weather conditions (low and dense versus
  high and empty), ninety seconds each. Ask listeners to say in one word what the space is,
  not which version is more beautiful. If the words differ systematically, the weather is
  working as dramaturgy.
- **Risk/uncertainty** — Weather simply becomes modulation (Auto, LFO) and then it is nothing
  new. The difference lies in the coupling to meaning — which events are admitted — not in
  continuous sound movement.

## 7. The attention budget (*het aandachtsbudget*)

*Hypothesis, not a requirement.*

- **Idea** — Instead of a chance per event, the system has a budget: per minute the room may
  do a limited number of things. You can spend the budget or save it; unused budget builds a
  debt that is repaid at an unexpected moment. This produces clusters and silences that feel
  composed.
- **Why it fits** — "Intervening less often than not" is a quantity you can hear, but a budget
  turns restraint into a form with memory. A guest at a party also talks in bursts, and then
  falls silent again.
- **Artistic effect** — The audience learns the rhythm of the room: a long silence, then a
  concentration of events. That feels like someone who has something to say, not like a
  generator.
- **Rough technical shape** — A budget meter per minute with a maximum and carry-over, plus a
  priority per candidate event. The score marks what is "expensive"; the scheduler spends
  until the budget is gone. Everything deterministic and exportable as an event trace. No
  complex planning, just a counter and a queue.
- **Smallest test** — Two one-minute traces with the same average density: Bernoulli versus
  the budget model, rendered with the same sound. Ask listeners which trace sounds "intended".
  Repeat with five people; statistics alone say nothing here.
- **Risk/uncertainty** — The scheduler can become greedy and spend everything in the first
  second, or the budget is invisible. The art lies in the rules for saving and spending, for
  example a minimum of silence after a spend. It must not become a game.

## 8. Presence without images (*aanwezigheid zonder beelden*)

*Hypothesis, not a requirement.*

- **Idea** — The room knows how many people are there and how they move, without cameras and
  without storing audio: only a few scalars (density, flux, silence) from the microphone. More
  people does not mean more sound but a different regime: a fuller audience asks for more
  restraint, or more party — the room may choose. A physical sign (a card on the table) turns
  listening into an agreed state.
- **Why it fits** — The family gathering is full of people; the child under the table is
  surrounded but not addressed. No face recognition, no recording, no names: that suits the
  intimate and temporary nature. The card makes consent visible without surveillance.
- **Artistic effect** — The installation feels social without looking. Visitors notice that
  the room is different when there are many of them, without being able to point at what is
  "smart" about it. The card provides a small ritual: you put it down and the room starts
  listening.
- **Rough technical shape** — Audio-only occupancy estimation (energy, spectral flux, voice
  activity) yields two or three scalars, no recording and no retention. A state rule
  "many/few/empty" influences the dramaturgy. A physical token switches listening mode with a
  fixed opening and closing action. No cameras, no names, no storage beyond the session.
- **Smallest test** — Thirty minutes of microphone in a room with zero, two and six people
  alternating; log only the scalars and compare with a manual count on paper. Then one
  twenty-minute session with the card on the table and afterwards only words, no data.
- **Risk/uncertainty** — The estimate is unreliable and may feel like surveillance if people
  catch on; the card can become a thing instead of an agreement. The biggest gain is perhaps
  the ritual, not the sensing.

## 9. Poste restante

*Hypothesis, not a requirement.*

- **Idea** — Two or more rooms are not a network but correspondents. They send each other
  short telegrams about what happened — no audio, only a description or id — and the receiver
  may interpret them as it likes. A room can refuse a telegram or put it in a drawer and open
  it much later. Undeliverable messages keep haunting the place.
- **Why it fits** — A party has several rooms; the child under the table hears the kitchen and
  the living room as two worlds that talk to each other through doors. Delay and
  misinterpretation belong to memory.
- **Artistic effect** — The rooms feel like neighbours writing each other letters. An event in
  room A lands in room B much later, and differently. Visitors can walk from room to room and
  hear the correspondence.
- **Rough technical shape** — Telegrams with a minimal set of fields (id, time, kind, duration,
  colour or description, seed), no audio. Bounded queue, rate limit, per-receiver rules for
  opening, postponing or refusing, and a dead-letter drawer with expiry. Each room is locally
  autonomous; no shared clock. Only as a separate experiment, not in the plug-in.
- **Smallest test** — Two laptops, one telegram per minute, an artificial delay of five
  seconds and twenty percent loss. Listen whether the rooms feel like correspondents or like a
  broken copy. One telegram goes "poste restante" and is opened only after five minutes.
- **Risk/uncertainty** — This is as good as a new project; scope explosion. The art lies in
  the delay and the misinterpretation, not in the protocol. If it asks too much technically,
  do it with two people and a telephone.

## 10. The almost-event (*de bijna-gebeurtenis*)

*Hypothesis, not a requirement.*

- **Idea** — Some events are prepared and then aborted: a light grain that reaches seventy
  percent of its brightness and goes out, a sound that begins and is withdrawn. The audience
  sees the shadow of an event. Almost-events form a category of their own in the score, with
  their own rules: how often, how close to "done".
- **Why it fits** — The child under the table often hears the adults begin a sentence that
  does not finish. Geborgen Geluid is full of unfinished things. Instead of yet another event,
  this is the dramaturgy of the postponed and the left unsaid.
- **Artistic effect** — More tension than a complete event; visitors fill in the almost-event
  themselves, which makes the piece personal. A series of almosts builds an expectation that
  is never paid out, or once.
- **Rough technical shape** — Events with an abort point and an envelope that stops before the
  peak. The engine plays the rise and stops at a fraction of the duration; what is measurable
  is how much of the envelope was reached and how often. Light: a short, incomplete rise.
  Sound: a grain that is triggered but not developed. Deterministic and exportable.
- **Smallest test** — Sixty seconds with six complete and six almost-events. Ask listeners
  afterwards to reconstruct the order. If they name the almosts as missed moments, it works;
  if they hear them as glitches, it has gone too far.
- **Risk/uncertainty** — The boundary between "almost" and "broken" is narrow and differs per
  device. Light may have no perceptible rise; then "almost" applies only to sound and the idea
  must become narrower.

---

## Cross-connections

- 1 and 9 share the thought that an event is a message that can change on the way.
- 2, 3 and 10 are all three variants of the same material: what does not happen, what is
  refused and what happens halfway. They can be tested as one dramaturgical family, but not as
  one feature.
- 6, 7 and 8 touch each other in the slow state that changes the behaviour of fast events; 8
  adds the social layer.
- 4 and 5 are about ownership of the light: one through rules, the other through state.

## Status

Everything in this file is a hypothesis or an invitation. No idea is a v1 requirement, a
contract, a parameter or a release criterion. Anyone who wants to try something here does so
in a separate experiment outside the plug-in, with its own small test, and only after explicit
activation by Lonn.

## Review note (after independent review)

The independent review found #2 (**The book of forgetting**) and #3 (**The art of ignoring**)
the strongest and most testable ideas; those two are the designated candidates for a second
experiment. #5 (the bored lamp), #10 (the almost-event) and #8 (presence without images) were
judged weaker: #5 and #10 as effect tricks, #8 because the sensing is unreliable and
privacy-sensitive while the strongest part — the card on the table as ritual — needs no
sensing at all. That is a note, not a deletion: the ideas remain invitations, but they are now
prioritised. See [REVIEWS.md](REVIEWS.md).
