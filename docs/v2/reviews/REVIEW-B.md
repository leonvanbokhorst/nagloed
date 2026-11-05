# Review B — Artistic intent, safety and scope

> Editorial note: this review was originally written in Dutch. It has been translated to English for the canonical documentation language; the Dutch original is preserved in git history (commit 58b1cd8).

**Reviewer:** reviewer-b, independent (not involved in producing this exploration)
**Date:** 5 November 2026
**Branch / checkout:** `feature/v2-exploration` (shared checkout)
**Read and assessed:** issue #14 (`gh issue view 14 --repo leonvanbokhorst/nagloed`), `docs/v2/README.md`, `ARTISTIC-VISION.md`, `IDEAS.md`, `EXPERIMENT-01.md`, `SAFETY-PRIVACY.md`, `SPECTRASTROBE.md`, `DISTRIBUTED.md`, `ARCHITECTURE-OPTIONS.md` and `TECHNICAL-UNCERTAINTIES.md` (as far as needed for scope and consistency), and `experiments/spectrastrobe-poc/` including `README.md`, `score.json`, `HARDWARE-PROTOCOL.md`, `out/events.csv`, `out/timeline.svg` and `out/verify-report.md`.
**Method:** document analysis plus an independent render and verification in `/tmp/review-b-out` (not `./run.sh` in the shared directory). Results: render exit 0, verify exit 0, 31 PASS / 0 FAIL / 0 SKIP, deterministic. The score check `score-L1-extinctie` confirms 8 ms as PASS there; that is relevant evidence for this review, not a detail.
**Limitations:** the Kasina and DeepVision Ganzfeld are not available, so M11–M13 were not measured; there was no conversation with Lonn; `render.py` and `verify.py` were not audited line by line, only for safety- and claim-relevance. This review concerns intent, safety, ethics and scope; technical signal integrity is reviewer-a's territory.

---

## Verdict

**REQUEST CHANGES.**

The exploration is not empty repackaging: the dramaturgy of not answering, memory/forgetting as compositional material, the attention budget, and the choice of an offline renderer before any plug-in change are genuine sharpenings of issue #14. R1–R12 are concrete, the sources are serious, and the tone is largely sober.

Yet this package cannot go to the next gate (hardware) in its current form. There are two blockers:

1. **The PoC violates its own hard safety rules, and the verification approves that violation.**
2. **The perceptual test is set up in such a way that it cannot answer the central question; the success criteria are partly unfalsifiable.**

In addition there are six majors, mainly concerning the executable safety of the first hardware session, the experiment's evidential claim, the guardrail against audio-reactive mapping, and scope (the exploration has become a de facto roadmap and specification for work that issue #14 explicitly does not want yet).

---

## Blockers

### B1 — The PoC breaks R4 and R5; the verification codifies this as PASS

`SAFETY-PRIVACY.md:103` (R4) literally says: "**Gladde envelopes:** attack/release ≥ 20 ms hard, ≥ 50 ms voor artistieke events." ["**Smooth envelopes:** attack/release ≥ 20 ms hard, ≥ 50 ms for artistic events."] And `SAFETY-PRIVACY.md:104` (R5): "**Geen sprong van zwart naar vol.** Elke lichtgebeurtenis start en eindigt op een tussenwaarde; fade-in bij aanvang, fade-out bij einde." ["**No jump from black to full.** Every light event starts and ends at an intermediate value; fade-in at the start, fade-out at the end."]

The central event of *Eerste Gloed* is, however, an extinction of 8 ms:

- `EXPERIMENT-01.md:40`: "| 14,0 s | Extinctie | Licht dooft abrupt (8 ms) | Stil | — |" [table row: extinction — light extinguishes abruptly (8 ms) — silence]
- `experiments/spectrastrobe-poc/score.json:90`: `"env": [[0.0, 0.0], [1.2, 0.55], [3.5, 0.75], [5.4, 0.95], [5.992, 1.0], [6.0, 0.0]]`
- `score.json:93`: `"extinction": "abrupt, 8 ms"`
- `HARDWARE-PROTOCOL.md:53`: T1 plays "lichtgrain + 8 ms-extinctie + 180 ms-impact" ["light grain + 8 ms extinction + 180 ms impact"] as the first exposure.

Worse than a loose inconsistency: `verify.py:498-504` turns "≤ 10 ms" into an explicit PASS criterion ("L1-extinctie is abrupt" ["L1 extinction is abrupt"], `0.0 < ext <= 0.010`), and `out/verify-report.md` reports "score-L1-extinctie ... PASS ... 8 ms". The verification thus rewards precisely what the binding safety rule prohibits. Nowhere is there an exception, justification or measurement that legitimises the 8 ms; I searched all docs for one and found only the design description, no trade-off.

There is a second, unrecognised discontinuity. `score.json:101` has L2 start with `"env": [[0.0, 0.35], ...]`. `render.py:76` initialises the envelope at the first value (`arr = array.array("d", [pts[0][1]]) * count`), so at t = 14.0 s the colour carrier jumps from 0 to 0.35 in a single sample. That is a step, and `SPECTRASTROBE.md:321-322` requires "gladde attacks/releases, **≥ 20 ms**, geen stappen of discontinuïteiten" ["smooth attacks/releases, **≥ 20 ms**, no steps or discontinuities"]. None of the offline checks (M1–M10) measures this; M12 targets periodic 3–30 Hz modulation and would not catch a one-off step as such either, and M12 was not performed.

Third: `SPECTRASTROBE.md:337-339` promises that "De encoder begrenst zichzelf, ook als de artistieke score meer vraagt: max 25 % koppeling, geen herhaalde luminantiemodulatie in 3–30 Hz ..." ["The encoder limits itself, even if the artistic score asks for more: max 25% coupling, no repeated luminance modulation in 3–30 Hz ..."]. In `render.py` no such self-limiting exists; the amplitude follows `score["light"]["events"]` directly (see `render.py:321-352`), and the only ratio check sits afterwards in `verify.py`. A score that asks for 25% or a 3–30 Hz flicker pattern is therefore not stopped by the encoder. That makes the safety claim in the encoder document unproven.

What must happen before the hardware gate:

