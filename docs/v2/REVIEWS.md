# Onafhankelijke reviews en verwerking

> **Status:** reviewverslag bij de v2-verkenning. Geen v2-specificatie, geen
> releaseplan. NAGLOED 1.0.4 blijft ongewijzigd. Dit bestand beschrijft wat twee
> onafhankelijke reviewers hebben gevonden en wat er met die bevindingen is
> gedaan.

## 1. Reviewpunt en methode

| | |
|---|---|
| Basis | `develop` @ `983d48c` = NAGLOED 1.0.4, tag `v1.0.4` |
| Reviewpunt | commit `0f6422e` (PoC) op `034f7cb` (docs), branch `feature/v2-exploration` |
| Reviewers | reviewer A (technische haalbaarheid en signaalintegriteit), reviewer B (artistieke intentie, veiligheid en scope) |
| Onafhankelijkheid | beide reviewers zijn met een verse context gestart en hebben niet aan de inhoud meegewerkt |
| Bewijs | reviewbestanden [REVIEW-A.md](reviews/REVIEW-A.md) en [REVIEW-B.md](reviews/REVIEW-B.md). Reviewer A heeft de v1-testsuite en de PoC gedraaid; reviewer B heeft de v1-basislijn gecontroleerd en de PoC in een eigen uitvoermap gerenderd en geverifieerd. Beide hebben eigen metingen gedaan. |

Beide reviewers kwamen op het reviewpunt tot **REQUEST CHANGES**. De
oorspronkelijke reviewtekst is ongewijzigd bewaard; de hercontrole-addenda zijn
eraan toegevoegd.

## 2. Wat de reviewers vaststelden

**Reviewer A (technisch).** Het audiosignaal zelf was schoon: dragers binnen
±0,04 Hz, referentie op −30 dBFS piek, kleurverhoudingen binnen 0,33 dB,
20 Hz-panning met 100 % diepte, geen clipping, geen metadata, geen dither, de
44,1 kHz-variant aantoonbaar native, twee renders byte-identiek. De blocker zat
in de **veiligheids- en verificatieclaims**: het 25 %-kleurplafond werd niet
afgedwongen, de envelope-eis van ≥ 20 ms werd geschonden (8 ms extinctie en een
0 ms-start), een panic-stop van ≤ 50 ms bestond niet, en de meetsuite beloonde
één overtreding. Daarnaast klopten M2 (piek vs. RMS), M6 (zijbanden vs.
inter-drager-isolatie), M7 (smalbandig programma) en M14 (onvolledige matrix)
niet met de bindende tabel; A1 werd als meer voorgesteld dan de PoC bouwde; A2
botste met de bevroren CMake; de 44,1/16-bit-versie werd minder volledig
getest; en `REVIEWS.md`/de reviewclaim bestonden nog niet.

**Reviewer B (artistiek/veiligheid/scope).** De visie is een echte aanscherping
(niet-antwoorden, geheugen/vergeten, aandacht, offline-eerst, veiligheidskader),
maar de claim "de vraag verandert" is te sterk en de succescriteria zijn een
kompas, geen meetlat. De blocker zat in **veiligheid en experimentdesign**: de
PoC schond R4/R5 (8 ms, 0 ms-start) en de verificatie keurde dat goed; de
perceptuele toets was N=1, sturend en had geen controleconditie, waardoor de
niet-antwoord-conditie niet falsifieerbaar was. Verder: het uitvoerbare
hardwareprotocol miste stop/consent/gehoor/leeftijd, de "onhoorbaar"- en
"veilig"-taal was te sterk, audio-reactive mapping werd genormaliseerd, het
eventvocabulaire stuurde de kunst, de verkenning werd een de facto roadmap/spec,
en de documentintegriteit had gaten (stale verwijzingen, meetdefinities,
20 % vs. 25 %).

## 3. Verwerking

