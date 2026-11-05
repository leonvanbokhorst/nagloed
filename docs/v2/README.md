# NAGLOED v2 — verkenning: Geborgen Geluid

> **Status:** artistieke en technische verkenning, **geen v2-specificatie en geen
> releaseplan**. NAGLOED 1.0.4 is en blijft de stabiele referentie. Er is niets
> gewijzigd aan de DSP-contracten, parameter-IDs, presets, build of
> releasebranch. De verkenning is expliciet geopend door Lonn; de guardrails uit
> issue #14 blijven gelden. Alles hier is uitnodiging, hypothese of open vraag —
> geen implementatieverplichting.

**Kern in één zin:** NAGLOED wordt geen audiovisueel instrument dat geluid
vertaalt naar licht, maar een omgeving met een eigen geheugen en aandacht,
waarin geluid, licht, machines en mensen elkaar kunnen verrassen — ook door
niets te doen.

---

## 1. Samenvatting

- **De vraag is niet welke features erbij kunnen.** De vraag is hoe NAGLOED een
  omgeving wordt waarin geluid, licht, machines en mensen elkaar werkelijk
  kunnen verrassen. Artistieke intentie staat voorop; techniek is ondergeschikt
  aan wat muzikaal de moeite waard blijkt.
- **Verwantschap, geen synesthesie.** Licht en geluid zijn verwanten met een
  gedeelde oorzaak of herinnering, niet twee kanalen van hetzelfde signaal. Ze
  mogen immediate, vertraagd, probabilistisch of helemaal niet op elkaar
  reageren.
- **Drie onderzoekslijnen.** (1) granular audiovisual composition met
  SpectraStrobe/Kasina/DeepVision Ganzfeld; (2) musical agency en generatieve
  compositie via een gebeurtenis- en geheugenmodel; (3) distributed sonic
  environments als langetermijnrichting.
- **Eerste experiment: *Eerste Gloed*.** Een offline studie van 72 seconden met
  één gedeeld gebaar, één bewust late en anders gevormde audio-respons, één late
  herinnering en één onbeantwoord lichtgrain. SpectraStrobe-WAV, vooraf
  geverifieerd, met een conservatief hardwareprotocol.
- **Architectuur: v1 onaantastbaar.** Eerste stap is een losse offline renderer
  (A1); pas daarna dezelfde score door de echte v1-processor (A2). Een
  in-plugin event-seam (B), een losgetrokken core (C) en distributed (D) zijn
  latere fasen, elk met harde poorten.
- **Technische top-risico's.** Waargenomen verwantschap bij delta-tijden
  (U02/U03), SpectraStrobe-integriteit over host/keten (U12/U13), runaway tussen
  audio en licht (U19) en de 12 s-geheugenlimiet (U10).
- **Veiligheid en ethiek zijn ontwerpmateriaal.** Conservatief licht, directe
  stop, geen wellness-claims, toestemming, privacy, tijdelijke opslag en
  local-only verwerking.
- **Geen v2-releaseplanning.** Pas als het eerste experiment muzikaal iets doet,
  volgt een tweede klein experiment. Niet eerder.

---

## 2. De artistieke beweging

NAGLOED 1.0.4 is een instrument van residu: capture, Sintel-grains, Ruimte, EAR.
De audiovisuele richting is een latere artistieke ontdekking, geen
productvereiste. *Geborgen geluid* — het kind onder de tafel op een familiefeest
— is het beeld: vertrouwd, vreemd, poreus, tijdelijk. De wereld gaat door, maar
je bent er even anders in.

De verkenning voegt daar drie soorten materiaal aan toe: **tijd** (nu, later,
nooit), **aandacht** (een budget, geen duty cycle) en **geheugen** (herinneren,
vervormen, vergeten). De mens is de vierde stem: bezoekers, performers,
instrumenten en agents kunnen initiëren, veranderen, imiteren of negeren. Geen
van hen heeft volledige controle.