- Choose explicitly: either 8 ms becomes a documented, justified exception to R4 (with a prior LED measurement on hardware, M12, and a signed trade-off), or the extinction becomes ≥ 20 ms. Twenty milliseconds is still perceptibly "abrupt"; the intent need not fail. The current combination ("hard, not optional" in one document, "8 ms" in the other) is untenable.
- Restore the L2 start to a fade-in from 0 and add an automated envelope check (minimum attack/release, no steps) to the verification, instead of a check that rewards a too-short release.
- Implement the promised encoder limiting or remove the claim from `SPECTRASTROBE.md` §6.
- Remove or correct the claim in `EXPERIMENT-01.md:67-68` that there is "een helderheidsladder in het hardwareprotocol" ["a brightness ladder in the hardware protocol"]. `HARDWARE-PROTOCOL.md:45-58` contains T0–T6 but no brightness or coupling ladder; the binding ladder is in `SPECTRASTROBE.md` §4.2 (lines 217-237: 0, 1/64, 1/32, 1/16, 1/8, 1/4 of the reference) and is skipped in the executable protocol for *Eerste Gloed*. The first full exposure is therefore immediately the full 20% coupling, while R3 names "standaard 6,25 %" ["default 6.25%"] (`SAFETY-PRIVACY.md:102`) and the PoC uses 20% (`experiments/spectrastrobe-poc/README.md:37`, `score.json:138`).

### B2 — The perceptual test cannot answer the question; criteria are not falsifiable

`EXPERIMENT-01.md:92` calls the perceptual phase "Perceptueel (Lonn, met hardware)" ["Perceptual (Lonn, with hardware)"] and line 94: "Vijf vragen, gevraagd na één keer kijken/luisteren in dim licht." ["Five questions, asked after one viewing/listening session in dim light."] That is one listener, N = 1, and that listener is the author/commissioner who knows the score. There is no blinding, no independent participant, no second measurement and no pre-specified analysis. This is therefore an author test, not a test of "ervaart een luisteraar verwantschap of illustratie?" ["does a listener experience kinship or illustration?"] (`EXPERIMENT-01.md:14-15`).

Moreover, the five questions are leading:

- `EXPERIMENT-01.md:97-98`: "Merkte je dat het licht en het geluid niet synchroon liepen — en stoorde dat, of maakte het de relatie interessanter?" ["Did you notice that the light and the sound were not synchronous — and did that bother you, or did it make the relationship more interesting?"] It presupposes that the asynchrony was noticed and offers only an evaluative judgement.
- `EXPERIMENT-01.md:99`: "Wat deed de stilte tussen 28 en 46 seconden?" ["What did the silence between 28 and 46 seconds do?"] It presupposes that the silence was experienced as a demarcated thing.
- `EXPERIMENT-01.md:100-101`: "Merkte je dat het lichtgrain om 38 seconden geen antwoord kreeg? Zo ja: voelde dat als intentie of als gemis?" ["Did you notice that the light grain at 38 seconds received no answer? If so: did that feel like intention or like something missing?"] This reveals the design and the target behaviour, and then asks for the interpretation.
- `EXPERIMENT-01.md:96`: "Waren licht en geluid voor jou verwant, of leken het twee losse dingen?" ["Were light and sound related for you, or did they seem like two separate things?"] A forced dichotomy without a middle category.
- `EXPERIMENT-01.md:102`: "Wat bleef er hangen: het licht, het geluid, de stilte, of niets?" ["What stayed with you: the light, the sound, the silence, or nothing?"] A closed list of options.

The cueing starts even earlier: the observation table in `HARDWARE-PROTOCOL.md:76` asks per step "Onbeantwoord grain om 38 s opgemerkt" ["unanswered grain at 38 s noticed"], and only afterwards (`HARDWARE-PROTOCOL.md:81`) come "de vijf perceptuele vragen ... na afloop" ["the five perceptual questions ... afterwards"]. The observer therefore already knows that there is an unanswered grain before being asked whether they experienced it as intention or as loss.

The criteria are not falsifiable:

- `EXPERIMENT-01.md:115`: "het niet-antwoord van 38 s opvalt (of juist productief onopgemerkt blijft)" ["the non-answer at 38 s is noticed (or is productively left unnoticed)"]. Both outcomes count as success; this makes the non-answer condition by definition irrefutable. The failure list (`EXPERIMENT-01.md:118-125`) contains no failure condition for the non-answer.
- The other success criteria are not operationalised: "de vertraging voelt niet als fout", "de stilte voelt als compositie", "de terugkeer is herkenbaar zonder plaatsbaar te zijn" ["the delay does not feel like an error", "the silence feels like composition", "the return is recognisable without being placeable"] — no scale, no threshold, no minimum number of observers, no comparison condition.
- Two of the six artistic success criteria from `ARTISTIC-VISION.md:180-191` cannot in principle be tested with this experiment: "Contrast dat muzikaal werkt" (lines 186-187) and "Gedeelde agency" (lines 188-189) ["Contrast that works musically"; "Shared agency"]. *Eerste Gloed* is uniformly sparse (the render logs "programma 7,4 s van 72,0 s (90% stilte)" ["program 7.4 s of 72.0 s (90% silence)"]) and contains no performer or system that decides anything itself.

What is missing is a real control group. The same score with 0 ms delay, the same score in which the grain at 38 s does receive an answer, and a version in which the pairs are placed randomly: without such an A/B, "kinship" cannot be distinguished from prior knowledge, expectation or production effect. The only comparison that currently exists is T6 (`HARDWARE-PROTOCOL.md:58`): listening only, without Ganzfeld. That tests whether the audio stands on its own, not the relationship.

The repetition is not a control either: `EXPERIMENT-01.md:104` says "twee keer kijken (de tweede keer wetend wat er komt)" ["watch twice (the second time knowing what is coming)"]. That is an expectation effect, not a repeated measurement; and because the hardware protocol has already played T0–T6 before it, the "second time" is in practice the seventh exposure.

What must happen before the hardware gate (and preferably before any conclusions are drawn):