| Bevinding | Verwerking | Bewijs |
|---|---|---|
| **B1** kleurplafond niet afgedwongen | `render.py` clampt elk kleurkanaal hard op 25 % van de referentiepiek, logt een waarschuwing en schrijft dit in het manifest; een adversariële score (`rgb=0,90`) wordt op 24,44 % geclampt. | `verify.py` check `sa-clamp-adversarieel` PASS; `out/verify-report.md` |
| **B1** envelope-eis geschonden | L1-extinctie 20,0 ms (was 8 ms); L2 start met 50 ms fade-in; renderer valideert start/eind op nul en attack/release ≥ 20 ms; verify eist 20–40 ms en keurt geen 8 ms meer goed. | `score.json`; checks `score-envelope-regels`, `score-L1-extinctie` PASS |
| **B1** panic-stop geclaimd | Claim ingetrokken: de offline PoC heeft geen encoder-panic en geen geteste 50 ms-ramp; primaire stop is menselijk (weergave stoppen, Kasina-helderheid laag, bril af/één oog). De 50 ms-eis staat nu expliciet in fase B. | `README.md`, `HARDWARE-PROTOCOL.md`, `SPECTRASTROBE.md` §6 |
| **B1** helderheidsladder ontbrak | `--colour-scale` toegevoegd; dim 6,25 % en mid 12,5 % als 44,1/16-varianten naast de 20 %-versie; protocol T1 dim → T2 mid → T3 volledig. | checks `score-heldrerheidsladder`, `ladder-dim`, `ladder-orde` PASS |
| **B2** perceptuele toets N=1/cueing/niet falsifieerbaar | `EXPERIMENT-01.md` herschreven: fase 0 is expliciet een auteurstest zonder bewijs; fase 1 is een vooraf geregistreerde toets met N ≥ 5 niet-auteurs, gerandomiseerde/blinde condities, drie controleversies (0 ms; 38 s-grain wél beantwoord; programma-only), open vragen vóór gerichte vragen, vooraf vastgelegde coderings- en beslissingsregels, en een falsifieerbare niet-antwoordconditie. De controleversies moeten nog worden gerenderd; zonder die is fase 1 niet geldig. | `EXPERIMENT-01.md` §5 |
| **A-M1** M2 piek vs. RMS | Bindende tabel meet nu **piekamplitude**; band-RMS wordt apart gerapporteerd. PoC: piek −29,96 dBFS (48k) / −29,89 dBFS (44,1k), band-RMS −36,02 dBFS informatief. | `SPECTRASTROBE.md` §3; `verify-report.md` |
| **A-M2** M7 smalbandig programma | Breedbandprobe toegevoegd: witte ruis door dezelfde 8e-orde LPF → demping 17 kHz −19,2 dB, 18,2 kHz −34,0 dB, 19,7 kHz −56,3 dB. Eerlijk als **INFO**: de −60 dBFS-eis is voor breedbandig v1-materiaal niet aangetoond; geen valse PASS. | check `m7-breedband-eerlijk` INFO |
| **A-M3** M6 zijbanden i.p.v. isolatie | Echte probe met één kleur tegelijk; inactieve kleurbanden ≤ −151 dB (meetondergrens; Kasina-decoderbandbreedte blijft open). | check `m6-inter-drager-isolatie` PASS |
| **A-M4** M14 onvolledig/tautologisch | Naïeve SRC, MP3 128 joint stereo en L/R-swap toegevoegd; preserved-rijen toetsen ook kleurfrequenties én verhoudingen. 15 rijen: 9 PASS, 5 EXPECTED-DEGRADED, 1 INFO, 0 ADVISORY-FAIL. Matrix blijft adviserend; alleen de kern M1–M10 bepaalt de exitcode. | `verify-report.md` §corruptiematrix |
| **A-M5** A1-scope overclaim | `ARCHITECTURE-OPTIONS.md` en `README.md` zeggen nu expliciet: A1 dekt de signaalhelft (encoder + integriteit), **niet** de event-engine, het geheugen of de budgetten. | `ARCHITECTURE-OPTIONS.md` §2.A/§3; `README.md` §4 |
| **A-M6** A2 en bevroren CMake | Als open punt geregistreerd (U30): A2 vraagt een apart buildproject of expliciete uitzondering, en hergebruikt het A1-programma niet letterlijk; v1-materiaal is breedbandig en maakt M7 juist uitdagender. | `TECHNICAL-UNCERTAINTIES.md` U30 |
| **A-M7** 44,1/16-bit minder getest | Volledige M4/M7/M8-set draait nu ook op 44,1/16-bit (inclusief extra program-only-44k1). | checks `m4/m7/m8-44k1` PASS |
| **A-M8** reviewclaim/placeholder | Dit bestand bestaat nu; de reviewclaim in `README.md` is gecorrigeerd. | `README.md` §8 |
| **B-M1** uitvoerbare veiligheid | Protocol uitgebreid met R11 (één oog), toestemming/leeftijd/begeleiding, vast playbackniveau en 10 s comfortbeluistering vóór de sessie, stopmiddelen, en geen panic-claim. | `HARDWARE-PROTOCOL.md`; `SAFETY-PRIVACY.md` §4.3 |
| **B-M2** visie overclaimt | "Verandert de vraag" → "scherpt de vraag aan", met expliciete lijst van wat werkelijk nieuw is; succescriteria expliciet een kompas dat per experiment moet worden geoperationaliseerd. | `ARTISTIC-VISION.md` §1/§10 |
| **B-M3** geschreven vs. emergent | `EXPERIMENT-01.md` §2 en §8: de relaties zijn geschreven, niet emergent; de herinnering op 46 s past niet in de 12 s-ring; systeem-autonomie wordt niet getest. | `EXPERIMENT-01.md` |
| **B-M4** audio-reactive mapping/vocabulaire | Mapping is nu expliciet hoogstens één optionele modus, nooit de default; de vocabulairegetallen zijn illustratief en twee alternatieven (continu vs. event-geheugen, budget vs. waarschijnlijkheid) blijven open. | `SAFETY-PRIVACY.md` §3; `ARCHITECTURE-OPTIONS.md` §5 |
| **B-M5** scope/roadmap | `README.md` markeert de kern (visie, experiment, veiligheid) als leidend en de rest als bijlagen; de A–D- en distributed-volgorde zijn expliciet geen backlog of fasering; de activering door Lonn is vastgelegd. | `README.md` §9; `DISTRIBUTED.md` §7 |
| **B-M6** documentintegriteit | Stale "zolang SAFETY-PRIVACY niet bestaat"-teksten weg; M2-definitie rechtgezet; 20 % (score) vs. 25 % (plafond) per drager verduidelijkt; reviewclaim rechtgezet. | PoC-README, `HARDWARE-PROTOCOL.md`, `SPECTRASTROBE.md` |
| **Minors/nits** | O.a. "onhoorbaar" gekwalificeerd (U15), "geen flikkering"-claim genuanceerd (20 Hz-referentiepanning is voorgeschreven en open), "veilig" → "signaalveilig", twee foutieve file:line-citaties gecorrigeerd, typo's, één-oog-noodmaatregel, manifest met script-hashes, timeline afgeleid uit de score. | `git diff 0f6422e..HEAD` |

