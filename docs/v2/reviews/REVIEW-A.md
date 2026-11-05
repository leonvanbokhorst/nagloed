# Onafhankelijke technische review A — NAGLOED v2-verkenning

> **Reviewer:** reviewer A (technische haalbaarheid en signaalintegriteit).
> **Niet betrokken** bij het opstellen van de verkenning of de PoC.
> **Branch:** `feature/v2-exploration`, commit `983d48c` (basis: `develop`, v1.0.4).
> **Datum:** 2026-11-05. **Omgeving:** macOS, CPython 3.14.5, geen numpy/scipy.
> **Scope:** `docs/v2/*`, `experiments/spectrastrobe-poc/*`, issue #14, en de
> v1.0.4-basislijn in `src/`.
>
> Elke bewering in dit reviewbestand is door mij opnieuw gemeten of in de bron
> nagekeken. Waar ik iets niet kon verifiëren, staat dat expliciet.

---

## 1. Verdict

**REQUEST CHANGES.**

Eén blocker en acht major-bevindingen. Het **audiosignaal zelf is schoon en
veilig**: ik heb alle carriers, niveaus, de panmodulatie, de pieken, de
determinisme en de bandbegrenzing onafhankelijk nagerekend en die kloppen. De
blokkerende punten zitten in de **veiligheids- en verificatieclaims**: de PoC
zegt de harde encoder-eisen uit `SPECTRASTROBE.md` §6 te volgen, maar overtreedt
er drie, en de eigen meetsuite beloont één overtreding. Daarnaast zijn M2, M6,
M7 en M14 niet de metingen die de bindende §3-tabel voorschrijft.

De wijzigingen zijn klein en concreet (clamp toevoegen of §6 opsplitsen,
meetmethoden rechtzetten, claims intrekken). Na verwerking is dit wat mij betreft
een APPROVE. De hardwarestap met de bestaande WAV is technisch verantwoord; dat
staat los van dit verdict.

---

## 2. Basislijn v1.0.4 — onaangetast, zelf gecontroleerd

| Controle | Commando | Resultaat |
|---|---|---|
| Working tree | `git status --short --branch` | alleen `?? docs/v2/`, `?? experiments/` |
| Diff t.o.v. develop | `git diff develop --stat` | leeg |
| v1-oppervlak | `git diff develop -- src CMakeLists.txt CMakePresets.json presets tests scripts .github README.md .gitignore` | leeg (exit 0) |
| Tests | `./build/nagl_tests_artefacts/Release/nagl_tests` | **167 checks, 0 failures**, exit 0 |

De testoutput bevestigt bovendien de architectuurfeiten die de documenten
claimen: `audio-thread allocations: 0`, `reported latency 51 samples`,
identieke RMS `0.068621` voor blokgroottes 128/512/4096, en CPU 2,04 % van één
core. Die claims in `docs/v2/README.md` §4.1 en `ARCHITECTURE-OPTIONS.md` §1.3
zijn dus correct.

De PoC draait: `cd experiments/spectrastrobe-poc && ./run.sh` → **exit 0**,
31 PASS / 0 FAIL, corruptiematrix 12 rijen (8 PASS, 3 EXPECTED-DEGRADED,
1 INFO, 0 ADVISORY-FAIL).

---

## 3. Wat ik zelf heb geverifieerd (eigen metingen, geen hergebruik van verify.py)

Ik heb vier eigen scripts geschreven (pure Python-stdlib, eigen FFT/Goertzel,
eigen RIFF-parser) en de WAV's opnieuw doorgemeten. Reproduceerbare kerncijfers:

| Meting | Specificatie | Mijn meting | Oordeel |
|---|---|---|---|
| 48k/24-bit RIFF-chunks | geen metadata | alleen `fmt ` (16 B) en `data` | OK |
| Carrierfrequenties 48k (eigen FFT 65536, Hann, parabolisch) | 18200/18700/19200/19700 ±1 Hz | 18199,99 / 18700,04 / 19199,97 / 19699,99 Hz | OK |
| Carrierfrequenties 44,1k/16-bit | idem | 18200,03 / 18700,03 / 19200,04 / 19700,04 Hz | OK |
| Referentie-piekamplitude | −30,0 dBFS ± 0,5 | exact −30,00 gerenderd; verify (5 ms) −29,96; mijn 1 ms-schatter −29,69 (bekende bias) | OK |
| Referentie-RMS (letterlijke §3-M2-methode) | −30,0 dBFS ± 0,5 | **−33,0 dBFS** (gecombineerd 2-kan.) | **wijkt 3 dB af** |
| Kleurverhouding t.o.v. referentie (4 probes) | volgt score ±0,5 dB | max afwijking 0,33 dB | OK |
| Max. kleurkoppeling (echte piek, niet probe) | ≤ 25 % | **20,0 %** (L1 rood, t=13,99 s) | OK, maar hoger dan gerapporteerd (18,6 %) |
| Som 3 kleurdragers in één oor (L1-piek) | ≤ 25 %? | **29,5 %** | definitie onduidelijk |
| Pan-LFO | 20 Hz ± 0,5, diepte ≥ 99 % | **20,000 Hz**, diepte 100 %, balance ±1,000 | OK |
| Gelijkvermogen-panning | totale dragerenergie constant | √mean(aL²+aR²) = −29,995 dBFS, constant | OK |
| Sample-peak 48k / 44,1k / programma | ≤ −6 dBFS, geen clipping | −17,399 / −16,960 / −19,099 dBFS, 0 samples op de rail | OK |
| True peak (4×, 32-taps, 14–18 s) | ≤ −6 dBTP | −17,399 dBTP (= sample-peak) | OK |
| Programma 17–20 kHz | ≤ −60 dBFS | −162 dBFS (maar zie M7-bevinding) | niet bewijzend |
| 8e-orde lowpass @15,5 kHz (impulsrespons) | ≥48 dB/oct bij 16 kHz | −3,01 @15,5k; −16,22 @17k; −30,84 @18,2k; −53,22 @19,7k; −108 @22k | steil genoeg, maar zie M7 |
| Metadata / dither | geen metadata, geen dither | 27.430 (24-bit) en 25.200 (16-bit) samples exact gelijk aan `clamp(round(float·schaal))` | OK |
| 44,1k is native (geen SRC) | native render | 16-bit waarden exact gelijk aan de float-render op 44,1 kHz | OK |
| Determinisme | twee renders byte-identiek | twee verse renders: alle 6 bestanden identiek; ook identiek aan `out/` | OK |
| Manifest | sha256 per output | alle hashes kloppen; geen tijdstempel | OK |
| Referentie lead-in | ≥ 1 s | referentie continu vanaf sample 0 (250 ms fade-in), eerste licht op 8,0 s | OK |
| L1-extinctie klikvrij | geen discontinuïteit | max \|x[n]−x[n−1]\| = 0,047 bij 14,0 s vs normale carrier-helling 0,074 | OK |

De 44,1 kHz-variant is dus aantoonbaar een **echte native render**: de
16-bit-waarden komen op elke gecontroleerde sample exact overeen met
`clamp(round(float·32767))` van de float-render op 44,1 kHz. Een SRC van de
48 kHz-master zou dat niet doen. Ook de eis "geen dither" is hiermee hard
gemaakt (dither zou tot 1 LSB afwijking geven; ik zie 0).

Externe bronnen: ik heb de Lumasonic *format specification* en
*production-and-mixing* opgehaald. Die bevestigen de vier frequenties, de eis van
"sine, 20 Hz, offset 0 %, amplitude 100 %" op de referentiepanning, het
6-kanaalsmodel (drie stereo kleurtracks), de "4 stereo tracks, elk −32 tot
−24 dBFS"-headroom, en het advies om het programma vanaf 13 kHz te taperen met de
steilste cutoff rond 17 kHz. De documenten citeren die bronnen correct.

Steekproef van ±40 `file:line`-verwijzingen in `ARCHITECTURE-OPTIONS.md` en
`TECHNICAL-UNCERTAINTIES.md`: vrijwel alle kloppen (o.a. `EngineDSP.cpp:600-664`
bevat inderdaad de ±2,0-clamp op regels 643–644; `getVisual` op 671–701;
`kControlInterval = 16` op `EngineDSP.h:73`; `PluginProcessor.h:32`
`acceptsMidi() { return false; }`). Twee verwijzingen zijn fout, zie m12.