- Pre-specify an evaluation plan: number of listeners (at least 5, not the author), randomisation and blinding, one or two control variants, a structured scale plus free observation, and a decision rule per criterion.
- Reformulate the questions without revealing the design: first open ("Wat merkte je op?" ["What did you notice?"]), then targeted items, and ask the 38 s question only after asking what was noticed at all.
- Separate the observation table in the hardware protocol from the perceptual questions (first observe, then ask the target questions).
- Make the non-answer condition falsifiable with the paired condition (38 s grain answered vs. unanswered) and a pre-specified distinction between "intention" and "loss".
- Clean up the success/failure list: no "or else productively unnoticed" as success, and a perceptible threshold per criterion.
- If this is consciously a demo and not a test: say so, remove the success criteria and do not claim that the question was tested.

---

## Majors

### M1 — The first hardware session is not tightly enough secured

R1–R12 are well thought through, but the executable protocol has gaps that matter precisely in this first session.

**Stop.** `HARDWARE-PROTOCOL.md:48` requires "De mute/stop-handeling moet binnen 50 ms werken; test dat eerst met de hand." ["The mute/stop action must work within 50 ms; test that first by hand."] With an offline WAV, the only "stop" is stopping the player; that cuts off the carriers abruptly. There is no rendered stop variant with a click-free 50 ms ramp, and M13 (`SPECTRASTROBE.md:179`) requires a photodiode measurement that is missing from the short protocol. At the same time, `SAFETY-PRIVACY.md:140-142` records as an open question that the decoder may hold "een laatste helderheid" ["a last brightness"] after a sudden signal loss. The "direct stop" from `README.md:251-254` is therefore a human fallback (glasses off, Kasina off), not a tested software stop. That is allowed, but then the protocol must name it explicitly as the primary stop and must not pretend that the 50 ms claim already works.

**Consent and age.** `HARDWARE-PROTOCOL.md:16-28` mentions exclusion of photosensitivity and an always-permitted stop, but no information/consent from the person under the glasses, no age check and no supervision requirement. `SAFETY-PRIVACY.md:108` (R9) requires adult supervision for children and `SAFETY-PRIVACY.md:155-156` excludes people who cannot or will not consent. The short protocol does not repeat that. For a solo session by Lonn this is theoretical, but the protocol is presented as a generic document.

**Hearing and the carriers.** `experiments/spectrastrobe-poc/README.md:46-47` states: "De dragers zijn onhoorbaar hoog (18,2–19,7 kHz)" ["The carriers are inaudibly high (18.2–19.7 kHz)"]. That is precisely the assumption that `README.md:214` (U15) and `TECHNICAL-UNCERTAINTIES.md:55` mark as **open**, with the explicit warning that 18–20 kHz at loud playback can be "hinderlijk of gehoorschadig" ["annoying or damaging to hearing"]. The PoC is very quiet at −17.4 dBTP while the reference sits continuously at −30 dBFS; in the 90% silence the carrier cluster is the only signal. A listener who wants to hear the programme well turns up the volume and thereby also amplifies the carriers. There is no playback-level rule, no hearing test with the carriers alone, and no exclusion of tinnitus/hyperacusis or young ears. R10 (`SAFETY-PRIVACY.md:109`) mentions hearing damage via headphones only. Add a separate 10 s carrier-only auditory comfort test at the intended level, plus a level and exclusion rule, before the work is played.

**Emergency instruction.** R11 (`SAFETY-PRIVACY.md:110`) mentions "bril af of één oog bedekken" ["glasses off or cover one eye"]; that second, fast action does not appear in `HARDWARE-PROTOCOL.md:22-28`. The environmental rule "ruimte verlicht, geen andere flikkerbronnen" ["room lit, no other flicker sources"] appears in the short protocol only in the now-outdated condition "Zolang dat document niet bestaat" ["As long as that document does not exist"] (`HARDWARE-PROTOCOL.md:18-20`); since `SAFETY-PRIVACY.md` exists, that instruction is no longer self-evident there.

### M2 — The vision is a genuine sharpening, but the claim "the question changes" is too strong; the criteria are directional, not testable

`ARTISTIC-VISION.md:22-24` says: "Deze verkenning bouwt daarop voort, maar verandert de vraag." ["This exploration builds on it, but changes the question."] That is too strong. The question "how do you compose kinship without illustration, without frame synchrony, with restraint as the default" is already issue #14's question. What is genuinely new is limited but valuable:

- the explicit dramaturgy of not answering (`ARTISTIC-VISION.md:158-162`),
- memory/forgetting as compositional material (`ARTISTIC-VISION.md:82-88`),
- the attention budget instead of a duty cycle (`ARTISTIC-VISION.md:73-80`),
- the offline renderer as the first, safe step (`README.md:171-178`),
- a codified safety framework.

The rest is elaboration, repetition or a different ordering of issue #14. That is not a problem in itself, but the documents sometimes present the elaboration as a new artistic truth. The six artistic success criteria (`ARTISTIC-VISION.md:180-191`) are a compass, not criteria: "Het moment van vergeetachtigheid", "Een stilte die klopt", "Contrast dat muzikaal werkt", "Geen demo-gevoel" ["The moment of forgetfulness"; "A silence that is right"; "Contrast that works musically"; "No demo feeling"] have no perceptible definition, no scale and no falsification condition. Say so, or operationalise them per experiment; as written they suggest a measurability that does not exist.

The tone is largely faithful to NAGLOED/BRAND: Dutch, concrete, no marketing, no "unleash" language, and the technical documents are precise and sober. But the framing documents drift towards manifesto and self-praise: "een omgeving met een eigen geheugen en een eigen aandacht" ["an environment with its own memory and its own attention"] (`ARTISTIC-VISION.md:28-30`), "de wereld gaat door, maar je bent er even anders in" ["the world goes on, but for a moment you are in it differently"] (`README.md:54-56`), "Dit is de artistieke kern van issue #14 en het minste risico voor v1.0.4" ["This is the artistic core of issue #14 and the least risk for v1.0.4"] (`README.md:176-178`), and the claim that the exploration has already been independently reviewed (`README.md:308-310`). `docs/BRAND.md:232-245` asks for "precise, tactile, unsentimental ... no hype"; the technical documents meet that, the vision documents do not always.