## 4. Wat bewust open blijft

Deze punten zijn **niet** weggepoetst; ze zijn de volgende poorten.

1. **Hardware (M11–M13).** Geen Kasina/DeepVision beschikbaar. Detectie,
   werkelijke helderheid, LED-uitgang, flikkering en stop-latentie zijn
   ongemeten. De hardwarestap is en blijft een harde poort.
2. **Decoder-helderschaal (U31).** De referentie is per oor gemoduleerd; een
   per-oor-interpretatie kan 25 % in de praktijk groter maken. Alleen met
   hardware/fotodiode te meten.
3. **Host/DAW-keten (U13).** De offline WAV is schoon; DAW-bounce, USB,
   OS-resampling/EQ en lossy weergave zijn niet gemeten. De corruptiematrix is
   een eerste indicatie, geen garantie.
4. **Breedbandige M7 voor A2.** De 8e-orde LPF haalt bij 17 kHz geen −60 dBFS;
   met breedbandig v1-materiaal is extra filtering of een andere bandbegrenzing
   nodig. Nu eerlijk als INFO geregistreerd.
5. **Perceptuele fase 1.** De methode staat, maar de controleversies zijn nog
   niet gerenderd en er zijn nog geen onafhankelijke deelnemers. Zonder die
   uitvoering is er geen bewijs dat de verwantschap werkt.