**De claim "VisualState is een displaykanaal, geen eventbus" is correct.**
`src/core/Contracts.h:27-57` bevat alleen een 256-record `GrainView`-array,
RMS/peak-scalars en een `frame`-teller; geen tijdstempel per event, geen
identiteit, geen geschiedenis. `Snapshot.h:9-32` is een dubbele buffer die elke
blok wordt overschreven. De A1/A2-aanbeveling (eerst offline, dan pas in-plugin)
is architectonisch de juiste volgorde.

---

## 4. Bevindingen

### BLOCKER

**B1 — De PoC claimt `SPECTRASTROBE.md` §6 te volgen, maar overtreedt §6 op drie
punten; de eigen meetsuite beloont één overtreding.**

`experiments/spectrastrobe-poc/README.md:132` zegt de "harde encoder-eisen (§6)"
te volgen; `SPECTRASTROBE.md:284-286` noemt §6 letterlijk "de
implementatie-opdracht voor `experiments/spectrastrobe-poc/`". Drie eisen worden
niet gehaald:

1. **Kleurplafond 25 % wordt niet afgedwongen.** `SPECTRASTROBE.md:298-301` en
   `:335-339`: "De encoder begrenst zichzelf, ook als de artistieke score meer
   vraagt: max 25 % koppeling". `render.py:336-342` doet
   `amp = amp_ref * brightness` zonder clamp; `max_colour_ratio` wordt alleen in
   het manifest gekopieerd (`render.py:653`) en geprint (`render.py:681-682`).
   Eigen test: score met `rgb = 0.90` → gemeten rooddrager ≈ 90 % van de
   referentiepiek (3,6× het plafond). De score van *Eerste Gloed* blijft met
   20,0 % binnen het plafond, dus het artifact is nu veilig; de claim "de encoder
   begrenst zichzelf" is onwaar. `SAFETY-PRIVACY.md:102` (R3) zegt bovendien
   "het plafond gaat vóór artistieke wens".
2. **Envelope-eis ≥ 20 ms wordt overtreden en de check keert hem om.**
   `SPECTRASTROBE.md:321-322` ("gladde attacks/releases, ≥ 20 ms, geen stappen")
   en `SAFETY-PRIVACY.md:103` (R4). `score.json:90` laat L1 in **8 ms** van 1,0
   naar 0,0 lopen; `score.json:101` laat L2 met een **attack van 0 ms** op 0,35
   beginnen (een stap van 0 naar 0,35 × 0,055 = 1,9 % helderheid).
   `verify.py:502-504` keurt de extinctie expliciet goed met het criterium
   "≤ 10 ms" — precies het tegenovergestelde van de harde eis. Er is geen enkele
   check op envelope-continuïteit, R4 of R5.
3. **Panic-stop ≤ 50 ms bestaat niet.** `SPECTRASTROBE.md:323-324` eist dat
   dragers en referentie binnen 50 ms klikvrij naar nul gaan. In `render.py`
   staat geen enkele stop/panic-functie (grep op `panic|stop` levert alleen
   `fade_out`); de enige stop is de 400 ms uitfade aan het einde
   (`score.json:23`). `HARDWARE-PROTOCOL.md:48` test de stop van de *speler*,
   niet van de encoder. Voor een offline WAV is een echte panic-stop niet
   toepasbaar; dan moet §6 dat expliciet naar fase B schuiven in plaats van de
   eis als "gehaald" te presenteren.

*Waarom blocker:* dit is de veiligheidsparagraaf van het enige document dat de
hardwarestap autoriseert. Een onware compliantieclaim op een veiligheidsregel
hoort niet in een verkenning die zichzelf als "conservatief, geen overclaims"
positioneert. De fix is klein: clamp + hard-fail in `render.py`, envelopes
verzoenen (of het 8 ms-gebaar expliciet als eenmalige uitzondering op R4
documenteren), en de stop-eis naar fase B verplaatsen.

### MAJOR

**M1 — M2 wordt anders gemeten dan de bindende §3-tabel voorschrijft.**