Licht is daarbij geen visualisatie. Een lichtgrain heeft eigen kleur, envelope,
duur, positie, beweging, persistentie en timing. De omkering is expliciet
toegestaan: een lichtgebeurtenis kan een geluid veroorzaken. De volledige
uitwerking staat in [ARTISTIC-VISION.md](ARTISTIC-VISION.md); de ideeën die
verder gaan dan de opdracht staan in [IDEAS.md](IDEAS.md).

**Artistieke succescriteria** (belangrijker dan feature-lijstjes): het moment
waarop je vergeet of licht en geluid bij elkaar horen; een stilte die klopt; een
herkenbare terugkeer die je niet kunt plaatsen; contrast met gewicht; gedeelde
agency; en na afloop niet "mooi effect", maar "wat was dat".

---

## 3. Drie samenhangende onderzoekslijnen

### 3.1 Granular audiovisual composition

**Intentie.** Onafhankelijke audio- en lichtgrains die ontstaan, evolueren en
elkaar beïnvloeden. SpectraStrobe, Kasina en DeepVision Ganzfeld zijn de eerste
concrete omgevingen: een output-medium, niet het conceptuele model. De Ganzfeld
maakt van licht een diffuus kleurveld in plaats van een beeld van LED's.

**Stand van zaken.** Het SpectraStrobe-signaal is goed gedocumenteerd: vier
hoogfrequente dragers (18,2 kHz referentie met 20 Hz stereo-panning; 18,7/19,2/
19,7 kHz voor R/G/B), helderheid als amplitude ten opzichte van de referentie,
zes lichtkanalen (L/R per kleur). Wat ontbreekt is de decoderkarakteristiek van
de Kasina en de werkelijke signaalintegriteit over hosts, resampling en codecs.
Daarom is meten vóór integreren een harde poort. Zie
[SPECTRASTROBE.md](SPECTRASTROBE.md).

**Eerste concrete vragen.** Overleeft het signaal een 44,1 ↔ 48 kHz-pad en een
DAW-bounce? Wat is de minimale waarneembare helderheid bij 25 % koppeling? Hoe
verhoudt DeepVision-helderheid zich tot de standaard Ganzframes? Welke delta
tussen licht en geluid voelt nog als verwantschap?

### 3.2 Musical agency en generatieve compositie

**Intentie.** Een gebeurtenis- en geheugenmodel dat muzikale contrasten,
autonomie, toeval, herhaling, herinnering en stilte mogelijk maakt — zonder de
realtime DSP afhankelijk te maken van AI-inferentie.

**Hypothese: een klein, begrensd vocabulaire.** Acht event-kinds
(`audio_grain`, `light_grain`, `score_gesture`, `rest`, `remember`, `forget`,
`echo`, `ignore`), vaste eigenschappen, sampletijd, één seed, refractaire
periodes, aandachtsbudgetten en causale diepte ≤ 3. `rest` en `ignore` zijn
gelijkwaardige gebeurtenissen, geen afwezigheid van data. Dit vocabulaire staat
volledig los van `EngineParams`, `VisualState` en de bestaande DSP; het is een
toetsbare hypothese, geen API. Uitwerking:
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §5.

**AI buiten de realtime loop.** Een interessant patroon is de *offstage
componist*: een script of model compileert offline een partituur; op de vloer
speelt alleen de gevalideerde, read-only partituur. Geen inferentie, geen
netwerk, geen model in de kamer. Dat maakt AI een auteur en niet een
onvoorspelbare laag in de audio-thread. Zie [IDEAS.md](IDEAS.md) #1.

**Open vragen.** Wanneer voelt een niet-reactie als intentie in plaats van bug?
Hoe lang mag een herinnering duren voordat ze sentimenteel wordt? Hoe voorkomen
we dat een gedeelde oorzaak een feedback-lus wordt? Hoe verhouden
aandachtsbudgetten zich tot muzikale spanning?