6. **Cross-platform determinisme.** Alleen op deze machine/CPython getest;
   bit-identieke renders op een tweede platform zijn niet aangetoond (U22).

## 5. Hercontrole na verwerking

De reviewers is gevraagd de blockers en belangrijkste majors opnieuw te
controleren op commit `6c9e831` (delta `0f6422e..6c9e831`). Beiden hebben dat
gericht gedaan, met eigen metingen; de addenda staan onderaan
[REVIEW-A.md](reviews/REVIEW-A.md) en [REVIEW-B.md](reviews/REVIEW-B.md).

| Reviewer | Bijgewerkt verdict | Wat opnieuw is gecontroleerd |
|---|---|---|
| A (technisch) | **APPROVE WITH COMMENTS** | Blocker B1 opgelost (clamp, 20 ms/50 ms-envelopes, stop-claim, ladder), M1–M8 opgelost, v1 onaangetast, eigen PoC-run 52 PASS / 1 INFO / 0 FAIL, eigen adversariële clamptest |
| B (artistiek/veiligheid/scope) | **APPROVE WITH COMMENTS** | B1 en B2 inhoudelijk opgelost; majors M1–M6 opgelost; PoC opnieuw gerenderd/geverifieerd in eigen map; v1-diff leeg |

De hercontrole liet nog een klein aantal niet-blokkerende punten staan. Die zijn
daarna verwerkt: het hardwareprotocol is in lijn gebracht met het herschreven
`EXPERIMENT-01.md` §5 (pre-cue eruit, fase-0-label, fase 1 apart), de
R4-onderbouwing voor de eenmalige 20 ms-extinctie is toegevoegd, de
dragers-only-comforttest heeft nu een echte dragers-only-render, de
coderingsonafhankelijkheid in fase 1 is aangescherpt, en de tellingen/claims in
`README.md` en dit bestand zijn feitelijk gemaakt. Die laatste wijzigingen zijn
niet opnieuw door de reviewers gecontroleerd; ze zijn administratief of
protocol-airtightheid, geen nieuwe inhoudelijke claim.

## 6. Conclusie

Op het reviewpunt waren er twee blockers: één in de veiligheids-/verificatie-
claims en één in het perceptuele ontwerp. Beide zijn in de documenten en de PoC
verwerkt. In de gerichte hercontrole gaven **beide reviewers APPROVE WITH
COMMENTS**: de blockers zijn inhoudelijk opgelost, met eigen metingen als
bewijs. De overgebleven punten zijn verwerkt of staan expliciet als open
restrisico in §4. De belangrijkste inhoudelijke beperking blijft: fase 1 van de
perceptuele toets is nog niet uitgevoerd en de hardware is niet gemeten.

Wat **niet** is veranderd: er is geen v2 gebouwd, geen DSP-contract gewijzigd,
geen releasebranch aangeraakt en geen hardware geclaimd. De eerstvolgende
inhoudelijke stap is een conservatieve hardwareverificatie door Lonn, met
`HARDWARE-PROTOCOL.md` en directe stop. Pas daarna is een artistieke uitspraak
over *Eerste Gloed* mogelijk.