### M3 — The experiment tests a written kinship, not the memory, attention or non-answering of a system

This is the most important substantive weakness after B2. The relations in the PoC are annotations, not behaviour:

- `score.json:131-135` describes R1–R4 as relations, but A1 is hard-coded at `"start_s": 14.18` (`score.json:56`); the renderer does not derive that timing from L1. The "response" is therefore a manual placement.
- `score.json:116` says of L3: "opzettelijk onbeantwoord: geen audio, geen latere terugkeer" ["deliberately unanswered: no audio, no later return"]. That is an absence the composer left out, not a decision by a system with an attention budget.
- The "memory" is 38 s after L1 (8.0 → 46.0) and 32.2 s after the start of A1 (14.18 → 46.4). The 12 s capture ring (`TECHNICAL-UNCERTAINTIES.md:40`, U10; `README.md:212`) cannot contain that material; the PoC synthesises the transformation. If *Eerste Gloed* succeeds musically, it therefore requires precisely the kind of memory outside the ring that issue #14 identifies as the most protected part of v1. That dependency belongs in the experiment's success criteria, not to be discovered later.

*Eerste Gloed* thereby proves something valuable — a composer can write a perceptible kinship with unequal envelopes and a deliberate delay — but not that NAGLOED can be an environment with its own attention and memory. The documents must make that distinction explicit, otherwise a successful demo will later be read as evidence for the vision.

Finally: "één gedeeld gebaar" ["one shared gesture"] (`EXPERIMENT-01.md:9-11`) is two events in the score (a light extinction and an audio impact 180 ms later). Conceptually that is defensible, but terminologically "one gesture" is used for a pair. Make that precise, because it is the core of the question.

### M4 — The guardrail against one-to-one mapping and the colour organ holds, but there are two erosions

What remains intact: there is no change to v1 (see `git status`: only untracked `docs/v2/` and `experiments/`), no new parameter, preset or UI, and the PoC composition is light→audio for L1→A1, which issue #14 explicitly permits ("een lichtgebeurtenis kan een geluid veroorzaken" ["a light event can cause a sound"]). No therapy, meditation or wellness claims were found; the safety documents avoid them consistently.

Two erosions:

1. **Audio-reactive mapping is normalised.** `SAFETY-PRIVACY.md:113-116`: "de licht-engine mag geen licht produceren op basis van een onbegrensde audio-reactive mapping. Elke mapping heeft een plafond, een snelheidslimiet en een dode zone (geen licht bij stilte of lage niveaus)" ["the light engine may not produce light on the basis of an unbounded audio-reactive mapping. Every mapping has a ceiling, a speed limit and a dead zone (no light during silence or at low levels)"]. This assumes that the light engine responds to audio levels and asks only for limitation. That conflicts with `ARTISTIC-VISION.md:28-30` and 114-130, where light is an autonomous world that can live independently of audio, and with issue #14's "rather than impose one-to-one audio-reactive behaviour". As a safety brake the dead zone is fine, but as an artistic model this is the colour organ with a limiter. Make explicit that a bounded mapping is one optional mode (and never the default), or remove the passage from the safety document.
2. **The event vocabulary steers the art.** `README.md:107-112` presents eight event kinds, "vaste eigenschappen, sampletijd, één seed, refractaire periodes, aandachtsbudgetten en causale diepte ≤ 3" ["fixed properties, sample time, one seed, refractory periods, attention budgets and causal depth ≤ 3"]; `ARCHITECTURE-OPTIONS.md:301-391` elaborates this with exact duration ranges, refractory values (20/40/250 ms), budgets (8/32/128 events/s, 16 light grains, 8 memories), field names and an example score. The documents call this a hypothesis, but the vocabulary returns in README, ARCHITECTURE, DISTRIBUTED, IDEAS and EXPERIMENT as the shared language. That is de facto vocabulary steering: memory *is* an event kind, attention *is* a budget per second, causality *is* depth ≤ 3. Keep at least two alternatives open (continuous memory vs. event memory; budget vs. probability) until a listening test decides, and mark the numbers as illustration, not as starting values for an implementation.

### M5 — Scope: the exploration is not compact; it contains a de facto roadmap and specifications for work issue #14 does not yet want

`docs/v2` is 176 KB and 2,548 lines (`wc -l`), plus the PoC README, hardware protocol and roughly 90 KB of Python, for one offline piece of 72 s with four light events. That is no longer a "small experiment".

More importantly: despite "Geen v2-releaseplanning" ["No v2 release planning"] (`README.md:45-46`) there is phased planning. `README.md:164-169` gives A–D with verdicts "Fase 2, pas na A + hardware", "Middellange termijn" and "Horizon" ["Phase 2, only after A + hardware"; "Medium term"; "Horizon"]; `README.md:276-279` says "Eerst een tweede experiment ... dan een derde ... Pas daarna nadenken over een gedeelde architectuur" ["First a second experiment ... then a third ... Only after that think about a shared architecture"]; `DISTRIBUTED.md:120-126` gives a numbered six-step prototype order; `TECHNICAL-UNCERTAINTIES.md` contains 29 assumptions with "Fase-1-toets"/"Fase-2-toets" ["Phase 1 test"/"Phase 2 test"] statuses. Issue #14 says: "Do not ... insert it into current workstreams or release gates" and "This issue ... is not a work order". This is a backlog with gates, even if every step is called "only with Lonn's permission".

There is also specification drift. `ARCHITECTURE-OPTIONS.md:303-306` says the vocabulary is "geen API, geen code" ["no API, no code"], but §5.2–5.4 (lines 327-376) gives mandatory fields and duration ranges per kind, refractory values, budgets, RNG derivation, and lines 395-433 an example score in JSON. A renderer that follows it is the reference implementation; the "hypothesis" is then in practice a spec.

The activation itself is not traceable. `README.md:6-8` and `ARTISTIC-VISION.md:5-7` state that "De verkenning expliciet geopend [is] door Lonn" ["the exploration has been explicitly opened by Lonn"], but issue #14 is still open with the instruction not to work on it, and there is no comment or other trace of that activation. Record who, when and with what scope activated it; otherwise the whole branch rests on an unverifiable assumption.