### 3.3 Distributed sonic environments

**Intentie.** Meerdere NAGLOED-instanties, computers, ruimtes en performers die
samenwerken. Wat reist is geen audio maar een telegram: een korte beschrijving
van een gebeurtenis met tijd, soort, kleur en een mate van zekerheid. Een
herinnering kan tussen ruimtes reizen, verkeerd begrepen worden of nooit
aankomen. Lokale autonomie is de default; verbinding is de uitzondering.

**Technische richting (hypothese).** Event-only over OSC/UDP, logische star met
een hub die alleen logt, begrensde queues, rate limits, ttl/confidence-verval,
geen gedeelde klok in de eerste test. Netwerkaudio (AES67/Dante/AVB) en
DMX/Art-Net/sACN zijn latere, aparte vragen. Distributie is het derde
experiment, niet het eerste. Detail:
[DISTRIBUTED.md](DISTRIBUTED.md).

**Open vragen.** Bij welke vertraging voelt een remote gebeurtenis nog verwant?
Hoeveel verlies verdraagt een ruimte voordat ze afwezig voelt? Is intentionele
drift productief of storend? Wat is de minimale gedeelde klok?

---

## 4. Architectuur: wat hergebruiken, wat zelfstandig

### 4.1 Wat de huidige code echt is

- `VisualState` / `SnapshotChannel` is een **displaykanaal** (1× per blok,
  verlieslatend, geen tijdstempels of identiteit), **geen eventbus**. De
  bestaande `getVisual()` is alleen voor de GUI.
- De enige echte realtime-seam is de audioblokgrens plus blokgekwantiseerde
  APVTS-parameters. De interne control-rate van 16 samples is een
  rekensnelheid, geen publieke event-resolutie.
- Er is in v1 geen licht-engine, geen SpectraStrobe-encoder, geen event- of
  geheugenmodel, geen post-DSP-routing en geen netwerklaag.
- Bewezen eigenschappen om te bewaren: 0 allocaties in `process()`, 51 samples
  latency bij 48 kHz, blokgrootte-onafhankelijk binnen 2 % RMS, 12 s capture-ring,
  256 grains.

### 4.2 Vier decomposities

| Optie | Wat | Hergebruik | Zelfstandig | Verdict |
|---|---|---|---|---|
| **A** | v1 onaangetast + externe companion/offline renderer | v1 volledig; patroon van `tools/render_main.cpp` | score, event-engine, licht, encoder, verificatie | **Aanbevolen eerste stap** |
| **B** | in-plugin control-rate event-seam + lichte engine | audiopad, capture-ring | licht-engine, scheduler, geheugen, encoder | Fase 2, pas na A + hardware |
| **C** | losgetrokken core + nieuwe v2-host | GrainEngine, Ruimte, EAR, EngineDSP | core-library + nieuwe host | Middellange termijn, aparte branch |
| **D** | distributed peer-model | eventvocabulaire en encoder per node | netwerklaag, lokale klok | Horizon |

### 4.3 Aanbevolen richting

**A1 — losse offline renderer, zonder v1-code.** Een declaratieve score, een
eigen deterministische renderer, een licht-engine en een SpectraStrobe-encoder;
uitvoer als 48 kHz/24-bit WAV, 44,1 kHz/16-bit variant, programma-only A/B-WAV,
`events.csv` en tijdlijn. Dit is de artistieke kern van issue #14 en het minste
risico voor v1.0.4. De uitvoerbare vorm staat in
`experiments/spectrastrobe-poc/` en [EXPERIMENT-01.md](EXPERIMENT-01.md).

**A2 — dezelfde score door de echte v1-processor.** Alleen als A1 muzikaal
slaagt: een headless render-tool in de geest van `tools/render_main.cpp`, zonder
wijziging aan `src/`. Zo klinkt hetzelfde materiaal met echt NAGLOED-materiaal.

