# Independent reviews and resolution

*English is the canonical documentation language for this exploration; the research was conducted and first written in Dutch. The Dutch originals remain in git history.*

> **Status:** review report accompanying the v2 exploration. Not a v2
> specification, not a release plan. NAGLOED 1.0.4 remains unchanged. This file
> describes what two independent reviewers found and what was done with those
> findings.

## 1. Review point and method

| | |
|---|---|
| Base | `develop` @ `983d48c` = NAGLOED 1.0.4, tag `v1.0.4` |
| Review point | commit `0f6422e` (PoC) on `034f7cb` (docs), branch `feature/v2-exploration` |
| Reviewers | reviewer A (technical feasibility and signal integrity), reviewer B (artistic intent, safety and scope) |
| Independence | both reviewers started with a fresh context and did not contribute to the content |
| Evidence | review files [REVIEW-A.md](reviews/REVIEW-A.md) and [REVIEW-B.md](reviews/REVIEW-B.md). Reviewer A ran the v1 test suite and the PoC; reviewer B checked the v1 baseline and rendered and verified the PoC in their own output directory. Both made their own measurements. |

Both reviewers reached **REQUEST CHANGES** at the review point. The original
review text is preserved unchanged; the re-check addenda have been appended to
it.

## 2. What the reviewers established

**Reviewer A (technical).** The audio signal itself was clean: carriers within
±0.04 Hz, reference at −30 dBFS peak, colour ratios within 0.33 dB, 20 Hz
panning with 100% depth, no clipping, no metadata, no dither, the 44.1 kHz
variant demonstrably native, two renders byte-identical. The blocker was in the
**safety and verification claims**: the 25% colour ceiling was not enforced,
the envelope requirement of ≥ 20 ms was violated (8 ms extinction and a 0 ms
start), a panic stop of ≤ 50 ms did not exist, and the measurement suite
rewarded one violation. In addition, M2 (peak vs. RMS), M6 (sidebands vs.
inter-carrier isolation), M7 (narrowband programme) and M14 (incomplete matrix)
did not match the binding table; A1 was presented as more than the PoC built;
A2 conflicted with the frozen CMake; the 44.1/16-bit version was tested less
thoroughly; and `REVIEWS.md`/the review claim did not yet exist.

**Reviewer B (artistic/safety/scope).** The vision is a genuine sharpening
(non-response, memory/forgetting, attention, offline first, safety framework),
but the claim "the question changes" is too strong and the success criteria are
a compass, not a measuring stick. The blocker was in **safety and experiment
design**: the PoC violated R4/R5 (8 ms, 0 ms start) and verification approved
it; the perceptual test was N=1, leading and had no control condition, so the
non-response condition was not falsifiable. Furthermore: the executable
hardware protocol lacked stop/consent/hearing/age, the "inaudible" and "safe"
language was too strong, audio-reactive mapping was normalised, the event
vocabulary steered the art, the exploration became a de facto roadmap/spec, and
document integrity had gaps (stale references, measurement definitions, 20% vs.
25%).

## 3. Resolution

