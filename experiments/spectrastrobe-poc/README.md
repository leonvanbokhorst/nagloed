# Eerste Gloed — SpectraStrobe proof-of-concept

> **EXPERIMENT — GEEN DEEL VAN NAGLOED v1.0.4.**
> Dit is een losstaand, offline experiment in `experiments/spectrastrobe-poc/`.
> Het is **niet** geïntegreerd in de plug-in, wijzigt **geen** `src/`, CMake,
> presets, README of build, en raakt geen DSP-contract. Het is veilig te
> verwijderen: deze map weghalen is genoeg. Geen medische of wellness-claims.
>
> **Nog niet op de hardware zonder [`HARDWARE-PROTOCOL.md`](HARDWARE-PROTOCOL.md).**

## Artistiek uitgangspunt

Issue #14 beschrijft *geborgen geluid*: het geluid van de kamer dat getransformeerd
terugkomt, en licht en geluid die **verwant** zijn zonder dat het ene het andere
illustreert. `docs/v2/EXPERIMENT-01.md` vertaalt dat naar vier momenten en veel
ruimte ertussen:

| Tijd | Gebeurtenis |
|---|---|
| 0–8 s | Kamer, bijna niets: alleen de referentiedraaggolf en één ver fragment |
| 8–14 s | Amberkleurig lichtgrain start links, drijft naar rechts, wordt iets helderder |
| 14,0 s | Abrupte extinctie (8 ms) — geen geluid |
| 14,18 s | Antwoord: lage impact, **180 ms later**, andere decay (2,5 s), iets rechts |
| 14–28 s | Diffuus amberrood veld ademt door en dooft; het licht overleeft het geluid |
| 38 s | Zwak lichtgrain dat vanzelf dooft — **opzettelijk onbeantwoord** |
| 46,0 s | Dimmere, tragere, omgekeerde echo van het eerste licht |
| 46,4 s | Omgekeerd en verlaagd fragment (herinnering) |
| 52–72 s | Uitdoving: stilte als einde |

De vertraging van 180 ms is een artistieke keuze, geen latency-bug. Licht en
geluid zijn familie, geen kopie: het licht dooft in milliseconden, het geluid
heeft een lange decay. De score is data (`score.json`), de renderer voert die
met een vaste seed uit.

## Veiligheid

- **Conservatief licht.** Referentie −30 dBFS; kleurkanalen maximaal 20% daarvan
  in deze score (harde bovengrens 25%). Geen flikkering in 3–30 Hz: alleen
  langzame kleurovergangen en één extinctie van 8 ms.
- **Direct stoppen.** Stop de weergave, zet de Kasina-helderheid op minimum,
  bril af. Eén persoon bedient, één persoon kijkt. Geen andere flikkerbronnen.
- **Fotosensitiviteit.** Geen persoon met (vermoede) fotosensitiviteit in de
  ruimte. Lees vóór de sessie `docs/v2/SAFETY-PRIVACY.md` (parallel opgesteld);
  zolang dat document er niet is, geldt de checklist in
  [`HARDWARE-PROTOCOL.md`](HARDWARE-PROTOCOL.md).
- **Headroom.** Eindpiek ruim onder −6 dBFS; geen clipping. De dragers zijn
  onhoorbaar hoog (18,2–19,7 kHz) maar verbruiken wel headroom.
- **Geen claims.** Dit is een artistiek experiment, geen therapie, geen
  entrainment en geen medisch hulpmiddel.

## Draaien

Vereist: Python 3 (stdlib). `ffmpeg`/`sox` zijn optioneel en worden alleen voor
de adviserende corruptiematrix gebruikt.

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

Of los:

```bash
python3 render.py --score score.json --out-dir out
python3 verify.py  --score score.json --out-dir out
```

`run.sh` geeft exitcode 0 als alle kernchecks slagen en schrijft naar `out/`:

| Bestand | Inhoud |
|---|---|
| `spectrastrobe-48k-24bit.wav` | master: programma + dragers, 48 kHz/24-bit stereo |
| `spectrastrobe-44k1-16bit.wav` | 44,1 kHz/16-bit variant, **native gerenderd** (geen SRC); **veilige weergaveversie** |
| `program-only-48k-24bit.wav` | A/B zonder dragers |
| `events.csv` | gebeurtenissen, niveaus, panning, relaties |
| `timeline.svg` | tijdlijn licht/audio/draaggolf |
| `verify-report.json` / `.md` | meetrapport met PASS/FAIL (M1–M10, M14) |
| `render-manifest.json` | sha256 per output (determinisme) |

`out/` staat in `.gitignore` en wordt niet gecommit.

## SpectraStrobe-parameters (conservatief)

| Parameter | Waarde |
|---|---|
| Referentie | 18.200 Hz, −30 dBFS piekamplitude, continu aanwezig |
| Referentie-panning | sinus, 20 Hz, diepte 100%, offset 0%, per sample |
| Kleuren | rood 18.700 Hz, groen 19.200 Hz, blauw 19.700 Hz |
| Kleurkoppeling | directe kanaalamplitude, lineaire 0-dB-middenpanning, max 25% (score: 20%) |
| Panlaw referentie | gelijkvermogen, zodat de totale dragerenergie constant blijft |
| Programma | gesynthetiseerd, bandbegrensd met 8e-orde Butterworth op 15,5 kHz |
| Volgorde | programma volledig verwerken → dragers optellen → niets meer |
| Piek | samplepiek en true peak ≤ −6 dBFS/dBTP; geen clipping, geen dither |

De 44,1 kHz-variant is een **aparte native render** van dezelfde score, niet een
sample-rate-conversie van de master. SRC kan de dragers dicht bij Nyquist
aantasten; de corruptiematrix meet dat effect juist expliciet.

## Wat de test wel en niet bewijst

**Wel (offline, op deze machine):**

- De vier dragers staan op de juiste frequentie (±1 Hz), de referentie op
  −30 dBFS en de kleurkanalen volgen de score met de bedoelde helderheid.
- De 20 Hz-panning is per sample aanwezig met de vereiste diepte; de
  referentie is in de "uit"-fase ver onder de drempel.
- Er is geen noemenswaardige crosstalk, het programma lekt niet in de
  controlebrand en 17–20 kHz is ver onder −60 dBFS.
- Er is ruime headroom, geen clipping en de render is deterministisch
  (twee renders byte-identiek).
- De corruptiematrix laat zien wat resampling, lowpass, MP3/AAC, gain, monosom
  en rate-mismatch met de dragers doen.

**Niet:**

- Of een echte Kasina het signaal herkent, of de DeepVision Ganzfeld de
  bedoelde kleuren en helderheid laat zien (M11–M13). **Hardware is hier niet
  beschikbaar.** Ook is **niet geverifieerd of de Kasina de 48 kHz/24-bit
  master kan afspelen**; gebruik voor weergave de **44,1 kHz/16-bit variant**
  (16-bit PCM, geen metadata, geen dither). De 48 kHz/24-bit master is de
  technische referentie, niet de bewezen weergavevorm.
- Of de vertraging als verwantschap voelt in plaats van als fout; dat is een
  perceptuele vraag, geen meting.
- Host/DAW-gedrag, weergaveketens, Bluetooth, OS-audio-EQ en streaming.
- Een uitspraak over de v2-architectuur. Dit is één klein experiment, niets meer.

## Herkomst

- `gh issue view 14 --repo leonvanbokhorst/nagloed` — artistieke richting.
- `docs/v2/EXPERIMENT-01.md` — de compositie *Eerste Gloed*.
- `docs/v2/SPECTRASTROBE.md` — signaalmodel, meetplan M1–M14 en de harde
  encoder-eisen (§6) die deze PoC volgt.
- Lumasonic SpectraStrobe-specificatie en productie/mixnotities:
  <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md>
  en <https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md>.
- SpiraStrober (MIT, onafhankelijke implementatie):
  <https://github.com/Syluminose/SpiraStrober>.

Licentie: GPL-3.0-or-later, net als NAGLOED.