**Harde poorten.** Geen integratie zonder (1) SpectraStrobe-verificatie op echte
hardware, conservatief en met directe stop; (2) byte-identieke deterministische
rerender; (3) aantoonbaar begrensde anti-runaway-scenario's. Als de dragers een
host-bounce niet overleven of de hardwaredetectie onbetrouwbaar is: stoppen, en
niet naar B gaan. B pas achter een build-flag en met expliciete toestemming van
Lonn; C als nieuwe branch; D als onderzoekslijn.

### 4.4 Niet aanraken

`src/core/ParamIDs.h`, parameterlayout en -ranges, `presets/*.json`,
`Engine`/`EngineDSP`-contract, `GrainEngine`-contract (256 stemmen, 12 s),
`VisualState`/`SnapshotChannel`, signaalvolgorde en latency, `CMakeLists.txt`,
`CMakePresets.json`, `scripts/`, `.github/`, testdrempels, `Demo/` en de
release-artefacten, `main`, `release/1.0.4` en tag `v1.0.4`, en de GPL/JUCE-
licentievoorwaarden. De volledige lijst met bewijs staat in
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §4.

---

## 5. Technische onzekerheden

Het volledige register telt 29 aannames (U01–U29) met feit/hypothese, toets,
impact en status: [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md). De
belangrijkste:

| ID | Onzekerheid | Status | Eerste toets |
|---|---|---|---|
| U02/U03 | Ontstaat verwantschap zonder synchronie, en bij welke delta? | Open | Twee renders: strak synchroon vs. gedeelde oorzaak met 80–200 ms offset; hardware-/luistertest |
| U10 | De 12 s capture-ring is te kort voor herinneringen op compositieschaal | Open | Scenariotest: fragment op t=0, terugkeer op t=90 s |
| U12/U13 | SpectraStrobe overleeft de destructieve keten en host-bounce niet; routing is onbewezen | Fase 1/2 | Corruptiematrix en hostmatrix met de analyzer uit de PoC |
| U15 | Dragers zijn "onhoorbaar" en acceptabel voor omstanders | Open | Sinustest, spectrumanalyse, comfortmeting |
| U17/U18 | Een bounded event-engine kan realtime-veilig en zonder allocaties | Fase 2 | Prototype met allocatieteller en CPU-meting |
| U19 | Refractaire periodes en budgets voorkomen runaway tussen audio en licht | Fase 2 | Adversariële scores, invariantencheck |
| U22 | Eén seed geeft reproduceerbare audio- én licht-renders | Deels | Byte-identieke rerender; tweede platform met tolerantie |
| U23 | AI blijft buiten de realtime loop | Open | Score-compiler-PoC; review van de datastroom |

**SpectraStrobe in het kort.** De dragers leven op 18,2–19,7 kHz, dicht bij
Nyquist op 44,1 kHz. Elke resampling, DAW-lowpass, saturatie, limiter, lossy
codec, monosom of OS-audio-EQ kan ze beschadigen; de encoder moet daarom ná alle
processing staan en elk bestand moet een verificatierapport krijgen. De
decoderkarakteristiek van de Kasina is niet publiek; ook de verhouding bij 100 %
helderheid is tegenstrijdig in de bronnen. De gekozen 25 %-koppeling is onder
beide interpretaties conservatief. Zie [SPECTRASTROBE.md](SPECTRASTROBE.md).

**Open technische vragen (selectie).** Wat is de decoder-passband? Resamplet de
Kasina intern tussen 44,1 en 48 kHz? Welke DAW/exportstap levert een schone,
onbewerkte stereobus? Is sample-accuraatheid artistiek nodig of is 50 ms al
"gelijktijdig"? Hoe lang mag een event- of herinneringswachtrij zijn voordat
drop-gedrag hoorbaar wordt? Hoe voorkomen we dat een echo van een echo een lus
wordt?

---

## 6. Eerste experiment — *Eerste Gloed*