| Finding | Resolution | Evidence |
|---|---|---|
| **B1** colour ceiling not enforced | `render.py` hard-clamps every colour channel to 25% of the reference peak, logs a warning and writes this into the manifest; an adversarial score (`rgb=0.90`) is clamped to 24.44%. | `verify.py` check `sa-clamp-adversarieel` PASS; `out/verify-report.md` |
| **B1** envelope requirement violated | L1 extinction 20.0 ms (was 8 ms); L2 starts with a 50 ms fade-in; renderer validates start/end at zero and attack/release ≥ 20 ms; verify requires 20–40 ms and no longer approves 8 ms. | `score.json`; checks `score-envelope-regels`, `score-L1-extinctie` PASS |
| **B1** panic stop claimed | Claim withdrawn: the offline PoC has no encoder panic and no tested 50 ms ramp; the primary stop is human (stop playback, Kasina brightness low, glasses off/one eye). The 50 ms requirement is now explicit in phase B. | `README.md`, `HARDWARE-PROTOCOL.md`, `SPECTRASTROBE.md` §6 |
| **B1** brightness ladder missing | `--colour-scale` added; dim 6.25% and mid 12.5% as 44.1/16 variants alongside the 20% version; protocol T1 dim → T2 mid → T3 full. | checks `score-heldrerheidsladder`, `ladder-dim`, `ladder-orde` PASS |
| **B2** perceptual test N=1/cueing/not falsifiable | `EXPERIMENT-01.md` rewritten: phase 0 is explicitly an author test without evidence; phase 1 is a pre-registered test with N ≥ 5 non-authors, randomised/blind conditions, three control versions (0 ms; 38 s grain actually answered; programme-only), open questions before targeted questions, pre-defined coding and decision rules, and a falsifiable non-response condition. The control versions still have to be rendered; without them phase 1 is not valid. | `EXPERIMENT-01.md` §5 |
| **A-M1** M2 peak vs. RMS | Binding table now measures **peak amplitude**; band RMS is reported separately. PoC: peak −29.96 dBFS (48k) / −29.89 dBFS (44.1k), band RMS −36.02 dBFS informative. | `SPECTRASTROBE.md` §3; `verify-report.md` |
| **A-M2** M7 narrowband programme | Broadband probe added: white noise through the same 8th-order LPF → attenuation 17 kHz −19.2 dB, 18.2 kHz −34.0 dB, 19.7 kHz −56.3 dB. Honestly reported as **INFO**: the −60 dBFS requirement is not demonstrated for broadband v1 material; no false PASS. | check `m7-breedband-eerlijk` INFO |
| **A-M3** M6 sidebands instead of isolation | Real probe with one colour at a time; inactive colour bands ≤ −151 dB (measurement floor; Kasina decoder bandwidth remains open). | check `m6-inter-drager-isolatie` PASS |
| **A-M4** M14 incomplete/tautological | Naive SRC, MP3 128 joint stereo and L/R swap added; preserved rows now also test colour frequencies and ratios. 15 rows: 9 PASS, 5 EXPECTED-DEGRADED, 1 INFO, 0 ADVISORY-FAIL. The matrix remains advisory; only the core M1–M10 determines the exit code. | `verify-report.md` §corruptiematrix |
| **A-M5** A1 scope overclaim | `ARCHITECTURE-OPTIONS.md` and `README.md` now state explicitly: A1 covers the signal half (encoder + integrity), **not** the event engine, the memory or the budgets. | `ARCHITECTURE-OPTIONS.md` §2.A/§3; `README.md` §4 |
| **A-M6** A2 and frozen CMake | Registered as an open point (U30): A2 requires a separate build project or an explicit exception, and does not reuse the A1 programme literally; v1 material is broadband and makes M7 all the more challenging. | `TECHNICAL-UNCERTAINTIES.md` U30 |
| **A-M7** 44.1/16-bit tested less | The full M4/M7/M8 set now also runs at 44.1/16-bit (including an extra program-only-44k1). | checks `m4/m7/m8-44k1` PASS |
| **A-M8** review claim/placeholder | This file now exists; the review claim in `README.md` has been corrected. | `README.md` §8 |
| **B-M1** operational safety | Protocol extended with R11 (one eye), consent/age/supervision, a fixed playback level and 10 s comfort listening before the session, stop mechanisms, and no panic claim. | `HARDWARE-PROTOCOL.md`; `SAFETY-PRIVACY.md` §4.3 |
| **B-M2** vision overclaims | "Changes the question" → "sharpens the question", with an explicit list of what is genuinely new; success criteria explicitly a compass that must be operationalised per experiment. | `ARTISTIC-VISION.md` §1/§10 |
| **B-M3** written vs. emergent | `EXPERIMENT-01.md` §2 and §8: the relationships are written, not emergent; the memory at 46 s does not fit in the 12 s ring; system autonomy is not tested. | `EXPERIMENT-01.md` |
| **B-M4** audio-reactive mapping/vocabulary | Mapping is now explicitly at most one optional mode, never the default; the vocabulary numbers are illustrative and two alternatives (continuous vs. event memory, budget vs. probability) remain open. | `SAFETY-PRIVACY.md` §3; `ARCHITECTURE-OPTIONS.md` §5 |
| **B-M5** scope/roadmap | `README.md` marks the core (vision, experiment, safety) as leading and the rest as appendices; the A–D and distributed order are explicitly not a backlog or phasing; Lonn's activation is recorded. | `README.md` §9; `DISTRIBUTED.md` §7 |
| **B-M6** document integrity | Stale "as long as SAFETY-PRIVACY does not exist" texts removed; M2 definition corrected; 20% (score) vs. 25% (ceiling) per carrier clarified; review claim corrected. | PoC README, `HARDWARE-PROTOCOL.md`, `SPECTRASTROBE.md` |
| **Minors/nits** | Among others: "inaudible" qualified (U15), "no flicker" claim nuanced (20 Hz reference panning is prescribed and open), "safe" → "signal-safe", two incorrect file:line citations corrected, typos, one-eye emergency measure, manifest with script hashes, timeline derived from the score. | `git diff 0f6422e..HEAD` |