The artistic/technical balance is also skewed: the technical and safety documents (SPECTRASTROBE 409, ARCHITECTURE 433, DISTRIBUTED 171, SAFETY 353, TECHNICAL 139 = 1,505 lines) are more than twice the size of the artistic ones (ARTISTIC 230, IDEAS 295, EXPERIMENT 163 = 688 lines), while `README.md:19-22` says that artistic intent comes first and technique is subordinate. That is not an artistic failure in itself, but it is a signal that the exploration is being pulled by the technique. Advice: keep one artistic note (a few pages), one experiment protocol and one safety sheet as the reading path; park architecture, distributed and the uncertainty register as annexes that explicitly fall outside the gate until Lonn activates them.

### M6 — Document integrity: claims that are not yet true and references that do not exist

- `README.md:308-311`: "De verkenning is onafhankelijk gereviewd door twee reviewers ... Hun verdicten en de verwerking daarvan staan in \[REVIEWS.md\](REVIEWS.md). [REVIEW-PLACEHOLDER]" ["The exploration has been independently reviewed by two reviewers ... Their verdicts and the processing thereof are in \[REVIEWS.md\](REVIEWS.md). [REVIEW-PLACEHOLDER]"] — `docs/v2/REVIEWS.md` does not exist (the reviews are being written now) and the sentence is in the past tense. That is a process claim that is untrue at the time of writing. Put the sentence in the future tense or let it appear only once `REVIEWS.md` exists, and remove the literal placeholder. [The historical review link and `[REVIEW-PLACEHOLDER]` are quoted here as they appeared then.]
- `experiments/spectrastrobe-poc/README.md:43-45` and `HARDWARE-PROTOCOL.md:18-20`: "Lees `docs/v2/SAFETY-PRIVACY.md` ... (parallel opgesteld); zolang dat document er niet is ..." ["Read `docs/v2/SAFETY-PRIVACY.md` ... (drafted in parallel); as long as that document does not exist ..."] and "Zolang dat document niet bestaat ..." ["As long as that document does not exist ..."]. The document exists. That is a sign that the documents were written in parallel and not reconciled; precisely on safety instructions that is risky.
- `SPECTRASTROBE.md:337-339` promises a self-limiting encoder that is not in the PoC (see B1).
- The M2 definition differs from the measurement: `SPECTRASTROBE.md:168` defines M2 as "RMS van 18,2 kHz-band, -30,0 dBFS ± 0,5 dB" ["RMS of the 18.2 kHz band, -30.0 dBFS ± 0.5 dB"], while `out/verify-report.md` reports a peak amplitude of −29.96 dBFS and the RMS is −32.97 dBFS. With the literal RMS definition the check would fall outside ±0.5 dB, but it is marked PASS. That is a technical detail for reviewer-a, but it touches the question of whether "verified" means the same thing in the document and in the report.
- The PoC says 20% (`experiments/spectrastrobe-poc/README.md:37`, `score.json:138`) while `EXPERIMENT-01.md:65-66` says "maximaal ongeveer 25%" ["at most about 25%"]; the renderer logs "max 25% van referentie (score gebruikt 20%)" ["max 25% of reference (score uses 20%)"]. Clarify which value is the design value and which is the ceiling.

---

## Minors

- **m1 — No fixed review point.** `git status --short` shows `?? docs/v2/` and `?? experiments/`: the whole exploration is untracked. A review belongs to a commit SHA or an explicitly fixed worktree; as it stands, the review is against a possibly moving target and the provenance of any changes cannot be traced in git. Record a commit or SHA before the reviews are processed.
- **m2 — R11 is missing from the executable protocol.** `SAFETY-PRIVACY.md:110` mentions "één oog bedekken" ["cover one eye"]; `HARDWARE-PROTOCOL.md:22-28` mentions only glasses off, Kasina off and unplugging. Add the one-eye action to the stop measures.
- **m3 — The repeat schedule is not what the document says.** `EXPERIMENT-01.md:104` speaks of "twee keer kijken (de tweede keer wetend wat er komt)" ["watch twice (the second time knowing what is coming)"]; `HARDWARE-PROTOCOL.md:50-58` has already played T0–T6 before that, so the "second" viewing is in practice the seventh. Moreover, there is no order balancing (first without, then with prior knowledge) between conditions.
- **m4 — "Safe" language versus R12.** `HARDWARE-PROTOCOL.md:34` calls the 44.1/16-bit file "**veilige, aanbevolen weergaveversie**" ["**safe, recommended playback version**"] and `ARTISTIC-VISION.md:134` says "Geborgen betekent veilig en warm" ["'Geborgen' means safe and warm"], while R12 (`SAFETY-PRIVACY.md:111`) prohibits "safe" claims. In context this concerns signal integrity and the meaning of a word, respectively, not health, but make that distinction explicit ("signaalveilig" ["signal-safe"] versus "gezondheidsclaim" ["health claim"]) to prevent misunderstanding.
- **m5 — 72 s and 90% silence is very little material.** The render logs "programma 7,4 s van 72,0 s (90% stilte)" ["program 7.4 s of 72.0 s (90% silence)"]. That makes the failure criteria "de audio te vol is en de stilte niet kan dragen" and "contrast" ["the audio is too full and cannot carry the silence"; "contrast"] not really testable; there is almost no density with which to make contrast. Consider a second, denser variant as a contrast condition instead of only this sparse version.
- **m6 — The non-answer condition has no established question.** L3 is a weak, cool grain (`score.json:108-117`) that is not set up as a "question"; the only basis for expectation is the earlier L1→A1 pairing. One earlier pair is weak evidence that the listener expects an answer. Make L3 a recognisable question or repeat the pattern more often before "non-answer" means anything.
- **m7 — No analysis plan for the qualitative answers.** `EXPERIMENT-01.md:94` asks five questions, but nowhere does it say who asks them, how they are recorded, how they are compared across conditions or when an answer counts as "related". Without that, the outcome is anecdotal.

---

## Nits