**De vraag.** Kan één gedeeld gebaar een voelbare verwantschap maken tussen één
lichtgrain en één audiogebeurtenis — zonder dat ze synchroon bewegen — en is een
bewust niet-antwoord even overtuigend als een antwoord?

**De compositie (72 s).** Een amberkleurig lichtgrain drijft van links naar
rechts en dooft abrupt op 14,0 s. Een lage audio-impact antwoordt 180 ms later
met een andere, veel langere envelope. Het licht leeft daarna nog even door als
diffuus veld. Tussen 28 en 46 s gebeurt bijna niets; één zwak lichtgrain om 38 s
blijft opzettelijk onbeantwoord. Op 46 s keert een getransformeerde herinnering
terug — omgekeerd, verlaagd, dimmer — met een lichtecho die niet samenvalt. Het
stuk eindigt in stilte, zonder slotakkoord.

**Waarom dit klein en veilig is.** Offline gerenderd; geen plug-inwijziging,
geen host-integratie, geen microfoon, geen netwerk, geen bezoekers. Conservatieve
helderheid, korte duur, directe stop in het protocol. Alles staat in
`experiments/spectrastrobe-poc/`, buiten de build en buiten de release.

**Verificatie vóór hardware.** De PoC meet carrierfrequenties, relatieve
amplitudes, crosstalk, de 20 Hz-panmodulatie, programma-energie boven 17 kHz,
piek/true peak, clipping en determinisme. Daarna volgt pas het
hardwareprotocol: Kasina + DeepVision Ganzfeld, helderheidsladder van 0 via
1/64 en 1/16 naar 1/4 van de referentie, 44,1 en 48 kHz, directe stop,
observatieformulier. Zie [SPECTRASTROBE.md](SPECTRASTROBE.md) §4 en
`experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md`.

**Wat we meten.** Technisch: de bovenstaande metingen met expliciete
toleranties. Perceptueel: vijf vragen over verwantschap, vertraging, stilte,
het niet-antwoord en wat blijft hangen. Succes is niet "het licht volgt het
geluid", maar: de vertraging voelt niet als fout, de stilte voelt als
compositie, de terugkeer is herkenbaar zonder plaatsbaar te zijn, en na afloop
denk je niet "mooi effect" maar "wat was dat".

**Wat het niet bewijst.** Geen realtime plug-inhaalbaarheid, geen host-routing,
geen distributed werking, geen AI, geen definitieve architectuur. Hardware-
validatie moet door Lonn gebeuren; zonder Kasina/Ganzfeld blijft dat een open
poort.

**Vervolg alleen als het muzikaal iets doet.** Eerst een tweede experiment over
geheugen en verval; dan een derde over twee kamers die alleen gebeurtenissen
uitwisselen. Pas daarna nadenken over een gedeelde architectuur. Geen
v2-releaseplanning.

---

## 7. Veiligheid, ethiek en toestemming

Licht vlak bij en rondom het gezichtsveld is een ander risico dan een scherm.
De Ganzfeld vult het gezichtsveld en zit dicht op de ogen; de DeepVision is
extra fel. Daarom gelden harde regels, geen UI-adviezen: standaard geen
herhaalde flikkering, nooit meer dan drie flitsen per seconde, geen aanhoudende
3–30 Hz-modulatie, kleurkanalen maximaal 25 % van de referentie (standaard
6,25 %), gladde envelopes (≥ 20 ms hard, ≥ 50 ms artistiek), nooit van zwart
naar vol, rood extra voorzichtig, elke sessie beginnen donker en laag, en een
panic-stop die alle dragers binnen 50 ms klikvrij naar nul brengt. Geen
"veilig voor iedereen"-claim. Zie [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) §3–4.

