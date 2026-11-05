# Eerste Gloed — SpectraStrobe proof-of-concept

> **EXPERIMENT — GEEN DEEL VAN NAGLOED v1.0.4.**
> Dit is een losstaand, offline experiment in `experiments/spectrastrobe-poc/`.
> Het is **niet** geïntegreerd in de plug-in, wijzigt **geen** `src/`, CMake,
> presets, README of build, en raakt geen DSP-contract. Het is veilig te
> verwijderen: deze map weghalen is genoeg. Geen medische of wellness-claims.

## Scope — wat dit wel en niet is

- **Wel:** een **encoder + signaalintegriteitsbewijs** voor SpectraStrobe-licht
  in een audio-WAV: vier dragers, kleurkoppeling, panning, bandbegrenzing,
  headroom, helderheidsladder, veiligheidsclamp en een offline meetrapport.
- **Niet:** een event-engine, een licht-scheduler, geheugen/herinnering of
  rate-/feedbackbudgetten. Dat is **fase 2** en de **A1-signaalhelft** van het
  experiment. `score.json` is een declaratieve partituur die de renderer
  uitvoert; er is geen realtime systeem.
- **Niet:** hardware. Of een Kasina dit herkent en of de Ganzfeld de bedoelde
  kleuren laat zien, is hier **niet gemeten** (M11–M13). Daarvoor is
  `HARDWARE-PROTOCOL.md` nodig.
- **Niet:** een geteste stop-ramp. De offline WAV heeft **geen encoder-panic en
  geen 50 ms-softwareramp**; die horen bij een latere realtime encoder
  (`docs/v2/SPECTRASTROBE.md` §6, fase B). De primaire stop is hier menselijk.

## Artistiek uitgangspunt

Issue #14 beschrijft *geborgen geluid*: het geluid van de kamer dat getransformeerd
terugkomt, en licht en geluid die **verwant** zijn zonder dat het ene het andere
illustreert. `docs/v2/EXPERIMENT-01.md` vertaalt dat naar vier momenten en veel
ruimte ertussen; de score volgt die structuur:

| Tijd | Gebeurtenis |
|---|---|
| 0–8 s | Kamer, bijna niets: alleen de referentiedraaggolf en één ver fragment |
| 8–14 s | Amberkleurig lichtgrain start links, drijft naar rechts, wordt iets helderder |
| 14,0 s | Abrupte, geïsoleerde extinctie in **20 ms** (R4) — geen geluid |
| 14,18 s | Antwoord: lage impact, **180 ms later**, andere decay (2,5 s), iets rechts |
| 14–28 s | Diffuus amberrood veld ademt door en dooft; het licht overleeft het geluid |
| 38 s | Zwak lichtgrain dat vanzelf dooft — **opzettelijk onbeantwoord** |
| 46,0 s | Dimmere, tragere, omgekeerde echo van het eerste licht |
| 46,4 s | Omgekeerd en verlaagd fragment (herinnering) |
| 52–72 s | Uitdoving: stilte als einde |

De vertraging van 180 ms is een artistieke keuze, geen latency-bug. Licht en
geluid zijn familie, geen kopie. De score is data (`score.json`); de renderer
voert die met een vaste seed uit. Envelopes volgen R4/R5 uit
`docs/v2/SAFETY-PRIVACY.md`: **attack/release ≥ 20 ms hard**, L1 heeft één
geïsoleerde extinctie van 20 ms, L2 start met een fade-in van 50 ms; geen stap
van zwart naar vol.

## Veiligheid

- **Conservatief licht.** Referentie −30 dBFS piekamplitude. Kleurkanalen zijn
  hard geclampt op **maximaal 25% per drager** van de referentiepiek
  (safety clamp in `render.py`, ook als de score meer vraagt). De score gebruikt
  20%; de hardwareladder begint op **6,25%** (dim), dan 12,5% (mid), dan 20%.
- **Som per oor.** 25% geldt **per drager** (R3). De som van rood+groen+blauw in
  één oor kan hoger zijn; die wordt apart gerapporteerd (in deze score ca. 29,5%
  op de L1-piek). Dat is informatie, geen extra vrijbrief.