## 4. What deliberately remains open

These points have **not** been polished away; they are the next gates.

1. **Hardware (M11–M13).** No Kasina/DeepVision available. Detection, actual
   brightness, LED output, flicker and stop latency are unmeasured. The hardware
   step is and remains a hard gate.
2. **Decoder brightness scale (U31).** The reference is modulated per ear; a
   per-ear interpretation can make 25% larger in practice. Measurable only with
   hardware/photodiode.
3. **Host/DAW chain (U13).** The offline WAV is clean; DAW bounce, USB, OS
   resampling/EQ and lossy playback are not measured. The corruption matrix is
   a first indication, not a guarantee.
4. **Broadband M7 for A2.** The 8th-order LPF does not reach −60 dBFS at 17 kHz;
   with broadband v1 material, extra filtering or different band limiting is
   needed. Now honestly registered as INFO.
5. **Perceptual phase 1.** The method is in place, but the control versions
   have not yet been rendered and there are no independent participants yet.
   Without that execution there is no evidence that the kinship works.
6. **Cross-platform determinism.** Tested only on this machine/CPython;
   bit-identical renders on a second platform are not demonstrated (U22).

## 5. Re-check after resolution

The reviewers were asked to re-check the blockers and the most important majors
at commit `6c9e831` (delta `0f6422e..6c9e831`). Both did so in a targeted way,
with their own measurements; the addenda are at the bottom of
[REVIEW-A.md](reviews/REVIEW-A.md) and [REVIEW-B.md](reviews/REVIEW-B.md).

| Reviewer | Updated verdict | What was re-checked |
|---|---|---|
| A (technical) | **APPROVE WITH COMMENTS** | Blocker B1 resolved (clamp, 20 ms/50 ms envelopes, stop claim, ladder), M1–M8 resolved, v1 untouched, own PoC run 52 PASS / 1 INFO / 0 FAIL, own adversarial clamp test |
| B (artistic/safety/scope) | **APPROVE WITH COMMENTS** | B1 and B2 resolved in substance; majors M1–M6 resolved; PoC re-rendered/re-verified in their own directory; v1 diff empty |

The re-check left a small number of non-blocking points. These were then
addressed: the hardware protocol was brought into line with the rewritten
`EXPERIMENT-01.md` §5 (pre-cue removed, phase-0 label, phase 1 separate), the
R4 rationale for the one-off 20 ms extinction was added, the carriers-only
comfort test now has a real carriers-only render, the coding independence in
phase 1 was tightened, and the counts/claims in `README.md` and this file were
made factual. Those last changes were not re-checked by the reviewers; they are
administrative or protocol tightening, not a new substantive claim.

## 6. Conclusion

At the review point there were two blockers: one in the safety/verification
claims and one in the perceptual design. Both have been addressed in the
documents and the PoC. In the targeted re-check **both reviewers gave APPROVE
WITH COMMENTS**: the blockers are resolved in substance, with their own
measurements as evidence. The remaining points have been addressed or are
explicitly listed as open residual risk in §4. The most important substantive
limitation remains: phase 1 of the perceptual test has not yet been carried out
and the hardware has not been measured.

What has **not** changed: no v2 has been built, no DSP contract has been
changed, no release branch has been touched and no hardware has been claimed.
The next substantive step is a conservative hardware verification by Lonn, with
`HARDWARE-PROTOCOL.md` and an immediate stop. Only after that is an artistic
statement about *Eerste Gloed* possible.
