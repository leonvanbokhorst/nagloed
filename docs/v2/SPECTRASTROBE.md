# SpectraStrobe — signaalmodel, signaalintegriteit en hardwareverificatie

> **Status:** onderzoeksdocument bij de v2-verkenning (issue #14), **buiten scope
> van v1.0.4**. Geen implementatievoorstel voor `src/`, presets of release. Dit
> document is de autoriteit voor het meetplan en het hardwareprotocol waarnaar
> `docs/v2/EXPERIMENT-01.md` en `docs/v2/ARCHITECTURE-OPTIONS.md` verwijzen.
>
> **Leeswijzer:** elke bewering is gemarkeerd als **[bewijs]** (direct in een
> primaire bron of in code), **[inferentie]** (technisch onderbouwd, niet direct
> bevestigd) of **[open]** (niet vast te stellen met de huidige bronnen; moet
> gemeten worden). Bronlinks staan in §9.

---

## 1. Signaalmodel

### 1.1 Dragers

SpectraStrobe codeert lichtbesturing als vier hoogfrequente sinussen in een
gewone stereo-audiomix. De apparatuur (Kasina/Limina) filtert die banden er bij
weergave uit en stuurt ze naar de LED-bril. **[bewijs]** —
[Lumasonic format spec](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[productie/mixnotities](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md)

| Drager | Frequentie | Functie |
|---|---|---|
| Referentie | 18.200 Hz | formatdetectie; schaal voor helderheid |
| Rood | 18.700 Hz | amplitude → helderheid rode LED's |
| Groen | 19.200 Hz | amplitude → helderheid groene LED's (tevens AudioStrobe-drager) |
| Blauw | 19.700 Hz | amplitude → helderheid blauwe LED's |

- Elke kleurdrager is een **stereotrack** met onafhankelijke L/R-amplitude; de
  Kasina stuurt daarmee beide ogen apart aan. Dat geeft 3 × 2 = 6 lichtkanalen.
  De Kasina-handleiding noemt expliciet "zes lichtstuursignalen in plaats van
  twee". **[bewijs]** — [Kasina Quickstart](https://mindplacesupport.com/download/1244/)
- De referentie draagt **geen** helderheid over; hij markeert het signaal als
  SpectraStrobe en levert de amplitudeschaal. **[bewijs]** — Lumasonic-spec.
- De referentie moet **hard links/rechts alterneren** met een panning-LFO van
  10–50 cycli per seconde; voorgeschreven is een **sinus**, 20 Hz, offset 0 %,
  diepte 100 %. Square/pulse mag volgens de spec ook, maar geeft meer hoorbare
  artefacten; de decoder is "enigsins vergevingsgezind". **[bewijs]** —
  Lumasonic-spec.
- De 500 Hz-afstand tussen de dragers is de ruimte waarbinnen de decoder de
  banden scheidt. **[inferentie]** — de spec noemt geen filterparameters; de
  afstand en de aanbevolen steile bandpassfilters maken dit de voor de hand
  liggende reden.

### 1.2 Helderheid, panning en referentie

- Helderheid van een kleurkanaal = **amplitude van de kleurdrager ten opzichte
  van de referentie-amplitude**. Volume -INF = kanaal uit; op referentieniveau =
  volledig aan. Pannen van de kleurtrack bepaalt de links/rechts-balans van dat
  kanaal. **[bewijs]** — Lumasonic-productiedoc.
- Er is een **bronconflict** over de exacte verhouding bij 100 % helderheid:
  - de productiedoc-tabel zegt: kleurtrack op referentieniveau = volle
    helderheid (dus 1:1);
  - een forumreactie van een Lumasonic-ontwikkelaar noemt "RGB op -24 dB =
    100 % helderheid" en "referentie = 1/4 van de kleurtonen" (dus 1:4).
  **[open]** — [MindPlace-forumthread](https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content)
- Onze PoC hoeft dit conflict niet te beslechten: met een **koppelingsmaximum
  van 25 % van de referentie** blijft de helderheid onder beide interpretaties
  conservatief (25 % bij 1:1, 6,25 % bij 1:4). Zie §7.
- De referentiepanning is tegelijk een van de veiligheidsrisico's: de LFO
  veroorzaakt in de brute drager een links/rechtsritme van 10–50 per seconde.
  Dat is niet zichtbaar als kleurhelderheid, maar hoort niet in de
  kleurkanalen te lekken. Zie `docs/v2/SAFETY-PRIVACY.md`.

### 1.3 Rekenfeiten voor 44,1 en 48 kHz

| Grootheid | 44.100 Hz | 48.000 Hz |
|---|---|---|
| Samples per cyclus @ 18,2 kHz | 2,423 | 2,637 |
| Samples per cyclus @ 19,7 kHz | 2,239 | 2,437 |
| 19,7 kHz als deel van Nyquist | 89,3 % | 82,1 % |
| 18,2 kHz als deel van Nyquist | 82,5 % | 75,8 % |

- Op 44,1 kHz liggen de dragers extreem dicht bij Nyquist (19,7 kHz op 89 % van
  22,05 kHz). **[bewijs]** — rekenkundig uit de specfrequenties.
- Elke sample-rateconversie, elk filter dat boven ~17 kHz begint te rollen en
  elke lossy codec raakt de dragers dus direct in amplitude of fase.
  **[inferentie]**
- Wordt een 44,1 kHz-bestand op 48 kHz afgespeeld zonder correcte rate, dan
  schuiven alle dragers +8,84 % op: 18,2 kHz wordt ~19,8 kHz, midden in het
  blauw-/referentiedetectiegebied. **[inferentie]** — eenvoudige
  frequentieschaalberekening.
- 44,1 en 48 kHz zijn beide toegestaan; 16-bit PCM WAV is de aanbevolen
  drager. **[bewijs]** —
  [Lumasonic file formats](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/04-file-formats.md),
  [render-output](https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/07-rendering-output.md)

### 1.4 Headroom: waarom vier stereotracks ruimte kosten

- De vier dragers gedragen zich als **vier stereotracks, elk tussen -32 en
  -24 dBFS wanneer volledig aan**. **[bewijs]** — Lumasonic-productiedoc.
- Volledig aan betekent: referentie plus drie kleuren, alle vier op maximaal
  -24 dBFS. Coherent opgeteld is dat 4 × 0,0631 FS ≈ 0,252 FS ≈ **-12 dBFS**;
  op -32 dBFS is de som ≈ -20 dBFS. **[bewijs]** — amplituderekenen.
  De dragers consumeren dus 12–20 dB van de beschikbare piekruimte nog voordat
  er muziek bij komt.
- In de praktijk van de PoC (referentie -30 dBFS, kleuren ≤ 25 % daarvan) is de
  dragerstaart veel kleiner: referentie 0,0316 FS plus drie kleuren van
  0,0079 FS ≈ 0,055 FS ≈ **-25 dBFS** coherent worst case. **[inferentie]** —
  rekenkundig; de gebruikte koppeling is een ontwerpkeuze, geen specwaarde.
- De rest van de mix moet dus onder een **totale true peak van -6 dBFS** blijven,
  inclusief de dragers. Zonder die marge klinkt clipping niet alleen slecht: de
  intermodulatie- en harmonische producten vallen terug in de 18–20 kHz-band en
  worden als valse helderheid gedecodeerd. **[inferentie]** — zie §3.
- 16-bit kwantisatie is geen probleem op deze niveaus: -30 dBFS is ruim
  1.000 LSB. Dithering moet wél uit, want noise shaping duwt ruis precies de
  controlebrand in. **[inferentie]**; Lumasonic adviseert dithering uit
  **[bewijs]** — render-output.

---

## 2. Waarom dit een signaalintegriteitsprobleem is, geen encodingprobleem

De encoder kan perfect zijn en de weergaveketen kan het signaal alsnog
verminken. De volgorde is altijd: **eerst audio volledig verwerken, dan pas
dragers toevoegen, daarna niets meer**. Alle onderstaande bedreigingen tasten
ofwel de dragerfrequentie, de relatieve amplitude, de L/R-fase of de
bandruimte aan.

### 2.1 Bedreigingenmatrix

| Bedreiging | Mechanisme | Effect op licht | Status |
|---|---|---|---|
| Resampling 44,1 ↔ 48 kHz | weinig samples/cyclus; filters bij Nyquist verzwakken 19,7 kHz en maken zijbanden | zwakkere of weggevallen kanalen; detectieverlies | [inferentie] |
| Verkeerde sample rate bij weergave | alle frequenties schalen mee (+8,84 % bij 44,1→48) | decoder herkent niets of verkeerd kanaal | [inferentie] |
| DAW-laagdoorlaat/EQ op master | roll-off vanaf 13–17 kHz, precies volgens Lumasonic-advies voor het *programma* | dragers verzwakt; kleurverhoudingen scheef | [bewijs] advies + [inferentie] effect |
| Saturatie/limiters/clippers na de encoder | harmonischen en intermodulatie; gain-pumping | valse helderheid, flikkering, verandering van kleurverhouding | [inferentie] |
| Lossy codecs (MP3/AAC, Bluetooth) | hoogafsnijding rond 19–20 kHz, psychoakoestiek, joint stereo | 19,7 kHz verdwijnt of L/R-fase vervormt | [inferentie]; MP3 320 kbps CBR stereo wordt door Lumasonic aangeraden [bewijs] |
| OS-audio-EQ/ruimtelijke audio/normalisatie | systeembrede EQ, spatial audio, loudness-processing | onvoorspelbare kleur- en helderheidsfouten | [bewijs] waarschuwing in Lumasonic-doc |
| Monosom | L+R optellen heft de referentiepanning op; 19,2 kHz blijft over | detectie faalt of valt terug op AudioStrobe | [inferentie] |
| Hoofd-/busroutering | encoder op een subgroep die daarna nog door de masterketen gaat (master-EQ, glue-compressor, limiter, "high cut"); of uitgang naar een monobus / verkeerde L/R | detectieverlies, kleurvervorming of ogen verwisseld | [inferentie] |
| Dither/noise shaping bij 16-bit export | ruis wordt naar 15–20 kHz gevormd | verhoogde ruisvloer in controlebranden | [inferentie]; dithering uit is [bewijs] |
| Oude Kasina-firmware + WAV-metadata | firmware speelde metadata als audio aan het einde | luide ruisburst + felle flits bij bestandseinde | [bewijs] — forumthread (2019, later gefixt) |
| USB-audiostuurprogramma | driver-glitches, sample-mismatch | klikken die als flitsen worden gedecodeerd; tijdelijk wegvallen | [bewijs] — forumthread |
| Luide programma-inhoud t.o.v. dragers | decoder verliest de referentie of raakt verzadigd | LED's vallen weg of kleur wordt instabiel | [bewijs] — forumthread (empirisch, één gebruiker) |

### 2.2 Consequenties voor de architectuur

- Dragers **nooit** door NAGLOED's eigen laagdoorlaat, saturatie, LoFi,
  time-stretch of pitch-shift halen; die staan in de EAR-keten. [inferentie op
  basis van issue #14 en de signaalfysica]
- Dragers toevoegen ná alle niet-lineaire processing, in een aparte
  (post-processing) stap of offline render. [technische oplossing]
- Programma vóór het optellen voorbereiden: taper vanaf 13 kHz, steile
  laagdoorlaat bij 16–17 kHz. Dat is exact het Lumasonic-advies. [bewijs]
- Eindverantwoordelijkheid blijft bij de export: één verificatierapport per
  bestand, geen aannames over de host. [inferentie]
- Controleer expliciet of het hoofd-/masterkanaal na de encoder nog processing
  toepast (high-cut, limiter, loudness-normalisatie). Zo ja, dan is dat het
  faalpunt, niet de encoder. [inferentie]

---

## 3. Meetplan en toleranties

Deze tabel is bindend voor de PoC-verificatie. Metingen gebeuren **offline op
het geëxporteerde WAV** (en waar genoemd op de fotodiode-opname), niet op
tussenresultaten in de DAW. Alle toleranties zijn voorlopig: de Kasina-specificatie
publiceert geen decoder-passband, dus dit zijn ontwerpdoelen die op hardware
worden gevalideerd. [open]

| # | Meting | Methode | Doel / tolerantie |
|---|---|---|---|
| M1 | Dragerfrequenties | FFT ≥ 65.536 punten, Hann, parabolische interpolatie | 18.200 / 18.700 / 19.200 / 19.700 Hz, **±1 Hz** |
| M2 | Absolute referentie | RMS van 18,2 kHz-band | **-30,0 dBFS ± 0,5 dB**, constant over het stuk |
| M3 | Relatieve kanaalamplitude | RMS per kleurband t.o.v. referentie | volgt de score binnen **±0,5 dB**; max **25 %** (-12,04 dB) |
| M4 | L/R-balans en isolatie | RMS L en R per kleurband | bij midden **±0,5 dB**; bij hard pannen isolatie **≥ 40 dB** |
| M5 | Referentie-panmodulatie | envelope van 18,2 kHz in L en R | LFO **20 Hz ± 0,5 Hz**, diepte **≥ 99 %** (uit-fase ≥ 30 dB onder), geen restdrager in de "uit"-fase |
| M6 | Crosstalk tussen kleuren | bandenergie op de andere dragers ± 100 Hz | **≤ -40 dB** t.o.v. de eigen drager |
| M7 | Programmaleak in de controlebrand | programma-only render, FFT 17–20 kHz | **≤ -60 dBFS** in elke band |
| M8 | Ruisvloer bij gedode kleuren | FFT van 18,7/19,2/19,7 kHz-band zonder kleursignaal | **≤ -80 dBFS** |
| M9 | Piek / true peak | 4× oversampled true-peak, sample-peak | **≤ -6 dBTP**, geen clipping, geen non-finite samples |
| M10 | Determinisme | twee renders, zelfde seed/platform/rate | **byte-identiek** |
| M11 | Detectie op hardware | Kasina-display + visueel | **120 s** onafgebroken SpectraStr-detectie |
| M12 | LED-uitgang (objectief) | fotodiode + audio-interface, FFT/autocorrelatie | geen periodieke luminantiemodulatie **3–30 Hz met ≥ 10 % diepte**; helderheid monotoon met de ladder |
| M13 | Stop-latentie | gemeten audiosignaal + fotodiode | dragers volledig uit binnen **50 ms** na stopactie |
| M14 | Corruptiematrix | bewust beschadigde varianten (§5.4) | elk geval gedocumenteerd als pass/fail, niet "waarschijnlijk goed" |

Aanvullend: één **programma-only A/B-WAV** (zonder dragers) hoort bij elk
render, zodat M7 en een subjectieve A/B altijd mogelijk zijn. Dat sluit aan bij
de uitvoer van `docs/v2/EXPERIMENT-01.md`.

---

## 4. Hardwareverificatieprotocol

### 4.1 Opstelling

- Mac met de PoC-renderer; **geen** audio-interface in de signaalketen tenzij
  expliciet als testvariant.
- MindPlace Kasina met actuele firmware (de WAV-metadata-bug uit 2019 is in
  latere firmware gefixt; controleer dit). [bewijs] — forumthread
- microSD-kaart met 16-bit PCM WAV's, 44,1 en 48 kHz, **zonder metadata**,
  eenvoudige bestandsnamen.
- Standaard Ganzframes (bedoeld voor ogen dicht) én DeepVision Ganzframes
  (bedoeld voor ogen open, hogere maximale helderheid, diffuus kleurveld).
  [bewijs] — Lumasonic-apparatuurdoc
- Kasina-LED-helderheid op een **vaste, genoteerde stand** (niet "max"); audio-
  en LED-helderheid zijn aparte knoppen. [bewijs] — Lumasonic Kasina-doc
- Optioneel maar sterk aanbevolen: fotodiode (bijv. BPW34) + audio-interface om
  de LED-uitgang objectief vast te leggen (M12, M13).
- **Stopmiddelen binnen handbereik:** software-panic, Kasina-helderheid naar
  minimum, stekker/voeding, bril af. Eén persoon bedient, één persoon kijkt.

### 4.2 Volgorde (niet overslaan, niet parallel)

1. **Veiligheid vooraf.** Lees `docs/v2/SAFETY-PRIVACY.md`. Geen persoon met
   (vermoede) fotosensitiviteit in de ruimte. Ruimte verlicht, geen andere
   flikkerbronnen. Eerste stappen met de **standaard Ganzframes, ogen dicht**,
   Kasina-helderheid laag.
2. **T0 — statische detectie.** 60 s referentie-only, alle kleuren 0 %. Kasina
   moet `Mode SpectraStr/AS` tonen met SpectraStr actief. Zo niet: stop, dit is
   een formaat-/panningfout, geen artistiek probleem.
3. **T1 — helderheidsladder 44,1 kHz.** Eén kleur per keer (R, G, B), daarna
   wit; relatieve amplitude 0, 1/64, 1/32, 1/16, 1/8, 1/4 van de referentie;
   10–15 s per trede, oplopend, 1 s fades, 5 s donker ertussen. Noteer per
   trede: detectie, zichtbare kleur, ongewenste kanalen, flikkering, comfort.
   Bij twijfel of ongemak: direct terug naar 0 en stoppen.
4. **T2 — idem op 48 kHz.** Zelfde ladder, zelfde Kasina-instellingen. Verschil
   in gedrag tussen 44,1 en 48 kHz is een bevinding, geen detail.
5. **T3 — programma + dragers.** Mix met programma op -24, -18, -12 en -6 dBTP;
   zoek het punt waar detectie wegvalt (forumervaring: luide laaginhoud kan de
   decoder laten uitvallen). Dit calibreert de veilige programma-/dragermarge.
6. **T4 — DeepVision, ogen open.** Alleen als T1–T3 comfortabel en stabiel
   waren: dezelfde ladder bij **lagere** Kasina-helderheid, omdat DeepVision
   veel feller overkomt. [bewijs] — Lumasonic-apparatuurdoc
7. **T5 — dynamiek en stop.** 1 s fade-in/out, 50 ms fade, harde cut;
   panic-stop; meet M13. Controleer op klikken en op flitsartefacten.
8. **T6 — objectieve LED-meting.** Fotodiode-opname bij 25 %-maximum: M12.
9. **T7 — corruptiematrix** (§4.4), alleen op lage helderheid en zo nodig met
   de bril losgekoppeld; observeer het Kasina-display als primaire indicator.

Pas na een volledig groene T0–T7 is er een hardwarebasis voor het artistieke
experiment. [technische oplossing]

### 4.3 Acceptatie en stopcriteria

- **Doorgaan** alleen als M11, M12 en M13 halen en de hele ladder zonder
  ongewenste kanalen of flikkering verloopt.
- **Stoppen en rapporteren** bij: detectieverlies, zichtbare flikkering buiten
  de bedoelde kleurovergang, ongemak, hoofdpijn, of een onverklaarbare flits.
  Dit is informatie, geen mislukking; documenteer oorzaak en omstandigheden.
- **Nooit** de helderheid opvoeren om een detectieprobleem te maskeren. Eerst
  signaalintegriteit oplossen (M1–M10), dan pas hardware.

### 4.4 Corruptiematrix (bewust)

| Variant | Verwachting | Doel |
|---|---|---|
| 48 → 44,1 kHz nette SRC | dragers intact, lichte HF-verzwakking | grens van SRC bepalen |
| 48 → 44,1 kHz met goedkope/naïeve SRC | vervorming, detectieverlies | faalmodus zichtbaar maken |
| 44,1-bestand op 48 afgespeeld | frequenties +8,84 %, detectie faalt | rate-mismatch bewijzen |
| Monosom | referentiepanning weg | fallback naar AudioStrobe aantonen |
| L/R verwisseld | detectie mogelijk, ogen verwisseld | tolerantie vastleggen |
| MP3 320 kbps CBR stereo | intact | productiepad valideren |
| MP3 128 kbps / joint stereo | hoorbaar en zichtbaar verlies | ondergrens documenteren |
| Referentie-LFO 10 en 50 Hz | detectie binnen specbereik | speelruimte vastleggen |
| Referentie-LFO 5 Hz | detectie faalt | ondergrens bevestigen |
| Square/pulse-panning i.p.v. sinus | detectie ok, meer artefacten | waveformkeuze onderbouwen |

---

## 5. Faalmodi

| Faalmodus | Observable | Waarschijnlijke oorzaak | Reactie |
|---|---|---|---|
| Geen SpectraStrobe-detectie | Kasina toont AS of ColorOrgan | referentie ontbreekt, LFO te langzaam, monosom, rate-mismatch | M5 en §2.1 nalopen |
| Kleur lekt bij stille kanalen | rode/groene LED's dimmen of flikkeren zonder opdracht | crosstalk, clipping, ruisvloer, oude firmware | M6/M8, dragerniveau, firmware |
| LED's vallen weg bij luide muziek | licht dooft bij bas/dynamiek | onvoldoende referentie t.o.v. programma; decoder raakt referentie kwijt | T3; programma-/dragermarge |
| Felle flits op bestandseinde | ruisburst + flits | WAV-metadata op oude firmware | metadata strippen, firmware bijwerken |
| Onbedoelde helderheidssprongen | licht "pompt" mee met muziek | limiter/saturatie/OS-processing na encoder | §2.1, keten opschonen |
| Verkeerde kleur of oog | rood/groen of L/R verwisseld | busrouting, kanaalvolgorde | M4, routingdtest |
| Detectie traag bij start | eerste seconden geen licht | referentie start te laat of te zacht | ≥ 1 s referentie-lead-in |
| Klikken/flitsen via USB | onregelmatige bursts | USB-driver, sample-mismatch | microSD-baseline, driver reset |
| Licht te zwak bij 25 % | Ganzfeld blijft donker | Kasina-helderheid of LED-kalibratie | hardwarehelderheid verhogen, niet de dragers |

---

## 6. Encoder-eisen voor de PoC

Deze eisen zijn de implementatie-opdracht voor `experiments/spectrastrobe-poc/`.
Ze zijn conservatief: ze mogen de artistieke helderheid beperken, nooit de
veiligheid of de detectie. [technische oplossing]

**Formaat en getallen**

- Sample rates: **48.000 Hz** (master, 24-bit float intern, 24-bit PCM export)
  en **44.100 Hz** (16-bit PCM variant). Alleen deze twee.
- Referentie: **18.200 Hz**, vast **-30,0 dBFS** (piekamplitude), aanwezig
  zolang het stuk duurt; ≥ 1 s lead-in; samen met de kleuren uitfaden bij stop.
- Referentiepanning: **sinus, 20,0 Hz, diepte 100 %**, offset 0 %, per sample
  berekend (geen blok-rate LFO), fasecontinu. Bereik 10–50 Hz is toegestaan;
  standaard 20 Hz.
- Kleuren: **18.700 / 19.200 / 19.700 Hz**, tolerantie **±1 Hz**.
- Kleurkoppeling: amplitude ten opzichte van de referentie, **maximaal 25 %**
  (-12,04 dB), **standaard 6,25 %** (-24,08 dB). Per oor apart, als directe
  kanaalamplitude; **geen -3 dB center-pan law** (die zou de gedecodeerde
  helderheid halveren). [inferentie]
- Som van alle dragers: geen clipping; totale **true peak ≤ -6 dBTP**
  (4× oversampled gemeten), sample-peak ≤ -6 dBFS, geen non-finite samples.

**Programma en volgorde**

- Programma eerst volledig verwerken, daarna dragers optellen; **niets** na de
  encoder: geen EQ, saturatie, limiter of normalisatie.
- Programma-laagdoorlaat: taper vanaf **13 kHz**, steil (≥ 48 dB/oct) bij
  **16 kHz**; gemeten energie 17–20 kHz **≤ -60 dBFS** (M7).
- Programmaniveau kiezen op de totale true peak, niet op luisterervaring.

**Export**

- WAV, PCM, stereo, **geen metadata**, **geen dither/noise shaping**.
- Bestandsnaam en rapport bevatten rate en versie; bij elk render een
  programma-only A/B-WAV en het verificatierapport (M1–M10, M14).

**Tijd en stop**

- Envelopes per kleur/per oor: gladde attacks/releases, **≥ 20 ms**, geen
  stappen of discontinuïteiten.
- Stop/panic: dragers en referentie volledig uit binnen **50 ms**, klikvrij
  (korte ramp ≤ 5 ms), daarna stilte. Geen "laatste flits" bij het stoppen.
- Duur en start: referentie eerst, daarna kleuren; geen detectie-afhankelijke
  opstarttrucs.

**Realtime (alleen als dit later in de plug-in komt)**

- Geen allocatie, locks, file- of netwerkacties in het audiopad; vaste buffers;
  double-precision faseaccumulator; deterministisch.
- De encoder is een aparte stap ná de bestaande DSP-keten, nooit erbinnen.
  [technische oplossing]

**Veiligheid**

- De encoder begrenst zichzelf, ook als de artistieke score meer vraagt:
  max 25 % koppeling, geen herhaalde luminantiemodulatie in 3–30 Hz boven de
  drempels uit `docs/v2/SAFETY-PRIVACY.md`, en een panic-stop die altijd werkt.

---

## 7. Open vragen

1. **[open]** Wat is de exacte decoder-passband en amplitudekarakteristiek van
   de Kasina-firmware? Publieke bronnen noemen alleen "steep bandpass filters".
2. **[open]** Is 100 % helderheid 1:1 of 1:4 ten opzichte van de referentie?
   (§1.2.) Beïnvloedt de absolute kalibratie, niet de veilige bovengrens.
3. **[open]** Hoeveel tolerantie heeft de decoder voor LFO-frequentie,
   golfvorm en duty cycle? De forumdraad suggereert "enige speelruimte"; een
   getal ontbreekt.
4. **[open]** Resamplet de Kasina intern tussen 44,1 en 48 kHz, en met welk
   filter? Bepaalt of de 44,1-variant echt anders is dan de 48-variant.
5. **[open]** Wat doen MP3 320 kbps en AAC met 19,7 kHz en met de L/R-fase van
   de referentie? Alleen met meting te beantwoorden.
6. **[open]** Hoe verhoudt DeepVision-helderheid zich kwantitatief tot de
   standaard Ganzframes? Lumasonic zegt "veel feller"; een getal ontbreekt.
7. **[open]** Wat is de minimale waarneembare helderheid bij 25 % koppeling met
   de Kasina op minimum? Als die te laag is, is de artistieke band te smal en
   moet de koppeling omhoog — met veiligheidscheck.
8. **[open]** Ondersteunt de Kasina 24-bit WAV? Lumasonic adviseert 16-bit;
   onze master is 24-bit. De 16-bit variant is de veilige weergaveversie.
9. **[open]** Kan een VST3-host een schone, onbewerkte stereobus leveren die
   zonder resampling of processing de Kasina haalt? Issue #14 vraagt dit; het
   antwoord is per host verschillend en moet gemeten worden.
10. **[open]** Wat is de latency van sample tot LED, en is die stabiel genoeg om
    licht en geluid als "verwant" te laten voelen? (Artistieke vraag, maar
    technisch meetbaar.)

---

## 8. Bronnen

Primair (specificatie en fabrikantdocumentatie):

- Lumasonic, *SpectraStrobe format specification*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md
- Lumasonic, *Production and mixing*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md
- Lumasonic, *Devices (Kasina/Limina, Ganzframes, DeepVision)*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md
- Lumasonic, *File formats*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/04-file-formats.md
- Lumasonic, *Kasina*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/08-kasina.md
- Lumasonic, *Rendering output*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/07-rendering-output.md
- Lumasonic, *Health and safety*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md
- MindPlace, *Kasina Quickstart Guide*:
  https://mindplacesupport.com/download/1244/
- MindPlace Support Forum, *Recommendations on maintaining precise
  SpectraStrobe control for given audio content?* (2019):
  https://mindplacesupport.com/forum/showthread.php?3173-Recommendations-on-maintaining-precise-SpectraStrobe-control-for-given-audio-content

Secundair / onafhankelijke implementatie:

- SpiraStrober (MIT, browser-based SpectraStrobe/AudioStrobe generator):
  https://github.com/Syluminose/SpiraStrober
  - code-inspectie: dragers 18.700/19.200/19.700/18.200 Hz, sample rate
    44.100 Hz hardcoded, 20 Hz sinuspanning van de referentie, sterke
    relatieve verzwakking van de kleurkanalen, WAV-export. Details uit
    minified code zijn **inferentie**, geen specificatie.

Artistiek/architectonisch kader:

- NAGLOED issue #14: `gh issue view 14 --repo leonvanbokhorst/nagloed`
- `docs/v2/EXPERIMENT-01.md`, `docs/v2/ARCHITECTURE-OPTIONS.md`,
  `docs/v2/SAFETY-PRIVACY.md`