- `EXPERIMENT-01.md:106`: "of hetgeluid op zichzelf staat" ["whether the sound stands on its own"] — missing space.
- `SPECTRASTROBE.md:41`: "enigsins" — should be "enigszins" ["somewhat"].
- `SPECTRASTROBE.md:275`: "routingdtest" — "routingtest".
- `README.md:311`: literal `[REVIEW-PLACEHOLDER]` in a document that may go public.
- `README.md:176-178`: "het minste risico" ["the least risk"] is a self-assessment, not a fact; phrase it as a design choice.
- Terminology alternates between English and Dutch ("restraint", "agency", "grains", "events") while `docs/BRAND.md:232-270` asks for a tight vocabulary. Make a short glossary for the exploration.

---

## Ideas: the three weakest and the two strongest

`IDEAS.md` has a smallest test and a named risk per idea; that makes the bundle usable. But the bundle is uneven, and the file itself admits that #2, #3 and #10 are "varianten van hetzelfde materiaal" ["variants of the same material"] (`IDEAS.md:283-285`) — that is a weakness, not a cross-connection.

**Three weakest:**

1. **#5 "De lamp verveelt zich" ["The lamp gets bored"]** (`IDEAS.md:124-146`). Anthropomorphism is a gimmick here: "verveling" ["boredom"] is, according to the idea itself, "moeilijk te onderscheiden van random" ["hard to distinguish from random"] (`IDEAS.md:144-145`), and the artist hopes that a state machine with five states reads as intention. It is also conceptually at odds with the Ganzfeld idea: there is no lamp, there is a diffuse colour field. The "bored lamp" is a character the installation does not have.
2. **#10 "De bijna-gebeurtenis" ["The almost-event"]** (`IDEAS.md:252-276`). An envelope trick (reaching 70% of the brightness and then fading) that the document itself already labels risky: "De grens tussen 'bijna' en 'kapot' is klein" ["The boundary between 'almost' and 'broken' is narrow"] (`IDEAS.md:274-276`). It is an effect, not a dramaturgy, and a variant of #3 (refusing) and #2 (incomplete memory); the art is not in the almost-event itself but in the expectation around it, and that expectation is not designed.
3. **#8 "Aanwezigheid zonder beelden" ["Presence without images"]** (`IDEAS.md:198-224`). This idea adds something that `ARTISTIC-VISION.md:97-100` explicitly considers unnecessary: "het systeem hoeft aanwezigheid niet te detecteren om erop te reageren ... zonder camera's of sensoren" ["the system need not detect presence in order to respond to it ... without cameras or sensors"]. Audio-only occupancy estimation is unreliable (the document says so itself, `IDEAS.md:222-224`), privacy-sensitive around microphones, and the strongest find — the card on the table as ritual — needs no sensing at all. Drop the sensing, keep the ritual.

**Two strongest:**

1. **#2 "Het vergeetboek" ["The forgetting book"]** (`IDEAS.md:47-72`). This is the only idea that adds a structural musical process instead of an effect: every return costs detail, some fragments disappear for good. It is testable (the control render "zesde recall als eerste recall" ["sixth recall as first recall"], `IDEAS.md:67-69`), it fits the premise (nagloed is what remains), and the risk is correctly named: degradation must not become a LoFi effect, the difference must lie in the structure (`IDEAS.md:70-72`). This deserves its own experiment after *Eerste Gloed*.
2. **#3 "De kunst van het negeren" ["The art of ignoring"]** (`IDEAS.md:74-97`). Refusing as a move with memory, including a "drawer" with an expiry date, is precisely the dramaturgy the vision promises and that *Eerste Gloed* does not test. The smallest test (eight of ten ignored versus four of ten) is concrete and falsifiable. Note the pitfall the idea itself mentions: the audience must notice the difference without reading it as a bug; that is why the paired condition from B2 is indispensable here.

#1 "De offstage componist" ["The offstage composer"] is the most distinctive idea, but it is mainly a distribution and authorship model (offline score, no runtime inference); artistically it is weaker than #2 and #3. #4 "Contrapunt" and #7 "Aandachtsbudget" are solid but close to what the vision already says; #6 "Weer" and #9 "Poste restante" are beautiful but respectively modulation-sensitive and scope-explosive.

---

## Per assessment question, in brief

| Question | Assessment |
|---|---|
| 1. Vision: sharpening or repackaging? | Partly a genuine sharpening (non-answering, memory/forgetting, attention, A1-first, safety framework); the claim "the question changes" is too strong and the success criteria are directional, not testable. Tone largely sober, locally manifesto-like and self-praising. **Major (M2).** |
| 2. Guardrails | "No v1 change" holds (only untracked docs/experiments); no therapy/wellness claims; no colour organ in the composition. But audio-reactive mapping is normalised (`SAFETY-PRIVACY.md:113-116`) and the event vocabulary steers the art; "no v2 release planning" is not maintained. **Major (M4, M5).** |
| 3. Experiment design | Tests one written pair, not system behaviour; questions are leading; criteria partly unfalsifiable; N = 1, no blinding, no A/B, no context, no analysis plan; 72 s with 90% silence is too little; the non-answer condition is not falsifiable. **Blocker (B2), major (M3).** |
| 4. Safety and ethics | R1–R12 are conservative and well substantiated; no medical claims. But 8 ms/R4, L2 step, missing brightness ladder, no tested stop, no consent/age/hearing test in the executable protocol, and an unproven "inaudible" claim. **Blocker (B1), major (M1).** |
| 5. Ideas | Uneven; weakest #5, #10, #8; strongest #2 and #3; #1 most distinctive but mainly infrastructure. **Minor, with one real recommendation: #2 as the second experiment.** |
| 6. Scope and creativity | Too much document (176 KB / 2,548 lines) for one small experiment; de facto roadmap (A–D, phase 2, six-step distributed) and specification (vocabulary, budgets, JSON); technical steering reduces the creative space; the activation by Lonn is not traceable. No hidden v1 change. **Major (M5).** |

---

## Five questions Lonn should ask