- **Geen bedoelde luminantiemodulatie in de kleurkanalen.** Kleurovergangen zijn
  traag; er is één geïsoleerde 20 ms-extinctie. De **20 Hz-referentiepanning is
  door het formaat voorgeschreven** en blijft een open aanname: dat hij niet als
  zichtbare flikkering in de kleurkanalen lekt, is meetpunt M5/M6 en een
  stopcriterium, geen bewezen feit.
- **Direct stoppen.** Stop de weergave, zet de Kasina-helderheid op minimum,
  bril af of **één oog bedekken** (R11). Eén persoon bedient, één persoon kijkt.
  Geen andere flikkerbronnen in de ruimte.
- **Fotosensitiviteit en toestemming.** Geen persoon met (vermoede)
  fotosensitiviteit in de ruimte; kinderen alleen met begeleiding en
  toestemming. Lees vóór de sessie `docs/v2/SAFETY-PRIVACY.md`.
- **Gehoor.** De dragers (18,2–19,7 kHz) zijn **niet gegarandeerd onhoorbaar**:
  dat hangt af van leeftijd, gehoor, transducers en weergaveniveau (open aanname
  **U15** in `docs/v2/TECHNICAL-UNCERTAINTIES.md`). Doe een korte
  comfortbeluistering op het bedoelde niveau vóór de sessie; bij tinnitus,
  hyperacusis of twijfel: lager of niet deelnemen.
- **Headroom.** Eindpiek ruim onder −6 dBFS; geen clipping, geen dither.
- **Geen claims.** Dit is een artistiek experiment, geen therapie, geen
  entrainment en geen medisch hulpmiddel.

## Draaien

Vereist: Python 3 (stdlib). `ffmpeg` is optioneel en wordt alleen voor de
adviserende corruptiematrix gebruikt.

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

Of los:

```bash
python3 render.py --score score.json --out-dir out
python3 verify.py  --score score.json --out-dir out
# optioneel: één exposure met aangepaste kleurschaal
python3 render.py --score score.json --out-dir out --colour-scale 0.3125   # 6,25% max
```

`run.sh` geeft exitcode 0 als alle kernchecks slagen en schrijft naar `out/`:

| Bestand | Inhoud |
|---|---|
| `spectrastrobe-48k-24bit.wav` | volledige mix, 48 kHz/24-bit — technische referentie, **niet op de Kasina geverifieerd** |
| `spectrastrobe-44k1-16bit.wav` | volledige mix, 44,1 kHz/16-bit — **signaalveilige/aanbevolen weergaveversie** (geen gezondheidsclaim) |
| `spectrastrobe-mid-44k1-16bit.wav` | helderheidsladder: 12,5% max per drager |
| `spectrastrobe-dim-44k1-16bit.wav` | helderheidsladder: 6,25% max per drager — **eerste hardware-exposure** |
| `program-only-48k-24bit.wav`, `program-only-44k1-16bit.wav` | A/B zonder dragers |
| `events.csv` | gebeurtenissen, niveaus, panning, relaties |
| `timeline.svg` | tijdlijn licht/audio/draaggolf, afgeleid uit de score |
| `verify-report.json` / `.md` | meetrapport met PASS/FAIL/INFO (M1–M10, clamp, ladder, M14) |
| `render-manifest.json` | versie, sha256 van score en scripts, clamp-info, exposures, output-hashes |

`out/` staat in `.gitignore` en wordt niet gecommit.

## SpectraStrobe-parameters (conservatief)

| Parameter | Waarde |
|---|---|
| Referentie | 18.200 Hz, **−30 dBFS piekamplitude** (bindend M2), continu aanwezig |
| Referentie-panning | sinus, 20 Hz, diepte 100%, offset 0%, per sample |
| Kleuren | rood 18.700 Hz, groen 19.200 Hz, blauw 19.700 Hz |
| Kleurkoppeling | directe kanaalamplitude, lineaire 0-dB-middenpanning; **clamp 25% per drager** |
| Panlaw referentie | gelijkvermogen (constante dragerenergie) |
| Programma | gesynthetiseerd, bandbegrensd met 8e-orde Butterworth op 15,5 kHz |
| Volgorde | programma volledig verwerken → dragers optellen → niets meer |
| Piek | samplepiek en true peak ≤ −6 dBFS/dBTP; geen clipping, geen dither |
| Exposures | dim 6,25% / mid 12,5% / volledig 20% (schaal 0,3125 / 0,625 / 1,0) |