Voor bezoekersinteractie gelden toestemming, privacy en tijdelijkheid:
opgevangen geluid is in principe vluchtig, bewaren is een bewuste keuze, geen
cloud en geen profilering, en omstanders zijn niet automatisch deelnemers. Een
fysiek teken (zoals een kaart op tafel) kan van luisteren een afgesproken
toestand maken. Distributed telegrammen bevatten geen audio en geen PII; de
netwerkmodus staat standaard uit en werkt alleen op een vertrouwd lokaal
netwerk. Geen medische, therapeutische of wellness-claims, ook niet in
presetnamen of UI.

---

## 8. Onafhankelijke review

De verkenning is onafhankelijk gereviewd door twee reviewers die niet aan de
inhoud hebben meegewerkt: één op technische haalbaarheid en signaalintegriteit,
één op artistieke intentie, veiligheid en scope-guardrails. Hun verdicten en de
verwerking daarvan staan in [REVIEWS.md](REVIEWS.md). [REVIEW-PLACEHOLDER]

---

## 9. Wat we niet doen, en wat nu voorligt

**Expliciet niet.** Geen wijziging aan NAGLOED 1.0.4 of zijn DSP-contracten;
geen nieuwe parameter-IDs, presets of UI; geen integratie in de releasebranch;
geen v2-releaseplanning; geen AI-inferentie in de realtime DSP; geen always-on
ambient passthrough; geen kleurenorgel of één-op-één mapping; geen
therapie-/meditatieclaims; geen netwerk-, DMX- of netwerkaudiocode in de
plug-in; geen cloud.

**Wat nu voorligt.** (1) Lonn beoordeelt deze verkenning en de PoC. (2) Als de
artistieke richting klopt: de hardwareverificatie met Kasina + DeepVision
Ganzfeld, conservatief en met directe stop. (3) Alleen bij een groen signaal en
een muzikaal overtuigend eerste experiment: A2, daarna een tweede klein
experiment. Geen grootschalige bouw voordat er iets is ontdekt dat muzikaal de
moeite waard is.

---

## 10. Documenten en bronnen

| Document | Inhoud |
|---|---|
| [ARTISTIC-VISION.md](ARTISTIC-VISION.md) | Aangescherpte artistieke visie, succescriteria, open artistieke vragen |
| [ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) | Huidige architectuur met bewijs, decomposities A–D, eventvocabulaire, niet-aanrakenlijst |
| [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md) | 29 aannames U01–U29 met toets en status |
| [SPECTRASTROBE.md](SPECTRASTROBE.md) | Signaalmodel, integriteitsmatrix, meetplan M1–M14, hardwareprotocol, encoder-eisen |
| [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) | Fotosensibiliteit, harde ontwerpregels R1–R12, stop, toestemming, privacy, retentie |
| [DISTRIBUTED.md](DISTRIBUTED.md) | Telegrammen tussen ruimtes, topologie, tijd, faalmodi, security |
| [IDEAS.md](IDEAS.md) | Tien mogelijkheden buiten de opdracht, elk met kleinste test |
| [EXPERIMENT-01.md](EXPERIMENT-01.md) | *Eerste Gloed*: vraag, score, metingen, succescriteria, vervolg |
| `experiments/spectrastrobe-poc/` | Uitvoerbare offline PoC, verificatie en hardwareprotocol |
| Issue [#14](https://github.com/leonvanbokhorst/nagloed/issues/14) | Oorspronkelijke artistieke richting en guardrails |

Belangrijkste externe bronnen: [Lumasonic SpectraStrobe-specificatie](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[productie/mixnotities](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md),
[SpiraStrober](https://github.com/Syluminose/SpiraStrober),
[OSC 1.0](https://ccrma.stanford.edu/groups/osc/spec-1_0.html),
[Art-Net 4](https://art-net.org.uk/downloads/art-net.pdf),
[AES67-2018 PICS](https://aes.org/wp-content/uploads/2024/05/AES67-2018-PICS-Summary_Merging-Technologies_HORUS_3.9_Audio-interface-to-and-from-AoIP-for-Recording.pdf),
[W3C WCAG 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html),
[Epilepsy Society](https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf).