`SPECTRASTROBE.md:168` schrijft voor: "RMS van 18,2 kHz-band, −30,0 dBFS
± 0,5 dB". `verify.py:566-595` meet echter de **piek van de complexe
hypot-envelope** (−29,96 dBFS) en noemt de RMS alleen ter informatie. Mijn eigen
meting van de letterlijke band-RMS (som over L en R) is **−33,00 dBFS**;
verify.py zelf rapporteert "totale RMS −32,97 dBFS". Een implementatie die M2
letterlijk volgens de tabel uitvoert, zou dus **FAILEN** met 3,0 dB. De oorzaak
is fysisch: bij equal-power panning is de som-RMS van een drager met
piekamplitude A gelijk aan A/√2 (−3,01 dB). De §6-definitie ("−30 dBFS
piekamplitude", `SPECTRASTROBE.md:292`) is wel gehaald; de §3-tabel is fout of
onduidelijk. Omdat §3 "bindend" heet, moet het een van beide worden: de tekst
naar "piekamplitude" wijzigen, of de RMS echt meten en de tolerantie aanpassen.

**M2 — M7 is inhoudelijk leeg: het programma heeft geen hoogfrequent inhoud.**

`SPECTRASTROBE.md:173` eist ≤ −60 dBFS in 17–20 kHz voor de programma-only
render. De PoC haalt −162 dBFS, maar dat komt niet door de lowpass: het
gesynthetiseerde programma is intrinsiek smalbandig. Eigen bandanalyse van
`program-only-48k-24bit.wav`: 100–1000 Hz = −50,9 dB, 1–5 kHz = −110,9 dB,
5–10 kHz = −157,9 dB, 14–16 kHz = −164,4 dB, 17–20 kHz = −162,3 dB. Er is dus
niets om te onderdrukken.

De gebruikte filter is wél gemeten (impulsrespons van de eigen code):
−3,01 dB @15,5 kHz, −16,22 @17 kHz, −30,84 @18,2 kHz, −53,22 @19,7 kHz. Dat is
steil (bilineaire frequentie-warping maakt hem bij Nyquist veel steiler dan de
analoge 8e-orde), maar voor **breedbandig** programmamateriaal is −60 dBFS in
17–20 kHz niet aangetoond en waarschijnlijk niet haalbaar: bij 17 kHz is de
demping 16 dB. Dit raakt direct A2, want v1 heeft breedbandige bronnen
(`src/dsp/GrainEngine.cpp:408-411`, noise/drone; U09). Ook §6's "taper vanaf
13 kHz" is niet gehaald: bij 13 kHz is de respons −0,02 dB. `verify.py` meet M7
alleen op deze ene, toevallig smalbandige render; de eis is dus niet getest maar
"toevallig gehaald". Voeg een breedbandtest toe (ruis of een echte v1-render) en
herformuleer de eis.

**M3 — M6 meet niet wat hij zegt: het is zijbandenergie, geen inter-drager-crosstalk.**

`SPECTRASTROBE.md:172` vraagt "bandenergie op de andere dragers ± 100 Hz".
`verify.py:651-667` vergelijkt echter binnen **dezelfde** drager de ±100 Hz-band
met de ±5 Hz-band (`leak = broad − narrow`). Dat is de energie in de zijbanden
rond de eigen drager, niet de energie op de andere dragers (die 500 Hz verder
liggen en dus buiten ±100 Hz vallen). Omdat de dragers pure sinussen zijn, is
inter-drager-crosstalk structureel nul; mijn meting in een kleur-loos venster
(29,5 s) geeft rood/groen/blauw op **−188/−180/−193 dBFS**. De gerapporteerde
"crosstalk −58,5 dB" is dus een verkeerd gelabelde maat die echte kanaallekkage
per definitie niet kan detecteren. Een correcte M6 rendert één kleur tegelijk en
meet de andere dragerbanden.

**M4 — M14 (corruptiematrix) dekt de bindende §4.4 niet en toetst de kleuren niet.**

`SPECTRASTROBE.md:249-263` somt tien varianten op en `:180` eist "elk geval
gedocumenteerd als pass/fail". Van die tien ontbreken er drie: goedkope/naïeve
SRC, L/R verwisseld, en MP3 128 kbps/joint stereo; AAC 256 is toegevoegd maar
staat niet in de tabel. Belangrijker: de matrix **controleert de kleurdragers
niet**. `verify.py:871-878` (`preserved`) eist voor frequenties alleen
`hz == hz and abs(hz - round(hz)) < 5000` — dat is voor elke plausibele
frequentie tautologisch waar (een fout van 1 kHz zou nog steeds slagen). De
kleuramplitudes en -verhoudingen worden helemaal niet getoetst. Daarmee is de
conclusie "MP3 320 / AAC 256 intact" niet onderbouwd voor de kanalen die de
helderheid dragen, terwijl juist U12/U13 (overleeft het signaal de keten?) de
harde poort is. Bovendien is de matrix adviserend en draait `run.sh:17`
`verify.py` zonder `--strict-corruption`, dus de exitcode blijft 0 ook als elke
rij faalt.

**M5 — A1 wordt geclaimd als "uitvoerbare vorm", maar de PoC implementeert alleen
de renderer + encoder.**

`ARCHITECTURE-OPTIONS.md:245-252` en `docs/v2/README.md:173-178` definiëren A1
als: declaratieve score, deterministische renderer, **licht-engine**, en een
SpectraStrobe-encoder. `ARCHITECTURE-OPTIONS.md` §5 koppelt daaraan een
eventvocabulaire van acht kinds (`audio_grain`, `light_grain`,
`score_gesture`, `rest`, `remember`, `forget`, `echo`, `ignore`) dat A en
later B hetzelfde moet laten praten. De PoC bevat daarvan vrijwel niets: het is
een vaste, handgeschreven tijdlijn met vier licht- en drie
audiogebeurtenissen — geen licht-engine met eigen toestand, geen scheduler, geen
budget, geen refractaire periode, geen `remember`/`forget`/`echo`/`ignore`, geen
invariantencheck. Harde poort 3 ("aantoonbaar begrensde anti-runaway-scenario's",
`ARCHITECTURE-OPTIONS.md:269`, `docs/v2/README.md:186-187`) is daardoor in deze
fase niet toetsbaar. Dat is geen verwijt aan de PoC — die is nuttig en goed
gemaakt — maar wel een scope-overclaim in de documenten. Benoem expliciet dat
A1 momenteel alleen de signaalintegriteit-helft dekt.

**M6 — A2 is onder-specificeerd en botst met de eigen bevriezing.**

`ARCHITECTURE-OPTIONS.md:253-257`: A2 is "een headless render-tool in de geest
van `tools/render_main.cpp` ... Geen wijziging aan `src/`; hooguit een nieuw
tool-bestand in de verkenning." Maar tools worden via `CMakeLists.txt:112-143`
gebouwd, en die staat op de niet-aanrakenlijst (`ARCHITECTURE-OPTIONS.md:292`).
Een nieuw target vereist dus óf een CMake-wijziging óf een apart buildproject;
dat is een onopgeloste tegenstrijdigheid. Daarnaast kan A2 het A1-programma niet
hergebruiken: A1 synthetiseert zijn eigen sweep + gefilterde ruis
(`render.py:183-218`), v1 heeft GrainEngine-bronnen. "Dezelfde score door de
echte v1-processor" levert dus ander materiaal, niet hetzelfde. En de
M2/M7-analyse hierboven maakt A2 met breedbandig v1-materiaal juist het
risicovolle deel. Werk het buildpad en de materiaalmapping uit vóór A2.

**M7 — De aanbevolen weergaveversie wordt minder geverifieerd dan de master.**

`verify.py:1216-1217` draait `analyse_core` met `exhaustive=True` alleen op de
48k-master; de 44,1k/16-bit-variant slaat M4 (isolatie), M7 (programmaleak) en
M8 (ruisvloer) over. Dat is precies het bestand dat volgens
`experiments/spectrastrobe-poc/README.md:73` en `HARDWARE-PROTOCOL.md:34` als
"veilige, aanbevolen weergaveversie" naar de microSD-kaart gaat. Juist
bij 16-bit is de ruisvloer in de kleurbanden (M8) relevant. Draai de volledige set op beide
bestanden.

**M8 — Documentatie-integriteit: `REVIEWS.md` bestaat niet en er staat een
letterlijke placeholder in de README.**

`docs/v2/README.md:308-311` stelt in de tegenwoordige tijd dat de verkenning
"onafhankelijk is gereviewd door twee reviewers" en verwijst naar
`REVIEWS.md`; dat bestand bestaat niet, en de regel eindigt op
`[REVIEW-PLACEHOLDER]`. Op het moment van schrijven bestaan wel de twee
reviewbestanden (`REVIEW-A.md`, `REVIEW-B.md`), maar is `REVIEWS.md` nog niet
aangemaakt. Dit is eenvoudig te repareren, maar het is wel een claim over
afgerond werk dat nog niet af is.

### MINOR

- **m1 — "onhoorbaar" als feit.** `experiments/spectrastrobe-poc/README.md:47`
  zegt "De dragers zijn onhoorbaar hoog". `TECHNICAL-UNCERTAINTIES.md:55` (U15)
  noemt diezelfde hoorbaarheid juist **open** en noemt leeftijd, gehoor,
  transducers en gehoorschade. De PoC-tekst moet de U15-qualificatie overnemen.
- **m2 — "Geen flikkering in 3–30 Hz" is ongenuanceerd.**
  `experiments/spectrastrobe-poc/README.md:37-39`. Mijn meting: het bestand
  bevat een **20,000 Hz, 100 %-diepe L/R-modulatie** van de referentie
  (balance ±1,000). Dat is de door de format-specificatie vereiste
  referentiepanning, geen kleurhelderheid — maar `SAFETY-PRIVACY.md:100-103`
  (R1/R2) formuleert een harde grens in precies die band zonder uitzondering
  voor de referentie, en `SAFETY-PRIVACY.md:70-73` noemt het risico wel maar
  meet het niet. Of de decoder dit als helderheidsmodulatie doorlaat is
  ongemeten (zie §6). Formuleer de claim als "geen *bedoelde*
  luminantiemodulatie in de kleurkanalen" en registreer de referentiepanning als
  open aanname.
- **m3 — "veilig" in EXPERIMENT-01.** `docs/v2/EXPERIMENT-01.md:24`: "Het is
  **veilig**". `SAFETY-PRIVACY.md:111` (R12) verbiedt de woordkeuze "veilig"
  expliciet. Ook al is de context "geen realtime licht/netwerk/microfoon", de
  eigen regel wordt hiermee overtreden.
- **m4 — "ver onder de drempel" is te sterk.**
  `experiments/spectrastrobe-poc/README.md:106`. De gemeten rust is −38,2 dB bij
  een drempel van −30 dB: 8 dB marge, niet "ver".
- **m5 — Twee verschillende tolerantiesets voor hetzelfde experiment.**
  `EXPERIMENT-01.md:81-90` hanteert "volgt de score binnen enkele dB",
  "voldoende onderdrukt" en "20 Hz detecteerbaar"; `SPECTRASTROBE.md:165-180`
  (bindend) hanteert ±0,5 dB, ≤ −60 dBFS en diepte ≥ 99 %. Een lezer kan per
  document een ander PASS halen. Laat EXPERIMENT-01 naar de bindende tabel
  verwijzen in plaats van eigen, lossere criteria te noemen.
- **m6 — M4-probes zijn tautologisch.** `verify.py:686-714` meet de
  middenbalans op L2 rond 16,05 s, waar de scorepan exact 0 is, en de
  hard-pan-isolatie op L1 8,5–8,65 s, waar `gain_r` exact 0 is. De test
  bevestigt de scorewaarden, niet de kanaalisolatie van de encoder.
- **m7 — M3 rapporteert niet de echte maximumkoppeling.** Het rapport noemt
  18,6 %, de werkelijke piek is 20,0 % (L1, t=13,99 s). De vier probes liggen
  niet op de enveloppiek. Scan de hele render of ten minste de enveloppiek.
- **m8 — Definieer "25 %" per drager of per oor.** Mijn meting: de som van de
  drie kleurdragers in één oor bereikt **29,5 %** van de referentie op het
  L1-hoogtepunt (0,20+0,075+0,02). De 25 %-cap geldt nu per drager; de
  tekst leest op sommige plaatsen als een totaalplafond.
- **m9 — Scorevelden en functies die niet gebruikt worden.**
  `score.json:19` `pan_lfo_offset` wordt door `render.py` niet gelezen;
  `render.py:112-126` definieert `pan_equal_power` en `pan_linear_0db` met
  uitgebreide docstrings, maar de kleurenpanning wordt inline herimplementeerd
  (`render.py:330-335`). De gedocumenteerde pan-law-functie is dus niet de
  uitgevoerde. Bind de kleuren aan `pan_linear_0db` of verwijder de dode functie.
- **m10 — Manifest identificeert de code niet.** `render.py:638-666` bevat
  `"generator": "render.py"` en de score-hash, maar geen versie of hash van
  `render.py` zelf. Een codewijziging is later niet herleidbaar uit het manifest.
- **m11 — Hardgecodeerde aantallen in de tijdlijn.** `render.py:514` zet
  "4 lichtgebeurtenissen · 3 audiogebeurtenissen" en `render.py:502-504`
  hardgecodeerde sectielabels. Bij een andere score klopt de SVG niet meer.
- **m12 — Twee foutieve `file:line`-verwijzingen.**
  `ARCHITECTURE-OPTIONS.md:296` verwijst naar `CONTRIBUTING.md:6-15`; het
  bestand is `docs/CONTRIBUTING.md` (inhoud op 6-15 klopt). Idem
  `ARCHITECTURE-OPTIONS.md:293` `preview_main.cpp:235-388`; het bestand is
  `tools/preview_main.cpp` (395 regels; bereik bestaat). De overige ±40
  steekproefverwijzingen kloppen.
- **m13 — M5-rust is een analyse-artefact.** De uit-fase is op de LFO-extreme
  exact nul; de gerapporteerde −38,2 dB is hoe dicht een 1 ms-blok bij de
  extreme komt. Robuust over blokgroottes (B=24…192: −38,2 tot −61,4 dB, altijd
  PASS), maar het is geen restdrager-meting. Benoem de meetconventie.
- **m14 — De activering door Lonn is niet vastgelegd.** Issue #14 heeft
  **0 comments** en de documenten stellen "de verkenning is expliciet geopend
  door Lonn" (`docs/v2/README.md:6-7`). Als dat klopt is het een procespunt;
  leg de activering vast in de issue, want de issue zelf zegt dat alleen Lonn
  dat kan en dat er niet standaard research-subagents voor gestart mogen worden.

### NITS

- `docs/v2/EXPERIMENT-01.md:106`: typefout "hetgeluid".
- `experiments/spectrastrobe-poc/README.md:43-45` en
  `HARDWARE-PROTOCOL.md:18-20` spreken nog over `SAFETY-PRIVACY.md` als
  "parallel opgesteld / zolang dat document er niet is"; het bestaat inmiddels.
- `render.py:293-294` leest `pan_lfo_shape` maar niet `pan_lfo_offset`; de
  score bevat dat veld wel.

---

## 5. Positieve punten (kort)

- v1.0.4 is aantoonbaar onaangetast; 167/167 tests groen; de verkenning zit
  volledig in nieuwe, losstaande mappen.
- Het audiosignaal is schoon en reproduceerbaar: frequenties binnen 0,04 Hz,
  niveau exact −30 dBFS piek, kleurverhoudingen binnen 0,33 dB van de score,
  per-sample 20 Hz gelijkvermogen-panning, geen clipping, geen metadata, geen
  dither, byte-identieke rerenders.
- De 44,1 kHz-variant is een echte native render, geen SRC — dat is met de
  quantisatie-gelijkheid hard aangetoond, niet alleen beweerd.
- De volgorde "programma eerst volledig verwerken, daarna dragers, daarna niets"
  is in de code afgedwongen (`render.py:353-367`) en klopt.
- Goede documenthygiëne: `[bewijs]/[inferentie]/[open]`-markers, bronlinks,
  open vragen, faalmodi, een hardwareprotocol met stopcriteria en een
  expliciete "wat dit niet bewijst"-sectie. Geen wellness- of
  therapieclaims. De architectuurfeiten over v1 kloppen.
- De A1-eerst-aanbeveling is de juiste risicovolle stap; niets in de PoC raakt
  de plugin, CMake, presets of release.

---

## 6. Wat ik niet heb geverifieerd

- **Hardware (M11–M13)**: geen Kasina/DeepVision beschikbaar. Detectie,
  helderheid, kleur, flikkering, stop-latentie en de fotodiode-meting zijn
  ongemeten.
- **Host/DAW-gedrag en weergaveketen**: geen DAW-bounce, geen audio-interface,
  geen Bluetooth/lossy weergave. De corruptiematrix van `verify.py` heb ik via
  `run.sh` zien draaien, maar de MP3/AAC-uitkomsten niet onafhankelijk
  nagerekend.
- **Decoder-gedrag**: passband, amplitudeschaal, gedrag bij plotseling stoppen,
  en of de 20 Hz-referentiepanning als luminantiemodulatie doorkomt.
- **Cross-platform determinisme**: alleen op deze machine en CPython 3.14.5
  getest; libm-verschillen tussen platformen zijn niet onderzocht (U22).
- **Volledige C++-build vanaf nul**: ik heb de bestaande Release-binary
  gebruikt.
- **Externe bronnen deels**: de Lumasonic format- en productiedocumenten heb ik
  opgehaald en de kernclaims kloppen; het forumconflict over 1:1 vs 1:4, het
  "dithering uit"-advies, de SpiraStrober-code en de Kasina-firmwareclaims heb
  ik niet geverifieerd.
- **Artistieke en veiligheidsafwegingen buiten het signaal**: dat is het terrein
  van de parallelle reviewer (REVIEW-B).

---

## 7. De drie belangrijkste resterende onzekerheden

1. **Hoe leidt de decoder de helderheidsschaal af uit een 20 Hz-panned
   referentie?** De spec zegt "amplitude van de kleur ten opzichte van de
   referentie", maar de referentie is per oor hard L/R gealterneerd. Meet de
   decoder per oor, dan is de referentie per oor continu in amplitude
   gemoduleerd en op de uitersten nul, en wordt de 25 %-koppeling in de praktijk
   veel groter (mijn meting: 31,5 % bij
   per-oor-RMS-interpretatie). Meet hij mono/piek, dan is 20 % conservatief. Dit
   bepaalt zowel de artistieke band als de flikkerveiligheid, en het is nergens
   als aanname geregistreerd.
2. **Overleeft het signaal de echte weergaveketen?** De offline WAV is schoon,
   maar de keten Kaart/Kasina-DAC/microSD, 44,1 vs 48 kHz en 16 vs 24-bit
   zijn ongemeten, en de bestaande corruptiematrix toetst de kleurkanalen niet. De
   U12/U13-poort is dus nog open, ook al lijkt hij met "PASS" afgedekt.
3. **Haalt A2 met echt v1-materiaal de carrierbescherming?** De gemeten
   programma-lowpass geeft −16 dB bij 17 kHz en −31 dB bij 18,2 kHz; v1 heeft
   breedbandige noise/drone-bronnen. Met een breedbandig programma is de
   −60 dBFS-eis in 17–20 kHz niet vanzelf gehaald, en het buildpad voor A2
   (nieuw tool-target vs. bevroren `CMakeLists.txt`) is nog niet opgelost.

---

## 8. Aanbevolen acties, in volgorde

1. **B1**: implementeer de 25 %-clamp (of hard-fail) in `render.py` en een
   globale M3-scan; verzoen §6/R4 met de 8 ms- en 0 ms-envelopes (documenteer
   een expliciete eenmalige uitzondering of pas de score aan); verplaats de
   panic-stop-eis naar fase B en zeg dat in §6.
2. **M1/M3/M4**: herstel de M2-methode (RMS of piek, consistent met de tabel),
   implementeer M6 als echte inter-drager-isolatie, en breid M14 uit naar alle
   tien §4.4-varianten inclusief kleurtoetsing; maak de integriteitsrijen
   niet-adviserend.
3. **M2/M6**: voeg een breedbandige M7-test toe (ruis of een echte v1-render) en
   werk het A2-buildpad en de materiaalmapping uit.
4. **M5/M8**: scope A1 expliciet als "encoder + signaalintegriteit, nog geen
   event-engine", en vul `REVIEWS.md` / verwijder de placeholder.
5. **m1–m5, m12**: trek de overclaims in ("onhoorbaar", "geen flikkering in
   3–30 Hz", "veilig"), trek de toleranties gelijk en fix de twee citaties.