De 44,1 kHz-variant is een **aparte native render** van dezelfde score, niet een
sample-rate-conversie van de master. SRC kan de dragers dicht bij Nyquist
aantasten; de corruptiematrix meet dat effect juist expliciet.

## Wat de test wel en niet bewijst

**Wel (offline, op deze machine, CPython-stdlib):**

- De vier dragers staan op de juiste frequentie (±1 Hz), de referentie op
  −30 dBFS piekamplitude en de kleurkanalen volgen de score; de maximale
  kleurkoppeling over de hele render is ≈20,0% (per drager), met de som per oor
  apart gerapporteerd.
- De **veiligheidsclamp werkt adversarieel**: een score met rgb = 0,90 wordt
  hard op 25% geclampt en de render logt dat.
- De **helderheidsladder** is echt oplopend: dim ≈6,25%, mid ≈12,5%,
  volledig ≈20%, gemeten via probes; de referentie blijft in alle varianten
  gelijk.
- De 20 Hz-panning is per sample aanwezig met de vereiste diepte; de
  referentie is in de "uit"-fase gemeten ≥ 30 dB onder de drempel.
- **Inter-drager-isolatie** is gemeten met een aparte probe (één kleur
  tegelijk): inactieve kleurbanden zijn ver onder −40 dB.
- Er is geen noemenswaardige crosstalk, het programma lekt niet in de
  controlebrand en 17–20 kHz is voor dit smalbandige materiaal ver onder
  −60 dBFS (voor breedbandig v1-materiaal is dat niet aangetoond; zie INFO).
- Er is ruime headroom, geen clipping en de render is deterministisch
  (twee renders byte-identiek, manifest met script-sha256's).
- De volledige M4/M7/M8-set is ook op de **44,1 kHz/16-bit** variant gedraaid.
- De corruptiematrix laat zien wat resampling (net/naïef), lowpass, MP3
  (320/128 joint), AAC, gain, monosom, L/R-verwisseling en rate-mismatch met de
  dragers doen — inclusief de kleurverhoudingen.

**Niet:**

- Of een echte Kasina het signaal herkent en of de DeepVision Ganzfeld de
  bedoelde kleuren/helderheid laat zien (M11–M13). **Hardware is hier niet
  beschikbaar.**
- Of de Kasina de 48 kHz/24-bit master kan afspelen; dat is niet geverifieerd.
  Gebruik voor weergave de 44,1 kHz/16-bit variant (16-bit PCM, geen metadata,
  geen dither) — signaalveilig, geen gezondheidsclaim.
- **Breedbandige v1-materiaalbandbegrenzing**: de −60 dBFS-eis is aangetoond
  voor het spaarzame, laagfrequent gesynthetiseerde materiaal. Een breedbandtest
  (witte ruis door dezelfde 8e-orde LPF) staat als **INFO** in het rapport: de
  filter alleen haalt bij 17 kHz ≈ −19 dB, dus de eis is voor breedbandig
  v1-materiaal **niet** aangetoond. Geen valse PASS.
- Host/DAW-gedrag, weergaveketens, Bluetooth, OS-audio-EQ en streaming buiten
  de matrix.
- Een event-engine, geheugen of budgetgedrag; en of de vertraging als
  verwantschap voelt in plaats van als fout (perceptuele vraag).
- Een geteste software-stop/50 ms-ramp; de primaire stop is menselijk.

## Herkomst

- `gh issue view 14 --repo leonvanbokhorst/nagloed` — artistieke richting.
- `docs/v2/EXPERIMENT-01.md` — de compositie *Eerste Gloed*.
- `docs/v2/SPECTRASTROBE.md` — signaalmodel, meetplan M1–M14, harde encoder-eisen §6.
- `docs/v2/SAFETY-PRIVACY.md` — R1–R12 (envelope-, plafond- en stopregels).
- `docs/v2/TECHNICAL-UNCERTAINTIES.md` — U15 (hoorbaarheid dragers).
- Lumasonic SpectraStrobe-specificatie en productie/mixnotities:
  <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md>
  en <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md>.
- SpiraStrober (MIT, onafhankelijke implementatie):
  <https://github.com/Syluminose/SpiraStrober>.

Licentie: GPL-3.0-or-later, net als NAGLOED.