1. **Is the 8 ms extinction a hard artistic requirement or a detail?** If hard: who takes written responsibility for the exception to R4/R5, and with which measurement (photodiode/M12) before anyone sits under the glasses? If not: why does the safety document say "≥ 20 ms hard" ["≥ 20 ms hard"] while the score says "8 ms"?
2. **Is *Eerste Gloed* a demo or a test?** If a test: which N, which blinding, which control condition (0 ms delay; 38 s grain answered), which pre-specified decision rule? If a demo: remove the success criteria and call it an exploration of material, not of kinship.
3. **What is the claim you want to test: that a composer can write kinship (which *Eerste Gloed* tests) or that a system can itself refuse, remember and forget (which it does not test)?** And if the latter: do you already accept that the 12 s ring must give way for that, or does it remain untouchable?
4. **Which documents are binding and which are disposable?** Does the eight-kind vocabulary become a specification or remain one of several hypotheses? Are you willing to take ARCHITECTURE-OPTIONS, DISTRIBUTED and TECHNICAL-UNCERTAINTIES temporarily out of the reading path and out of the gate until the artistic question is answered?
5. **Who is responsible for the first session with microphone and visitors, and for the first session with the glasses?** Which consent, which retention, which level and hearing test, which emergency procedure (epilepsy, one eye, 18–20 kHz), and where is the activation of this exploration by you documented — issue #14 still says "do not implement; do not start research"?

---

## What I checked, and what I did not

**Checked:**

- `gh issue view 14 --repo leonvanbokhorst/nagloed` in full, including the guardrails and the instruction to agents not to implement this.
- All requested documents, plus `ARCHITECTURE-OPTIONS.md` and `TECHNICAL-UNCERTAINTIES.md` for the scope and consistency assessment.
- `git status --short` (only `?? docs/v2/`, `?? experiments/`; no changes in v1 files), `git diff --stat` (empty).
- Independent render and verification in `/tmp/review-b-out`: render and verify exit 0, 31 PASS / 0 FAIL / 0 SKIP; the score check confirms 8 ms as PASS and the determinism check passes. `./run.sh` was not run in the shared directory.
- `out/events.csv`, `out/timeline.svg`, `out/verify-report.md` and the score/relations checked against the documents.

**Not checked:**

- The actual Kasina + DeepVision (not available): M11–M13, the decoder characteristic, the LED output and the stop latency are unmeasured. Everything I say about safety on hardware is therefore an ex-ante requirement, not a measurement.
- The full implementation of `render.py` and `verify.py`; I followed only the safety- and claim-relevant paths.
- Whether Lonn actually opened the exploration explicitly; that is stated in the documents but is not traceable in the repository or the issue.
- The legal durability of the privacy and consent paragraph; that requires a lawyer, as `SAFETY-PRIVACY.md:191-192` itself says.

Nothing outside this file was changed; no git write commands were executed and no code or documentation was fixed.

---

# Addendum — re-check after processing

**Date:** 5 November 2026
**Review point:** commit `6c9e831`, delta `0f6422e..6c9e831`, branch `feature/v2-exploration`.
**Scope of this re-check:** only the two blockers (B1, B2) and the six majors (M1–M6); not a full re-review. The original review text above was left substantively unchanged as historical evidence; this addendum was added after the processing.
**Method:** delta diff read; `git diff 0f6422e..6c9e831 -- src presets tools tests CMakeLists.txt CMakePresets.json scripts .github Demo README.md docs/ARCHITECTURE.md docs/BRAND.md` is empty (v1.0.4 untouched); the PoC re-rendered and re-verified in `/tmp/review-b-out2` (not `./run.sh` in the shared directory): render exit 0, verify exit 0, `KERN: PASS (52 PASS, 0 FAIL, 1 INFO, 0 SKIP of 53)`.

## B1 — resolved: YES

Evidence that the earlier blocker was removed:

- **20 ms instead of 8 ms.** `score.json:111` has `[5.98, 1.0], [6.0, 0.0]`; `score.json:26-32` sets `min_attack_ms`/`min_release_ms` to 20 ms. Verify: `score-L1-extinctie` PASS, 20.0 ms; `score-envelope-regels` PASS (`verify.py:550,559`).
- **50 ms fade-in for L2.** `score.json:122`: `[0.0, 0.0], [0.05, 0.35]`; the start is no longer a step.
- **Encoder really clamps.** `render.py:133` (`validate_score`) and `render.py:346-413` hard-limit each colour channel to 25% of the reference peak and log the request; verify `sa-clamp-adversarieel` PASS with measured max 24.44% and 3 clamp entries (`verify.py:953-991`); the manifest contains clamp and exposure information.
- **Ladder in the executable protocol.** `HARDWARE-PROTOCOL.md:67-79`: T0/T1 dim 6.25%, T2 mid 12.5%, T3 full 20%, and only then 48 kHz and DeepVision; verify `ladder-dim/mid/full/orde` PASS (6.20% / 12.38% / 19.79%).
- **Panic claim withdrawn.** `score.json:163`, `HARDWARE-PROTOCOL.md:34-36` and `SPECTRASTROBE.md:234,334,357` now state explicitly that the offline WAV has no encoder panic and no tested 50 ms ramp; the 50 ms requirement sits in phase B and the human stop is primary.

Remaining points on B1 (not blockers):

- R4 was amended to permit the extinction (`SAFETY-PRIVACY.md:103`: "Een eenmalige, geïsoleerde extinctie van 20 ms is toegestaan" ["A one-off, isolated extinction of 20 ms is permitted"]). That is a deliberate policy change to make the artwork fit. Substantiate in the safety document why 20 ms is acceptable in a full visual field (link it explicitly to R2, which concerns repetition, and to an M12 measurement on hardware), otherwise it reads as a rule that adapts itself to the composition.
- The "comfortbeluistering (10 s, dragers-only)" ["comfort listening (10 s, carriers-only)"] (`HARDWARE-PROTOCOL.md:45-49`) plays `spectrastrobe-dim-44k1-16bit.wav` or the master — those are mixes with programme, not carriers-only. Render a `carriers-only-44k1-16bit.wav` (the option already exists: `render.py:706`) or correct the text.

## B2 — resolved in design: YES; the executable annex still lags behind

Evidence that the core of the blocker was removed:

- **Phase 0 versus phase 1.** `EXPERIMENT-01.md:106` makes phase 0 explicitly an author test without evidence; `EXPERIMENT-01.md:113-166` describes phase 1 as a pre-registered test: N ≥ 5 non-authors, randomised and blind conditions A/B/C/D, and B and C as controls that still have to be rendered (`:126-127`).
- **Open before targeted.** The questions were rewritten (`EXPERIMENT-01.md:136-148`): first open ("Wat merkte je op?" ["What did you notice?"]), the 38 s question only afterwards.
- **Falsifiable.** `EXPERIMENT-01.md:156-159`: "Als niemand het opmerkt, is de niet-antwoordconditie voor deze uitvoering **niet geslaagd**; dat is een uitkomst, geen succes." ["If nobody notices it, the non-answer condition has **not succeeded** for this run; that is an outcome, not a success."] Plus coding and decision rules (`:150-166`).
- The old leading questions have disappeared from `EXPERIMENT-01.md`.

Remaining points on B2 (major, before a session):

- **The executable protocol has not been updated.** `HARDWARE-PROTOCOL.md:97` still contains the pre-cue item "Onbeantwoord grain om 38 s opgemerkt" ["unanswered grain at 38 s noticed"], and `HARDWARE-PROTOCOL.md:107-113` still contains the old five leading questions, with the reference "uit `docs/v2/EXPERIMENT-01.md` §5" ["from `docs/v2/EXPERIMENT-01.md` §5"] — that §5 has been rewritten and no longer contains those questions. Label §5 explicitly as a phase-0 author test (where cueing is not evidence), or replace it with the open-first questions and make a separate phase-1 form; in any case remove the pre-cue from the observation form.
- **Coder independence.** Phase 1 mentions "twee codeerders (of Lonn en één buitenstaander)" ["two coders (or Lonn and one outsider)"] (`EXPERIMENT-01.md:131-133`). Lonn is the author and knows the conditions; at least one coder must be blind to condition (or coding must happen on anonymised fragments). Record that in the phase-1 plan.

## Majors — status

| Major | Status | Evidence / remaining point |
|---|---|---|
| M1 — executable safety (stop/consent/age/hearing/R11/level) | **Resolved** | `HARDWARE-PROTOCOL.md:14-52`: consent and instruction beforehand, children only with supervision, room lit, cover one eye (R11), stop measures, no panic claim, fixed playback level and 10 s comfort listening. Remaining point: carriers-only test plays a mix (see B1). |
| M2 — vision overclaims; criteria not testable; tone | **Resolved** | `ARTISTIC-VISION.md:22` "scherpt de vraag aan" ["sharpens the question"]; `ARTISTIC-VISION.md:185-188` criteria explicitly "een kompas, geen meetlat" ["a compass, not a yardstick"] that must be operationalised per experiment. |
| M3 — written versus emergent; 12 s ring | **Resolved** | `EXPERIMENT-01.md:24-38`: relations are "geschreven, niet emergent" ["written, not emergent"]; the "memory" at 46 s does not fit in the 12 s ring; "één gedeeld gebaar" is explicitly "één paar" ["one pair"]. |
| M4 — audio-reactive mapping and vocabulary steering | **Resolved** | `SAFETY-PRIVACY.md:113-119`: bounded mapping is "hoogstens één optionele modus, nooit de default en nooit het conceptuele model" ["at most one optional mode, never the default and never the conceptual model"]; `ARCHITECTURE-OPTIONS.md:303-314`: numbers illustrative, two alternatives explicitly open. |
| M5 — scope/roadmap/activation | **Resolved** | `README.md:6-8` records the activation by Lonn on 5 November 2026; `README.md:13-16` separates core and annexes ("geen poort en geen backlog" ["no gate and no backlog"]); `README.md:179` calls the architecture table "geen fasering of backlog" ["no phasing or backlog"]; `DISTRIBUTED.md:118-126` likewise. Remaining point: `README.md:232-233` still uses "Fase 2" ["Phase 2"] as a status; rename to "later te toetsen" ["to be tested later"] if the "no phasing" claim is to hold. |
| M6 — document integrity | **Resolved** | Stale "zolang SAFETY-PRIVACY niet bestaat" ["as long as SAFETY-PRIVACY does not exist"] texts removed; `SPECTRASTROBE.md:168` now defines M2 as peak amplitude; 20% (score) versus 25% (ceiling) is clarified in `EXPERIMENT-01.md:76` and `experiments/spectrastrobe-poc/README.md:54-57`; `README.md:344-355` and `REVIEWS.md` now exist and the review claim has been corrected. |

Remaining points on M6/REVIEWS.md (minor):

- `REVIEWS.md:16` states that "beide reviewers de v1-basislijn, de tests en de PoC zelf [hebben] gedraaid" ["both reviewers ran the v1 baseline, the tests and the PoC themselves"]. Reviewer B rendered and verified the PoC and checked the v1 diff/issue, but did not run the v1 test suite. Make the sentence factual per reviewer or drop the "tests" part.
- `REVIEWS.md:19-20` says that the review files were "ongewijzigd bewaard" ["preserved unchanged"]. Because of this addendum that is no longer literally true; phrase it as "the original review text is preserved unchanged; the re-check addendum was added".
- `README.md:42,291` reports "52/52 kernchecks PASS, 1 INFO". The report says 52 PASS out of 53 checks, with 1 INFO. Write "52 of 53 PASS, 1 INFO, 0 FAIL".

## Updated verdict

**APPROVE WITH COMMENTS.**

Both blockers are substantively resolved: the PoC now respects its own safety rules and the verification no longer rewards violations; the perceptual design distinguishes an author test from a pre-registered test with controls and falsifiable criteria. No new blocker has appeared and v1.0.4 is demonstrably untouched.

Conditional: one major remaining point must be resolved before an actual session — `HARDWARE-PROTOCOL.md` §5 must be brought into line with the rewritten `EXPERIMENT-01.md` §5 (phase-0 label or open-first form, pre-cue removed). In addition, a short list of minors (R4 substantiation, carriers-only test, REVIEWS factual accuracy, 52/53 count, "Phase 2" labels). Phase 1 may only be run after the control versions have been rendered and the phase-1 form has been fixed; that is already stated explicitly in `EXPERIMENT-01.md:126-127`.

I changed nothing outside this addendum; no git write commands, no fixes to code or documentation.
